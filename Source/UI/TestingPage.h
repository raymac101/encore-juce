#pragma once

#include <JuceHeader.h>
#include <functional>
#include "ResponsiveLayout.h"

// The form lives in the nested ContentPanel, at a self-corrected natural
// size, inside a juce::Viewport -- shrinking the window reveals a
// scrollbar instead of squeezing the fixed-pixel layout into negative-size
// rectangles.
class TestingPage : public ResponsiveLayout
{
public:
    struct SeedOptions
    {
        int numMobileSingers = 5;
        int numEncoreSingers = 5;
        int numSongsMin      = 1;
        int numSongsMax      = 3;
        bool randomPitch     = false;
    };

    TestingPage();
    ~TestingPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateAllText();

    std::function<void(int width, int height)> onApplyResolution;
    std::function<void(const SeedOptions& opts,
                       std::function<void(float)> onProgress,
                       std::function<void(bool, juce::String)> onDone)> onCreateQueue;

private:
    class ContentPanel;
    std::unique_ptr<ContentPanel> content_;
    juce::Viewport viewport_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestingPage)
};
