/*
  ==============================================================================

    TopBar.cpp
    Created: 16 Apr 2026 11:00:00am
    Author:  GitHub Copilot

    Top bar component implementation

  ==============================================================================
*/

#include "TopBar.h"
#include "../Services/UserPreferences.h"
#include <cmath>

//==============================================================================
// Color constants from original app
const juce::Colour TopBar::BACKGROUND_COLOR = juce::Colour(0x26, 0x26, 0x26);  // #262626
const juce::Colour TopBar::OFFLINE_COLOR = juce::Colour(0x8b, 0x00, 0x00);     // darkred
const juce::Colour TopBar::TEXT_COLOR = juce::Colours::white;
const juce::Colour TopBar::ACCENT_COLOR = juce::Colour(0x30, 0xda, 0xff);      // #30DAFF

// Resize handle colors - Visual Studio style
static const juce::Colour RESIZE_HANDLE_INACTIVE_COLOR = juce::Colour(0x3c, 0x3c, 0x3c); // Dark grey
static const juce::Colour RESIZE_HANDLE_ACTIVE_COLOR = juce::Colour(0x30, 0xda, 0xff);   // #30DAFF

//==============================================================================
TopBar::TopBar()
{
    setupUI();
    loadAssets();

    const int savedStyle = UserPreferences::getInstance().getVuMeterStyle();
    if (savedStyle >= 0 && savedStyle < kNumVuMeterStyles)
        vuMeterStyle_ = static_cast<VuMeterStyle>(savedStyle);

    // Start timer for VU meter updates (60 FPS)
    startTimer(16);

    DBG("TopBar initialized");
}

TopBar::~TopBar()
{
    stopTimer();
}

//==============================================================================
void TopBar::setupUI()
{
    // Version label -- same version reported at build time (CMakeLists.txt's
    // ENCORE_VERSION_MAJOR_MINOR + build_number.txt, baked into
    // ProjectInfo::versionString by JUCE at configure time), not a
    // hand-maintained string that drifts from the real build.
    versionLabel = std::make_unique<juce::Label>("version", "ver. " + juce::String(ProjectInfo::versionString));
    versionLabel->setFont(juce::Font(10.0f));
    versionLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    versionLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    versionLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(versionLabel.get());
    
    // Track name label
    trackNameLabel = std::make_unique<juce::Label>("trackName", "Test Song - Test Artist");
    trackNameLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    trackNameLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    trackNameLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    trackNameLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(trackNameLabel.get());
    
    // Artist name label (part of track info)
    artistNameLabel = std::make_unique<juce::Label>("artistName", "Karaoke Version");
    artistNameLabel->setFont(juce::Font(14.0f));
    artistNameLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    artistNameLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    artistNameLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(artistNameLabel.get());
    
    // Key title label (bold)
    keyTitleLabel = std::make_unique<juce::Label>("keyTitle", LocalizationManager::getInstance().getText("topbar.key"));
    keyTitleLabel->setFont(juce::Font(15.0f, juce::Font::bold));
    keyTitleLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    keyTitleLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    keyTitleLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(keyTitleLabel.get());
    
    // Key value label (normal)
    keyValueLabel = std::make_unique<juce::Label>("keyValue", "");
    keyValueLabel->setFont(juce::Font(14.0f));
    keyValueLabel->setColour(juce::Label::textColourId, juce::Colour(0xffb8c1cf));
    keyValueLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    keyValueLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(keyValueLabel.get());
    
    // BPM title label (bold)
    bpmTitleLabel = std::make_unique<juce::Label>("bpmTitle", LocalizationManager::getInstance().getText("topbar.bpm"));
    bpmTitleLabel->setFont(juce::Font(15.0f, juce::Font::bold));
    bpmTitleLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    bpmTitleLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    bpmTitleLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmTitleLabel.get());
    
    // BPM value label (normal)  
    bpmValueLabel = std::make_unique<juce::Label>("bpmValue", "");
    bpmValueLabel->setFont(juce::Font(14.0f));
    bpmValueLabel->setColour(juce::Label::textColourId, juce::Colour(0xffb8c1cf));
    bpmValueLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    bpmValueLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmValueLabel.get());
    
    // Offline warning label
    offlineWarningLabel = std::make_unique<juce::Label>("offline", LocalizationManager::getInstance().getText("topbar.offline_warning"));
    offlineWarningLabel->setFont(juce::Font(14.0f, juce::Font::bold));
    offlineWarningLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    offlineWarningLabel->setJustificationType(juce::Justification::centred);
    offlineWarningLabel->setVisible(false);
    addAndMakeVisible(offlineWarningLabel.get());
    
    // User name label -- bigger font per request; mouse clicks pass through
    // to userButton beneath (added after, so it's already on top for hit
    // testing, but this makes that explicit/robust rather than incidental).
    userNameLabel = std::make_unique<juce::Label>("userName", "Test User");
    userNameLabel->setFont(juce::Font(16.0f));
    userNameLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    userNameLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    userNameLabel->setJustificationType(juce::Justification::centredLeft);
    userNameLabel->setInterceptsMouseClicks(false, false);
    addAndMakeVisible(userNameLabel.get());

    // User button -- covers the avatar AND the name (see resized()) so
    // clicking either opens the popup menu. InvisibleClickTarget paints
    // nothing, so there's no LookAndFeel button background/outline drawn
    // around them.
    userButton = std::make_unique<InvisibleClickTarget>();
    userButton->setWantsKeyboardFocus(false);
    userButton->onClick = [this]() {
        if (onUserButtonClicked)
            onUserButtonClicked();
    };
    addAndMakeVisible(userButton.get());

    // Update pill -- VS Code-style persistent title-bar button, hidden
    // until setUpdateAvailable(true, ...) is called. Coloured with the
    // app's primary accent (same cyan as ACCENT_COLOR / QueueBar's
    // "Clear Queue" button) with black text for contrast, matching that
    // button's look exactly.
    updateButton_ = std::make_unique<juce::TextButton>("Update");
    updateButton_->setButtonText(LocalizationManager::getInstance().getText("update.pill_label"));
    updateButton_->setColour(juce::TextButton::buttonColourId, ACCENT_COLOR);
    updateButton_->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    updateButton_->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    updateButton_->onClick = [this]() {
        if (onUpdateButtonClicked)
            onUpdateButtonClicked();
    };
    addAndMakeVisible(updateButton_.get());
    updateButton_->setVisible(false); // addAndMakeVisible() forces visible(true), so this must come after

    // Logo button
    logoButton = std::make_unique<juce::ImageButton>("Logo");
    logoButton->onClick = [this]() {
        if (onLogoClicked)
            onLogoClicked();
    };
    addAndMakeVisible(logoButton.get());
}

