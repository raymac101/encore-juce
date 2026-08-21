#include "AdMediaCache.h"

namespace
{
    bool isPlayableVideoFile (const juce::File& file)
    {
        if (! file.existsAsFile() || file.getSize() < 12)
            return false;

        auto input = file.createInputStream();
        unsigned char bytes[12] {};
        if (input == nullptr || input->read (bytes, (int) sizeof (bytes)) != (int) sizeof (bytes))
            return false;

        const bool isIsoMedia = bytes[4] == 'f' && bytes[5] == 't' && bytes[6] == 'y' && bytes[7] == 'p';
        const bool isWebM = bytes[0] == 0x1a && bytes[1] == 0x45 && bytes[2] == 0xdf && bytes[3] == 0xa3;
        return isIsoMedia || isWebM;
    }

    juce::File cacheFileFor (const juce::String& url, const juce::String& fileName)
    {
        auto cacheDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("EncoreKaraoke")
                            .getChildFile ("ad-media");
        cacheDir.createDirectory();

        auto safeName = juce::File::createLegalFileName (fileName);
        if (safeName.isEmpty())
            safeName = "ad-video.mp4";

        return cacheDir.getChildFile (juce::String::toHexString (url.hashCode64()) + "-" + safeName);
    }

    void deliver (AdMediaCache::FetchCallback callback, bool ok,
                  const juce::File& file, const juce::String& error)
    {
        if (callback)
            juce::MessageManager::callAsync ([callback = std::move (callback), ok, file, error]() mutable
            {
                callback (ok, file, error);
            });
    }
}

void AdMediaCache::getOrFetch (const juce::String& url,
                               const juce::String& fileName,
                               FetchCallback callback)
{
    juce::Thread::launch ([url, fileName, callback = std::move (callback)]() mutable
    {
        const auto target = cacheFileFor (url, fileName);
        if (isPlayableVideoFile (target))
        {
            deliver (std::move (callback), true, target, {});
            return;
        }
        target.deleteFile();

        int httpStatus = 0;
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs (30000)
                           .withNumRedirectsToFollow (5)
                           .withStatusCode (&httpStatus);
        auto input = juce::URL (url).createInputStream (options);
        if (input == nullptr || httpStatus < 200 || httpStatus >= 300)
        {
            deliver (std::move (callback), false, {},
                     "Could not download the video (HTTP " + juce::String (httpStatus) + ").");
            return;
        }

        auto temporary = target.getSiblingFile (target.getFileName() + ".download-"
                                                 + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        if (auto output = temporary.createOutputStream())
        {
            output->writeFromInputStream (*input, -1);
            output->flush();
        }

        if (! isPlayableVideoFile (temporary))
        {
            temporary.deleteFile();
            deliver (std::move (callback), false, {}, "The downloaded file is not a supported video.");
            return;
        }

        if (! temporary.moveFileTo (target))
        {
            temporary.deleteFile();
            if (! isPlayableVideoFile (target))
            {
                deliver (std::move (callback), false, {}, "Could not cache the video.");
                return;
            }
        }

        deliver (std::move (callback), true, target, {});
    });
}