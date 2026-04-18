/*
  ==============================================================================

    BottomBar.cpp
    Created: 16 Apr 2026
    Author:  GitHub Copilot

    Bottom music control bar component implementation.

  ==============================================================================
*/

#include "BottomBar.h"
#include <cmath>

namespace
{
    const juce::Colour kBackground = juce::Colour(0x0f, 0x0f, 0x0f);
    const juce::Colour kSectionBackground = juce::Colour(0x18, 0x18, 0x18);
    const juce::Colour kAccent = juce::Colour(0x76, 0xc7, 0xc0);
    const juce::Colour kText = juce::Colours::white;
    const juce::Colour kMutedText = juce::Colours::white.withAlpha(0.65f);

    constexpr int kTransportIconSize = 22;
    constexpr int kPlayPauseIconSize = 30;
}

BottomBar::BottomBar()
{
    setupUI();
    generateWaveform();
    startTimerHz(30);
}

void BottomBar::setupUI()
{
    configureTransportButton(returnToZeroButton, false);
    configureTransportButton(stopButton, false);
    configureTransportButton(playPauseButton, true);
    configureTransportButton(jumpToEndButton, false);

    returnToZeroButton.setTooltip("Return to 0");
    stopButton.setTooltip("Stop and return to zero");
    playPauseButton.setTooltip("Play/Pause");
    jumpToEndButton.setTooltip("Jump to end");

    addAndMakeVisible(returnToZeroButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(playPauseButton);
    addAndMakeVisible(jumpToEndButton);

    loadTransportIcons();

    returnToZeroButton.onClick = [this]()
    {
        setProgress(0.0f);
        if (onReturnToZero)
            onReturnToZero();
    };

    stopButton.onClick = [this]()
    {
        hasPlaybackStarted = false;
        setPlaying(false);
        setProgress(0.0f);
        if (onStopAndReturnToZero)
            onStopAndReturnToZero();
    };

    playPauseButton.onClick = [this]()
    {
        if (!isPlaying)
            hasPlaybackStarted = true;
        setPlaying(!isPlaying);
        if (onPlayPause)
            onPlayPause(isPlaying);
    };

    jumpToEndButton.onClick = [this]()
    {
        setProgress(1.0f);
        if (onJumpToEnd)
            onJumpToEnd();
    };

    pitchSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    pitchSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    pitchSlider.setRange(-7, 7, 1);
    pitchSlider.setValue(0);
    pitchSlider.onValueChange = [this]()
    {
        auto semitones = static_cast<int>(pitchSlider.getValue());
        pitchLabel.setText("PITCH " + juce::String(semitones) + " st", juce::dontSendNotification);
        if (onPitchChanged)
            onPitchChanged(semitones);
    };

    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange(0, 10, 1);
    volumeSlider.setValue(5);
    volumeSlider.onValueChange = [this]()
    {
        auto volume = static_cast<int>(volumeSlider.getValue());
        volumeLabel.setText("VOL " + juce::String(volume), juce::dontSendNotification);
        if (onVolumeChanged)
            onVolumeChanged(volume);
    };

    pitchSlider.setColour(juce::Slider::trackColourId, juce::Colour(0x35, 0x35, 0x35));
    pitchSlider.setColour(juce::Slider::thumbColourId, kAccent);
    pitchSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0x2a, 0x2a, 0x2a));

    volumeSlider.setColour(juce::Slider::trackColourId, juce::Colour(0x35, 0x35, 0x35));
    volumeSlider.setColour(juce::Slider::thumbColourId, kAccent);
    volumeSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0x2a, 0x2a, 0x2a));

    pitchLabel.setText("PITCH 0 st", juce::dontSendNotification);
    pitchLabel.setJustificationType(juce::Justification::centredLeft);
    pitchLabel.setColour(juce::Label::textColourId, kMutedText);
    pitchLabel.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)).boldened());

    volumeLabel.setText("VOL 5", juce::dontSendNotification);
    volumeLabel.setJustificationType(juce::Justification::centredLeft);
    volumeLabel.setColour(juce::Label::textColourId, kMutedText);
    volumeLabel.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)).boldened());

    addAndMakeVisible(pitchLabel);
    addAndMakeVisible(pitchSlider);
    addAndMakeVisible(volumeLabel);
    addAndMakeVisible(volumeSlider);

    currentTimeLabel.setText("0:00", juce::dontSendNotification);
    currentTimeLabel.setJustificationType(juce::Justification::centredRight);
    currentTimeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 200, 225));
    currentTimeLabel.setFont(juce::Font(juce::FontOptions().withHeight(30.0f)));
    addAndMakeVisible(currentTimeLabel);

    durationLabel.setText("0:00", juce::dontSendNotification);
    durationLabel.setJustificationType(juce::Justification::centredLeft);
    durationLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 200, 225));
    durationLabel.setFont(juce::Font(juce::FontOptions().withHeight(30.0f)));
    addAndMakeVisible(durationLabel);

    updateTimeLabels();
}

void BottomBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour(kBackground);
    g.fillRect(bounds);

    auto transportArea = getTransportArea();
    auto waveformArea = getWaveformArea();
    auto slidersArea = getSlidersArea();

    g.setColour(kSectionBackground);
    g.fillRect(waveformArea.reduced(2));
    g.fillRect(slidersArea.reduced(2));

    // Draw waveform bars and highlight played section.
    auto drawArea = getWaveformDrawArea();
    if (!waveform.empty() && drawArea.getWidth() > 20)
    {
        const int barCount = static_cast<int>(waveform.size());
        const float barWidth = static_cast<float>(drawArea.getWidth()) / static_cast<float>(barCount);
        const float playedX = drawArea.getX() + progress * static_cast<float>(drawArea.getWidth());

        for (int i = 0; i < barCount; ++i)
        {
            const float barX = drawArea.getX() + static_cast<float>(i) * barWidth;
            const float amplitude = waveform[static_cast<size_t>(i)];
            const int barHeight = juce::jmax(2, static_cast<int>(amplitude * (drawArea.getHeight() * 0.9f)));
            const int y = drawArea.getCentreY() - (barHeight / 2);

            g.setColour((barX <= playedX) ? kAccent : juce::Colours::white.withAlpha(0.25f));
            g.fillRoundedRectangle(barX, static_cast<float>(y), juce::jmax(1.5f, barWidth - 1.0f), static_cast<float>(barHeight), 1.2f);
        }

        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawRect(drawArea, 1);

        const int markerX = juce::jlimit(drawArea.getX(), drawArea.getRight(), static_cast<int>(playedX));
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawLine(static_cast<float>(markerX), static_cast<float>(drawArea.getY()),
                   static_cast<float>(markerX), static_cast<float>(drawArea.getBottom()), 1.5f);
    }
}

void BottomBar::resized()
{
    auto transportArea = getTransportArea().reduced(10, 16);
    auto waveformArea = getWaveformArea();
    auto slidersArea = getSlidersArea().reduced(12, 14);

    auto waveformLayoutArea = waveformArea.reduced(8, 0);
    constexpr int timeLabelWidth = 74;
    auto leftTimeArea = waveformLayoutArea.removeFromLeft(timeLabelWidth);
    auto rightTimeArea = waveformLayoutArea.removeFromRight(timeLabelWidth);
    currentTimeLabel.setBounds(leftTimeArea.withTrimmedRight(8));
    durationLabel.setBounds(rightTimeArea.withTrimmedLeft(8));

    // Keep transport icons compact like the reference UI, with a slightly larger play/pause.
    auto slotWidth = juce::jmax(24, transportArea.getWidth() / 4);
    auto prevSlot = transportArea.removeFromLeft(slotWidth);
    auto stopSlot = transportArea.removeFromLeft(slotWidth);
    auto playSlot = transportArea.removeFromLeft(slotWidth);
    auto nextSlot = transportArea;

    returnToZeroButton.setBounds(prevSlot.withSizeKeepingCentre(kTransportIconSize, kTransportIconSize));
    stopButton.setBounds(stopSlot.withSizeKeepingCentre(kTransportIconSize, kTransportIconSize));
    playPauseButton.setBounds(playSlot.withSizeKeepingCentre(kPlayPauseIconSize, kPlayPauseIconSize));
    jumpToEndButton.setBounds(nextSlot.withSizeKeepingCentre(kTransportIconSize, kTransportIconSize));

    auto pitchArea = slidersArea.removeFromTop(slidersArea.getHeight() / 2).reduced(2, 4);
    pitchLabel.setBounds(pitchArea.removeFromTop(18));
    pitchSlider.setBounds(pitchArea);

    auto volumeArea = slidersArea.reduced(2, 4);
    volumeLabel.setBounds(volumeArea.removeFromTop(18));
    volumeSlider.setBounds(volumeArea);

    repaint(waveformArea);
}

