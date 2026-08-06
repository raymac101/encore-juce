/*
  ==============================================================================

    RibbonMenu.cpp

  ==============================================================================
*/

#include "RibbonMenu.h"
#include "SpriteIcon.h"
#include "../Localization/LocalizationManager.h"

namespace
{
const auto kBg = juce::Colour (0xff1d2432);
const auto kCardBg = juce::Colour (0xff243047);
const auto kCardBgHover = juce::Colour (0xff2a3955);
const auto kAccent = juce::Colour (0xff30daff);
const auto kText = juce::Colours::white;
const auto kSfxIconTint = juce::Colour (0xffd3d7de);

juce::String tr (const juce::String& key)
{
    return LocalizationManager::getInstance().getText (key);
}

juce::String formatSeconds (double seconds)
{
    const int totalSeconds = juce::jmax (0, (int) std::round (seconds));
    const int mins = totalSeconds / 60;
    const int secs = totalSeconds % 60;
    return juce::String::formatted ("%d:%02d", mins, secs);
}

void styleCardButton (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId, kCardBg);
    b.setColour (juce::TextButton::buttonOnColourId, kCardBgHover);
    b.setColour (juce::TextButton::textColourOffId, kText);
    b.setColour (juce::TextButton::textColourOnId, kText);
}

void styleActionButton (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff121826));
    b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff1b2334));
    b.setColour (juce::TextButton::textColourOffId, kText);
    b.setColour (juce::TextButton::textColourOnId, kText);
}

void styleActionButton (juce::DrawableButton& b)
{
    b.setColour (juce::DrawableButton::backgroundColourId, juce::Colour (0xff121826));
    b.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colour (0xff1b2334));
}

std::unique_ptr<juce::Drawable> loadSvgDrawable (const juce::File& svgFile)
{
    auto xml = juce::XmlDocument::parse (svgFile);
    if (xml == nullptr)
        return {};

    std::function<void(juce::XmlElement&)> tintSvgTree = [&tintSvgTree] (juce::XmlElement& node)
    {
        const auto tag = node.getTagName().toLowerCase();
        const bool isShape = tag == "path" || tag == "circle" || tag == "ellipse"
                          || tag == "rect" || tag == "polygon" || tag == "polyline"
                          || tag == "line";

        auto tintAttribute = [&node] (const char* attrName)
        {
            if (! node.hasAttribute (attrName))
                return;

            const auto value = node.getStringAttribute (attrName).trim();
            if (value.equalsIgnoreCase ("none"))
                return;

            node.setAttribute (attrName, kSfxIconTint.toDisplayString (true));
        };

        tintAttribute ("fill");
        tintAttribute ("stroke");

        // Some icon paths omit fill/stroke entirely and default to black.
        if (isShape && ! node.hasAttribute ("fill") && ! node.hasAttribute ("stroke"))
            node.setAttribute ("fill", kSfxIconTint.toDisplayString (true));

        for (auto* child = node.getFirstChildElement(); child != nullptr; child = child->getNextElement())
            tintSvgTree (*child);
    };

    tintSvgTree (*xml);

    // This JUCE version dropped Drawable::createFromSVG(XmlElement) in
    // favour of file-based loading only — round-trip through a temp file.
    auto tempFile = juce::File::createTempFile (".svg");
    if (! xml->writeTo (tempFile))
        return {};
    auto drawable = juce::Drawable::createFromSVGFile (tempFile);
    tempFile.deleteFile();
    return drawable;
}

void setSfxButtonIcon (juce::DrawableButton& button, const juce::String& iconRelativePath)
{
    auto iconFile = SpriteIcon::resolveAssetFile (iconRelativePath);
    if (! iconFile.existsAsFile())
        return;

    auto normal = loadSvgDrawable (iconFile);
    if (normal == nullptr)
        return;

    // Safety pass for SVGs that define dark tones via style blocks/classes.
    const juce::Colour darksToReplace[] = {
        juce::Colour (0xff000000),
        juce::Colour (0xff111111),
        juce::Colour (0xff1a1a1a),
        juce::Colour (0xff222222),
        juce::Colour (0xff333333),
        juce::Colour (0xff444444),
        juce::Colour (0xff555555),
        juce::Colour (0xff666666),
        juce::Colour (0xff777777)
    };

    for (auto dark : darksToReplace)
        normal->replaceColour (dark, kSfxIconTint);

    auto over = normal->createCopy();
    auto down = normal->createCopy();

    for (auto dark : darksToReplace)
    {
        over->replaceColour (dark, kSfxIconTint);
        down->replaceColour (dark, kSfxIconTint);
    }

    button.setImages (normal.release(), over.release(), down.release());
}