//==============================================================================
void TopBar::loadAssets()
{
    // Use executable directory to find assets (getCurrentWorkingDirectory is unreliable on macOS)
    auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();

    // Try to load logo image
    juce::File logoFile = appDir.getChildFile("assets/images/encore-logo-white.png");
    
    if (logoFile.existsAsFile())
    {
        logoImage = juce::ImageFileFormat::loadFrom(logoFile);
        if (logoImage.isValid())
        {
            logoButton->setImages(false, true, true, logoImage, 1.0f, juce::Colours::transparentBlack,
                                 logoImage, 0.8f, juce::Colours::transparentBlack,
                                 logoImage, 0.6f, juce::Colours::transparentBlack);
        }
    }
    
    // Load user avatar image
    juce::File avatarFile = appDir.getChildFile("assets/icon/2345434.png");
    
    DBG("Looking for custom avatar at: " + avatarFile.getFullPathName());
    
    if (avatarFile.existsAsFile())
    {
        userAvatarImage = juce::ImageFileFormat::loadFrom(avatarFile);
        if (userAvatarImage.isValid())
        {
            // Resize to avatar size while maintaining aspect ratio
            userAvatarImage = userAvatarImage.rescaled(AVATAR_SIZE, AVATAR_SIZE);
            DBG("User avatar loaded from icon/2345434.png");
            DBG("Avatar size after rescaling: " + juce::String(userAvatarImage.getWidth()) + "x" + juce::String(userAvatarImage.getHeight()));
        }
        else
        {
            userAvatarImage = juce::Image(); // Clear invalid image
            DBG("Custom avatar file exists but failed to load");
        }
    }
    else
    {
        DBG("Custom avatar file doesn't exist");
    }
    
    // Load default avatar image (AvatarWhite.png)
    juce::File defaultAvatarFile = appDir.getChildFile("assets/images/AvatarWhite.png");
    
    DBG("Looking for default avatar at: " + defaultAvatarFile.getFullPathName());
    
    if (defaultAvatarFile.existsAsFile())
    {
        defaultAvatarImage = juce::ImageFileFormat::loadFrom(defaultAvatarFile);
        if (defaultAvatarImage.isValid())
        {
            defaultAvatarImage = defaultAvatarImage.rescaled(AVATAR_SIZE, AVATAR_SIZE);
            DBG("Default avatar loaded from AvatarWhite.png");
            DBG("Default avatar size: " + juce::String(defaultAvatarImage.getWidth()) + "x" + juce::String(defaultAvatarImage.getHeight()));
        }
    }
    else
    {
        DBG("Default avatar file doesn't exist");
    }
    
    // If no user avatar is loaded, use default avatar
    if (!userAvatarImage.isValid() && defaultAvatarImage.isValid())
    {
        userAvatarImage = defaultAvatarImage;
        DBG("Using default avatar (AvatarWhite.png)");
    }
    
    // Load default album art image (AlbumIconWhite.png)
    juce::File defaultAlbumFile = appDir.getChildFile("assets/images/AlbumIconWhite.png");
    
    if (defaultAlbumFile.existsAsFile())
    {
        defaultAlbumArtImage = juce::ImageFileFormat::loadFrom(defaultAlbumFile);
        if (defaultAlbumArtImage.isValid())
        {
            DBG("Default album art loaded from AlbumIconWhite.png");
        }
    }
    
    // VU meter is vector-drawn now (see drawVUMeter() and its per-style
    // helpers) -- except the Analog Needle style's dial-face background,
    // which uses the real photographed meter faces below rather than a
    // drawn approximation.
    juce::File vuBackFile = appDir.getChildFile("assets/images/VUMeterBack.png");
    if (vuBackFile.existsAsFile())
    {
        vuMeterBackImage = juce::ImageFileFormat::loadFrom(vuBackFile);
        DBG("VU meter back image loaded: " + juce::String(vuMeterBackImage.isValid() ? "valid" : "invalid"));
    }

    DBG("Assets loaded - Cover art: " + juce::String(coverArtImage.isValid() ? "custom" : "will use default"));
    DBG("Assets loaded - Avatar: " + juce::String(userAvatarImage.isValid() ? "loaded" : "failed"));
    DBG("Assets loaded - Default avatar: " + juce::String(defaultAvatarImage.isValid() ? "loaded" : "failed"));
    DBG("Assets loaded - Default album: " + juce::String(defaultAlbumArtImage.isValid() ? "loaded" : "failed"));
}

