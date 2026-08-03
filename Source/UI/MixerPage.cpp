/*
  ==============================================================================

    MixerPage.cpp

  ==============================================================================
*/

#include "MixerPage.h"
#include "MenuTheme.h"

namespace
{
    class FaderLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPos, float minSliderPos, float maxSliderPos,
                             const juce::Slider::SliderStyle style, juce::Slider& slider) override
        {
            if (style == juce::Slider::LinearVertical)
            {
                const float trackWidth = 8.0f;
                const float trackX = x + (width - trackWidth) * 0.5f;
                
                // Draw background track (full height)
                g.setColour(juce::Colour(0xff2a3445));
                g.fillRoundedRectangle(trackX, (float)y, trackWidth, (float)height, 2.0f);
                
                // Draw filled portion (from bottom up to thumb)
                float filledHeight = maxSliderPos - sliderPos;
                if (filledHeight > 0.0f)
                {
                    auto filledBounds = juce::Rectangle<float>(trackX, sliderPos, trackWidth, filledHeight);
                    g.setColour(slider.findColour(juce::Slider::trackColourId));
                    g.fillRoundedRectangle(filledBounds, 2.0f);
                }
                
                // Draw the thumb
                auto thumbBounds = juce::Rectangle<float>(trackX - 4.0f, sliderPos - 6.0f, trackWidth + 8.0f, 12.0f);
                g.setColour(slider.findColour(juce::Slider::thumbColourId));
                g.fillRoundedRectangle(thumbBounds, 3.0f);
            }
            else
            {
                // Fallback for horizontal sliders
                juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 
                                                       minSliderPos, maxSliderPos, style, slider);
            }
        }
    };
    
    static FaderLookAndFeel kFaderLookAndFeel;

    juce::Slider* makeFader()
    {
        auto* s = new juce::Slider(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);
        s->setRange(0.0, 1.0, 0.001);
        s->setTextValueSuffix(" dB");
        s->setNumDecimalPlacesToDisplay(2);
        s->setDoubleClickReturnValue(true, 0.8);
        s->setOpaque(false);
        return s;
    }

    juce::Slider* makeEqKnob()
    {
        auto* s = new juce::Slider(juce::Slider::RotaryHorizontalVerticalDrag,
                                   juce::Slider::TextBoxBelow);
        s->setRange(-18.0, 18.0, 0.1);
        s->setDoubleClickReturnValue(true, 0.0);
        s->setTextValueSuffix(" dB");
        s->setNumDecimalPlacesToDisplay(1);
        return s;
    }

    juce::Slider* makeDynamicsSlider(double min, double max, double step, const juce::String& suffix, int decimals)
    {
        auto* s = new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        s->setRange(min, max, step);
        s->setNumDecimalPlacesToDisplay(decimals);
        s->setTextValueSuffix(suffix);
        return s;
    }

    class CompressorPopup final : public juce::Component
    {
    public:
        explicit CompressorPopup(AudioEngine& e) : engine(e)
        {
            addAndMakeVisible(title);
            title.setText("Master Compressor", juce::dontSendNotification);
            title.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)).boldened());
            title.setJustificationType(juce::Justification::centredLeft);

            addAndMakeVisible(enabledToggle);
            enabledToggle.setButtonText("Enabled");
            enabledToggle.setToggleState(engine.isMasterCompressorEnabled(), juce::dontSendNotification);
            enabledToggle.onClick = [this] { engine.setMasterCompressorEnabled(enabledToggle.getToggleState()); };

            addSlider(threshold, thresholdLabel, "Threshold", -48.0, 0.0, 0.1, " dB", 1,
                      (double) engine.getMasterCompressorThreshold(),
                      [this](double v) { engine.setMasterCompressorThreshold((float) v); });

            addSlider(ratio, ratioLabel, "Ratio", 1.0, 20.0, 0.1, ":1", 1,
                      (double) engine.getMasterCompressorRatio(),
                      [this](double v) { engine.setMasterCompressorRatio((float) v); });

            addSlider(attack, attackLabel, "Attack", 1.0, 200.0, 1.0, " ms", 0,
                      (double) engine.getMasterCompressorAttackMs(),
                      [this](double v) { engine.setMasterCompressorAttackMs((float) v); });

            addSlider(release, releaseLabel, "Release", 10.0, 1000.0, 1.0, " ms", 0,
                      (double) engine.getMasterCompressorReleaseMs(),
                      [this](double v) { engine.setMasterCompressorReleaseMs((float) v); });

            addSlider(makeup, makeupLabel, "Makeup", 0.0, 18.0, 0.1, " dB", 1,
                      (double) engine.getMasterCompressorMakeupDb(),
                      [this](double v) { engine.setMasterCompressorMakeupDb((float) v); });
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(10);
            title.setBounds(area.removeFromTop(24));
            enabledToggle.setBounds(area.removeFromTop(26));
            area.removeFromTop(4);
            layoutRow(area, thresholdLabel, threshold);
            layoutRow(area, ratioLabel, ratio);
            layoutRow(area, attackLabel, attack);
            layoutRow(area, releaseLabel, release);
            layoutRow(area, makeupLabel, makeup);
        }

    private:
        AudioEngine& engine;
        juce::Label title;
        juce::ToggleButton enabledToggle;
        juce::Label thresholdLabel, ratioLabel, attackLabel, releaseLabel, makeupLabel;
        std::unique_ptr<juce::Slider> threshold, ratio, attack, release, makeup;

        static void layoutRow(juce::Rectangle<int>& area, juce::Label& label, std::unique_ptr<juce::Slider>& slider)
        {
            auto row = area.removeFromTop(34);
            label.setBounds(row.removeFromLeft(92));
            slider->setBounds(row);
        }

        static void styleLabel(juce::Label& l, const juce::String& text)
        {
            l.setText(text, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centredLeft);
            l.setColour(juce::Label::textColourId, juce::Colour(0xffd3dce9));
        }

        void addSlider(std::unique_ptr<juce::Slider>& s,
                       juce::Label& l,
                       const juce::String& text,
                       double min,
                       double max,
                       double step,
                       const juce::String& suffix,
                       int decimals,
                       double initial,
                       std::function<void(double)> onChange)
        {
            styleLabel(l, text);
            addAndMakeVisible(l);

            s.reset(makeDynamicsSlider(min, max, step, suffix, decimals));
            s->setValue(initial, juce::dontSendNotification);
            s->onValueChange = [slider = s.get(), cb = std::move(onChange)] { cb(slider->getValue()); };
            addAndMakeVisible(*s);
        }
    };

    class LimiterPopup final : public juce::Component
    {
    public:
        explicit LimiterPopup(AudioEngine& e) : engine(e)
        {
            addAndMakeVisible(title);
            title.setText("Master Limiter", juce::dontSendNotification);
            title.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)).boldened());
            title.setJustificationType(juce::Justification::centredLeft);

            addAndMakeVisible(enabledToggle);
            enabledToggle.setButtonText("Enabled");
            enabledToggle.setToggleState(engine.isMasterLimiterEnabled(), juce::dontSendNotification);
            enabledToggle.onClick = [this] { engine.setMasterLimiterEnabled(enabledToggle.getToggleState()); };

            addSlider(ceiling, ceilingLabel, "Ceiling", -12.0, -0.1, 0.1, " dB", 1,
                      (double) engine.getMasterLimiterCeilingDb(),
                      [this](double v) { engine.setMasterLimiterCeilingDb((float) v); });

            addSlider(release, releaseLabel, "Release", 5.0, 500.0, 1.0, " ms", 0,
                      (double) engine.getMasterLimiterReleaseMs(),
                      [this](double v) { engine.setMasterLimiterReleaseMs((float) v); });
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(10);
            title.setBounds(area.removeFromTop(24));
            enabledToggle.setBounds(area.removeFromTop(26));
            area.removeFromTop(4);

            auto row = area.removeFromTop(34);
            ceilingLabel.setBounds(row.removeFromLeft(92));
            ceiling->setBounds(row);

            row = area.removeFromTop(34);
            releaseLabel.setBounds(row.removeFromLeft(92));
            release->setBounds(row);
        }

    private:
        AudioEngine& engine;
        juce::Label title;
        juce::ToggleButton enabledToggle;
        juce::Label ceilingLabel, releaseLabel;
        std::unique_ptr<juce::Slider> ceiling, release;

        static void styleLabel(juce::Label& l, const juce::String& text)
        {
            l.setText(text, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centredLeft);
            l.setColour(juce::Label::textColourId, juce::Colour(0xffd3dce9));
        }

        void addSlider(std::unique_ptr<juce::Slider>& s,
                       juce::Label& l,
                       const juce::String& text,
                       double min,
                       double max,
                       double step,
                       const juce::String& suffix,
                       int decimals,
                       double initial,
                       std::function<void(double)> onChange)
        {
            styleLabel(l, text);
            addAndMakeVisible(l);

            s.reset(makeDynamicsSlider(min, max, step, suffix, decimals));
            s->setValue(initial, juce::dontSendNotification);
            s->onValueChange = [slider = s.get(), cb = std::move(onChange)] { cb(slider->getValue()); };
            addAndMakeVisible(*s);
        }
    };
}