void setSpriteButtonIcon (juce::DrawableButton& button, const juce::String& symbolId)
{
    auto normal = SpriteIcon::create (symbolId, juce::Colours::white);
    if (normal == nullptr)
        return;

    // Use the same colour replacement approach as setSfxButtonIcon
    // to ensure dark tones are replaced with the light grey icon colour
    const juce::Colour darksToReplace[] = {
        juce::Colour (0xff000000),
        juce::Colour (0xff111111),
        juce::Colour (0xff1a1a1a),
        juce::Colour (0xff222222),
        juce::Colour (0xff333333),
        juce::Colour (0xff444444),
        juce::Colour (0xff555555),
        juce::Colour (0xff666666),
        juce::Colour (0xff777777)
    };

    for (auto dark : darksToReplace)
        normal->replaceColour (dark, kSfxIconTint);

    auto over = normal->createCopy();
    auto down = normal->createCopy();

    for (auto dark : darksToReplace)
    {
        over->replaceColour (dark, kSfxIconTint);
        down->replaceColour (dark, kSfxIconTint);
    }

    button.setImages(normal.release(), over.release(), down.release());
}

juce::String panelTitleFor (RibbonMenu::PanelId panel)
{
    if (panel == RibbonMenu::PanelId::backgroundMusic)
        return tr ("ribbon.panel.background.title");
    if (panel == RibbonMenu::PanelId::lyricDisplay)
        return tr ("ribbon.panel.lyric.title");
    if (panel == RibbonMenu::PanelId::nextSinger)
        return tr ("ribbon.panel.next_singer.title");
    if (panel == RibbonMenu::PanelId::soundEffects)
        return tr ("ribbon.panel.sfx.title");
    return {};
}
}

