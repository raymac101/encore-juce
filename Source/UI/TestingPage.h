#pragma once

#include <JuceHeader.h>
#include <functional>
#include "ResponsiveLayout.h"

class TestingPage : public ResponsiveLayout,
                    private juce::Button::Listener
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
    ~TestingPage() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateAllText();

    std::function<void(int width, int height)> onApplyResolution;
    std::function<void(const SeedOptions& opts,
                       std::function<void(float)> onProgress,
                       std::function<void(bool, juce::String)> onDone)> onCreateQueue;

private:
    void buttonClicked(juce::Button* button) override;
    void updateUIForScreenSize() override;

    void applySelectedResolution();
    void triggerCreateQueue();
    SeedOptions readOptions() const;
    void populateResolutionDropdown();
    juce::String currentWindowResolutionText() const;
    static bool resolutionForSize(ScreenSize size, int& width, int& height);

    juce::Label titleLabel_;
    juce::Label currentResolutionLabel_;
    juce::ComboBox resolutionBox_;
    juce::TextButton applyResolutionButton_;

    juce::Label mobileLabel_;
    juce::Label encoreLabel_;
    juce::Label songsMinLabel_;
    juce::Label songsMaxLabel_;
    juce::Label pitchLabel_;

    juce::Slider mobileSlider_;
    juce::Slider encoreSlider_;
    juce::Slider songsMinSlider_;
    juce::Slider songsMaxSlider_;
    juce::ToggleButton randomPitchToggle_;

    juce::TextButton createQueueButton_;
    juce::Label progressTextLabel_;
    double progressValue_ = 0.0;
    juce::ProgressBar progressBar_;

    bool creating_ = false;
    juce::Array<ScreenSize> resolutionSizes_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestingPage)
};