MixerPage::MixerPage()
    : contentHolder_ (std::make_unique<ContentHolder>())
{
    setOpaque(true);
    addAndMakeVisible(viewport_);
    viewport_.setViewedComponent(contentHolder_.get(), false);
    viewport_.setScrollBarsShown(true, false);

    titleLabel.setText({}, juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd8dde3));
    contentHolder_->addAndMakeVisible(titleLabel);

    subtitleLabel.setText({}, juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f, juce::Font::plain));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7f8b9c));
    contentHolder_->addAndMakeVisible(subtitleLabel);

    bypassAllButton_.setButtonText("Bypass All Plugins");
    bypassAllButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1c2735));
    bypassAllButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd3dce9));
    bypassAllButton_.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffd3dce9));
    bypassAllButton_.setClickingTogglesState(false);
    contentHolder_->addAndMakeVisible(bypassAllButton_);

    buildStrip(stripMusic,  {}, juce::Colour(0xff2ea8ff));
    buildStrip(stripVocal1, {}, juce::Colour(0xffeaa700));
    buildStrip(stripVocal2, {}, juce::Colour(0xffd47f1a));
    buildStrip(stripFx,     {}, juce::Colour(0xff9b7fff));
    buildStrip(stripPlugin, {}, juce::Colour(0xff4ad2a2));
    buildStrip(stripMaster, {}, juce::Colour(0xffff5d73));

    bindControlCallbacks();
    refreshPluginPickers();
    updateAllText();
    pushStateFromEngine();
    startTimerHz(30);

    // Decorative strip cards + live meters, drawn against contentHolder_'s
    // own bounds (not MixerPage's) so they scroll along with the rest of
    // the content.
    contentHolder_->onPaint = [this](juce::Graphics& g)
    {
        auto area = contentHolder_->getLocalBounds();
        MenuTheme::drawHeaderPanel(g, area.removeFromTop(74).reduced(12, 6));

        auto stripBounds = contentHolder_->getLocalBounds().reduced(14).withTrimmedTop(80);
        const int stripW = stripBounds.getWidth() / stripCount;

        for (int i = 0; i < stripCount; ++i)
        {
            auto col = stripBounds.removeFromLeft(stripW).reduced(4, 0);

            MenuTheme::drawGlassCard(g, col.toFloat(), 10.0f);

            auto meter = col.removeFromRight(10).reduced(3, 36);
            g.setColour(juce::Colour(0xff0a0f16));
            g.fillRoundedRectangle(meter.toFloat(), 3.0f);

            const float m = juce::jlimit(0.0f, 1.0f, strips[(size_t) i].meterLevel);
            auto fill = meter.withTop((int) juce::jmap(1.0f - m, 0.0f, 1.0f,
                                                       (float) meter.getY(), (float) meter.getBottom()));
            g.setColour(strips[(size_t) i].accent.withMultipliedBrightness(1.1f));
            g.fillRoundedRectangle(fill.toFloat(), 3.0f);

            if (i == stripMaster)
            {
                auto& sw = strips[(size_t) i];
                auto compBounds = sw.compMeterBounds;
                auto limBounds = sw.limiterMeterBounds;

                g.setColour(juce::Colour(0xff0a0f16));
                g.fillRoundedRectangle(compBounds.toFloat(), 2.0f);
                g.fillRoundedRectangle(limBounds.toFloat(), 2.0f);

                g.setColour(juce::Colour(0xff2a3445));
                g.drawRoundedRectangle(compBounds.toFloat(), 2.0f, 1.0f);
                g.drawRoundedRectangle(limBounds.toFloat(), 2.0f, 1.0f);

                const float compLevel = juce::jlimit(0.0f, 1.0f, sw.compOutputMeterLevel);
                auto compFill = compBounds.withTop((int) juce::jmap(1.0f - compLevel,
                                                                     0.0f,
                                                                     1.0f,
                                                                     (float) compBounds.getY(),
                                                                     (float) compBounds.getBottom()));
                g.setColour(juce::Colour(0xff2dd4bf));
                g.fillRoundedRectangle(compFill.toFloat(), 2.0f);

                const float limReduction = juce::jlimit(0.0f, 1.0f, sw.limiterReductionMeterLevel);
                auto limFill = limBounds.withHeight((int) juce::jmap(limReduction,
                                                                      0.0f,
                                                                      1.0f,
                                                                      0.0f,
                                                                      (float) limBounds.getHeight()));
                g.setColour(juce::Colour(0xffff6b6b));
                g.fillRoundedRectangle(limFill.toFloat(), 2.0f);
            }
        }
    };
}

