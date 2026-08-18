/*
  ==============================================================================

    AdsService.cpp

  ==============================================================================
*/

#include "AdsService.h"
#include "FirestoreClient.h"
#include "../Firebase/FirebaseConfig.h"

#include <algorithm>
#include <map>

//==============================================================================
namespace
{
    juce::String trimTrailingSlash (juce::String s)
    {
        while (s.endsWithChar ('/'))
            s = s.dropLastCharacters (1);
        return s;
    }

    juce::String mimeTypeForFile (const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".png")  return "image/png";
        if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".gif")  return "image/gif";
        if (ext == ".webp") return "image/webp";
        if (ext == ".mp4")  return "video/mp4";
        if (ext == ".m4v")  return "video/x-m4v";
        if (ext == ".mov")  return "video/quicktime";
        if (ext == ".webm") return "video/webm";
        return "application/octet-stream";
    }

    juce::var httpGetJson (const juce::URL& url, const juce::StringArray& headers, int* httpStatus)
    {
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs (10000)
                           .withNumRedirectsToFollow (5)
                           .withExtraHeaders (headers.joinIntoString ("\r\n"))
                           .withStatusCode (httpStatus);

        if (auto stream = url.createInputStream (options))
            return juce::JSON::parse (stream->readEntireStreamAsString());

        return {};
    }

    juce::String storageObjectPath (const juce::String& venueId, const juce::String& docId)
    {
        return "Venues/" + venueId + "/Ads/" + docId;
    }

    struct StorageFile
    {
        juce::String name;
        juce::String url;
        juce::String mimeType;
    };

    // Lists Firebase Storage objects directly under Venues/{venueId}/{subfolder}/.
    std::vector<StorageFile> listStorageAdsPrefix (const juce::String& venueId, const juce::String& subfolder)
    {
        std::vector<StorageFile> out;

        juce::StringArray headers;
        const auto token = FirestoreClient::getInstance().getFreshIdToken();
        if (token.isNotEmpty())
            headers.add ("Authorization: Bearer " + token);

        const auto prefix = "Venues/" + venueId + "/" + subfolder;
        const juce::String listUrl = "https://firebasestorage.googleapis.com/v0/b/"
                                   + FirebaseConfig::storageBucket
                                   + "/o?prefix=" + juce::URL::addEscapeChars (prefix, true);

        int status = 0;
        const auto listJson = httpGetJson (juce::URL (listUrl), headers, &status);
        if (! (status >= 200 && status < 300) || ! listJson.isObject())
        {
            DBG ("AdsService: list failed for prefix " + prefix + " (HTTP " + juce::String (status) + ")");
            return out;
        }

        const auto items = listJson.getProperty ("items", juce::var());
        if (auto* arr = items.getArray())
        {
            for (const auto& item : *arr)
            {
                const auto objectPath = item.getProperty ("name", juce::String()).toString();
                if (objectPath.isEmpty() || objectPath.endsWithChar ('/'))
                    continue;

                StorageFile f;
                f.name = juce::File (objectPath).getFileName();
                f.mimeType = item.getProperty ("contentType", juce::String()).toString();

                juce::String tokenValue;
                auto tokenList = item.getProperty ("downloadTokens", juce::String()).toString();
                if (tokenList.isNotEmpty())
                    tokenValue = tokenList.upToFirstOccurrenceOf (",", false, false);

                f.url = "https://firebasestorage.googleapis.com/v0/b/"
                      + FirebaseConfig::storageBucket
                      + "/o/" + juce::URL::addEscapeChars (objectPath, true) + "?alt=media";
                if (tokenValue.isNotEmpty())
                    f.url << "&token=" << juce::URL::addEscapeChars (tokenValue, true);

                out.push_back (std::move (f));
            }
        }

        return out;
    }

    bool uploadBinaryToStorage (const juce::String& objectPath, const juce::MemoryBlock& data,
                                const juce::String& contentType, juce::String& outError)
    {
        auto url = juce::URL ("https://firebasestorage.googleapis.com/v0/b/" + FirebaseConfig::storageBucket
                              + "/o?uploadType=media&name=" + juce::URL::addEscapeChars (objectPath, true));
        url = url.withPOSTData (data);

        int status = 0;
        const auto headers = "Authorization: Bearer " + FirestoreClient::getInstance().getFreshIdToken() + "\r\n"
                             + "Content-Type: " + contentType + "\r\n"
                             + "Accept: application/json";

        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs (60000)
            .withExtraHeaders (headers)
            .withHttpRequestCmd ("POST")
            .withStatusCode (&status);

        auto stream = std::unique_ptr<juce::InputStream> (url.createInputStream (opts));
        if (stream == nullptr)
        {
            outError = "Storage upload connection failed.";
            return false;
        }

        stream->readEntireStreamAsString();
        if (status < 200 || status >= 300)
        {
            outError = "Storage upload failed (HTTP " + juce::String (status) + ")";
            return false;
        }

        return true;
    }

    bool deleteStorageObject (const juce::String& objectPath, juce::String& outError)
    {
        auto url = juce::URL ("https://firebasestorage.googleapis.com/v0/b/" + FirebaseConfig::storageBucket
                              + "/o/" + juce::URL::addEscapeChars (objectPath, true));

        int status = 0;
        const auto headers = "Authorization: Bearer " + FirestoreClient::getInstance().getFreshIdToken() + "\r\n"
                             + "Accept: application/json";

        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs (20000)
            .withExtraHeaders (headers)
            .withHttpRequestCmd ("DELETE")
            .withStatusCode (&status);

        auto stream = std::unique_ptr<juce::InputStream> (url.createInputStream (opts));
        if (stream != nullptr)
            stream->readEntireStreamAsString();

        // 404 means it's already gone -- treat as success, same philosophy
        // as FirestoreClient::deleteDocument().
        if (status != 0 && (status == 404 || (status >= 200 && status < 300)))
            return true;

        outError = "Storage delete failed (HTTP " + juce::String (status) + ")";
        return false;
    }

    bool writeAdMetadataDoc (const juce::String& venueId, const juce::String& docId,
                             const AdMetadata& meta, juce::String& outError)
    {
        auto fields = FirestoreClient::makeFields ({
            { "mediaType",   FirestoreClient::stringValue (meta.mediaType) },
            { "durationSec", FirestoreClient::integerValue (meta.durationSec) },
            { "frequency",   FirestoreClient::integerValue (meta.frequency) },
            { "startDateMs", FirestoreClient::integerValue (meta.startDateMs) },
            { "endDateMs",   FirestoreClient::integerValue (meta.endDateMs) },
        });

        const bool ok = FirestoreClient::getInstance().patchDocument ("venues/" + venueId + "/Ads/" + docId, fields);
        if (! ok)
            outError = "Could not save ad metadata.";
        return ok;
    }
}

