/*
  ==============================================================================

    TopBar.cpp
    Created: 16 Apr 2026 11:00:00am
    Author:  GitHub Copilot

    Top bar component implementation

  ==============================================================================
*/

#include "TopBar.h"

//==============================================================================
// Color constants from original app
const juce::Colour TopBar::BACKGROUND_COLOR = juce::Colour(0x0f, 0x0f, 0x0f);  // #0F0F0F
const juce::Colour TopBar::OFFLINE_COLOR = juce::Colour(0x8b, 0x00, 0x00);     // darkred
const juce::Colour TopBar::TEXT_COLOR = juce::Colours::white;
const juce::Colour TopBar::ACCENT_COLOR = juce::Colour(0x76, 0xc7, 0xc0);      // #76c7c0

// Resize handle colors - Visual Studio style
static const juce::Colour RESIZE_HANDLE_INACTIVE_COLOR = juce::Colour(0x3c, 0x3c, 0x3c); // Dark grey
static const juce::Colour RESIZE_HANDLE_ACTIVE_COLOR = juce::Colour(0x00, 0x7a, 0xcc);   // VS Blue

//==============================================================================
TopBar::TopBar()
{
    setupUI();
    loadAssets();
    
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
    // Version label
    versionLabel = std::make_unique<juce::Label>("version", "ver. 2.75.1");
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
    keyTitleLabel = std::make_unique<juce::Label>("keyTitle", "KEY");
    keyTitleLabel->setFont(juce::Font(12.0f, juce::Font::bold));
    keyTitleLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    keyTitleLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    keyTitleLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(keyTitleLabel.get());
    
    // Key value label (normal)
    keyValueLabel = std::make_unique<juce::Label>("keyValue", "C Major");
    keyValueLabel->setFont(juce::Font(12.0f));
    keyValueLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    keyValueLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    keyValueLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(keyValueLabel.get());
    
    // BPM title label (bold)
    bpmTitleLabel = std::make_unique<juce::Label>("bpmTitle", "BPM");
    bpmTitleLabel->setFont(juce::Font(12.0f, juce::Font::bold));
    bpmTitleLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    bpmTitleLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    bpmTitleLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmTitleLabel.get());
    
    // BPM value label (normal)  
    bpmValueLabel = std::make_unique<juce::Label>("bpmValue", "120");  
    bpmValueLabel->setFont(juce::Font(12.0f));
    bpmValueLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    bpmValueLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    bpmValueLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmValueLabel.get());
    
    // Offline warning label
    offlineWarningLabel = std::make_unique<juce::Label>("offline", "YOUR COMPUTER IS OFFLINE - PLEASE CONNECT TO THE INTERNET");
    offlineWarningLabel->setFont(juce::Font(14.0f, juce::Font::bold));
    offlineWarningLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    offlineWarningLabel->setJustificationType(juce::Justification::centred);
    offlineWarningLabel->setVisible(false);
    addAndMakeVisible(offlineWarningLabel.get());
    
    // User name label
    userNameLabel = std::make_unique<juce::Label>("userName", "Test User");
    userNameLabel->setFont(juce::Font(12.0f));
    userNameLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    userNameLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    userNameLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(userNameLabel.get());
    
    // User button
    userButton = std::make_unique<juce::TextButton>("User");
    userButton->setButtonText("");
    userButton->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    userButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    userButton->setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    userButton->setColour(juce::TextButton::textColourOnId, juce::Colours::transparentBlack);
    userButton->onClick = [this]() {
        if (onUserButtonClicked)
            onUserButtonClicked();
    };
    addAndMakeVisible(userButton.get());
    
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
    // Try to load logo image
    juce::File logoFile = juce::File::getCurrentWorkingDirectory()
                         .getChildFile("assets/images/encore-logo-white.png");
    
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
    juce::File avatarFile = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("assets/icon/2345434.png");
    
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
    juce::File defaultAvatarFile = juce::File::getCurrentWorkingDirectory()
                                  .getChildFile("assets/images/AvatarWhite.png");
    
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
    juce::File defaultAlbumFile = juce::File::getCurrentWorkingDirectory()
                                 .getChildFile("assets/images/AlbumIconWhite.png");
    
    if (defaultAlbumFile.existsAsFile())
    {
        defaultAlbumArtImage = juce::ImageFileFormat::loadFrom(defaultAlbumFile);
        if (defaultAlbumArtImage.isValid())
        {
            DBG("Default album art loaded from AlbumIconWhite.png");
        }
    }
    
    // Load VU meter images
    juce::File vuMeterOffFile = juce::File::getCurrentWorkingDirectory()
                               .getChildFile("assets/images/Meter-Side-blue-off.png");
    
    if (vuMeterOffFile.existsAsFile())
    {
        vuMeterOffImage = juce::ImageFileFormat::loadFrom(vuMeterOffFile);
        DBG("VU meter OFF image loaded: " + juce::String(vuMeterOffImage.isValid() ? "valid" : "invalid"));
    }
    
    juce::File vuMeterOnFile = juce::File::getCurrentWorkingDirectory()
                              .getChildFile("assets/images/Meter-Side-blue-on.png");
    
    if (vuMeterOnFile.existsAsFile())
    {
        vuMeterOnImage = juce::ImageFileFormat::loadFrom(vuMeterOnFile);
        DBG("VU meter ON image loaded: " + juce::String(vuMeterOnImage.isValid() ? "valid" : "invalid"));
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
    
    // Logo area
    auto logoArea = getLogoArea();
    logoButton->setBounds(logoArea.removeFromTop(LOGO_SIZE).withSizeKeepingCentre(LOGO_SIZE, LOGO_SIZE));
    versionLabel->setBounds(logoArea);
    
    // Track info area 
    auto trackArea = getTrackInfoArea();
    auto coverArea = getCoverArtArea();
    
    // Adjust track area to account for cover art
    trackArea.removeFromLeft(coverArea.getWidth() + 10);
    
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
        
        // KEY and BPM area (right side) - side by side
        auto keyBpmArea = trackArea.withWidth(keyBpmWidth);
        auto keyAreaWidth = keyBpmArea.getWidth() / 2;
        const int keyBpmTopOffset = 8;
        const int keyBpmTitleHeight = 16;
        const int keyBpmValueTopGap = 2;
        
        // Key area split between title and value with a slight downward offset
        auto keyArea = keyBpmArea.removeFromLeft(keyAreaWidth);
        auto keyContentArea = keyArea.withTrimmedTop(keyBpmTopOffset);
        keyTitleLabel->setBounds(keyContentArea.removeFromTop(keyBpmTitleHeight));
        keyValueLabel->setBounds(keyContentArea.withTrimmedTop(keyBpmValueTopGap));
        
        // BPM area split between title and value with a slight downward offset
        auto bpmArea = keyBpmArea;
        auto bpmContentArea = bpmArea.withTrimmedTop(keyBpmTopOffset);
        bpmTitleLabel->setBounds(bpmContentArea.removeFromTop(keyBpmTitleHeight));
        bpmValueLabel->setBounds(bpmContentArea.withTrimmedTop(keyBpmValueTopGap));
    }
    
    // User area
    auto userArea = getUserArea();
    userButton->setBounds(userArea.removeFromLeft(AVATAR_SIZE));
    userNameLabel->setBounds(userArea.reduced(5, 0));
}