//==============================================================================
void TopBar::paint(juce::Graphics& g)
{
    // Draw background for the entire current bar height
    auto barArea = getLocalBounds().withHeight(currentBarHeight);
    g.fillAll(isOnlineStatus ? BACKGROUND_COLOR : OFFLINE_COLOR);
    
    // Draw resize handle at bottom with Visual Studio style behavior
    auto resizeHandleHeight = getCurrentResizeHandleHeight();
    auto resizeArea = getLocalBounds().removeFromBottom(resizeHandleHeight);
    
    // Choose color based on interaction state
    juce::Colour handleColor;
    if (isResizing || isOverResizeHandle)
        handleColor = RESIZE_HANDLE_ACTIVE_COLOR;  // Blue when active
    else
        handleColor = RESIZE_HANDLE_INACTIVE_COLOR; // Dark grey when inactive
    
    g.setColour(handleColor);
    g.fillRect(resizeArea);
    
    // Draw cover art if available (can extend beyond bar background)
    auto coverArea = getCoverArtArea();
    if (coverArtImage.isValid())
    {
        g.drawImage(coverArtImage, coverArea.toFloat(), 
                   juce::RectanglePlacement::centred);
    }
    else if (defaultAlbumArtImage.isValid())
    {
        // Use default album art (AlbumIconWhite.png)
        g.drawImage(defaultAlbumArtImage, coverArea.toFloat(),
                   juce::RectanglePlacement::centred);
    }
    else
    {
        // Fallback to programmatic placeholder if default image not available
        g.setColour(juce::Colours::darkgrey);
        g.fillRect(coverArea);
        g.setColour(TEXT_COLOR);
        g.setFont(juce::Font(12.0f));
        g.drawText("No\\nCover", coverArea, juce::Justification::centred);
    }
    
    // Draw VU meter
    drawVUMeter(g, getVUMeterArea());
    
    // Draw user avatar
    auto userArea = getUserArea();
    auto avatarBounds = userArea.removeFromLeft(AVATAR_SIZE).reduced(2);

    if (userAvatarImage.isValid())
    {
        g.drawImage(userAvatarImage, avatarBounds.toFloat(),
                   juce::RectanglePlacement::centred);
    }
    else if (defaultAvatarImage.isValid())
    {
        // Use default avatar if user avatar didn't load
        g.drawImage(defaultAvatarImage, avatarBounds.toFloat(),
                   juce::RectanglePlacement::centred);
    }
    else
    {
        // Fallback: draw a simple circle if no image available
        g.setColour(juce::Colours::lightgrey);
        g.fillEllipse(avatarBounds.toFloat());
        g.setColour(juce::Colours::darkgrey);
        g.drawEllipse(avatarBounds.toFloat(), 2.0f);
    }
    
    // Separator lines removed as requested
}

