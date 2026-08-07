/*
  ==============================================================================

    RibbonMenu.cpp

  ==============================================================================
*/

#include "RibbonMenu.h"
#include "SpriteIcon.h"
#include "../Localization/LocalizationManager.h"
#include "../Services/SfxLibraryService.h"
#include "../Services/UserPreferences.h"

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

// Shared by setSfxButtonIcon (fixed relative-path callers below) and by
// RibbonMenu::refreshSfxSlots(), which resolves an icon File directly via
// SfxLibraryService rather than a fixed relative path string.
void setSfxButtonIconFromFile (juce::DrawableButton& button, const juce::File& iconFile)
{
    auto normal = SpriteIcon::createFromSvgFile (iconFile, kSfxIconTint);
    if (normal == nullptr)
        return;

    // Already tinted by createFromSvgFile -- copies inherit that, no need
    // to re-tint them.
    auto over = normal->createCopy();
    auto down = normal->createCopy();
    button.setImages (normal.release(), over.release(), down.release());
}

void setSfxButtonIcon (juce::DrawableButton& button, const juce::String& iconRelativePath)
{
    auto iconFile = SpriteIcon::resolveAssetFile (iconRelativePath);
    setSfxButtonIconFromFile (button, iconFile);
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

    // Gear icons -- added after the cards so they sit on top in z-order.
    // Same action as clicking the card itself; just an explicit affordance.
    for (auto* gear : { &backgroundCardGear_, &lyricCardGear_, &nextSingerCardGear_, &sfxCardGear_ })
    {
        addAndMakeVisible (*gear);
        setSpriteButtonIcon (*gear, "icon-cog");
        gear->setTooltip (tr ("ribbon.expand"));
    }
    backgroundCardGear_.onClick = [this]() { togglePanel (PanelId::backgroundMusic); };
    lyricCardGear_.onClick      = [this]() { togglePanel (PanelId::lyricDisplay); };
    nextSingerCardGear_.onClick = [this]() { togglePanel (PanelId::nextSinger); };
    sfxCardGear_.onClick        = [this]() { togglePanel (PanelId::soundEffects); };

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
    bgVolumeSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 20);
    // Displayed/dragged on a 0-10 scale (more familiar than a raw 0-1
    // fraction); converted to/from the real 0.0-1.0 volume at the boundary
    // here so onBackgroundVolumeChanged's contract (and backgroundVolume01_)
    // stay in the same 0-1 range every other caller already expects.
    bgVolumeSlider_.setRange (0.0, 10.0, 1.0);
    bgVolumeSlider_.onValueChange = [this]()
    {
        backgroundVolume01_ = (float) bgVolumeSlider_.getValue() / 10.0f;
        if (onBackgroundVolumeChanged)
            onBackgroundVolumeChanged (backgroundVolume01_);
    };

    addAndMakeVisible (bgVolumeLabel_);
    bgVolumeLabel_.setColour (juce::Label::textColourId, kText);

    bgLibraryPanel_ = std::make_unique<BackgroundMusicLibraryPanel>();
    addAndMakeVisible (*bgLibraryPanel_);
    bgLibraryPanel_->onFolderChanged = [this] (juce::File folder)
    {
        if (onBackgroundFolderChanged)
            onBackgroundFolderChanged (folder);
    };
    bgLibraryPanel_->onSelectionChanged = [this] (juce::StringArray selected)
    {
        if (onBackgroundSelectionChanged)
            onBackgroundSelectionChanged (selected);
    };
    bgLibraryPanel_->onPreviewRequested = [this] (juce::File file)
    {
        if (onBackgroundPreviewRequested)
            onBackgroundPreviewRequested (file);
    };
    bgLibraryPanel_->onUseDefaultRequested = [this]
    {
        if (onBackgroundUseDefaultRequested)
            onBackgroundUseDefaultRequested();
    };

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

    // Icons + onClick effectNames are data-driven from UserPreferences (see
    // refreshSfxSlots(), called at the end of this constructor) rather than
    // hardcoded here, now that any of SfxLibraryService's sounds can occupy
    // any of these 8 buttons.
    for (auto* button : sfxSlotButtons())
    {
        addAndMakeVisible (*button);
        styleActionButton (*button);
    }

    for (auto* clearButton : sfxSlotClearButtons())
    {
        addAndMakeVisible (*clearButton);
        clearButton->setButtonText (juce::String::fromUTF8 ("\xc3\x97")); // "x" (multiplication sign)
        clearButton->setTooltip (tr ("ribbon.sfx.clear_slot"));
    }
    for (int i = 0; i < 8; ++i)
    {
        auto* clearButton = sfxSlotClearButtons()[(size_t) i];
        clearButton->onClick = [this, i]()
        {
            UserPreferences::getInstance().setSfxSlotAssignment (i, {});
            refreshSfxSlots();
        };
    }

    sfxLibraryPanel_ = std::make_unique<SfxLibraryListPanel>();
    addAndMakeVisible (*sfxLibraryPanel_);
    sfxLibraryPanel_->onPreviewRequested = [this] (juce::String soundName)
    {
        if (onTriggerSfx)
            onTriggerSfx (soundName);
    };

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

    refreshSfxSlots();
    updateAllText();
    updateControlState();
}