void BottomBar::mouseDown(const juce::MouseEvent& event)
{
    auto waveformArea = getWaveformDrawArea();
    if (waveformArea.contains(event.getPosition()))
    {
        const float newProgress = (event.position.x - static_cast<float>(waveformArea.getX())) /
                                  static_cast<float>(waveformArea.getWidth());
        setProgress(newProgress);
        if (onSeek)
            onSeek(progress);
    }
}

void BottomBar::setPlaying(bool playing)
{
    isPlaying = playing;
    updateTransportButtonIcons();
}

void BottomBar::setProgress(float progress01)
{
    progress = juce::jlimit(0.0f, 1.0f, progress01);
    updateTimeLabels();
    repaint(getWaveformArea());
}

void BottomBar::setDurationSeconds(double seconds)
{
    durationSeconds = juce::jmax(0.0, seconds);
    updateTimeLabels();
}

void BottomBar::setPitch(int semitones)
{
    pitchSlider.setValue(semitones, juce::dontSendNotification);
    pitchLabel.setText("PITCH " + juce::String(semitones) + " st", juce::dontSendNotification);
}

void BottomBar::setVolume(int volumeStep)
{
    volumeSlider.setValue(volumeStep, juce::dontSendNotification);
    volumeLabel.setText("VOL " + juce::String(volumeStep), juce::dontSendNotification);
}

void BottomBar::timerCallback()
{
    if (!isPlaying)
        return;

    progress += 0.0015f;
    if (progress >= 1.0f)
    {
        progress = 1.0f;
        isPlaying = false;
        updateTransportButtonIcons();
    }

    repaint(getWaveformArea());
}

void BottomBar::updateTimeLabels()
{
    const double total = juce::jmax(0.0, durationSeconds);
    const double current = juce::jlimit(0.0, total, progress * total);

    currentTimeLabel.setText(formatTime(current), juce::dontSendNotification);
    durationLabel.setText(formatTime(total), juce::dontSendNotification);
}

juce::String BottomBar::formatTime(double seconds) const
{
    const int totalSeconds = juce::jmax(0, static_cast<int>(std::round(seconds)));
    const int minutes = totalSeconds / 60;
    const int secs = totalSeconds % 60;
    return juce::String(minutes) + ":" + juce::String(secs).paddedLeft('0', 2);
}

void BottomBar::configureTransportButton(juce::ShapeButton& button, bool isPrimary)
{
    juce::ignoreUnused(isPrimary);
    button.setTriggeredOnMouseDown(false);
    button.setClickingTogglesState(false);
    button.setWantsKeyboardFocus(false);
    button.setTooltip("");
    button.setBorderSize(juce::BorderSize<int>(0));
    button.setOutline(juce::Colours::transparentBlack, 0.0f);
}