//==============================================================================
void TopBar::resized()
{
    // Update component height to current bar height
    setSize(getWidth(), currentBarHeight);
    
    // Use a fixed resize handle height for UI layout (always 1px) to prevent text repositioning
    auto bounds = getLocalBounds().withHeight(currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT);
    
    // Logo area. Two things this has to get right that a plain square
    // button+label stack didn't:
    //
    // 1. The actual asset (encore-logo-white.png) is a wide wordmark
    //    (540x110, ~4.9:1), not square. A square button with
    //    preserveImageProportions=true (see loadAssets()) scales to fit
    //    the *narrower* constraint -- for a square box that's always
    //    width -- so the image rendered at only ~1/5 of the box's height,
    //    looking tiny with a big empty square around it. Sizing the
    //    button to the image's own aspect ratio instead lets it actually
    //    fill the available width.
    // 2. Logo and version need to sit right next to each other as one
    //    tight block, not "logo centred in the whole column, version
    //    pinned to the column's bottom edge" -- the latter drifts them far
    //    apart on a tall (user-resized) bar, since the column's height can
    //    be much bigger than the logo+version block itself.
    auto logoArea = getLogoArea();
    constexpr int kVersionLabelHeight = 16;
    constexpr int kLogoVersionGap = 6;
    constexpr int kMaxLogoHeight = 31;  // was 70 -> 66 -> 63 -> 44 -> 31 (30% cuts), per request

    const float logoAspect = logoImage.isValid()
        ? (float) logoImage.getWidth() / (float) juce::jmax (1, logoImage.getHeight())
        : (540.0f / 110.0f);

    int logoHeight = juce::jmin (kMaxLogoHeight, logoArea.getHeight());
    logoHeight = juce::jmax (0, logoHeight);
    int logoWidth = juce::jmin (logoArea.getWidth(), (int) std::round (logoHeight * logoAspect));
    // Width is the tighter constraint in a narrow column -- rederive height
    // from it so the logo still fills the column's width edge-to-edge.
    logoHeight = (int) std::round (logoWidth / logoAspect);

    // Logo is centred vertically in the *whole* bar height, independent of
    // the version label below it -- previously the logo+version pair was
    // centred as one block, which left the (much taller) logo graphic
    // itself sitting above true centre, pulled up by the shorter version
    // label under it.
    auto logoRect = logoArea.withSizeKeepingCentre (logoWidth, logoHeight);
    logoButton->setBounds (logoRect);

    // Version label anchored just below the centred logo, clamped so it
    // never runs past the bottom of the area -- stays visible regardless
    // of how small the logo or how short the bar gets.
    const int versionY = juce::jmin (logoRect.getBottom() + kLogoVersionGap,
                                     logoArea.getBottom() - kVersionLabelHeight);
    versionLabel->setBounds (logoArea.getX(), versionY, logoArea.getWidth(), kVersionLabelHeight);
    
    // Track info area 
    auto trackArea = getTrackInfoArea();
    auto coverArea = getCoverArtArea();
    
    // Adjust track area to account for cover art -- including the gap
    // before it now that it's shifted right of trackArea's own start
    // (see getCoverArtArea()), so song info/KEY/BPM don't creep under it.
    const int gapBeforeCoverArt = coverArea.getX() - trackArea.getX();
    trackArea.removeFromLeft(gapBeforeCoverArt + coverArea.getWidth() + 10);
    
    if (!isOnlineStatus)
    {
        offlineWarningLabel->setBounds(trackArea);
    }
    else
    {
        // Layout track info and KEY/BPM side by side
        auto songInfoWidth = trackArea.getWidth() * 0.65f; // Reserve 65% for song info
        auto keyBpmWidth = trackArea.getWidth() * 0.35f; // Reserve 35% for KEY/BPM
        
        // Song info area (left side)
        auto songInfoArea = trackArea.removeFromLeft(songInfoWidth);
        auto infoHeight = songInfoArea.getHeight() / 2;
        trackNameLabel->setBounds(songInfoArea.removeFromTop(infoHeight));
        artistNameLabel->setBounds(songInfoArea);
        
        // KEY and BPM area (right side) - side by side. Title font size
        // scales with the bar height (drag-resized by the user) so growing
        // the bar actually fills the extra space with bigger text instead
        // of just leaving a bigger gap; the title+value pair is then laid
        // out as one tight block (title, small gap, value) centred in the
        // column -- previously the value label's bounds took "whatever's
        // left" of the column, which on a tall bar left it floating far
        // below the title since Justification::centred also centres
        // vertically within however much height a label's bounds span.
        auto keyBpmArea = trackArea.withWidth(keyBpmWidth);
        auto keyAreaWidth = keyBpmArea.getWidth() / 2;

        const float sizeScale = juce::jlimit(1.0f, 1.8f, (float) currentBarHeight / (float) DEFAULT_BAR_HEIGHT);
        const float titleFontPx = 15.0f * sizeScale;
        const float valueFontPx = 14.0f * sizeScale;
        keyTitleLabel->setFont(juce::Font(titleFontPx, juce::Font::bold));
        bpmTitleLabel->setFont(juce::Font(titleFontPx, juce::Font::bold));
        keyValueLabel->setFont(juce::Font(valueFontPx));
        bpmValueLabel->setFont(juce::Font(valueFontPx));

        const int titleLineHeight = (int) std::ceil(titleFontPx * 1.2f);
        const int valueLineHeight = (int) std::ceil(valueFontPx * 1.2f);
        constexpr int kKeyBpmGap = 2;
        const int blockHeight = titleLineHeight + kKeyBpmGap + valueLineHeight;

        // Key area: title then value, tightly grouped
        auto keyArea = keyBpmArea.removeFromLeft(keyAreaWidth);
        auto keyBlock = keyArea.withSizeKeepingCentre(keyArea.getWidth(), juce::jmin(blockHeight, keyArea.getHeight()));
        keyTitleLabel->setBounds(keyBlock.removeFromTop(titleLineHeight));
        keyBlock.removeFromTop(kKeyBpmGap);
        keyValueLabel->setBounds(keyBlock.removeFromTop(valueLineHeight));

        // BPM area: same treatment
        auto bpmArea = keyBpmArea;
        auto bpmBlock = bpmArea.withSizeKeepingCentre(bpmArea.getWidth(), juce::jmin(blockHeight, bpmArea.getHeight()));
        bpmTitleLabel->setBounds(bpmBlock.removeFromTop(titleLineHeight));
        bpmBlock.removeFromTop(kKeyBpmGap);
        bpmValueLabel->setBounds(bpmBlock.removeFromTop(valueLineHeight));
    }
    
    // User area -- userButton covers the whole thing (avatar + name) so
    // either one can be clicked to open the popup menu.
    auto userArea = getUserArea();
    userButton->setBounds(userArea);
    auto nameArea = userArea;
    nameArea.removeFromLeft(AVATAR_SIZE);
    userNameLabel->setBounds(nameArea.reduced(5, 0));

    if (updateButton_->isVisible())
        updateButton_->setBounds(getUpdateButtonArea());
}

//==============================================================================
void TopBar::timerCallback()
{
    // Smooth VU meter animation -- fast-ish rise/fall on the bars
    // themselves, plus a separate, much slower-decaying peak-hold mark
    // (classic VU ballistics: the bar tracks the signal, a thin tick
    // lingers at the loudest recent point and creeps back down).
    currentLevelL_ += (targetLevelL_ - currentLevelL_) * 0.3f;
    currentLevelR_ += (targetLevelR_ - currentLevelR_) * 0.3f;

    if (targetLevelL_ > peakL_) peakL_ = targetLevelL_;
    else peakL_ = juce::jmax(0.0f, peakL_ - 0.01f);

    if (targetLevelR_ > peakR_) peakR_ = targetLevelR_;
    else peakR_ = juce::jmax(0.0f, peakR_ - 0.01f);

    repaint(getVUMeterArea());
}

//==============================================================================
void TopBar::setTrackInfo(const juce::String& trackName, 
                         const juce::String& artistName,
                         const juce::String& version)
{
    currentTrackName = trackName;
    currentArtistName = artistName;
    currentVersion = version;
    
    if (trackNameLabel)
    {
        juce::String displayName = trackName;
        if (!artistName.isEmpty())
            displayName += " - " + artistName;
        if (!version.isEmpty())
            displayName += " (" + version + ")";
            
        trackNameLabel->setText(displayName, juce::dontSendNotification);
    }
    
    updateOfflineDisplay();
    resized(); // Update layout
}