std::array<juce::DrawableButton*, 8> RibbonMenu::sfxSlotButtons()
{
    return {
        &sfxAreYouReadyButton_, &sfxChickenButton_, &sfxBurpButton_, &sfxBruhButton_,
        &sfxBuzzerButton_, &sfxDrumFillButton_, &sfxDrumRollButton_, &sfxWooHooButton_
    };
}

std::array<juce::TextButton*, 8> RibbonMenu::sfxSlotClearButtons()
{
    return {
        &sfxSlotClearButtons_[0], &sfxSlotClearButtons_[1], &sfxSlotClearButtons_[2], &sfxSlotClearButtons_[3],
        &sfxSlotClearButtons_[4], &sfxSlotClearButtons_[5], &sfxSlotClearButtons_[6], &sfxSlotClearButtons_[7]
    };
}

void RibbonMenu::refreshSfxSlots()
{
    const auto assignments = UserPreferences::getInstance().getSfxSlotAssignments();
    sfxSlotAssignmentsCache_ = assignments;
    auto buttons = sfxSlotButtons();
    auto clearButtons = sfxSlotClearButtons();

    for (int i = 0; i < 8; ++i)
    {
        const auto name = assignments[i];
        auto* button = buttons[(size_t) i];

        if (name.isEmpty())
        {
            button->setImages (nullptr);
            button->onClick = nullptr;
            button->setTooltip ({});
            clearButtons[(size_t) i]->setVisible (false);
            continue;
        }

        button->setTooltip (name);
        button->onClick = [this, name]() { if (onTriggerSfx) onTriggerSfx (name); };

        if (auto* entry = SfxLibraryService::getInstance().findByName (name))
        {
            if (entry->iconFile.existsAsFile())
                setSfxButtonIconFromFile (*button, entry->iconFile);
            else
                setSpriteButtonIcon (*button, "icon-volume-high");
        }
        else
        {
            // Assigned name no longer matches any sound on disk.
            setSpriteButtonIcon (*button, "icon-volume-high");
        }
    }

    // Clear-button visibility/positioning is finished in resized() (needs
    // each slot's on-screen bounds, which only exist there), except for
    // the "hide when empty" case handled above.
    resized();
}

bool RibbonMenu::isInterestedInDragSource (const SourceDetails& details)
{
    // Only sound tiles dragged from sfxLibraryPanel_ encode themselves as a
    // plain string naming a real SfxLibraryService entry -- nothing else in
    // this app starts a drag with that description shape.
    return details.description.isString()
        && SfxLibraryService::getInstance().findByName (details.description.toString()) != nullptr;
}