void MixerPage::setAudioEngine(AudioEngine* engine)
{
    audioEngine = engine;
    pushStateFromEngine();

    if (! restoredAudioEngineSlots_)
    {
        restoredAudioEngineSlots_ = true;
        for (int stripIdx : { stripMusic, stripVocal1, stripVocal2, stripFx, stripMaster })
            restorePluginSlotsForChannel(stripIdx);
    }

    refreshPluginPickers();
}

void MixerPage::setBackgroundMusicPlayer(BackgroundMusicPlayer* player)
{
    bgPlayer = player;
    pushStateFromEngine();

    if (! restoredBgMusicSlots_)
    {
        restoredBgMusicSlots_ = true;
        restorePluginSlotsForChannel(stripPlugin);
    }

    refreshPluginPickers();
}

void MixerPage::buildStrip(int index, const juce::String& name, juce::Colour accent)
{
    auto& sw = strips[(size_t) index];
    sw.accent = accent;

    sw.name = std::make_unique<juce::Label>();
    sw.name->setText(name, juce::dontSendNotification);
    sw.name->setFont(juce::Font(13.0f, juce::Font::bold));
    sw.name->setJustificationType(juce::Justification::centred);
    sw.name->setColour(juce::Label::textColourId, juce::Colour(0xffc6d1df));
    contentHolder_->addAndMakeVisible(*sw.name);

    sw.fader.reset(makeFader());
    sw.fader->setLookAndFeel(&kFaderLookAndFeel);
    sw.fader->setColour(juce::Slider::thumbColourId, accent);
    sw.fader->setColour(juce::Slider::trackColourId, accent.withAlpha(0.85f));
    sw.fader->setColour(juce::Slider::backgroundColourId, juce::Colour(0x1affffff));
    sw.fader->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd9dee7));
    sw.fader->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2a3445));
    sw.fader->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x1affffff));
    contentHolder_->addAndMakeVisible(*sw.fader);

    sw.mute = std::make_unique<juce::ToggleButton>(name);
    sw.solo = std::make_unique<juce::ToggleButton>(name);
    sw.mute->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffc6d1df));
    sw.solo->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffc6d1df));
    sw.mute->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xfff55f6f));
    sw.solo->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff48d39f));
    contentHolder_->addAndMakeVisible(*sw.mute);
    contentHolder_->addAndMakeVisible(*sw.solo);

    sw.eqLow.reset(makeEqKnob());
    sw.eqMid.reset(makeEqKnob());
    sw.eqHigh.reset(makeEqKnob());
    for (auto* knob : { sw.eqLow.get(), sw.eqMid.get(), sw.eqHigh.get() })
    {
        knob->setColour(juce::Slider::rotarySliderFillColourId, accent.withAlpha(0.92f));
        knob->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff1a2230));
        knob->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd9dee7));
        knob->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2a3445));
        contentHolder_->addAndMakeVisible(*knob);
    }

    sw.insertA = std::make_unique<juce::ComboBox>();
    sw.insertB = std::make_unique<juce::ComboBox>();
    for (auto* cb : { sw.insertA.get(), sw.insertB.get() })
    {
        cb->addItem({}, 1);
        cb->addItem({}, 2);
        cb->addItem({}, 3);
        cb->addItem({}, 4);
        cb->setSelectedId(1, juce::dontSendNotification);
        cb->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff141a24));
        cb->setColour(juce::ComboBox::textColourId, juce::Colour(0xffd3dce9));
        cb->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a3445));
        cb->setColour(juce::ComboBox::arrowColourId, accent);
        contentHolder_->addAndMakeVisible(*cb);
    }

    sw.pluginAButton = std::make_unique<juce::TextButton>("Comp");
    sw.pluginBButton = std::make_unique<juce::TextButton>("Lim");
    for (auto* b : { sw.pluginAButton.get(), sw.pluginBButton.get() })
    {
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1c2735));
        b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd3dce9));
        b->setColour(juce::TextButton::textColourOnId, juce::Colour(0xffd3dce9));
        b->setClickingTogglesState(false);
        contentHolder_->addAndMakeVisible(*b);
        b->setVisible(index == stripMaster);
    }

    sw.pluginSlotA = std::make_unique<juce::ComboBox>();
    sw.pluginSlotB = std::make_unique<juce::ComboBox>();
    for (auto* cb : { sw.pluginSlotA.get(), sw.pluginSlotB.get() })
    {
        cb->addItem("None", 1);
        cb->setSelectedId(1, juce::dontSendNotification);
        cb->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff141a24));
        cb->setColour(juce::ComboBox::textColourId, juce::Colour(0xffd3dce9));
        cb->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a3445));
        cb->setColour(juce::ComboBox::arrowColourId, accent);
        contentHolder_->addAndMakeVisible(*cb);
    }

    sw.pluginSlotAEdit = std::make_unique<juce::TextButton>("...");
    sw.pluginSlotBEdit = std::make_unique<juce::TextButton>("...");
    for (auto* b : { sw.pluginSlotAEdit.get(), sw.pluginSlotBEdit.get() })
    {
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1c2735));
        b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd3dce9));
        b->setColour(juce::TextButton::textColourOnId, juce::Colour(0xffd3dce9));
        b->setClickingTogglesState(false);
        b->setEnabled(false);
        contentHolder_->addAndMakeVisible(*b);
    }
}