//==============================================================================
void TopBar::setMusicInfo(const juce::String& key, int bpm)
{
    currentKey = key;
    currentBpm = bpm;
    
    // Blank (not a "-" placeholder) when there's no metadata for the
    // current song -- the KEY/BPM labels stay visible either way.
    if (keyValueLabel)
        keyValueLabel->setText(key, juce::dontSendNotification);

    if (bpmValueLabel)
        bpmValueLabel->setText(bpm > 0 ? juce::String(bpm) : juce::String(), juce::dontSendNotification);
        
    updateOfflineDisplay();
}

//==============================================================================
void TopBar::setCoverArt(const juce::Image& coverImage)
{
    coverArtImage = coverImage;
    repaint(getCoverArtArea());
}

//==============================================================================
void TopBar::setAudioLevels(float left, float right)
{
    targetLevelL_ = juce::jlimit(0.0f, 1.0f, left);
    targetLevelR_ = juce::jlimit(0.0f, 1.0f, right);
}

void TopBar::setSpectrumLevels(const std::array<float, SpectrumAnalyzer::kNumBands>& levels)
{
    spectrumLevels_ = levels;
    if (vuMeterStyle_ == VuMeterStyle::SpectrumBand)
        repaint(getVUMeterArea());
}

//==============================================================================
void TopBar::setOnlineStatus(bool isOnline)
{
    isOnlineStatus = isOnline;
    updateOfflineDisplay();
    repaint();
}

//==============================================================================
void TopBar::setUserInfo(const juce::String& name, const juce::Image& avatar)
{
    userName = name;
    
    if (userNameLabel)
        userNameLabel->setText(name.isEmpty() ? LocalizationManager::getInstance().getText("topbar.anonymous") : name, juce::dontSendNotification);
    
    if (avatar.isValid())
        userAvatarImage = avatar;

    repaint(getUserArea());
}

void TopBar::setUpdateAvailable(bool available, const juce::String& version)
{
    pendingUpdateVersion_ = version;
    updateButton_->setVisible(available);
    updateButton_->setTooltip(available
        ? LocalizationManager::getInstance().getText("update.banner_available") + " " + version
        : juce::String());
    resized();
    repaint();
}

void TopBar::setUpdateButtonBusy(bool busy, const juce::String& busyText)
{
    updateButton_->setEnabled(!busy);
    updateButton_->setButtonText(busy ? busyText
                                       : LocalizationManager::getInstance().getText("update.pill_label"));
    resized();
    repaint();
}

//==============================================================================
void TopBar::updateOfflineDisplay()
{
    bool showOfflineWarning = !isOnlineStatus;
    
    if (offlineWarningLabel)
        offlineWarningLabel->setVisible(showOfflineWarning);
    
    if (trackNameLabel)
        trackNameLabel->setVisible(isOnlineStatus && !currentTrackName.isEmpty());
    
    if (artistNameLabel)
        artistNameLabel->setVisible(isOnlineStatus && !currentArtistName.isEmpty());
        
    if (keyTitleLabel)
        keyTitleLabel->setVisible(isOnlineStatus);
        
    if (keyValueLabel)
        keyValueLabel->setVisible(isOnlineStatus);
        
    if (bpmTitleLabel)
        bpmTitleLabel->setVisible(isOnlineStatus);
        
    if (bpmValueLabel)
        bpmValueLabel->setVisible(isOnlineStatus);
}

//==============================================================================
void TopBar::drawVUMeter(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    switch (vuMeterStyle_)
    {
        case VuMeterStyle::GradientBar:   drawGradientBarMeter(g, bounds);   break;
        case VuMeterStyle::SegmentedLed:  drawSegmentedLedMeter(g, bounds);  break;
        case VuMeterStyle::AnalogNeedle:  drawAnalogNeedleMeter(g, bounds);  break;
        case VuMeterStyle::SpectrumBand:  drawSpectrumBandMeter(g, bounds);  break;
    }
}

juce::Colour TopBar::vuLevelColour(float level01)
{
    if (level01 < 0.7f)  return juce::Colour(0xff3ddc72);  // green
    if (level01 < 0.9f)  return juce::Colour(0xffe8d24a);  // yellow
    return juce::Colour(0xffe0483f);                        // red
}

//==============================================================================
juce::String TopBar::formatLevelDb(float level01)
{
    const float db = juce::Decibels::gainToDecibels(level01, -60.0f);
    if (db <= -59.9f)
        return juce::String(juce::CharPointer_UTF8("-\xe2\x88\x9e"));  // "-∞"
    return juce::String(db, 1);
}

void TopBar::drawMeterTitle(juce::Graphics& g, juce::Rectangle<int>& bounds, const juce::String& text) const
{
    auto titleArea = bounds.removeFromTop(13);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText(text, titleArea, juce::Justification::centred);
}

juce::String TopBar::formatBandFrequency(double freqHz)
{
    if (freqHz < 1000.0)
    {
        // Round UP to the nearest 5 (117 -> 120, 179 -> 180, 274 -> 275) --
        // clean, familiar-looking numbers instead of the raw log-spaced
        // band centers.
        const double rounded = std::ceil(freqHz / 5.0) * 5.0;
        // Close enough to 1k that showing e.g. "995" right next to "1.5k"
        // would look like an odd near-miss -- just call it "1k".
        if (rounded >= 995.0)
            return "1k";
        return juce::String((int) rounded);
    }

    const double khz = freqHz / 1000.0;
    // "1k" for whole numbers, "1.2k" for anything with a fractional part.
    return (std::abs(khz - std::round(khz)) < 0.05)
        ? juce::String((int) std::round(khz)) + "k"
        : juce::String(khz, 1) + "k";
}