void RibbonMenu::itemDropped (const SourceDetails& details)
{
    const auto soundName = details.description.toString();
    if (SfxLibraryService::getInstance().findByName (soundName) == nullptr)
        return;

    auto buttons = sfxSlotButtons();
    for (int i = 0; i < 8; ++i)
    {
        // localPosition is already relative to this component (the target
        // receiving the drop), the same space the slot buttons' own
        // getBounds() live in -- no conversion needed.
        if (buttons[(size_t) i]->getBounds().contains (details.localPosition))
        {
            UserPreferences::getInstance().setSfxSlotAssignment (i, soundName);
            refreshSfxSlots();
            return;
        }
    }
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
        backgroundCardGear_.setVisible (false);
        lyricCardGear_.setVisible (false);
        nextSingerCardGear_.setVisible (false);
        sfxCardGear_.setVisible (false);
        collapsePanelButton_.setVisible (false);
        panelTitleLabel_.setVisible (false);
        bgPlayPauseButton_.setVisible (false);
        bgVolumeSlider_.setVisible (false);
        bgVolumeLabel_.setVisible (false);
        bgProgressSlider_.setVisible (false);
        bgTrackLabel_.setVisible (false);
        bgTimeLabel_.setVisible (false);
        bgLibraryPanel_->setVisible (false);
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
        sfxLibraryPanel_->setVisible (false);
        for (auto* clearButton : sfxSlotClearButtons())
            clearButton->setVisible (false);
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
    backgroundCardGear_.setVisible (compact);
    lyricCardGear_.setVisible (compact);
    nextSingerCardGear_.setVisible (compact);
    sfxCardGear_.setVisible (compact);
    collapsePanelButton_.setVisible (expanded);
    panelTitleLabel_.setVisible (expanded);

    bgPlayPauseButton_.setVisible (showBackground);
    bgVolumeSlider_.setVisible (showBackground);
    bgVolumeLabel_.setVisible (expandedPanel_ == PanelId::backgroundMusic);
    bgProgressSlider_.setVisible (showBackground);
    bgTrackLabel_.setVisible (showBackground);
    bgTimeLabel_.setVisible (showBackground);
    bgLibraryPanel_->setVisible (expandedPanel_ == PanelId::backgroundMusic);

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
    const bool sfxExpanded = expanded && expandedPanel_ == PanelId::soundEffects;
    sfxVolumeSlider_.setVisible (sfxExpanded);
    sfxVolumeLabel_.setVisible (sfxExpanded);
    sfxLibraryPanel_->setVisible (sfxExpanded);

    auto clearButtons = sfxSlotClearButtons();
    for (int i = 0; i < 8; ++i)
    {
        const bool slotOccupied = i < sfxSlotAssignmentsCache_.size()
                                && sfxSlotAssignmentsCache_[i].isNotEmpty();
        clearButtons[(size_t) i]->setVisible (sfxExpanded && slotOccupied);
    }

    if (compact)
    {
        auto cardsArea = area.reduced (4, 6);
        const int gap = 8;
        const int cardW = juce::jmax (90, (cardsArea.getWidth() - gap * 3) / 4);

        constexpr int gearSize = 18;
        auto positionGear = [gearSize] (juce::DrawableButton& gear, juce::Rectangle<int> card)
        {
            gear.setBounds (card.getRight() - gearSize - 4, card.getY() + 4, gearSize, gearSize);
            // Force frontmost every layout pass -- construction order alone
            // wasn't enough to keep these above each card's own controls
            // (e.g. the SFX grid's top-right button sits at this same
            // corner), so they were unclickable.
            gear.toFront (false);
        };

        auto bgCard = cardsArea.removeFromLeft (cardW);
        backgroundCard_.setBounds (bgCard);
        positionGear (backgroundCardGear_, bgCard);
        cardsArea.removeFromLeft (gap);
        auto lyricCardBounds = cardsArea.removeFromLeft (cardW);
        lyricCard_.setBounds (lyricCardBounds);
        positionGear (lyricCardGear_, lyricCardBounds);
        cardsArea.removeFromLeft (gap);
        auto nextCard = cardsArea.removeFromLeft (cardW);
        nextSingerCard_.setBounds (nextCard);
        positionGear (nextSingerCardGear_, nextCard);
        cardsArea.removeFromLeft (gap);
        auto sfxCardBounds = cardsArea;
        sfxCard_.setBounds (sfxCardBounds);
        positionGear (sfxCardGear_, sfxCardBounds);

        auto bgInner = bgCard.reduced (10, 8);
        bgTrackLabel_.setBounds (bgInner.removeFromTop (22));

        // Progress bar, with the elapsed/duration time to its right --
        // stays pinned right under the track label.
        auto bgProgressRow = bgInner.removeFromTop (16);
        bgTimeLabel_.setBounds (bgProgressRow.removeFromRight (60));
        bgProgressSlider_.setBounds (bgProgressRow);
        bgInner.removeFromTop (4);

        // Volume slider stays pinned to the bottom of the box...
        bgVolumeSlider_.setBounds (bgInner.removeFromBottom (18));
        bgInner.removeFromBottom (4);

        // ...and the transport buttons are vertically centred in whatever
        // space is left between the progress row and the volume slider,
        // rather than sitting flush against either one.
        constexpr int bgButtonsRowH = 26;
        auto bgButtonsRow = bgInner.withSizeKeepingCentre (bgInner.getWidth(), bgButtonsRowH);
        bgPrevButton_.setBounds (bgButtonsRow.removeFromLeft (30));
        bgButtonsRow.removeFromLeft (2);
        bgPlayPauseButton_.setBounds (bgButtonsRow.removeFromLeft (38));
        bgButtonsRow.removeFromLeft (2);
        bgNextButton_.setBounds (bgButtonsRow.removeFromLeft (30));

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
        auto transportRow = content.removeFromTop (34);
        bgPrevButton_.setBounds (transportRow.removeFromLeft (46));
        transportRow.removeFromLeft (8);
        bgPlayPauseButton_.setBounds (transportRow.removeFromLeft (120));
        transportRow.removeFromLeft (8);
        bgNextButton_.setBounds (transportRow.removeFromLeft (46));
        content.removeFromTop (8);

        auto volumeRow = content.removeFromTop (28);
        bgVolumeLabel_.setBounds (volumeRow.removeFromLeft (80));
        bgVolumeSlider_.setBounds (volumeRow);

        content.removeFromTop (10);
        bgLibraryPanel_->setBounds (content);
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
        auto buttons = sfxSlotButtons();
        auto clearButtons = sfxSlotClearButtons();
        constexpr int clearSize = 18;

        for (int i = 0; i < 8; ++i)
        {
            const int col = i % columns;
            const int row = i / columns;
            const juce::Rectangle<int> cell (grid.getX() + col * (cellW + gap),
                                             grid.getY() + row * (cellH + gap),
                                             cellW, cellH);
            buttons[(size_t) i]->setBounds (cell);

            // Same overlay-z-order fix as the card gear icons -- add order
            // alone doesn't reliably keep this on top of the slot button.
            clearButtons[(size_t) i]->setBounds (cell.getRight() - clearSize - 2, cell.getY() + 2, clearSize, clearSize);
            clearButtons[(size_t) i]->toFront (false);
        }

        content.removeFromTop (10);
        auto volRow = content.removeFromTop (30);
        sfxVolumeLabel_.setBounds (volRow.removeFromLeft (90));
        sfxVolumeSlider_.setBounds (volRow);

        content.removeFromTop (10);
        sfxLibraryPanel_->setBounds (content);
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

void RibbonMenu::setBackgroundFolderPath (const juce::String& path)
{
    bgLibraryPanel_->setFolderPath (path);
}

void RibbonMenu::setBackgroundAvailableTracks (const std::vector<juce::File>& tracks,
                                               const juce::StringArray& selectedFilenames)
{
    bgLibraryPanel_->setTracks (tracks, selectedFilenames);
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
    bgVolumeSlider_.setValue (backgroundVolume01_ * 10.0f, juce::dontSendNotification);
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
    // Slot tooltips are data-driven -- see refreshSfxSlots(), which already
    // sets each button's tooltip to its assigned sound's name and must not
    // be overwritten here with the old fixed 8 names.

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