//==============================================================================
AdsService& AdsService::getInstance()
{
    static AdsService instance;
    return instance;
}

juce::String AdsService::sanitizeAdDocId (const juce::String& fileName)
{
    juce::String s = fileName.trim();
    s = s.replaceCharacters ("/\\", "__");
    if (s.isEmpty() || s == "." || s == "..")
        s = "ad_" + juce::String (juce::Time::currentTimeMillis());
    if (s.length() > 200)
        s = s.substring (0, 200);
    return s;
}

std::vector<AdMetadata> AdsService::mergeVenueAds (const juce::String& venueId)
{
    std::vector<AdMetadata> out;
    if (venueId.isEmpty())
        return out;

    // Firestore scheduling/rotation docs, keyed by their document id.
    std::map<juce::String, juce::var> metaDocs;
    {
        const auto docs = FirestoreClient::getInstance().listCollection ("venues/" + venueId + "/Ads", 200);
        for (const auto& doc : docs)
        {
            const auto fullName = doc.getProperty ("name", juce::String()).toString();
            const auto docId = fullName.fromLastOccurrenceOf ("/", false, false);
            if (docId.isNotEmpty())
                metaDocs[docId] = doc;
        }
    }

    // Canonical Storage folder, plus legacy lowercase "ads" casing for venues
    // created before the "Ads" (capital A) convention was settled on.
    auto files = listStorageAdsPrefix (venueId, "Ads/");
    {
        auto legacy = listStorageAdsPrefix (venueId, "ads/");
        for (auto& f : legacy)
        {
            const bool dup = std::any_of (files.begin(), files.end(), [&] (const StorageFile& existing)
            {
                return trimTrailingSlash (existing.url) == trimTrailingSlash (f.url);
            });
            if (! dup)
                files.push_back (std::move (f));
        }
    }

    for (auto& f : files)
    {
        AdMetadata ad;
        ad.name = f.name;
        ad.url = f.url;
        ad.mimeType = f.mimeType;
        ad.docId = sanitizeAdDocId (f.name);

        auto it = metaDocs.find (ad.docId);
        if (it != metaDocs.end())
        {
            const auto& doc = it->second;
            ad.hasMetadataDoc = true;
            ad.mediaType = FirestoreClient::readString (doc, "mediaType");
            ad.durationSec = juce::jmax (1, (int) FirestoreClient::readInt (doc, "durationSec", ad.isVideo() ? 8 : 10));
            ad.frequency = juce::jmax (1, (int) FirestoreClient::readInt (doc, "frequency", 1));
            ad.startDateMs = FirestoreClient::readInt (doc, "startDateMs", 0);
            ad.endDateMs = FirestoreClient::readInt (doc, "endDateMs", 0);
        }
        else
        {
            // Storage-only file with no metadata doc yet -- same defaults
            // the idle screen has always used.
            ad.durationSec = ad.isVideo() ? 8 : 10;
            ad.frequency = 1;
        }

        out.push_back (std::move (ad));
    }

    std::sort (out.begin(), out.end(), [] (const AdMetadata& a, const AdMetadata& b)
    {
        return a.name.compareIgnoreCase (b.name) < 0;
    });

    return out;
}