//==============================================================================
// Style 1: horizontal gradient bar, stereo L/R rows with a peak-hold tick
// (closest to your "Headphone Mic Level" reference).
void TopBar::drawGradientBarMeter(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    drawMeterTitle(g, bounds, LocalizationManager::getInstance().getText("topbar.vu.mainoutput"));

    auto area = bounds.reduced(2);
    const int rowGap = 3;
    const int rowHeight = (area.getHeight() - rowGap) / 2;

    auto drawRow = [&g] (juce::Rectangle<int> row, float level, float peak, const juce::String& label)
    {
        auto labelArea = row.removeFromLeft(12);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::Font(9.5f, juce::Font::bold));
        g.drawText(label, labelArea, juce::Justification::centred);

        auto valueArea = row.removeFromLeft(34);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::Font(9.5f));
        g.drawText(formatLevelDb(level), valueArea, juce::Justification::centredRight);
        row.removeFromLeft(4);

        g.setColour(juce::Colour(0xff0d1117));
        g.fillRoundedRectangle(row.toFloat(), 3.0f);

        const int fillWidth = (int) std::round (row.getWidth() * level);
        if (fillWidth > 0)
        {
            juce::ColourGradient grad (juce::Colour(0xff3ddc72), (float) row.getX(), 0.0f,
                                        juce::Colour(0xffe0483f), (float) row.getRight(), 0.0f, false);
            grad.addColour (0.75, juce::Colour(0xffe8d24a));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (row.withWidth(fillWidth).toFloat(), 3.0f);
        }

        const int peakX = row.getX() + (int) std::round (row.getWidth() * peak);
        if (peak > 0.02f)
        {
            g.setColour(juce::Colours::white);
            g.fillRect (juce::Rectangle<int>(juce::jlimit(row.getX(), row.getRight() - 2, peakX), row.getY(), 2, row.getHeight()));
        }

        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawRoundedRectangle(row.toFloat(), 3.0f, 1.0f);
    };

    drawRow (area.removeFromTop(rowHeight), currentLevelL_, peakL_, "L");
    area.removeFromTop(rowGap);
    drawRow (area, currentLevelR_, peakR_, "R");
}

//==============================================================================
// Style 2: discrete LED-block segments, stereo L/R rows (your "VU Meter"
// block-style reference).
void TopBar::drawSegmentedLedMeter(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    drawMeterTitle(g, bounds, LocalizationManager::getInstance().getText("topbar.vu.mainoutput"));

    auto area = bounds.reduced(2);
    const int rowGap = 3;
    const int rowHeight = (area.getHeight() - rowGap) / 2;
    constexpr int kNumSegments = 24;
    constexpr float kSegGapFrac = 0.18f;

    auto drawRow = [] (juce::Graphics& g2, juce::Rectangle<int> row, float level)
    {
        // Leave room on the left for the "L"/"R" tag + dB value drawn
        // once, below, outside the segment grid.
        row.removeFromLeft(48);

        const float segWidth = (float) row.getWidth() / (float) kNumSegments;
        const int litCount = (int) std::round (level * kNumSegments);

        for (int i = 0; i < kNumSegments; ++i)
        {
            const float frac = (float) i / (float) kNumSegments;
            juce::Colour onColour = frac < 0.7f ? juce::Colour(0xff3ddc72)
                                   : frac < 0.9f ? juce::Colour(0xffe8d24a)
                                                  : juce::Colour(0xffe0483f);
            const bool lit = i < litCount;
            g2.setColour (lit ? onColour : onColour.withAlpha(0.14f));

            juce::Rectangle<float> seg ((float) row.getX() + i * segWidth + segWidth * kSegGapFrac * 0.5f,
                                        (float) row.getY(),
                                        segWidth * (1.0f - kSegGapFrac),
                                        (float) row.getHeight());
            g2.fillRoundedRectangle (seg, 1.5f);
        }
    };

    auto rowL = area.removeFromTop(rowHeight);
    area.removeFromTop(rowGap);
    auto rowR = area;

    drawRow (g, rowL, currentLevelL_);
    drawRow (g, rowR, currentLevelR_);

    // "L"/"R" tags + dB value drawn once per row, outside the segment
    // grid -- matching the block-meter reference's layout.
    auto drawRowLabel = [&g] (juce::Rectangle<int> row, const juce::String& letter, float level)
    {
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(letter, row.getX(), row.getY(), 12, row.getHeight(), juce::Justification::centred);

        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::Font(9.5f));
        g.drawText(formatLevelDb(level), row.getX() + 16, row.getY(), 32, row.getHeight(), juce::Justification::centredRight);
    };

    drawRowLabel(rowL, "L", currentLevelL_);
    drawRowLabel(rowR, "R", currentLevelR_);
}

//==============================================================================
// Style 3: classic analog needle gauges (your "SonoTonex" reference) --
// the real photographed dial faces (assets/images/VUMeterBack.png, both
// L/R gauges side by side in one image) drawn as the background, with two
// needles overlaid, calibrated to that image's own printed arc/pivot
// geometry rather than a separately drawn dial.
void TopBar::drawAnalogNeedleMeter(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    if (! vuMeterBackImage.isValid())
    {
        // Asset missing -- fall back to the gradient-bar style rather than
        // drawing nothing.
        drawGradientBarMeter (g, bounds);
        return;
    }

    g.drawImage (vuMeterBackImage, bounds.toFloat(), juce::RectanglePlacement::stretchToFit);

    // The source image is two identical gauge cards side by side with a
    // thin gap between them -- split the drawn area the same way so each
    // needle's pivot/radius stay correctly aligned to its own printed dial
    // regardless of how big the meter is drawn.
    const int gaugeWidth = bounds.getWidth() / 2;
    auto leftGauge  = bounds.withWidth(gaugeWidth);
    auto rightGauge = bounds.withX(bounds.getX() + gaugeWidth).withWidth(bounds.getWidth() - gaugeWidth);

    drawSingleNeedleGauge (g, leftGauge,  currentLevelL_);
    drawSingleNeedleGauge (g, rightGauge, currentLevelR_);
}