RibbonMenu::RibbonMenu()
{
    addAndMakeVisible (backgroundCard_);
    addAndMakeVisible (lyricCard_);
    addAndMakeVisible (nextSingerCard_);
    addAndMakeVisible (sfxCard_);

    styleCardButton (backgroundCard_);
    styleCardButton (lyricCard_);
    styleCardButton (nextSingerCard_);
    styleCardButton (sfxCard_);

    backgroundCard_.onClick = [this]() { togglePanel (PanelId::backgroundMusic); };
    lyricCard_.onClick = [this]() { togglePanel (PanelId::lyricDisplay); };
    nextSingerCard_.onClick = [this]() { togglePanel (PanelId::nextSinger); };
    sfxCard_.onClick = [this]() { togglePanel (PanelId::soundEffects); };

    addAndMakeVisible (collapsePanelButton_);
    styleActionButton (collapsePanelButton_);
    collapsePanelButton_.onClick = [this]() { togglePanel (PanelId::none); };

    addAndMakeVisible (panelTitleLabel_);
    panelTitleLabel_.setColour (juce::Label::textColourId, kText);
    panelTitleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (20.0f)).boldened());

    addAndMakeVisible (bgPrevButton_);
    styleActionButton (bgPrevButton_);
    setSpriteButtonIcon (bgPrevButton_, "icon-previous2");
    bgPrevButton_.onClick = [this]() { if (onBackgroundPrevTrack) onBackgroundPrevTrack(); };

    addAndMakeVisible (bgPlayPauseButton_);
    styleActionButton (bgPlayPauseButton_);
    setSpriteButtonIcon (bgPlayPauseButton_, "icon-play3");
    bgPlayPauseButton_.onClick = [this]()
    {
        if (onBackgroundPlayPause)
            onBackgroundPlayPause (! backgroundPlaying_);
    };

    addAndMakeVisible (bgNextButton_);
    styleActionButton (bgNextButton_);
    setSpriteButtonIcon (bgNextButton_, "icon-next2");
    bgNextButton_.onClick = [this]() { if (onBackgroundNextTrack) onBackgroundNextTrack(); };

    addAndMakeVisible (bgVolumeSlider_);
    bgVolumeSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    bgVolumeSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 20);
    bgVolumeSlider_.setRange (0.0, 1.0, 0.01);
    bgVolumeSlider_.onValueChange = [this]()
    {
        backgroundVolume01_ = (float) bgVolumeSlider_.getValue();
        if (onBackgroundVolumeChanged)
            onBackgroundVolumeChanged (backgroundVolume01_);
    };

    addAndMakeVisible (bgVolumeLabel_);
    bgVolumeLabel_.setColour (juce::Label::textColourId, kText);

    addAndMakeVisible (bgProgressSlider_);
    bgProgressSlider_.setSliderStyle (juce::Slider::LinearBar);
    bgProgressSlider_.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    bgProgressSlider_.setRange (0.0, 1.0, 0.001);
    bgProgressSlider_.onValueChange = [this]()
    {
        if (onBackgroundSeekRequested == nullptr || backgroundTotalSeconds_ <= 0.0)
            return;

        onBackgroundSeekRequested (juce::jlimit (0.0,
                                                 backgroundTotalSeconds_,
                                                 bgProgressSlider_.getValue() * backgroundTotalSeconds_));
    };

    addAndMakeVisible (bgTrackLabel_);
    bgTrackLabel_.setColour (juce::Label::textColourId, kText);
    bgTrackLabel_.setJustificationType (juce::Justification::centredLeft);
    bgTrackLabel_.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)).boldened());

    addAndMakeVisible (bgTimeLabel_);
    bgTimeLabel_.setColour (juce::Label::textColourId, kText.withAlpha (0.82f));
    bgTimeLabel_.setJustificationType (juce::Justification::centredRight);
    bgTimeLabel_.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));

    addAndMakeVisible (lyricToggleWindowButton_);
    addAndMakeVisible (lyricFullscreenButton_);
    addAndMakeVisible (lyricStateLabel_);
    styleActionButton (lyricToggleWindowButton_);
    styleActionButton (lyricFullscreenButton_);
    lyricStateLabel_.setColour (juce::Label::textColourId, kText);
    lyricToggleWindowButton_.onClick = [this]() { if (onLyricToggleWindow) onLyricToggleWindow(); };
    lyricFullscreenButton_.onClick = [this]() { if (onLyricToggleFullscreen) onLyricToggleFullscreen(); };

    lyricPreview_ = std::make_unique<LyricDisplayComponent>();
    lyricPreview_->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (lyricPreview_.get());

    addAndMakeVisible (nextSingerNameLabel_);
    addAndMakeVisible (nextSingerSongLabel_);
    addAndMakeVisible (nextSingerPlayButton_);
    nextSingerNameLabel_.setColour (juce::Label::textColourId, kText);
    nextSingerNameLabel_.setFont (juce::Font (juce::FontOptions().withHeight (18.0f)).boldened());
    nextSingerSongLabel_.setColour (juce::Label::textColourId, kText.withAlpha (0.8f));
    nextSingerSongLabel_.setJustificationType (juce::Justification::topLeft);
    styleActionButton (nextSingerPlayButton_);
    nextSingerPlayButton_.onClick = [this]() { if (onPlayNextSinger) onPlayNextSinger(); };

    auto setupSfx = [this] (juce::DrawableButton& button, const juce::String& iconPath, const juce::String& effectName)
    {
        addAndMakeVisible (button);
        styleActionButton (button);
        setSfxButtonIcon (button, iconPath);
        button.onClick = [this, effectName]() { if (onTriggerSfx) onTriggerSfx (effectName); };
    };

    setupSfx (sfxAreYouReadyButton_, "assets/sound-icons/ARE YOU READY!.svg", "Are You Ready");
    setupSfx (sfxChickenButton_, "assets/sound-icons/Chicken.svg", "Chicken");
    setupSfx (sfxBurpButton_, "assets/sound-icons/Burp.svg", "Burp");
    setupSfx (sfxBruhButton_, "assets/sound-icons/BRUH.svg", "Bruh");
    setupSfx (sfxBuzzerButton_, "assets/sound-icons/Buzzer.svg", "Buzzer");
    setupSfx (sfxDrumFillButton_, "assets/sound-icons/Drum-Fill.svg", "Drum Fill");
    setupSfx (sfxDrumRollButton_, "assets/sound-icons/drum-roll.svg", "Drum Roll");
    setupSfx (sfxWooHooButton_, "assets/sound-icons/WooHoo.svg", "WooHoo");

    addAndMakeVisible (sfxVolumeSlider_);
    sfxVolumeSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    sfxVolumeSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    sfxVolumeSlider_.setRange (0.0, 1.0, 0.01);
    sfxVolumeSlider_.onValueChange = [this]()
    {
        sfxVolume01_ = (float) sfxVolumeSlider_.getValue();
        if (onSfxVolumeChanged)
            onSfxVolumeChanged (sfxVolume01_);
    };

    addAndMakeVisible (sfxVolumeLabel_);
    sfxVolumeLabel_.setColour (juce::Label::textColourId, kText);

    updateAllText();
    updateControlState();
}