void MixerPage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();

    titleLabel.setText(lm.getText("mixer.title"), juce::dontSendNotification);
    subtitleLabel.setText(lm.getText("mixer.subtitle"), juce::dontSendNotification);

    static const char* stripKeys[] = {
        "mixer.channel.music",
        "mixer.channel.vocal1",
        "mixer.channel.vocal2",
        "mixer.channel.fx",
        "mixer.channel.inserts",
        "mixer.channel.master"
    };

    for (int i = 0; i < stripCount; ++i)
    {
        auto& sw = strips[(size_t) i];
        sw.name->setText(lm.getText(stripKeys[i]), juce::dontSendNotification);
        sw.mute->setButtonText(lm.getText("mixer.mute"));
        sw.solo->setButtonText(lm.getText("mixer.solo"));

        for (auto* cb : { sw.insertA.get(), sw.insertB.get() })
        {
            cb->changeItemText(1, lm.getText("mixer.insert.none"));
            cb->changeItemText(2, lm.getText("mixer.insert.compressor"));
            cb->changeItemText(3, lm.getText("mixer.insert.saturator"));
            cb->changeItemText(4, lm.getText("mixer.insert.limiter"));
        }
    }
}

void MixerPage::bindControlCallbacks()
{
    for (int stripIdx = 0; stripIdx < stripCount; ++stripIdx)
    {
        auto& sw = strips[(size_t) stripIdx];

        sw.fader->onValueChange = [this] { pushStateToEngine(); };
        sw.eqLow->onValueChange = [this] { pushStateToEngine(); };
        sw.eqMid->onValueChange = [this] { pushStateToEngine(); };
        sw.eqHigh->onValueChange = [this] { pushStateToEngine(); };
        sw.insertA->onChange = [this] { pushStateToEngine(); };
        sw.insertB->onChange = [this] { pushStateToEngine(); };
        sw.mute->onClick = [this] { pushStateToEngine(); };
        sw.solo->onClick = [this] { pushStateToEngine(); };

        sw.pluginSlotA->onChange = [this, stripIdx] { onPluginSlotSelected(stripIdx, 0); };
        sw.pluginSlotB->onChange = [this, stripIdx] { onPluginSlotSelected(stripIdx, 1); };
        sw.pluginSlotAEdit->onClick = [this, stripIdx] { onPluginEditClicked(stripIdx, 0); };
        sw.pluginSlotBEdit->onClick = [this, stripIdx] { onPluginEditClicked(stripIdx, 1); };
    }

    bypassAllButton_.onClick = [this]
    {
        allPluginsBypassed_ = ! allPluginsBypassed_;
        setAllPluginsBypassed(allPluginsBypassed_);
    };

    auto& master = strips[stripMaster];
    if (master.pluginAButton)
        master.pluginAButton->onClick = [this]
        {
            if (auto* b = strips[stripMaster].pluginAButton.get())
                showCompressorDialog(*b);
        };

    if (master.pluginBButton)
        master.pluginBButton->onClick = [this]
        {
            if (auto* b = strips[stripMaster].pluginBButton.get())
                showLimiterDialog(*b);
        };
}

