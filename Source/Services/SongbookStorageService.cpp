/*
  ==============================================================================

    SongbookStorageService.cpp

  ==============================================================================
*/

#include "SongbookStorageService.h"
#include "LibraryScanner.h"
#include "FirestoreClient.h"
#include "../Firebase/FirebaseConfig.h"

//==============================================================================
SongbookStorageService& SongbookStorageService::getInstance()
{
    static SongbookStorageService instance;
    return instance;
}

juce::String SongbookStorageService::storageObjectPath(const juce::String& venueId)
{
    return "venues/" + venueId + "/songbook.json";
}

//==============================================================================
namespace
{
    // GET the object's metadata (no ?alt=media -- we only need the status
    // code, not the file contents). Returns the HTTP status, or 0 if the
    // request couldn't even be sent (network down, etc).
    int checkStorageObjectExists(const juce::String& objectPath)
    {
        auto url = juce::URL("https://firebasestorage.googleapis.com/v0/b/" + FirebaseConfig::storageBucket
                              + "/o/" + juce::URL::addEscapeChars(objectPath, true));

        int status = 0;
        const auto headers = "Authorization: Bearer " + FirestoreClient::getInstance().getFreshIdToken() + "\r\n"
                             + "Accept: application/json";

        auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(15000)
            .withExtraHeaders(headers)
            .withHttpRequestCmd("GET")
            .withStatusCode(&status);

        auto stream = std::unique_ptr<juce::InputStream>(url.createInputStream(opts));
        if (stream != nullptr)
            stream->readEntireStreamAsString(); // drain, we only care about `status`

        return status;
    }

    bool uploadJsonToStorage(const juce::String& objectPath, const juce::MemoryBlock& data, juce::String& outError)
    {
        auto url = juce::URL("https://firebasestorage.googleapis.com/v0/b/" + FirebaseConfig::storageBucket
                              + "/o?uploadType=media&name=" + juce::URL::addEscapeChars(objectPath, true));
        url = url.withPOSTData(data);

        int status = 0;
        const auto headers = "Authorization: Bearer " + FirestoreClient::getInstance().getFreshIdToken() + "\r\n"
                             + "Content-Type: application/json\r\n"
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
}

//==============================================================================
void SongbookStorageService::uploadLocalSongbook(const juce::String& venueId,
                                                  std::function<void(bool ok, juce::String error)> onDone)
{
    const auto objectPath = storageObjectPath(venueId);

    juce::Thread::launch([objectPath, onDone]
    {
        const auto localFile = LibraryScanner::getDefaultSongbookFile();

        juce::MemoryBlock data;
        if (! localFile.existsAsFile() || ! localFile.loadFileAsData(data))
        {
            if (onDone)
                juce::MessageManager::callAsync([onDone] { onDone(false, "No local songbook.json found to upload."); });
            return;
        }

        juce::String error;
        const bool ok = uploadJsonToStorage(objectPath, data, error);

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok, error] { onDone(ok, error); });
    });
}

void SongbookStorageService::seedEmptySongbook(const juce::String& venueId,
                                                std::function<void(bool ok, juce::String error)> onDone)
{
    const auto objectPath = storageObjectPath(venueId);

    juce::Thread::launch([objectPath, onDone]
    {
        juce::MemoryBlock data("[]", 2);
        juce::String error;
        const bool ok = uploadJsonToStorage(objectPath, data, error);

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok, error] { onDone(ok, error); });
    });
}

void SongbookStorageService::ensureSongbookExists(const juce::String& venueId,
                                                   std::function<void(bool exists, bool justUploaded, juce::String error)> onDone)
{
    const auto objectPath = storageObjectPath(venueId);

    juce::Thread::launch([objectPath, venueId, onDone]
    {
        const int status = checkStorageObjectExists(objectPath);

        if (status == 200)
        {
            if (onDone)
                juce::MessageManager::callAsync([onDone] { onDone(true, false, {}); });
            return;
        }

        // Missing (404) or the check itself failed (status == 0, e.g. no
        // network) -- either way, try to fall back to whatever's scanned
        // locally on this PC rather than leaving TAGG with nothing.
        const auto localFile = LibraryScanner::getDefaultSongbookFile();
        if (! localFile.existsAsFile())
        {
            if (onDone)
                juce::MessageManager::callAsync([onDone] { onDone(false, false, {}); });
            return;
        }

        juce::MemoryBlock data;
        if (! localFile.loadFileAsData(data))
        {
            if (onDone)
                juce::MessageManager::callAsync([onDone] { onDone(false, false, "Could not read local songbook.json"); });
            return;
        }

        juce::String error;
        const bool uploaded = uploadJsonToStorage(objectPath, data, error);

        if (onDone)
            juce::MessageManager::callAsync([onDone, uploaded, error] { onDone(uploaded, uploaded, error); });
    });
}