void RibbonMenu::updateAllText()
{
    updateControlState();
}

void RibbonMenu::setAudioEngine (AudioEngine* engine)
{
    audioEngine_ = engine;
    if (lyricPreview_ != nullptr)
        lyricPreview_->setAudioEngine (engine);
}

void RibbonMenu::setLyricPreviewFile (const juce::File& cdgFile)
{
    if (lyricPreviewFile_ == cdgFile)
        return;

    lyricPreviewFile_ = cdgFile;
    if (lyricPreview_ != nullptr)
        lyricPreview_->loadCDG (lyricPreviewFile_);
}

void RibbonMenu::paint (juce::Graphics& g)
{
    g.setColour (kBg);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 10.0f);

    g.setColour (kAccent.withAlpha (0.35f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 10.0f, 1.0f);

    auto handleArea = getLocalBounds().reduced (8, 0).removeFromTop (dragHandleHeight_);
    g.setColour (kText.withAlpha (0.32f));
    g.fillRoundedRectangle (handleArea.toFloat().reduced (0.0f, 3.0f), 6.0f);

    auto grip = handleArea.withSizeKeepingCentre (38, 5);
    g.setColour (kText.withAlpha (0.7f));
    g.fillRoundedRectangle (grip.toFloat(), 2.5f);
}

