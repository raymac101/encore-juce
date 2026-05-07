/*
  ==============================================================================

    MixerPage.h

    Digital mixer page for host-side DSP controls.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Audio/AudioEngine.h"
#include "../Localization/LocalizationManager.h"
#include <array>

class MixerPage : public juce::Component,
                  private juce::Timer
{
public:
    MixerPage();
    ~MixerPage() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setAudioEngine(AudioEngine* engine);
    void updateAllText();

private:
    enum StripIndex
    {
        stripMusic = 0,
        stripVocal,
        stripFx,
        stripPlugin,
        stripMaster,
        stripCount
    };

    struct StripWidgets
    {
        std::unique_ptr<juce::Label> name;
        std::unique_ptr<juce::Slider> fader;
        std::unique_ptr<juce::ToggleButton> mute;
        std::unique_ptr<juce::ToggleButton> solo;
        std::unique_ptr<juce::Slider> eqLow;
        std::unique_ptr<juce::Slider> eqMid;
        std::unique_ptr<juce::Slider> eqHigh;
        std::unique_ptr<juce::ComboBox> insertA;
        std::unique_ptr<juce::ComboBox> insertB;
        std::unique_ptr<juce::TextButton> pluginAButton;
        std::unique_ptr<juce::TextButton> pluginBButton;
        float meterLevel = 0.0f;
        float compOutputMeterLevel = 0.0f;
        float limiterReductionMeterLevel = 0.0f;
        juce::Rectangle<int> compMeterBounds;
        juce::Rectangle<int> limiterMeterBounds;
        juce::Colour accent = juce::Colour(0xff39a9ff);
    };

    std::array<StripWidgets, stripCount> strips;
    AudioEngine* audioEngine = nullptr;

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    void buildStrip(int index, const juce::String& name, juce::Colour accent);
    void bindControlCallbacks();
    void pushStateFromEngine();
    void pushStateToEngine();
    void refreshMasterDynamicsButtons();
    void showCompressorDialog(juce::Component& anchor);
    void showLimiterDialog(juce::Component& anchor);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPage)
};