//==============================================================================
void TopBar::timerCallback()
{
    // Smooth VU meter animation
    if (juce::approximatelyEqual(currentAudioLevel, targetAudioLevel))
        return;
        
    float diff = targetAudioLevel - currentAudioLevel;
    currentAudioLevel += diff * 0.3f; // Smooth interpolation
    
    // Trigger repaint for VU meter area only
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
    
    if (keyValueLabel)
        keyValueLabel->setText(key.isEmpty() ? "-" : key, juce::dontSendNotification);
        
    if (bpmValueLabel)
        bpmValueLabel->setText(bpm > 0 ? juce::String(bpm) : "-", juce::dontSendNotification);
        
    updateOfflineDisplay();
}

//==============================================================================
void TopBar::setCoverArt(const juce::Image& coverImage)
{
    coverArtImage = coverImage;
    repaint(getCoverArtArea());
}

//==============================================================================
void TopBar::setAudioLevel(float level)
{
    targetAudioLevel = juce::jlimit(0.0f, 1.0f, level);
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
        userNameLabel->setText(name.isEmpty() ? "Anonymous" : name, juce::dontSendNotification);
    
    if (avatar.isValid())
        userAvatarImage = avatar;
        
    repaint(getUserArea());
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
    // Use image-based VU meter if images are loaded
    if (vuMeterOffImage.isValid() && vuMeterOnImage.isValid())
    {
        // Draw the OFF image as background
        g.drawImage(vuMeterOffImage, bounds.toFloat(), juce::RectanglePlacement::stretchToFit);
        
        // Draw the ON image cropped from right based on volume percentage
        if (currentAudioLevel > 0.0f)
        {
            float cropWidth = bounds.getWidth() * currentAudioLevel;
            auto onBounds = bounds.withWidth(static_cast<int>(cropWidth));
            
            // Create a cropped version of the ON image
            auto croppedOnImage = vuMeterOnImage.getClippedImage(juce::Rectangle<int>(0, 0, 
                static_cast<int>(vuMeterOnImage.getWidth() * currentAudioLevel), 
                vuMeterOnImage.getHeight()));
            
            g.drawImage(croppedOnImage, onBounds.toFloat(), juce::RectanglePlacement::stretchToFit);
        }
    }
    else
    {
        // Fallback to programmatic drawing if images not loaded
        g.setColour(juce::Colours::darkgrey);
        g.fillRect(bounds);
        
        // Draw VU meter level
        float levelWidth = bounds.getWidth() * currentAudioLevel;
        auto levelBounds = bounds.withWidth(static_cast<int>(levelWidth));
        
        // Color based on level (green -> yellow -> red)
        juce::Colour meterColor;
        if (currentAudioLevel < 0.7f)
            meterColor = juce::Colours::green;
        else if (currentAudioLevel < 0.9f)
            meterColor = juce::Colours::yellow;
        else
            meterColor = juce::Colours::red;
            
        g.setColour(meterColor);
        g.fillRect(levelBounds);
        
        // Draw border
        g.setColour(TEXT_COLOR);
        g.drawRect(bounds, 1);
        
        // Draw label
        g.setFont(juce::Font(10.0f));
        g.drawText("VU", bounds.withHeight(12), juce::Justification::centred);
    }
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
    // Logo area uses fixed resize handle height for consistent layout
    return getLocalBounds().withHeight(currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT).removeFromLeft(getWidth() * 0.10f).reduced(10);
}

