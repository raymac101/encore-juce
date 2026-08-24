/*
  ==============================================================================

    SongDeliveryService.cpp

  ==============================================================================
*/

#include "SongDeliveryService.h"
#include "FirestoreClient.h"
#include "VenueService.h"
#include "../Firebase/FirebaseConfig.h"
#include "../Models/CdgSong.h"

namespace
{
    using FC = FirestoreClient;

    const juce::StringArray kAudioExtensions { ".mp3", ".wav", ".m4a", ".aac", ".flac", ".ogg" };

    juce::String storageObjectPath(const juce::String& venueId, const juce::String& songId, const juce::String& ext)
    {
        return "Venues/" + venueId + "/pendingSongs/" + songId + ext;
    }

    bool uploadBinaryToStorage(const juce::String& objectPath, const juce::MemoryBlock& data, juce::String& outError)
    {
        auto url = juce::URL("https://firebasestorage.googleapis.com/v0/b/" + FirebaseConfig::storageBucket
                              + "/o?uploadType=media&name=" + juce::URL::addEscapeChars(objectPath, true));
        url = url.withPOSTData(data);

        int status = 0;
        const auto headers = "Authorization: Bearer " + FC::getInstance().getFreshIdToken() + "\r\n"
                             + "Content-Type: application/octet-stream\r\n"
                             + "Accept: application/json";

        auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(30000)
            .withExtraHeaders(headers)
            .withHttpRequestCmd("POST")
            .withStatusCode(&status);

        auto stream = std::unique_ptr<juce::InputStream>(url.createInputStream(opts));
        if (stream == nullptr)
        {
            outError = "Storage upload connection failed.";
            return false;
        }

        stream->readEntireStreamAsString();
        if (status < 200 || status >= 300)
        {
            outError = "Storage upload failed (HTTP " + juce::String(status) + ")";
            return false;
        }
        return true;
    }

    bool downloadBinaryFromStorage(const juce::String& objectPath, juce::MemoryBlock& outData)
    {
        auto url = juce::URL("https://firebasestorage.googleapis.com/v0/b/" + FirebaseConfig::storageBucket
                              + "/o/" + juce::URL::addEscapeChars(objectPath, true) + "?alt=media");

        int status = 0;
        const auto headers = "Authorization: Bearer " + FC::getInstance().getFreshIdToken() + "\r\n"
                             + "Accept: */*";

        auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(30000)
            .withExtraHeaders(headers)
            .withHttpRequestCmd("GET")
            .withStatusCode(&status);

        auto stream = std::unique_ptr<juce::InputStream>(url.createInputStream(opts));
        if (stream == nullptr || status != 200)
            return false;

        stream->readIntoMemoryBlock(outData);
        return true;
    }

    // "Artist - Song" is the common karaoke-file naming convention (also
    // used informally by LibraryScanner's filename parsing); fall back to
    // the whole base name as the song title if there's no " - " separator.
    void splitArtistSong(const juce::String& baseName, juce::String& outArtist, juce::String& outSong)
    {
        const auto sep = baseName.indexOf(" - ");
        if (sep >= 0)
        {
            outArtist = baseName.substring(0, sep).trim();
            outSong   = baseName.substring(sep + 3).trim();
        }
        else
        {
            outArtist = {};
            outSong   = baseName.trim();
        }
    }

    // Raw .cdg picks need their same-basename audio companion zipped
    // alongside them (mirroring LibraryScanner's own CDG+audio pairing,
    // LibraryScanner.cpp) so the result is a self-contained, already-
    // recognized .zip song file on the receiving end. Returns the file to
    // actually upload (either `source` itself, or a temp zip -- caller owns
    // cleanup of the latter via the returned bool).
    juce::File prepareUploadFile(const juce::File& source, bool& outIsTempZip)
    {
        outIsTempZip = false;
        const auto ext = source.getFileExtension().toLowerCase();
        if (ext != ".cdg")
            return source;

        juce::File pairedAudio;
        for (auto& audioExt : kAudioExtensions)
        {
            auto candidate = source.getParentDirectory().getChildFile(source.getFileNameWithoutExtension() + audioExt);
            if (candidate.existsAsFile())
            {
                pairedAudio = candidate;
                break;
            }
        }

        auto tempZip = juce::File::createTempFile(".zip");
        juce::ZipFile::Builder builder;
        builder.addFile(source, 6);
        if (pairedAudio.existsAsFile())
            builder.addFile(pairedAudio, 6);

        auto stream = tempZip.createOutputStream();
        if (stream == nullptr || ! builder.writeToStream(*stream, nullptr))
            return source; // fall back to uploading the bare .cdg rather than failing outright

        stream.reset();
        outIsTempZip = true;
        return tempZip;
    }
}

//==============================================================================
SongDeliveryService& SongDeliveryService::getInstance()
{
    static SongDeliveryService instance;
    return instance;
}

juce::File SongDeliveryService::getCloudSongsFolder()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("EncoreKaraoke").getChildFile("CloudSongs");
}