void BottomBar::loadTransportIcons()
{
    const juce::Colour neutralIcon = juce::Colours::white;
    const juce::Colour neutralIconOver = juce::Colour::fromRGB(255, 255, 255);
    const juce::Colour neutralIconDown = juce::Colour::fromRGB(240, 240, 240);

    const juce::Colour stopIcon = juce::Colour::fromRGB(255, 92, 92);
    const juce::Colour stopIconOver = juce::Colour::fromRGB(255, 118, 118);
    const juce::Colour stopIconDown = juce::Colour::fromRGB(245, 86, 86);

    juce::Path previousPath;
    previousPath.addRectangle(10.0f, 10.0f, 12.0f, 80.0f);
    previousPath.addTriangle(88.0f, 10.0f, 26.0f, 50.0f, 88.0f, 90.0f);
    returnToZeroButton.setShape(previousPath, false, true, false);
    returnToZeroButton.setColours(neutralIcon, neutralIconOver, neutralIconDown);

    juce::Path stopPath;
    stopPath.addRectangle(16.0f, 16.0f, 68.0f, 68.0f);
    stopButton.setShape(stopPath, false, true, false);
    stopButton.setColours(stopIcon, stopIconOver, stopIconDown);

    juce::Path nextPath;
    nextPath.addRectangle(78.0f, 10.0f, 12.0f, 80.0f);
    nextPath.addTriangle(12.0f, 10.0f, 74.0f, 50.0f, 12.0f, 90.0f);
    jumpToEndButton.setShape(nextPath, false, true, false);
    jumpToEndButton.setColours(neutralIcon, neutralIconOver, neutralIconDown);

    updateTransportButtonIcons();
}

void BottomBar::updateTransportButtonIcons()
{
    juce::String playPauseSymbol;

    // Requested states:
    // - icon-play3 when not playing and not paused (stopped)
    // - icon-play when paused
    // - icon-pause2 when actively playing
    if (isPlaying)
        playPauseSymbol = "icon-pause2";
    else if (hasPlaybackStarted)
        playPauseSymbol = "icon-play";
    else
        playPauseSymbol = "icon-play3";

    const juce::Colour playIcon = kAccent.brighter(0.35f);
    const juce::Colour playIconOver = kAccent.brighter(0.50f);
    const juce::Colour playIconDown = kAccent.brighter(0.42f);

    juce::Path playPausePath;
    if (isPlaying)
    {
        playPausePath.addRectangle(22.0f, 12.0f, 20.0f, 76.0f);
        playPausePath.addRectangle(58.0f, 12.0f, 20.0f, 76.0f);
    }
    else
    {
        playPausePath.addTriangle(18.0f, 10.0f, 84.0f, 50.0f, 18.0f, 90.0f);
    }

    playPauseButton.setShape(playPausePath, false, true, false);
    playPauseButton.setColours(playIcon, playIconOver, playIconDown);
}