void RibbonMenu::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (dragHandleHeight_);

    if (hidden_)
    {
        backgroundCard_.setVisible (false);
        lyricCard_.setVisible (false);
        nextSingerCard_.setVisible (false);
        sfxCard_.setVisible (false);
        collapsePanelButton_.setVisible (false);
        panelTitleLabel_.setVisible (false);
        bgPlayPauseButton_.setVisible (false);
        bgVolumeSlider_.setVisible (false);
        bgVolumeLabel_.setVisible (false);
        bgProgressSlider_.setVisible (false);
        bgTrackLabel_.setVisible (false);
        bgTimeLabel_.setVisible (false);
        lyricToggleWindowButton_.setVisible (false);
        lyricFullscreenButton_.setVisible (false);
        lyricStateLabel_.setVisible (false);
        if (lyricPreview_ != nullptr)
            lyricPreview_->setVisible (false);
        nextSingerNameLabel_.setVisible (false);
        nextSingerSongLabel_.setVisible (false);
        nextSingerPlayButton_.setVisible (false);
        sfxAreYouReadyButton_.setVisible (false);
        sfxChickenButton_.setVisible (false);
        sfxBurpButton_.setVisible (false);
        sfxBruhButton_.setVisible (false);
        sfxBuzzerButton_.setVisible (false);
        sfxDrumFillButton_.setVisible (false);
        sfxDrumRollButton_.setVisible (false);
        sfxWooHooButton_.setVisible (false);
        sfxVolumeSlider_.setVisible (false);
        sfxVolumeLabel_.setVisible (false);
        return;
    }

    const bool expanded = (expandedPanel_ != PanelId::none);
    const bool compact = ! expanded;
    const bool showBackground = compact || expandedPanel_ == PanelId::backgroundMusic;
    const bool showNextSinger = compact || expandedPanel_ == PanelId::nextSinger;
    const bool showSfx = compact || expandedPanel_ == PanelId::soundEffects;

    backgroundCard_.setVisible (compact);
    lyricCard_.setVisible (compact);
    nextSingerCard_.setVisible (compact);
    sfxCard_.setVisible (compact);
    collapsePanelButton_.setVisible (expanded);
    panelTitleLabel_.setVisible (expanded);

    bgPlayPauseButton_.setVisible (showBackground);
    bgVolumeSlider_.setVisible (showBackground);
    bgVolumeLabel_.setVisible (expandedPanel_ == PanelId::backgroundMusic);
    bgProgressSlider_.setVisible (showBackground);
    bgTrackLabel_.setVisible (showBackground);
    bgTimeLabel_.setVisible (showBackground);

    lyricToggleWindowButton_.setVisible (expanded && expandedPanel_ == PanelId::lyricDisplay);
    lyricFullscreenButton_.setVisible (expanded && expandedPanel_ == PanelId::lyricDisplay);
    lyricStateLabel_.setVisible (expanded && expandedPanel_ == PanelId::lyricDisplay);
    if (lyricPreview_ != nullptr)
        lyricPreview_->setVisible (compact);

    nextSingerNameLabel_.setVisible (showNextSinger);
    nextSingerSongLabel_.setVisible (showNextSinger);
    nextSingerPlayButton_.setVisible (showNextSinger);

    sfxAreYouReadyButton_.setVisible (showSfx);
    sfxChickenButton_.setVisible (showSfx);
    sfxBurpButton_.setVisible (showSfx);
    sfxBruhButton_.setVisible (showSfx);
    sfxBuzzerButton_.setVisible (showSfx);
    sfxDrumFillButton_.setVisible (showSfx);
    sfxDrumRollButton_.setVisible (showSfx);
    sfxWooHooButton_.setVisible (showSfx);
    sfxVolumeSlider_.setVisible (expanded && expandedPanel_ == PanelId::soundEffects);
    sfxVolumeLabel_.setVisible (expanded && expandedPanel_ == PanelId::soundEffects);

    if (compact)
    {
        auto cardsArea = area.reduced (4, 6);
        const int gap = 8;
        const int cardW = juce::jmax (90, (cardsArea.getWidth() - gap * 3) / 4);

        auto bgCard = cardsArea.removeFromLeft (cardW);
        backgroundCard_.setBounds (bgCard);
        cardsArea.removeFromLeft (gap);
        auto lyricCardBounds = cardsArea.removeFromLeft (cardW);
        lyricCard_.setBounds (lyricCardBounds);
        cardsArea.removeFromLeft (gap);
        auto nextCard = cardsArea.removeFromLeft (cardW);
        nextSingerCard_.setBounds (nextCard);
        cardsArea.removeFromLeft (gap);
        auto sfxCardBounds = cardsArea;
        sfxCard_.setBounds (sfxCardBounds);

        auto bgInner = bgCard.reduced (10, 8);
        bgTrackLabel_.setBounds (bgInner.removeFromTop (22));
        bgProgressSlider_.setBounds (bgInner.removeFromTop (18));
        bgInner.removeFromTop (8);
        auto bgBottomRow = bgInner.removeFromBottom (28);
        bgPrevButton_.setBounds (bgBottomRow.removeFromLeft (30));
        bgBottomRow.removeFromLeft (2);
        bgPlayPauseButton_.setBounds (bgBottomRow.removeFromLeft (38));
        bgBottomRow.removeFromLeft (2);
        bgNextButton_.setBounds (bgBottomRow.removeFromLeft (30));
        bgBottomRow.removeFromLeft (4);
        bgTimeLabel_.setBounds (bgBottomRow.removeFromRight (60));
        bgVolumeSlider_.setBounds (bgBottomRow);

        if (lyricPreview_ != nullptr)
            lyricPreview_->setBounds (lyricCardBounds.reduced (8));

        auto nextInner = nextCard.reduced (10, 8);
        nextSingerNameLabel_.setBounds (nextInner.removeFromTop (24));
        nextSingerSongLabel_.setBounds (nextInner.removeFromTop (42));
        nextInner.removeFromTop (6);
        nextSingerPlayButton_.setBounds (nextInner.removeFromBottom (28));

        auto sfxInner = sfxCardBounds.reduced (8);
        const int columns = 4;
        const int rows = 2;
        const int cellGap = 6;
        const int cellW = (sfxInner.getWidth() - cellGap * (columns - 1)) / columns;
        const int cellH = (sfxInner.getHeight() - cellGap * (rows - 1)) / rows;
        juce::DrawableButton* buttons[] = {
            &sfxAreYouReadyButton_, &sfxChickenButton_, &sfxBurpButton_, &sfxBruhButton_,
            &sfxBuzzerButton_, &sfxDrumFillButton_, &sfxDrumRollButton_, &sfxWooHooButton_
        };

        for (int i = 0; i < 8; ++i)
        {
            const int col = i % columns;
            const int row = i / columns;
            buttons[i]->setBounds (sfxInner.getX() + col * (cellW + cellGap),
                                   sfxInner.getY() + row * (cellH + cellGap),
                                   cellW,
                                   cellH);
        }

        return;
    }

    auto header = area.removeFromTop (40);
    collapsePanelButton_.setBounds (header.removeFromLeft (140));
    panelTitleLabel_.setBounds (header.reduced (8, 0));

    auto content = area.reduced (10, 8);

    if (expandedPanel_ == PanelId::backgroundMusic)
    {
        bgTrackLabel_.setBounds (content.removeFromTop (28));
        auto seekRow = content.removeFromTop (24);
        bgProgressSlider_.setBounds (seekRow.removeFromLeft (juce::jmax (180, seekRow.getWidth() - 74)));
        bgTimeLabel_.setBounds (seekRow.reduced (8, 0));
        content.removeFromTop (12);
        auto controls = content.removeFromTop (34);
        auto transportRow = controls.removeFromTop (34);
        bgPrevButton_.setBounds (transportRow.removeFromLeft (46));
        transportRow.removeFromLeft (8);
        bgPlayPauseButton_.setBounds (transportRow.removeFromLeft (120));
        transportRow.removeFromLeft (8);
        bgNextButton_.setBounds (transportRow.removeFromLeft (46));
        controls.removeFromTop (8);
        bgVolumeLabel_.setBounds (controls.removeFromLeft (80));
        bgVolumeSlider_.setBounds (controls);
    }
    else if (expandedPanel_ == PanelId::lyricDisplay)
    {
        auto row = content.removeFromTop (34);
        lyricToggleWindowButton_.setBounds (row.removeFromLeft (180));
        row.removeFromLeft (10);
        lyricFullscreenButton_.setBounds (row.removeFromLeft (180));
        content.removeFromTop (16);
        lyricStateLabel_.setBounds (content.removeFromTop (28));
    }
    else if (expandedPanel_ == PanelId::nextSinger)
    {
        nextSingerNameLabel_.setBounds (content.removeFromTop (34));
        nextSingerSongLabel_.setBounds (content.removeFromTop (30));
        content.removeFromTop (10);
        nextSingerPlayButton_.setBounds (content.removeFromTop (34).removeFromLeft (220));
    }
    else if (expandedPanel_ == PanelId::soundEffects)
    {
        auto grid = content.removeFromTop (120);
        const int columns = 4;
        const int rows = 2;
        const int gap = 12;
        const int cellW = (grid.getWidth() - gap * (columns - 1)) / columns;
        const int cellH = (grid.getHeight() - gap * (rows - 1)) / rows;
        juce::DrawableButton* buttons[] = {
            &sfxAreYouReadyButton_, &sfxChickenButton_, &sfxBurpButton_, &sfxBruhButton_,
            &sfxBuzzerButton_, &sfxDrumFillButton_, &sfxDrumRollButton_, &sfxWooHooButton_
        };

        for (int i = 0; i < 8; ++i)
        {
            const int col = i % columns;
            const int row = i / columns;
            buttons[i]->setBounds (grid.getX() + col * (cellW + gap),
                                   grid.getY() + row * (cellH + gap),
                                   cellW,
                                   cellH);
        }

        content.removeFromTop (10);
        auto volRow = content.removeFromTop (30);
        sfxVolumeLabel_.setBounds (volRow.removeFromLeft (90));
        sfxVolumeSlider_.setBounds (volRow);
    }
}