juce::Rectangle<int> TopBar::getCoverArtArea() const
{
    auto trackArea = getTrackInfoArea();
    // Cover art sized to match bar height and positioned within visible area
    int coverSize = currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT;
    return juce::Rectangle<int>(trackArea.getX(), 0, coverSize, coverSize);
}

juce::Rectangle<int> TopBar::getTrackInfoArea() const
{
    // Track info area uses fixed resize handle height for consistent layout
    auto bounds = getLocalBounds().withHeight(currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT);
    bounds.removeFromLeft(getWidth() * 0.10f); // Skip logo area
    return bounds.removeFromLeft(getWidth() * 0.40f);
}

juce::Rectangle<int> TopBar::getVUMeterArea() const
{
    // VU meter area uses fixed resize handle height for consistent layout
    auto bounds = getLocalBounds().withHeight(currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT);
    bounds.removeFromLeft(getWidth() * 0.58f); // Skip logo and track areas, moved 80px more to the right
    return bounds.removeFromLeft(getWidth() * 0.13f).withSizeKeepingCentre(VU_METER_WIDTH, VU_METER_HEIGHT);
}

juce::Rectangle<int> TopBar::getUserArea() const
{
    // User area positioned from the right with 10px margin
    auto bounds = getLocalBounds().withHeight(currentBarHeight - RESIZE_HANDLE_INACTIVE_HEIGHT);
    return bounds.removeFromRight(AVATAR_SIZE + 150).reduced(10, 0); // Right-justified with 10px margin
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
    
    // Change cursor when over resize handle
    setMouseCursor(isOverResizeHandle ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void TopBar::mouseUp(const juce::MouseEvent& event)
{
    if (isResizing)
    {
        isResizing = false;
        repaint(); // Repaint to show inactive state
    }
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