void MixerPage::pushStateFromEngine()
{
    if (audioEngine != nullptr)
    {
        strips[stripMusic].fader->setValue(audioEngine->getMusicVolume(), juce::dontSendNotification);
        strips[stripVocal1].fader->setValue(audioEngine->getVocal1Gain(), juce::dontSendNotification);
        strips[stripVocal2].fader->setValue(audioEngine->getVocal2Gain(), juce::dontSendNotification);
        strips[stripFx].fader->setValue(audioEngine->getSfxVolume(), juce::dontSendNotification);
        strips[stripMaster].fader->setValue(audioEngine->getMasterVolume(), juce::dontSendNotification);

        strips[stripMaster].eqLow->setValue(audioEngine->getMasterEqLow(), juce::dontSendNotification);
        strips[stripMaster].eqMid->setValue(audioEngine->getMasterEqMid(), juce::dontSendNotification);
        strips[stripMaster].eqHigh->setValue(audioEngine->getMasterEqHigh(), juce::dontSendNotification);

        // Master dynamics are always-on in AudioEngine.
        strips[stripMaster].insertA->setSelectedId(2, juce::dontSendNotification);
        strips[stripMaster].insertB->setSelectedId(4, juce::dontSendNotification);
        strips[stripMaster].insertA->setEnabled(false);
        strips[stripMaster].insertB->setEnabled(false);

        refreshMasterDynamicsButtons();
    }

    if (bgPlayer != nullptr)
        strips[stripPlugin].fader->setValue(bgPlayer->getVolume(), juce::dontSendNotification);
}

void MixerPage::pushStateToEngine()
{
    // Solo semantics: if any input strip (everything but Master) has solo
    // engaged, every non-soloed input strip is silenced regardless of its
    // own mute state — matches a standard hardware mixer.
    const bool anySolo = strips[stripMusic].solo->getToggleState()
                       || strips[stripVocal1].solo->getToggleState()
                       || strips[stripVocal2].solo->getToggleState()
                       || strips[stripFx].solo->getToggleState()
                       || strips[stripPlugin].solo->getToggleState();

    auto effectiveLevel = [anySolo](const StripWidgets& sw, float rawLevel)
    {
        const bool muted = sw.mute->getToggleState()
                         || (anySolo && ! sw.solo->getToggleState());
        return muted ? 0.0f : rawLevel;
    };

    // Master isn't gated by other strips' solo — it's the combined output
    // level for everything below, so its own mute checkbox is the only
    // thing that should silence it.
    const bool masterMuted = strips[stripMaster].mute->getToggleState();
    const float rawMaster = (float) strips[stripMaster].fader->getValue();
    const float masterLevel = masterMuted ? 0.0f : rawMaster;

    if (audioEngine != nullptr)
    {
        // Music already combines with Master internally in AudioEngine
        // (gain = masterVolume * musicVolume), so send raw values here.
        audioEngine->setMusicVolume(effectiveLevel(strips[stripMusic], (float) strips[stripMusic].fader->getValue()));
        audioEngine->setVocal1Gain(effectiveLevel(strips[stripVocal1], (float) strips[stripVocal1].fader->getValue()));
        audioEngine->setVocal2Gain(effectiveLevel(strips[stripVocal2], (float) strips[stripVocal2].fader->getValue()));

        // Sound effects are mixed in a separate stage of AudioEngine that
        // also multiplies by its own copy of masterVolume, so send the raw
        // Effects-strip value and let the engine combine it with Master.
        audioEngine->setSfxVolume(effectiveLevel(strips[stripFx], (float) strips[stripFx].fader->getValue()));

        audioEngine->setMasterEqLow((float) strips[stripMaster].eqLow->getValue());
        audioEngine->setMasterEqMid((float) strips[stripMaster].eqMid->getValue());
        audioEngine->setMasterEqHigh((float) strips[stripMaster].eqHigh->getValue());
        audioEngine->setMasterVolume(masterLevel);
    }

    if (bgPlayer != nullptr)
    {
        // BackgroundMusicPlayer runs on its own audio device, entirely
        // separate from AudioEngine, so there's no shared masterVolume to
        // combine with automatically — apply Master's level here instead.
        const float bgLevel = effectiveLevel(strips[stripPlugin], (float) strips[stripPlugin].fader->getValue());
        bgPlayer->setVolume(masterLevel * bgLevel);
    }
}

void MixerPage::refreshMasterDynamicsButtons()
{
    auto& sw = strips[stripMaster];
    if (audioEngine == nullptr || sw.pluginAButton == nullptr || sw.pluginBButton == nullptr)
        return;

    const bool compOn = audioEngine->isMasterCompressorEnabled();
    const bool limOn = audioEngine->isMasterLimiterEnabled();

    sw.pluginAButton->setButtonText(compOn ? "Compressor ON" : "Compressor OFF");
    sw.pluginBButton->setButtonText(limOn ? "Limiter ON" : "Limiter OFF");

    sw.pluginAButton->setColour(juce::TextButton::buttonColourId,
                                compOn ? juce::Colour(0xff1f5a3f) : juce::Colour(0xff3c2328));
    sw.pluginBButton->setColour(juce::TextButton::buttonColourId,
                                limOn ? juce::Colour(0xff1f5a3f) : juce::Colour(0xff3c2328));
}

void MixerPage::showCompressorDialog(juce::Component& anchor)
{
    if (audioEngine == nullptr)
        return;

    auto popup = std::make_unique<CompressorPopup>(*audioEngine);
    popup->setSize(380, 235);
    juce::CallOutBox::launchAsynchronously(std::move(popup),
                                           anchor.getScreenBounds(),
                                           nullptr);
}

void MixerPage::showLimiterDialog(juce::Component& anchor)
{
    if (audioEngine == nullptr)
        return;

    auto popup = std::make_unique<LimiterPopup>(*audioEngine);
    popup->setSize(360, 150);
    juce::CallOutBox::launchAsynchronously(std::move(popup),
                                           anchor.getScreenBounds(),
                                           nullptr);
}