void RibbonMenu::mouseDown (const juce::MouseEvent& event)
{
    if (! isInDragHandle (event.getPosition()))
        return;

    draggingHandle_ = true;
    dragStartHeight_ = collapsedHeight_;
    dragStartScreenY_ = event.getScreenY();
}

void RibbonMenu::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingHandle_)
        return;

    const int delta = dragStartScreenY_ - event.getScreenY();
    const int targetHeight = juce::jlimit (minCollapsedHeight_, getMaxCollapsedHeight(), dragStartHeight_ + delta);

    if (hidden_ && delta > 2)
        hidden_ = false;

    if (! hidden_)
    {
        expandedPanel_ = PanelId::none;
        collapsedHeight_ = targetHeight;
        lastOpenHeight_ = collapsedHeight_;

        if (onLayoutChanged)
            onLayoutChanged();
    }
}

void RibbonMenu::mouseUp (const juce::MouseEvent&)
{
    draggingHandle_ = false;
}

void RibbonMenu::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! isInDragHandle (event.getPosition()))
        return;

    toggleHidden();
}

void RibbonMenu::setBackgroundState (bool playing, float volume01)
{
    backgroundPlaying_ = playing;
    backgroundVolume01_ = juce::jlimit (0.0f, 1.0f, volume01);
    updateControlState();
}

