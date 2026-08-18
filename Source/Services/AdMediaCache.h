#pragma once

#include <JuceHeader.h>

class AdMediaCache
{
public:
    using FetchCallback = std::function<void (bool ok, juce::File file, juce::String error)>;

    static void getOrFetch (const juce::String& url,
                            const juce::String& fileName,
                            FetchCallback callback);
};