std::vector<AdMetadata> AdsService::fetchActiveAdsSync (const juce::String& venueId)
{
    auto all = mergeVenueAds (venueId);

    std::vector<AdMetadata> active;
    const auto now = juce::Time::currentTimeMillis();
    for (auto& ad : all)
        if (ad.isActiveAt (now))
            active.push_back (std::move (ad));

    return active;
}

void AdsService::listAllAds (const juce::String& venueId, ListCallback onDone)
{
    juce::Thread::launch ([venueId, onDone]
    {
        auto ads = mergeVenueAds (venueId);
        if (onDone)
            juce::MessageManager::callAsync ([onDone, ads = std::move (ads)]() mutable
            {
                onDone (true, std::move (ads));
            });
    });
}

void AdsService::uploadAd (const juce::String& venueId, const juce::File& localFile,
                           AdMetadata meta, WriteCallback onDone)
{
    juce::Thread::launch ([venueId, localFile, meta, onDone]
    {
        juce::MemoryBlock data;
        if (! localFile.existsAsFile() || ! localFile.loadFileAsData (data))
        {
            if (onDone)
                juce::MessageManager::callAsync ([onDone] { onDone (false, "Could not read the selected file."); });
            return;
        }

        const auto docId = sanitizeAdDocId (localFile.getFileName());
        const auto objectPath = storageObjectPath (venueId, docId);
        const auto contentType = mimeTypeForFile (localFile);

        juce::String error;
        if (! uploadBinaryToStorage (objectPath, data, contentType, error))
        {
            if (onDone)
                juce::MessageManager::callAsync ([onDone, error] { onDone (false, error); });
            return;
        }

        const bool ok = writeAdMetadataDoc (venueId, docId, meta, error);
        if (onDone)
            juce::MessageManager::callAsync ([onDone, ok, error] { onDone (ok, error); });
    });
}

void AdsService::updateAdMetadata (const juce::String& venueId, const juce::String& docId,
                                   AdMetadata meta, WriteCallback onDone)
{
    juce::Thread::launch ([venueId, docId, meta, onDone]
    {
        juce::String error;
        const bool ok = writeAdMetadataDoc (venueId, docId, meta, error);
        if (onDone)
            juce::MessageManager::callAsync ([onDone, ok, error] { onDone (ok, error); });
    });
}

void AdsService::deleteAd (const juce::String& venueId, const juce::String& docId, WriteCallback onDone)
{
    juce::Thread::launch ([venueId, docId, onDone]
    {
        juce::String error;
        const auto objectPath = storageObjectPath (venueId, docId);
        const bool storageOk = deleteStorageObject (objectPath, error);

        const bool docOk = FirestoreClient::getInstance().deleteDocument ("venues/" + venueId + "/Ads/" + docId);

        const bool ok = storageOk && docOk;
        if (! ok && error.isEmpty())
            error = "Could not delete the ad.";

        if (onDone)
            juce::MessageManager::callAsync ([onDone, ok, error] { onDone (ok, error); });
    });
}