void RibbonMenu::setBackgroundTrackInfo (const juce::String& songName,
                                         double positionSeconds,
                                         double totalSeconds)
{
    backgroundSongName_ = songName;
    backgroundPositionSeconds_ = juce::jmax (0.0, positionSeconds);
    backgroundTotalSeconds_ = juce::jmax (0.0, totalSeconds);
    updateControlState();
}

void RibbonMenu::setSfxVolume (float volume01)
{
    sfxVolume01_ = juce::jlimit (0.0f, 1.0f, volume01);
    updateControlState();
}

void RibbonMenu::setLyricWindowVisible (bool visible)
{
    lyricWindowVisible_ = visible;
    updateControlState();
}

void RibbonMenu::setLyricWindowFullScreen (bool fullScreen)
{
    lyricWindowFullScreen_ = fullScreen;
    updateControlState();
}

void RibbonMenu::setNextSinger (const juce::String& singerName, const juce::String& songName)
{
    nextSingerName_ = singerName;
    nextSingerSong_ = songName;
    updateControlState();
}

void RibbonMenu::toggleHidden()
{
    if (hidden_)
    {
        hidden_ = false;
        expandedPanel_ = PanelId::none;
        collapsedHeight_ = juce::jlimit (minCollapsedHeight_, getMaxCollapsedHeight(), lastOpenHeight_);
    }
    else
    {
        hidden_ = true;
        lastOpenHeight_ = collapsedHeight_;
        expandedPanel_ = PanelId::none;
    }

    updateControlState();
    if (onLayoutChanged)
        onLayoutChanged();
}

void RibbonMenu::togglePanel (PanelId id)
{
    if (id == PanelId::none)
        expandedPanel_ = PanelId::none;
    else if (expandedPanel_ == id)
        expandedPanel_ = PanelId::none;
    else
        expandedPanel_ = id;

    updateControlState();
    if (onLayoutChanged)
        onLayoutChanged();
}