ChannelPluginChain* MixerPage::getPluginChainForStrip(int stripIndex) const
{
    switch (stripIndex)
    {
        case stripMusic:  return audioEngine != nullptr ? &audioEngine->getMusicPluginChain()  : nullptr;
        case stripVocal1: return audioEngine != nullptr ? &audioEngine->getVocal1PluginChain() : nullptr;
        case stripVocal2: return audioEngine != nullptr ? &audioEngine->getVocal2PluginChain() : nullptr;
        case stripFx:     return audioEngine != nullptr ? &audioEngine->getFxPluginChain()     : nullptr;
        case stripPlugin: return bgPlayer != nullptr ? &bgPlayer->getPluginChain() : nullptr;
        case stripMaster: return audioEngine != nullptr ? &audioEngine->getMasterPluginChain() : nullptr;
        default:          return nullptr;
    }
}

juce::String MixerPage::getChannelIdForStrip(int stripIndex) const
{
    switch (stripIndex)
    {
        case stripMusic:  return "music";
        case stripVocal1: return "vocal1";
        case stripVocal2: return "vocal2";
        case stripFx:     return "fx";
        case stripPlugin: return "bgmusic";
        case stripMaster: return "master";
        default:          return {};
    }
}

void MixerPage::refreshPluginPickers()
{
    cachedPluginList_.clear();
    for (const auto& d : PluginHostService::getInstance().getAvailablePlugins())
        cachedPluginList_.push_back(d);

    for (int i = 0; i < stripCount; ++i)
    {
        auto& sw = strips[(size_t) i];
        auto* chain = getPluginChainForStrip(i);

        for (int slotIdx = 0; slotIdx < ChannelPluginChain::numSlots; ++slotIdx)
        {
            auto& combo = (slotIdx == 0 ? *sw.pluginSlotA : *sw.pluginSlotB);
            auto& editButton = (slotIdx == 0 ? *sw.pluginSlotAEdit : *sw.pluginSlotBEdit);

            combo.clear(juce::dontSendNotification);
            combo.addItem("None", 1);
            for (int p = 0; p < (int) cachedPluginList_.size(); ++p)
                combo.addItem(cachedPluginList_[(size_t) p].name, p + 2);

            int foundIndex = -1;
            if (chain != nullptr && chain->hasPluginInSlot(slotIdx))
            {
                const auto currentId = chain->getDescriptionForSlot(slotIdx).createIdentifierString();
                for (int p = 0; p < (int) cachedPluginList_.size(); ++p)
                {
                    if (cachedPluginList_[(size_t) p].createIdentifierString() == currentId)
                    {
                        foundIndex = p;
                        break;
                    }
                }
            }

            if (foundIndex >= 0)
            {
                combo.setSelectedId(foundIndex + 2, juce::dontSendNotification);
                editButton.setEnabled(true);
            }
            else
            {
                combo.setSelectedId(1, juce::dontSendNotification);
                editButton.setEnabled(false);
            }
        }
    }
}

void MixerPage::onPluginSlotSelected(int stripIndex, int slotIndex)
{
    auto* chain = getPluginChainForStrip(stripIndex);
    if (chain == nullptr)
        return;

    auto& sw = strips[(size_t) stripIndex];
    auto& combo = (slotIndex == 0 ? *sw.pluginSlotA : *sw.pluginSlotB);
    auto& editButton = (slotIndex == 0 ? *sw.pluginSlotAEdit : *sw.pluginSlotBEdit);
    const int selectedId = combo.getSelectedId();

    if (selectedId <= 1)
    {
        chain->unloadPlugin(slotIndex);
        UserPreferences::getInstance().clearPluginSlotState(getChannelIdForStrip(stripIndex), slotIndex);
        editButton.setEnabled(false);
        return;
    }

    const int descIndex = selectedId - 2;
    if (descIndex < 0 || descIndex >= (int) cachedPluginList_.size())
        return;

    const auto desc = cachedPluginList_[(size_t) descIndex];
    juce::Component::SafePointer<MixerPage> safe(this);

    chain->loadPlugin(slotIndex, desc, PluginHostService::getInstance().getFormatManager(),
        [safe, stripIndex, slotIndex, desc](bool success, juce::String error)
        {
            if (safe == nullptr)
                return;

            auto& sw2 = safe->strips[(size_t) stripIndex];
            auto& editButton2 = (slotIndex == 0 ? *sw2.pluginSlotAEdit : *sw2.pluginSlotBEdit);

            if (success)
            {
                editButton2.setEnabled(true);
                safe->persistPluginSlotState(stripIndex, slotIndex);
            }
            else
            {
                editButton2.setEnabled(false);

                // Selection failed to load — reset the combo to "None" and tell the user.
                auto& combo2 = (slotIndex == 0 ? *sw2.pluginSlotA : *sw2.pluginSlotB);
                combo2.setSelectedId(1, juce::dontSendNotification);

                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Plugin Failed to Load", error);
            }
        });
}

