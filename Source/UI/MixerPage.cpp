/*
  ==============================================================================

    MixerPage.cpp

  ==============================================================================
*/

#include "MixerPage.h"

namespace
{
    juce::Slider* makeFader()
    {
        auto* s = new juce::Slider(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);
        s->setRange(0.0, 1.0, 0.001);
        s->setTextValueSuffix(" dB");
        s->setNumDecimalPlacesToDisplay(2);
        s->setDoubleClickReturnValue(true, 0.8);
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
{
    titleLabel.setText({}, juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd8dde3));
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText({}, juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f, juce::Font::plain));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7f8b9c));
    addAndMakeVisible(subtitleLabel);

    buildStrip(stripMusic,  {}, juce::Colour(0xff2ea8ff));
    buildStrip(stripVocal,  {}, juce::Colour(0xffeaa700));
    buildStrip(stripFx,     {}, juce::Colour(0xff9b7fff));
    buildStrip(stripPlugin, {}, juce::Colour(0xff4ad2a2));
    buildStrip(stripMaster, {}, juce::Colour(0xffff5d73));

    bindControlCallbacks();
    updateAllText();
    pushStateFromEngine();
    startTimerHz(30);
}

void MixerPage::setAudioEngine(AudioEngine* engine)
{
    audioEngine = engine;
    pushStateFromEngine();
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
    addAndMakeVisible(*sw.name);

    sw.fader.reset(makeFader());
    sw.fader->setColour(juce::Slider::thumbColourId, accent);
    sw.fader->setColour(juce::Slider::trackColourId, accent.withAlpha(0.85f));
    sw.fader->setColour(juce::Slider::backgroundColourId, juce::Colour(0xff161c26));
    sw.fader->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd9dee7));
    sw.fader->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2a3445));
    addAndMakeVisible(*sw.fader);

    sw.mute = std::make_unique<juce::ToggleButton>(name);
    sw.solo = std::make_unique<juce::ToggleButton>(name);
    sw.mute->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffc6d1df));
    sw.solo->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffc6d1df));
    sw.mute->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xfff55f6f));
    sw.solo->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff48d39f));
    addAndMakeVisible(*sw.mute);
    addAndMakeVisible(*sw.solo);

    sw.eqLow.reset(makeEqKnob());
    sw.eqMid.reset(makeEqKnob());
    sw.eqHigh.reset(makeEqKnob());
    for (auto* knob : { sw.eqLow.get(), sw.eqMid.get(), sw.eqHigh.get() })
    {
        knob->setColour(juce::Slider::rotarySliderFillColourId, accent.withAlpha(0.92f));
        knob->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff1a2230));
        knob->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd9dee7));
        knob->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2a3445));
        addAndMakeVisible(*knob);
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
        addAndMakeVisible(*cb);
    }

    sw.pluginAButton = std::make_unique<juce::TextButton>("Comp");
    sw.pluginBButton = std::make_unique<juce::TextButton>("Lim");
    for (auto* b : { sw.pluginAButton.get(), sw.pluginBButton.get() })
    {
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1c2735));
        b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd3dce9));
        b->setColour(juce::TextButton::textColourOnId, juce::Colour(0xffd3dce9));
        b->setClickingTogglesState(false);
        addAndMakeVisible(*b);
        b->setVisible(index == stripMaster);
    }
}

void MixerPage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();

    titleLabel.setText(lm.getText("mixer.title"), juce::dontSendNotification);
    subtitleLabel.setText(lm.getText("mixer.subtitle"), juce::dontSendNotification);

    static const char* stripKeys[] = {
        "mixer.channel.music",
        "mixer.channel.vocal",
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
    for (auto& sw : strips)
    {
        sw.fader->onValueChange = [this] { pushStateToEngine(); };
        sw.eqLow->onValueChange = [this] { pushStateToEngine(); };
        sw.eqMid->onValueChange = [this] { pushStateToEngine(); };
        sw.eqHigh->onValueChange = [this] { pushStateToEngine(); };
        sw.insertA->onChange = [this] { pushStateToEngine(); };
        sw.insertB->onChange = [this] { pushStateToEngine(); };
    }

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
    if (audioEngine == nullptr)
        return;

    strips[stripMusic].fader->setValue(audioEngine->getMusicVolume(), juce::dontSendNotification);
    strips[stripVocal].fader->setValue(audioEngine->getVocalVolume(), juce::dontSendNotification);
    strips[stripFx].fader->setValue(audioEngine->getVocalEffectsLevel(), juce::dontSendNotification);
    strips[stripPlugin].fader->setValue(audioEngine->getMasterInsertDrive(), juce::dontSendNotification);
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

void MixerPage::pushStateToEngine()
{
    if (audioEngine == nullptr)
        return;

    audioEngine->setMusicVolume((float) strips[stripMusic].fader->getValue());
    audioEngine->setVocalVolume((float) strips[stripVocal].fader->getValue());
    audioEngine->setVocalEffectsLevel((float) strips[stripFx].fader->getValue());

    const int insertA = strips[stripPlugin].insertA->getSelectedId();
    const int insertB = strips[stripPlugin].insertB->getSelectedId();
    const bool saturatorSelected = (insertA == 3 || insertB == 3);
    const float drive = saturatorSelected ? (float) strips[stripPlugin].fader->getValue() : 0.0f;
    audioEngine->setMasterInsertDrive(drive);

    audioEngine->setMasterEqLow((float) strips[stripMaster].eqLow->getValue());
    audioEngine->setMasterEqMid((float) strips[stripMaster].eqMid->getValue());
    audioEngine->setMasterEqHigh((float) strips[stripMaster].eqHigh->getValue());
    audioEngine->setMasterVolume((float) strips[stripMaster].fader->getValue());
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

void MixerPage::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();

    juce::ColourGradient bg(juce::Colour(0xff0a0f17),
                            0.0f, 0.0f,
                            juce::Colour(0xff131a25),
                            0.0f, (float) area.getBottom(),
                            false);
    g.setGradientFill(bg);
    g.fillAll();

    g.setColour(juce::Colour(0xff0f141d).withAlpha(0.7f));
    g.fillRect(area.removeFromTop(74));

    auto stripBounds = getLocalBounds().reduced(14).withTrimmedTop(80);
    const int stripW = stripBounds.getWidth() / stripCount;

    for (int i = 0; i < stripCount; ++i)
    {
        auto col = stripBounds.removeFromLeft(stripW).reduced(4, 0);

        g.setColour(juce::Colour(0xff101722));
        g.fillRoundedRectangle(col.toFloat(), 10.0f);
        g.setColour(juce::Colour(0xff263245));
        g.drawRoundedRectangle(col.toFloat().reduced(0.5f), 10.0f, 1.0f);

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
}

void MixerPage::resized()
{
    auto area = getLocalBounds().reduced(14);

    auto head = area.removeFromTop(68);
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
    strips[stripVocal].meterLevel = strips[stripVocal].mute->getToggleState() ? 0.0f : engineLevel * 0.75f;
    strips[stripFx].meterLevel = strips[stripFx].mute->getToggleState() ? 0.0f : engineLevel * 0.55f;
    strips[stripPlugin].meterLevel = strips[stripPlugin].mute->getToggleState() ? 0.0f : engineLevel * 0.7f;
    strips[stripMaster].meterLevel = strips[stripMaster].mute->getToggleState() ? 0.0f : engineLevel;

    refreshMasterDynamicsButtons();

    repaint();
}
