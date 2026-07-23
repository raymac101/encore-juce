/*
  ==============================================================================

    MixerPage.h

    Digital mixer page for host-side DSP controls.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Audio/AudioEngine.h"
#include "../Services/BackgroundMusicPlayer.h"
#include "../Services/PluginHostService.h"
#include "../Services/UserPreferences.h"
#include "../Localization/LocalizationManager.h"
#include <array>
#include <vector>

class MixerPage : public juce::Component,
                  private juce::Timer
{
public:
    MixerPage();
    ~MixerPage() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setAudioEngine(AudioEngine* engine);
    void setBackgroundMusicPlayer(BackgroundMusicPlayer* player);
    void updateAllText();

private:
    enum StripIndex
    {
        stripMusic = 0,
        stripVocal1,
        stripVocal2,
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
        std::unique_ptr<juce::ComboBox> pluginSlotA;
        std::unique_ptr<juce::ComboBox> pluginSlotB;
        std::unique_ptr<juce::TextButton> pluginSlotAEdit;
        std::unique_ptr<juce::TextButton> pluginSlotBEdit;
        float meterLevel = 0.0f;
        float compOutputMeterLevel = 0.0f;
        float limiterReductionMeterLevel = 0.0f;
        juce::Rectangle<int> compMeterBounds;
        juce::Rectangle<int> limiterMeterBounds;
        juce::Colour accent = juce::Colour(0xff39a9ff);
    };

    std::array<StripWidgets, stripCount> strips;
    AudioEngine* audioEngine = nullptr;
    BackgroundMusicPlayer* bgPlayer = nullptr;

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    std::vector<juce::PluginDescription> cachedPluginList_;
    std::array<std::unique_ptr<juce::DialogWindow>, stripCount> editorWindowA_, editorWindowB_;
    juce::TextButton bypassAllButton_;
    bool restoredAudioEngineSlots_ = false;
    bool restoredBgMusicSlots_ = false;
    bool allPluginsBypassed_ = false;

    void buildStrip(int index, const juce::String& name, juce::Colour accent);
    void bindControlCallbacks();
    void pushStateFromEngine();
    void pushStateToEngine();
    void refreshMasterDynamicsButtons();
    void showCompressorDialog(juce::Component& anchor);
    void showLimiterDialog(juce::Component& anchor);

    ChannelPluginChain* getPluginChainForStrip(int stripIndex) const;
    juce::String getChannelIdForStrip(int stripIndex) const;
    void refreshPluginPickers();
    void onPluginSlotSelected(int stripIndex, int slotIndex);
    void onPluginEditClicked(int stripIndex, int slotIndex);
    void persistPluginSlotState(int stripIndex, int slotIndex);
    void restorePluginSlotsForChannel(int stripIndex);
    void setAllPluginsBypassed(bool shouldBypass);

    void timerCallback() override;

    // Plain Component with a paint callback -- lets the decorative strip
    // cards / meters scroll along with the rest of the content (see
    // LibraryPage for the same pattern and rationale). The strip layout is
    // a fixed total height regardless of window height (unlike its width,
    // which already divides safely across a fixed stripCount), so this
    // only ever needs a vertical scrollbar, not a horizontal one.
    class ContentHolder : public juce::Component
    {
    public:
        std::function<void(juce::Graphics&)> onPaint;
        void paint(juce::Graphics& g) override { if (onPaint) onPaint(g); }
    };

    std::unique_ptr<ContentHolder> contentHolder_;
    juce::Viewport viewport_;
    void layoutContent();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPage)
};