void SongDeliveryService::uploadSongToVenues(const juce::File& localFile,
                                             const std::vector<juce::String>& venueIds,
                                             UploadCallback onDone)
{
    if (! localFile.existsAsFile() || venueIds.empty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, 0, 0, "No file or no target venues."); });
        return;
    }

    juce::Thread::launch([localFile, venueIds, onDone = std::move(onDone)]()
    {
        bool isTempZip = false;
        auto uploadFile = prepareUploadFile(localFile, isTempZip);

        juce::MemoryBlock data;
        if (! uploadFile.loadFileAsData(data))
        {
            if (isTempZip) uploadFile.deleteFile();
            if (onDone) juce::MessageManager::callAsync([onDone, total = (int) venueIds.size()]
                { onDone(false, 0, total, "Could not read song file."); });
            return;
        }

        juce::String artist, song;
        splitArtistSong(localFile.getFileNameWithoutExtension(), artist, song);

        const auto songId = juce::Uuid().toString();
        const auto ext = uploadFile.getFileExtension().toLowerCase();
        const auto uploadedBy = FC::getInstance().getUserId();

        int succeeded = 0;
        juce::String lastError;
        for (auto& venueId : venueIds)
        {
            const auto objectPath = storageObjectPath(venueId, songId, ext);
            juce::String uploadError;
            if (! uploadBinaryToStorage(objectPath, data, uploadError))
            {
                lastError = uploadError;
                continue;
            }

            auto fields = FC::makeFields({
                { "songName",    FC::stringValue(song) },
                { "artistName",  FC::stringValue(artist) },
                { "storagePath", FC::stringValue(objectPath) },
                { "uploadedAt",  FC::timestampValue(juce::Time::getCurrentTime()) },
                { "uploadedBy",  FC::stringValue(uploadedBy) }
            });

            if (FC::getInstance().createDocument("venues/" + venueId + "/pendingSongs", fields, songId).isObject())
                ++succeeded;
            else
                lastError = "Could not write pending-song record.";
        }

        if (isTempZip)
            uploadFile.deleteFile();

        const int total = (int) venueIds.size();
        if (onDone)
            juce::MessageManager::callAsync([onDone, succeeded, total, lastError]
                { onDone(succeeded == total, succeeded, total, lastError); });
    });
}

void SongDeliveryService::getPendingSongs(const juce::String& venueId, PendingSongsCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(true, {}, {}); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        bool ok = false;
        auto docs = FC::getInstance().listCollection("venues/" + venueId + "/pendingSongs", 100, &ok);

        std::vector<PendingSong> out;
        out.reserve((size_t) docs.size());
        for (auto& d : docs)
        {
            PendingSong s;
            s.id          = d.getProperty("name", "").toString().fromLastOccurrenceOf("/", false, false);
            s.songName    = FC::readString(d, "songName");
            s.artistName  = FC::readString(d, "artistName");
            s.storagePath = FC::readString(d, "storagePath");
            if (s.id.isNotEmpty() && s.storagePath.isNotEmpty())
                out.push_back(std::move(s));
        }

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok, out = std::move(out)]() mutable
                { onDone(ok, std::move(out), {}); });
    });
}

void SongDeliveryService::dismissPendingSong(const juce::String& venueId, const juce::String& songId, WriteCallback onDone)
{
    if (venueId.isEmpty() || songId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Missing venueId/songId"); });
        return;
    }

    juce::Thread::launch([venueId, songId, onDone = std::move(onDone)]()
    {
        const bool ok = FC::getInstance().deleteDocument("venues/" + venueId + "/pendingSongs/" + songId);
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok] { onDone(ok, ok ? juce::String() : juce::String("deleteDocument failed")); });
    });
}

void SongDeliveryService::downloadAndInstall(const juce::String& venueId, const PendingSong& song, WriteCallback onDone)
{
    if (venueId.isEmpty() || song.storagePath.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Missing venueId/storagePath"); });
        return;
    }

    juce::Thread::launch([venueId, song, onDone = std::move(onDone)]()
    {
        juce::MemoryBlock data;
        if (! downloadBinaryFromStorage(song.storagePath, data))
        {
            if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Download failed."); });
            return;
        }

        auto destFolder = SongDeliveryService::getCloudSongsFolder().getChildFile(venueId);
        destFolder.createDirectory();

        const auto ext = juce::File(song.storagePath).getFileExtension();
        const auto baseName = song.artistName.isNotEmpty()
            ? (song.artistName + " - " + song.songName) : song.songName;
        auto destFile = destFolder.getChildFile(juce::File::createLegalFileName(baseName) + ext);

        destFile.deleteFile();
        if (! destFile.getParentDirectory().createDirectory()
            || ! destFile.replaceWithData(data.getData(), data.getSize()))
        {
            if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Could not save downloaded song locally."); });
            return;
        }

        CdgSong newSong;
        newSong.id         = song.id.toStdString();
        newSong.songName   = song.songName.toStdString();
        newSong.artistName = song.artistName.toStdString();

        VenueService::getInstance().addSongToNewSongs(newSong, nullptr);

        const auto songId = song.id;
        FC::getInstance().deleteDocument("venues/" + venueId + "/pendingSongs/" + songId);

        if (onDone)
            juce::MessageManager::callAsync([onDone] { onDone(true, {}); });
    });
}