void TopBar::drawSingleNeedleGauge(juce::Graphics& g, juce::Rectangle<int> bounds, float level) const
{
    auto area = bounds.toFloat();

    // Calibrated by eye against VUMeterBack.png's printed dial: the pivot
    // sits centred horizontally between the two chrome screws, low in the
    // card near the bottom notch; the needle reaches up to just past the
    // printed tick arc at full deflection.
    const auto pivot = juce::Point<float> (area.getCentreX(), area.getY() + area.getHeight() * 0.90f);
    const float radius = area.getHeight() * 0.74f;

    // 0deg = straight up. The printed scale (-20 .. +3 VU) sweeps further
    // to the left of vertical than to the right, matching the image.
    constexpr float sweepLeftDeg  = 52.0f;
    constexpr float sweepRightDeg = 46.0f;
    const float angleFromVerticalDeg = juce::jmap (level, 0.0f, 1.0f, -sweepLeftDeg, sweepRightDeg);
    const float angleRad = juce::degreesToRadians (angleFromVerticalDeg);

    juce::Path needle;
    needle.startNewSubPath (pivot);
    needle.lineTo (pivot.x + radius * std::sin (angleRad), pivot.y - radius * std::cos (angleRad));
    g.setColour (juce::Colours::black);
    g.strokePath (needle, juce::PathStrokeType (1.5f));

    g.setColour (juce::Colours::black.withAlpha(0.7f));
    g.fillEllipse (pivot.x - 2.0f, pivot.y - 2.0f, 4.0f, 4.0f);
}

//==============================================================================
// Style 4: LED spectrum band analyzer, real per-band FFT data (your
// "Spectrum Band" reference) -- single mono row across the full width.
void TopBar::drawSpectrumBandMeter(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    drawMeterTitle(g, bounds, LocalizationManager::getInstance().getText("topbar.vu.spectrumband"));

    constexpr int kNumBands = SpectrumAnalyzer::kNumBands;
    constexpr float kGapFrac = 0.22f;

    auto freqLabelArea = bounds.removeFromBottom(11);
    auto area = bounds.reduced(2, 0);
    area.removeFromBottom(1);  // small breathing room above the frequency labels

    const float barWidth = (float) area.getWidth() / (float) kNumBands;
    const auto centerFreqs = SpectrumAnalyzer::getBandCenterFrequencies();

    g.setFont(juce::Font(7.0f));

    for (int i = 0; i < kNumBands; ++i)
    {
        const float level = spectrumLevels_[(size_t) i];
        const float x = (float) area.getX() + i * barWidth + barWidth * kGapFrac * 0.5f;
        const float w = barWidth * (1.0f - kGapFrac);

        juce::Rectangle<float> track (x, (float) area.getY(), w, (float) area.getHeight());
        g.setColour (juce::Colour(0xff0d1117));
        g.fillRoundedRectangle (track, 1.5f);

        const float litHeight = area.getHeight() * level;
        if (litHeight > 1.0f)
        {
            juce::Rectangle<float> lit (x, (float) area.getBottom() - litHeight, w, litHeight);
            juce::ColourGradient grad (juce::Colour(0xff3ddc72), 0.0f, (float) area.getBottom(),
                                        juce::Colour(0xffe0483f), 0.0f, (float) area.getY(), false);
            grad.addColour (0.75, juce::Colour(0xffe8d24a));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (lit, 1.5f);
        }

        g.setColour (juce::Colours::white.withAlpha(0.5f));
        // A couple of extra px of slop on each side (labels can overlap
        // the gap between bars, which is otherwise unused) plus no forced
        // ellipsis -- at 14 bands in a compact meter, a plain small clip
        // reads better than "12…" on the widest labels.
        juce::Rectangle<int> labelSlot ((int) std::round ((float) area.getX() + i * barWidth) - 2, freqLabelArea.getY(),
                                        (int) std::round (barWidth) + 4, freqLabelArea.getHeight());
        g.drawText (formatBandFrequency (centerFreqs[(size_t) i]), labelSlot, juce::Justification::centred, false);
    }
}

//==============================================================================
void TopBar::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    
    if (keyTitleLabel)
        keyTitleLabel->setText(lm.getText("topbar.key"), juce::dontSendNotification);
    if (bpmTitleLabel)
        bpmTitleLabel->setText(lm.getText("topbar.bpm"), juce::dontSendNotification);
    if (offlineWarningLabel)
        offlineWarningLabel->setText(lm.getText("topbar.offline_warning"), juce::dontSendNotification);
    if (updateButton_)
        updateButton_->setButtonText(lm.getText("update.pill_label"));

    repaint();
}

//==============================================================================
//==============================================================================
int TopBar::getCurrentResizeHandleHeight() const
{
    // Return active height when hovering or dragging, otherwise inactive height
    return (isOverResizeHandle || isResizing) ? RESIZE_HANDLE_ACTIVE_HEIGHT : RESIZE_HANDLE_INACTIVE_HEIGHT;
}

juce::Rectangle<int> TopBar::getLogoArea() const
{
    // Logo is centred across the whole span from the bar's left edge to
    // the album artwork's (shifted-right) position -- not a fixed-width
    // column -- so it sits in the middle of the actual free space to the
    // artwork's left rather than a narrow arbitrary slice.
    auto bounds = getLocalBounds().withHeight(currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT);
    return bounds.removeFromLeft(getCoverArtArea().getX()).reduced(8, 4);
}