std::unique_ptr<juce::Drawable> BottomBar::createSpriteIcon(const juce::String& symbolId,
                                                            const juce::Colour& colour) const
{
    auto createInlineIcon = [&]() -> std::unique_ptr<juce::Drawable>
    {
        juce::String pathData;

        if (symbolId == "icon-play3" || symbolId == "icon-play")
            pathData = "M6 4l20 12-20 12z";
        else if (symbolId == "icon-pause2")
            pathData = "M4 4h10v24h-10zM18 4h10v24h-10z";
        else if (symbolId == "icon-previous2")
            pathData = "M8 28v-24h4v11l10-10v22l-10-10v11z";
        else if (symbolId == "icon-next2")
            pathData = "M24 4v24h-4v-11l-10 10v-22l10 10v-11z";
        else if (symbolId == "icon-stop")
            pathData = "M4 4h24v24h-24z"; // Fallback stop glyph when sprite symbol isn't available.

        if (pathData.isEmpty())
            return {};

        const juce::String inlineSvg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\" viewBox=\"0 0 32 32\">"
            "<path fill=\"" + colour.toDisplayString(true) + "\" d=\"" + pathData + "\"/>"
            "</svg>";

        if (auto xml = juce::XmlDocument::parse(inlineSvg))
            return juce::Drawable::createFromSVG(*xml);

        return {};
    };

    const auto spriteFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/images/sprite.svg");
    if (!spriteFile.existsAsFile())
        return createInlineIcon();

    auto root = juce::XmlDocument::parse(spriteFile);
    if (!root)
        return createInlineIcon();

    juce::XmlElement* symbol = nullptr;

    if (auto* defs = root->getChildByName("defs"))
    {
        for (auto* child = defs->getFirstChildElement(); child != nullptr; child = child->getNextElement())
        {
            if (child->hasTagName("symbol") && child->getStringAttribute("id") == symbolId)
            {
                symbol = child;
                break;
            }
        }
    }

    if (symbol == nullptr)
    {
        // Fallback for sprites that declare symbols at top level.
        for (auto* child = root->getFirstChildElement(); child != nullptr; child = child->getNextElement())
        {
            if (child->hasTagName("symbol") && child->getStringAttribute("id") == symbolId)
            {
                symbol = child;
                break;
            }
        }
    }

    if (symbol == nullptr)
        return createInlineIcon();

    juce::XmlElement iconSvg("svg");
    iconSvg.setAttribute("xmlns", "http://www.w3.org/2000/svg");
    iconSvg.setAttribute("xmlns:xlink", "http://www.w3.org/1999/xlink");
    iconSvg.setAttribute("width", "32");
    iconSvg.setAttribute("height", "32");
    iconSvg.setAttribute("viewBox", symbol->getStringAttribute("viewBox", "0 0 32 32"));

    const auto forceFill = [&](juce::XmlElement& element, const auto& self) -> void
    {
        if (element.hasTagName("path")
            || element.hasTagName("polygon")
            || element.hasTagName("rect")
            || element.hasTagName("circle")
            || element.hasTagName("ellipse")
            || element.hasTagName("line")
            || element.hasTagName("polyline"))
        {
            element.setAttribute("fill", colour.toDisplayString(true));
            element.removeAttribute("style");
        }

        for (auto* child = element.getFirstChildElement(); child != nullptr; child = child->getNextElement())
            self(*child, self);
    };

    for (auto* symbolChild = symbol->getFirstChildElement(); symbolChild != nullptr; symbolChild = symbolChild->getNextElement())
    {
        auto* copied = new juce::XmlElement(*symbolChild);
        forceFill(*copied, forceFill);
        iconSvg.addChildElement(copied);
    }

    if (auto drawable = juce::Drawable::createFromSVG(iconSvg))
        return drawable;

    return createInlineIcon();
}

void BottomBar::generateWaveform()
{
    waveform.clear();
    waveform.reserve(240);

    juce::Random rng(2026);
    for (int i = 0; i < 240; ++i)
    {
        const float x = static_cast<float>(i) / 16.0f;
        const float base = 0.45f + 0.25f * std::sin(x) + 0.15f * std::sin(x * 0.35f + 1.2f);
        const float noise = (rng.nextFloat() - 0.5f) * 0.18f;
        waveform.push_back(juce::jlimit(0.08f, 1.0f, base + noise));
    }
}

juce::Rectangle<int> BottomBar::getTransportArea() const
{
    auto bounds = getLocalBounds();
    return bounds.removeFromLeft(static_cast<int>(getWidth() * 0.16f));
}

juce::Rectangle<int> BottomBar::getWaveformArea() const
{
    auto bounds = getLocalBounds();
    const int leftWidth = static_cast<int>(getWidth() * 0.16f);
    const int rightWidth = static_cast<int>(getWidth() * 0.24f);
    bounds.removeFromLeft(leftWidth);
    bounds.removeFromRight(rightWidth);
    return bounds;
}

juce::Rectangle<int> BottomBar::getWaveformDrawArea() const
{
    auto area = getWaveformArea().reduced(8, 20);
    constexpr int timeLabelWidth = 74;
    area.removeFromLeft(timeLabelWidth);
    area.removeFromRight(timeLabelWidth);
    return area.reduced(10, 0);
}

juce::Rectangle<int> BottomBar::getSlidersArea() const
{
    auto bounds = getLocalBounds();
    return bounds.removeFromRight(static_cast<int>(getWidth() * 0.24f));
}