void RibbonMenu::updateCardTexts()
{
    backgroundCard_.setButtonText ({});
    lyricCard_.setButtonText ({});
    nextSingerCard_.setButtonText ({});
    sfxCard_.setButtonText ({});

    nextSingerNameLabel_.setText (nextSingerName_.isNotEmpty() ? nextSingerName_ : tr ("ribbon.next_singer.none"),
                                  juce::dontSendNotification);
    nextSingerSongLabel_.setText (nextSingerSong_, juce::dontSendNotification);
}

void RibbonMenu::updateControlState()
{
    panelTitleLabel_.setText (panelTitleFor (expandedPanel_), juce::dontSendNotification);
    collapsePanelButton_.setButtonText (tr ("ribbon.back"));

    setSpriteButtonIcon (bgPlayPauseButton_, backgroundPlaying_ ? "icon-pause2" : "icon-play3");
    bgVolumeSlider_.setValue (backgroundVolume01_, juce::dontSendNotification);
    bgVolumeLabel_.setText (tr ("ribbon.volume"), juce::dontSendNotification);
    bgTrackLabel_.setText (backgroundSongName_.isNotEmpty() ? backgroundSongName_ : tr ("ribbon.background.none"),
                           juce::dontSendNotification);
    bgProgressSlider_.setValue (backgroundTotalSeconds_ > 0.0 ? backgroundPositionSeconds_ / backgroundTotalSeconds_ : 0.0,
                                juce::dontSendNotification);
    bgTimeLabel_.setText (formatSeconds (backgroundPositionSeconds_) + " / " + formatSeconds (backgroundTotalSeconds_),
                          juce::dontSendNotification);

    lyricToggleWindowButton_.setButtonText (lyricWindowVisible_ ? tr ("ribbon.action.hide_lyric_window")
                                                               : tr ("ribbon.action.show_lyric_window"));
    lyricFullscreenButton_.setButtonText (lyricWindowFullScreen_ ? tr ("ribbon.action.exit_full_screen")
                                                                 : tr ("ribbon.action.full_screen"));
    const auto lyricWindowState = lyricWindowVisible_ ? tr ("ribbon.state.visible") : tr ("ribbon.state.hidden");
    const auto lyricFullState = lyricWindowFullScreen_ ? tr ("ribbon.state.yes") : tr ("ribbon.state.no");
    lyricStateLabel_.setText (tr ("ribbon.lyric.status").replace ("{0}", lyricWindowState).replace ("{1}", lyricFullState),
                              juce::dontSendNotification);

    nextSingerPlayButton_.setButtonText (tr ("ribbon.action.play_next_singer"));

    sfxVolumeSlider_.setValue (sfxVolume01_, juce::dontSendNotification);
    sfxVolumeLabel_.setText (tr ("ribbon.sfx_volume"), juce::dontSendNotification);
    sfxAreYouReadyButton_.setTooltip (tr ("ribbon.sfx.are_you_ready"));
    sfxChickenButton_.setTooltip (tr ("ribbon.sfx.chicken"));
    sfxBurpButton_.setTooltip (tr ("ribbon.sfx.burp"));
    sfxBruhButton_.setTooltip (tr ("ribbon.sfx.bruh"));
    sfxBuzzerButton_.setTooltip (tr ("ribbon.sfx.buzzer"));
    sfxDrumFillButton_.setTooltip (tr ("ribbon.sfx.drum_fill"));
    sfxDrumRollButton_.setTooltip (tr ("ribbon.sfx.drum_roll"));
    sfxWooHooButton_.setTooltip (tr ("ribbon.sfx.woohoo"));

    updateCardTexts();
    resized();
    repaint();
}

bool RibbonMenu::isInDragHandle (juce::Point<int> p) const noexcept
{
    return getLocalBounds().reduced (8, 0).removeFromTop (dragHandleHeight_).contains (p);
}

int RibbonMenu::getMaxCollapsedHeight() const noexcept
{
    const int parentHeight = getParentHeight() > 0 ? getParentHeight() : maxCollapsedHeightCap_;
    return juce::jmax (minCollapsedHeight_, juce::jmin (maxCollapsedHeightCap_, parentHeight - 48));
}