juce::Rectangle<int> TopBar::getCoverArtArea() const
{
    auto trackArea = getTrackInfoArea();
    // Cover art sized to match bar height and positioned within visible area
    int coverSize = currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT;
    // Shifted right by half the artwork's own size, per request -- opens
    // up room for the logo to be centred to its left instead of squeezed
    // into a fixed narrow column (see getLogoArea()).
    const int shiftRight = coverSize / 2;
    return juce::Rectangle<int>(trackArea.getX() + shiftRight, 0, coverSize, coverSize);
}

juce::Rectangle<int> TopBar::getTrackInfoArea() const
{
    // Track info area uses fixed resize handle height for consistent layout.
    // This 10%-width anchor is the *nominal*, unshifted column start --
    // getCoverArtArea() shifts the artwork right of it, and getLogoArea()
    // spans left of it, so this value doesn't need to change for either.
    auto bounds = getLocalBounds().withHeight(currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT);
    bounds.removeFromLeft(getWidth() * 0.10f);
    return bounds.removeFromLeft(getWidth() * 0.40f);
}

juce::Rectangle<int> TopBar::getVUMeterArea() const
{
    // VU meter area uses fixed resize handle height for consistent layout.
    // Shifted right by the same amount as the album artwork, so the KEY/
    // BPM/VU group all moved together rather than the VU meter alone
    // staying at its old position.
    auto bounds = getLocalBounds().withHeight(currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT);
    const int artworkShift = getCoverArtArea().getX() - (int) (getWidth() * 0.10f);
    bounds.removeFromLeft(getWidth() * 0.56f + (float) artworkShift); // Skip logo and track areas, plus the artwork's shift
    return bounds.removeFromLeft(getWidth() * 0.18f).withSizeKeepingCentre(VU_METER_WIDTH, VU_METER_HEIGHT);
}

juce::Rectangle<int> TopBar::getUserArea() const
{
    // User area positioned from the right with 10px margin
    auto bounds = getLocalBounds().withHeight(currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT);
    return bounds.removeFromRight(AVATAR_SIZE + 150).reduced(10, 0); // Right-justified with 10px margin
}

juce::Rectangle<int> TopBar::getUpdateButtonArea() const
{
    constexpr int kWidth = 84;
    constexpr int kGap = 10;
    auto userArea = getUserArea();
    auto area = userArea.withX(userArea.getX() - kGap - kWidth).withWidth(kWidth);
    return area.withSizeKeepingCentre(kWidth, 26); // slim pill, vertically centred in the row
}

//==============================================================================
// Mouse handling for resizing
void TopBar::mouseDown(const juce::MouseEvent& event)
{
    auto resizeArea = getLocalBounds().removeFromBottom(getCurrentResizeHandleHeight());
    
    if (resizeArea.contains(event.getPosition()))
    {
        isResizing = true;
        resizeStartHeight = currentBarHeight;
        resizeStartPosition = event.getPosition();
        repaint(); // Repaint to show active state
    }
}

void TopBar::mouseDrag(const juce::MouseEvent& event)
{
    if (isResizing)
    {
        int deltaY = event.getPosition().getY() - resizeStartPosition.getY();
        int newHeight = juce::jlimit(MIN_BAR_HEIGHT, MAX_BAR_HEIGHT, resizeStartHeight + deltaY);
        setBarHeight(newHeight);
    }
}

void TopBar::mouseMove(const juce::MouseEvent& event)
{
    auto resizeArea = getLocalBounds().removeFromBottom(getCurrentResizeHandleHeight());
    bool wasOverHandle = isOverResizeHandle;
    isOverResizeHandle = resizeArea.contains(event.getPosition());
    
    if (wasOverHandle != isOverResizeHandle)
    {
        repaint(); // Repaint entire component to update handle size and color
    }
    
    // Change cursor when over resize handle, or a pointing hand over the
    // VU meter as a hint that clicking it does something (cycles style).
    if (isOverResizeHandle)
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    else if (getVUMeterArea().contains(event.getPosition()))
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void TopBar::mouseUp(const juce::MouseEvent& event)
{
    const bool wasResizing = isResizing;
    if (isResizing)
    {
        isResizing = false;
        repaint(); // Repaint to show inactive state
    }

    // A plain click (not the end of a resize drag) on the meter cycles its
    // style -- no separate Settings entry, per how you wanted this reached.
    if (! wasResizing && getVUMeterArea().contains(event.getPosition()))
        cycleVuMeterStyle();
}

void TopBar::cycleVuMeterStyle()
{
    const int next = (static_cast<int>(vuMeterStyle_) + 1) % kNumVuMeterStyles;
    vuMeterStyle_ = static_cast<VuMeterStyle>(next);
    UserPreferences::getInstance().setVuMeterStyle(next);
    repaint(getVUMeterArea());
}

void TopBar::mouseExit(const juce::MouseEvent& event)
{
    // Reset hover state when mouse leaves the component
    if (isOverResizeHandle && !isResizing)
    {
        isOverResizeHandle = false;
        repaint();
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void TopBar::mouseDoubleClick(const juce::MouseEvent& event)
{
    auto resizeArea = getLocalBounds().removeFromBottom(getCurrentResizeHandleHeight());
    
    if (resizeArea.contains(event.getPosition()))
    {
        // Reset to default height on double-click
        setBarHeight(DEFAULT_BAR_HEIGHT);
    }
}

void TopBar::setBarHeight(int newHeight)
{
    newHeight = juce::jlimit(MIN_BAR_HEIGHT, MAX_BAR_HEIGHT, newHeight);
    
    if (currentBarHeight != newHeight)
    {
        currentBarHeight = newHeight;
        resized();
        repaint();
        
        // Notify parent component about height change
        if (onHeightChanged)
            onHeightChanged(currentBarHeight);
    }
}