void MixerPage::onPluginEditClicked(int stripIndex, int slotIndex)
{
    auto* chain = getPluginChainForStrip(stripIndex);
    if (chain == nullptr)
        return;

    auto* editor = chain->getEditorForSlot(slotIndex);
    if (editor == nullptr)
        return;

    auto& windowSlot = (slotIndex == 0 ? editorWindowA_[(size_t) stripIndex] : editorWindowB_[(size_t) stripIndex]);
    if (windowSlot != nullptr)
    {
        windowSlot->toFront(true);
        return;
    }

    class PluginEditorWindow final : public juce::DialogWindow
    {
    public:
        std::function<void()> onCloseRequested;

        PluginEditorWindow(const juce::String& name, juce::Component* editorComp)
            : DialogWindow(name, juce::Colour(0xff1a2230), true, true)
        {
            setUsingNativeTitleBar(true);
            setContentNonOwned(editorComp, true);
            setResizable(true, true);
            centreWithSize(editorComp->getWidth(), editorComp->getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            if (onCloseRequested)
                onCloseRequested();
        }
    };

    auto window = std::make_unique<PluginEditorWindow>(chain->getDescriptionForSlot(slotIndex).name, editor);
    juce::Component::SafePointer<MixerPage> safe(this);
    window->onCloseRequested = [safe, stripIndex, slotIndex]()
    {
        if (safe == nullptr)
            return;

        safe->persistPluginSlotState(stripIndex, slotIndex);   // commit any tweaks made via the editor
        auto& slot = (slotIndex == 0 ? safe->editorWindowA_[(size_t) stripIndex]
                                      : safe->editorWindowB_[(size_t) stripIndex]);
        slot.reset();   // deletes the DialogWindow; setContentNonOwned means the plugin's editor Component itself is NOT deleted here
    };

    windowSlot = std::move(window);
}

void MixerPage::persistPluginSlotState(int stripIndex, int slotIndex)
{
    auto* chain = getPluginChainForStrip(stripIndex);
    if (chain == nullptr || ! chain->hasPluginInSlot(slotIndex))
        return;

    const auto desc = chain->getDescriptionForSlot(slotIndex);
    const auto stateBlock = chain->getStateForSlot(slotIndex);

    UserPreferences::PluginSlotState s;
    s.channelId = getChannelIdForStrip(stripIndex);
    s.slotIndex = slotIndex;
    if (auto xml = desc.createXml())
        s.descriptionXml = xml->toString();
    s.pluginName = desc.name;
    s.stateBase64 = juce::Base64::toBase64(stateBlock.getData(), stateBlock.getSize());

    UserPreferences::getInstance().setPluginSlotState(s);
}

void MixerPage::restorePluginSlotsForChannel(int stripIndex)
{
    auto* chain = getPluginChainForStrip(stripIndex);
    if (chain == nullptr)
        return;

    const auto channelId = getChannelIdForStrip(stripIndex);

    for (const auto& saved : UserPreferences::getInstance().getPluginSlotStates())
    {
        if (saved.channelId != channelId)
            continue;
        if (saved.slotIndex < 0 || saved.slotIndex >= ChannelPluginChain::numSlots)
            continue;

        juce::PluginDescription desc;
        auto xml = juce::XmlDocument::parse(saved.descriptionXml);
        if (xml == nullptr || ! desc.loadFromXml(*xml))
            continue; // couldn't parse — skip this slot rather than guessing

        juce::MemoryOutputStream decoded;
        juce::Base64::convertFromBase64(decoded, saved.stateBase64);
        juce::MemoryBlock stateBlock = decoded.getMemoryBlock();

        const int slotIndex = saved.slotIndex;
        juce::Component::SafePointer<MixerPage> safe(this);
        chain->loadPlugin(slotIndex, desc, PluginHostService::getInstance().getFormatManager(),
            [safe, stripIndex, slotIndex](bool success, juce::String /*error*/)
            {
                if (safe == nullptr || ! success)
                    return;

                auto& sw = safe->strips[(size_t) stripIndex];
                auto& editButton = (slotIndex == 0 ? *sw.pluginSlotAEdit : *sw.pluginSlotBEdit);
                editButton.setEnabled(true);
            },
            &stateBlock);
    }
}

void MixerPage::setAllPluginsBypassed(bool shouldBypass)
{
    for (int i = 0; i < stripCount; ++i)
        if (auto* chain = getPluginChainForStrip(i))
            chain->setBypassed(shouldBypass);

    bypassAllButton_.setButtonText(shouldBypass ? "PLUGINS BYPASSED" : "Bypass All Plugins");
    bypassAllButton_.setColour(juce::TextButton::buttonColourId,
                               shouldBypass ? juce::Colour(0xffb23a3a) : juce::Colour(0xff1c2735));
}

void MixerPage::paint(juce::Graphics& g)
{
    MenuTheme::drawPageBackground(g, getLocalBounds());
}

void MixerPage::resized()
{
    viewport_.setBounds(getLocalBounds());

    const int startingWidth  = viewport_.getWidth() - viewport_.getScrollBarThickness();
    // Unlike a page of stacked fixed-height rows, the channel strips'
    // faders are deliberately stretchy (they claim whatever's left of the
    // column after the fixed-height sections), so the content height must
    // start from the viewport's actual visible height -- not just whatever
    // it happened to be last time -- or the strips stay squished into a
    // stale/default size even when the window has plenty of room.
    const int startingHeight = juce::jmax(viewport_.getHeight(), 500);
    contentHolder_->setSize(startingWidth, startingHeight);
    layoutContent();

    // Grow to fit the tallest strip (and never shrink below the viewport)
    // -- lets the viewport's scrollbar reach the bottom of every strip on
    // any window size. layoutContent() only depends on width, so a second
    // pass at the corrected height reproduces the same positions.
    int neededHeight = 0;
    for (auto& sw : strips)
        neededHeight = juce::jmax(neededHeight, sw.fader->getBottom());
    neededHeight += 14;
    neededHeight = juce::jmax(neededHeight, viewport_.getHeight());
    if (neededHeight != contentHolder_->getHeight())
    {
        contentHolder_->setSize(startingWidth, neededHeight);
        layoutContent();
    }
}

void MixerPage::layoutContent()
{
    auto area = contentHolder_->getLocalBounds().reduced(14);

    auto head = area.removeFromTop(68);
    auto bypassArea = head.removeFromRight(180).removeFromTop(30);
    bypassAllButton_.setBounds(bypassArea.reduced(4, 2));
    titleLabel.setBounds(head.removeFromTop(30));
    subtitleLabel.setBounds(head.removeFromTop(22));

    const int stripW = area.getWidth() / stripCount;

    for (int i = 0; i < stripCount; ++i)
    {
        auto col = area.removeFromLeft(stripW).reduced(8, 8);
        auto body = col.withTrimmedTop(34);

        auto& sw = strips[(size_t) i];

        sw.name->setBounds(col.removeFromTop(24));

        auto insertsArea = body.removeFromBottom(62);
        sw.insertA->setBounds(insertsArea.removeFromTop(28));
        sw.insertB->setBounds(insertsArea.removeFromTop(28));

        // Real VST3 plugin-chain slots — unconditional, shown on every strip
        // including Master (unlike insertA/insertB and pluginAButton/B above).
        auto pluginSlotArea = body.removeFromBottom(62);
        auto slotRowA = pluginSlotArea.removeFromTop(28);
        sw.pluginSlotAEdit->setBounds(slotRowA.removeFromRight(36));
        slotRowA.removeFromRight(4);
        sw.pluginSlotA->setBounds(slotRowA);

        pluginSlotArea.removeFromTop(6);
        auto slotRowB = pluginSlotArea.removeFromTop(28);
        sw.pluginSlotBEdit->setBounds(slotRowB.removeFromRight(36));
        slotRowB.removeFromRight(4);
        sw.pluginSlotB->setBounds(slotRowB);

        if (i == stripMaster)
        {
            sw.insertA->setVisible(false);
            sw.insertB->setVisible(false);

            auto pluginArea = body.removeFromBottom(62);
            sw.pluginAButton->setBounds(pluginArea.removeFromTop(28));
            sw.pluginBButton->setBounds(pluginArea.removeFromTop(28));
            sw.pluginAButton->setVisible(true);
            sw.pluginBButton->setVisible(true);
        }
        else if (i == stripVocal1 || i == stripVocal2 || i == stripFx || i == stripPlugin)
        {
            // Vocal 1/2, Effects, and Background Music are volume/mute/solo-only
            // strips now — the insert-effect picker doesn't apply to any of them.
            sw.insertA->setVisible(false);
            sw.insertB->setVisible(false);
            if (sw.pluginAButton) sw.pluginAButton->setVisible(false);
            if (sw.pluginBButton) sw.pluginBButton->setVisible(false);
        }
        else
        {
            sw.insertA->setVisible(true);
            sw.insertB->setVisible(true);
            if (sw.pluginAButton) sw.pluginAButton->setVisible(false);
            if (sw.pluginBButton) sw.pluginBButton->setVisible(false);
        }

        auto eqArea = body.removeFromBottom(122);
        const int eqW = eqArea.getWidth() / 3;
        sw.eqLow->setBounds(eqArea.removeFromLeft(eqW).reduced(4));
        sw.eqMid->setBounds(eqArea.removeFromLeft(eqW).reduced(4));
        sw.eqHigh->setBounds(eqArea.reduced(4));

        auto ms = body.removeFromBottom(24);
        sw.mute->setBounds(ms.removeFromLeft(ms.getWidth() / 2));
        sw.solo->setBounds(ms);

        sw.fader->setBounds(body.reduced(10, 6));

        if (i == stripMaster)
        {
            auto faderBounds = sw.fader->getBounds();
            const int meterWidth = 7;
            const int meterGap = 3;
            const int meterTopInset = 8;
            const int meterBottomInset = 8;
            const int meterHeight = juce::jmax(20, faderBounds.getHeight() - (meterTopInset + meterBottomInset));
            const int meterY = faderBounds.getY() + meterTopInset;
            const int meterX = faderBounds.getRight() + 4;

            sw.compMeterBounds = { meterX, meterY, meterWidth, meterHeight };
            sw.limiterMeterBounds = { meterX + meterWidth + meterGap, meterY, meterWidth, meterHeight };
        }
    }
}

void MixerPage::timerCallback()
{
    const float engineLevel = audioEngine != nullptr ? audioEngine->getCurrentLevel() : 0.0f;

    if (audioEngine != nullptr)
    {
        const double master = (double) audioEngine->getMasterVolume();
        if (std::abs(strips[stripMaster].fader->getValue() - master) > 0.0005)
            strips[stripMaster].fader->setValue(master, juce::dontSendNotification);

        strips[stripMaster].compOutputMeterLevel = audioEngine->getMasterCompressorOutputMeter();
        strips[stripMaster].limiterReductionMeterLevel = audioEngine->getMasterLimiterReductionMeter();
    }
    else
    {
        strips[stripMaster].compOutputMeterLevel = 0.0f;
        strips[stripMaster].limiterReductionMeterLevel = 0.0f;
    }

    strips[stripMusic].meterLevel = strips[stripMusic].mute->getToggleState() ? 0.0f : engineLevel;
    strips[stripVocal1].meterLevel = strips[stripVocal1].mute->getToggleState() ? 0.0f : engineLevel * 0.8f;
    strips[stripVocal2].meterLevel = strips[stripVocal2].mute->getToggleState() ? 0.0f : engineLevel * 0.8f;
    strips[stripFx].meterLevel = strips[stripFx].mute->getToggleState() ? 0.0f : engineLevel * 0.55f;

    // Background Music runs on its own audio device, unrelated to the
    // karaoke engine's level — show its own playing/volume state instead.
    const bool bgAudible = bgPlayer != nullptr && bgPlayer->isPlaying()
                         && ! strips[stripPlugin].mute->getToggleState();
    strips[stripPlugin].meterLevel = bgAudible ? bgPlayer->getVolume() : 0.0f;

    strips[stripMaster].meterLevel = strips[stripMaster].mute->getToggleState() ? 0.0f : engineLevel;

    refreshMasterDynamicsButtons();

    contentHolder_->repaint();
}
