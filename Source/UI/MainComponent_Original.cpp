/*
  ==============================================================================

    MainComponent.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Main application component implementation

  ==============================================================================
*/

#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Set initial size (will be adjusted by responsive system)
    setSize(1200, 800);
    
    // Setup UI components
    setupUI();
    
    // Start timer for periodic updates (every 2 seconds)
    startTimer(2000);
    
    // Initial screen configuration
    detectAndConfigureScreens();
    
    DBG("MainComponent initialized");
}

MainComponent::~MainComponent()
{
    stopTimer();
}

//==============================================================================
void MainComponent::setupUI()
{
    auto& lm = LocalizationManager::getInstance();
    
    // Title label
    titleLabel = std::make_unique<juce::Label>("title", lm.getText("app.title"));
    titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(28.0f)).boldened());
    titleLabel->setJustificationType(juce::Justification::centred);
    titleLabel->setColour(juce::Label::textColourId, juce::Colours::darkblue);
    addAndMakeVisible(titleLabel.get());
    
    // Language selection button
    languageButton = std::make_unique<juce::TextButton>(lm.getText("settings.language"));
    languageButton->onClick = [this]() { showLanguageSelector(); };
    addAndMakeVisible(languageButton.get());
    
    // Status display
    statusLabel = std::make_unique<juce::Label>("status", lm.getText("status.initializing"));
    statusLabel->setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
    statusLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel.get());
    
    // Debug information (hidden in release builds)
    debugLabel = std::make_unique<juce::Label>("debug", "");
    debugLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    debugLabel->setJustificationType(juce::Justification::bottomLeft);
    debugLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
    
    #if JUCE_DEBUG
    addAndMakeVisible(debugLabel.get());
    #endif
    
    updateAllText();
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    // Background gradient
    juce::ColourGradient gradient(
        juce::Colour(0xff1a1a2e), 0, 0,
        juce::Colour(0xff16213e), getWidth(), getHeight(), false);
    g.setGradientFill(gradient);
    g.fillAll();
    
    // Draw responsive layout debug info in debug builds
    #if JUCE_DEBUG
    g.setColour(juce::Colours::yellow.withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 2);
    
    auto screenSizeText = getScreenSizeText(getCurrentScreenSize());
    g.setColour(juce::Colours::yellow);
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    g.drawText(screenSizeText, getLocalBounds().reduced(5), 
               juce::Justification::topRight, true);
    #endif
}

//==============================================================================
void MainComponent::resized()
{
    ResponsiveLayout::resized(); // Call parent implementation first
    
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(20); // Use 20 pixels as base margin
    bounds.reduce(margin, margin);
    
    // Layout components based on screen category
    auto screenCategory = ResponsiveLayout::getScreenCategory(getCurrentScreenSize());
    
    switch (screenCategory)
    {
        case ScreenCategory::Mobile:
            configureForMobile();
            break;
            
        case ScreenCategory::Standard:
            configureForStandard();
            break;
            
        case ScreenCategory::Wide:
            configureForWide();
            break;
            
        case ScreenCategory::UltraWide:
            configureForUltraWide();
            break;
            
        case ScreenCategory::HighResolution:
            configureForHighResolution();
            break;
    }
}

//==============================================================================
void MainComponent::configureForMobile()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(12); // Smaller margin for mobile
    bounds.reduce(margin, margin);
    
    // Vertical stack layout for mobile
    auto titleHeight = 60;
    auto buttonHeight = 44;
    auto spacing = 16;
    
    titleLabel->setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(spacing);
    
    languageButton->setBounds(bounds.removeFromTop(buttonHeight));
    bounds.removeFromTop(spacing);
    
    statusLabel->setBounds(bounds.removeFromTop(30));
    
    #if JUCE_DEBUG
    debugLabel->setBounds(bounds.removeFromBottom(40));
    #endif
}

void MainComponent::configureForStandard()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(16); // Standard margin
    bounds.reduce(margin, margin);
    
    // Standard laptop/desktop layout (HD, WXGA, etc.)
    auto titleArea = bounds.removeFromTop(80);
    titleLabel->setBounds(titleArea);
    
    auto topControls = bounds.removeFromTop(50);
    languageButton->setBounds(topControls.removeFromRight(150));
    
    bounds.removeFromTop(20);
    statusLabel->setBounds(bounds.removeFromTop(30));
    
    #if JUCE_DEBUG
    debugLabel->setBounds(bounds.removeFromBottom(40));
    #endif
}

void MainComponent::configureForWide()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(20); // Larger margin for wide screens
    bounds.reduce(margin, margin);
    
    // Wide screen layout (Full HD, QHD, etc.)
    auto header = bounds.removeFromTop(100);
    titleLabel->setBounds(header.removeFromTop(60));
    
    auto controls = header.removeFromTop(40);
    languageButton->setBounds(controls.removeFromRight(150));
    
    // More space available, adjust accordingly
    bounds.removeFromTop(20);
    statusLabel->setBounds(bounds.removeFromTop(25));
    
    #if JUCE_DEBUG
    debugLabel->setBounds(bounds.removeFromBottom(30));
    #endif
}

void MainComponent::configureForUltraWide()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(24); // Even larger margin for ultra-wide
    bounds.reduce(margin, margin);
    
    // Ultra-wide layout (21:9, 32:9 aspect ratios)
    // Use horizontal space more efficiently
    auto header = bounds.removeFromTop(80);
    
    // Split header horizontally for ultra-wide
    auto titleArea = header.removeFromLeft(header.getWidth() * 0.6f);
    titleLabel->setBounds(titleArea);
    
    auto controlArea = header.reduced(20, 0);
    languageButton->setBounds(controlArea.removeFromRight(150));
    
    bounds.removeFromTop(20);
    
    // Status area - use left portion of ultra-wide screen
    auto statusArea = bounds.removeFromTop(30);
    statusLabel->setBounds(statusArea.removeFromLeft(statusArea.getWidth() * 0.7f));
    
    #if JUCE_DEBUG
    auto debugArea = bounds.removeFromBottom(30);
    debugLabel->setBounds(debugArea.removeFromLeft(debugArea.getWidth() * 0.8f));
    #endif
}

void MainComponent::configureForHighResolution()
{
    // 4K/5K/8K displays - use desktop layout with automatic font scaling applied
    configureForWide();
    
    // Additional adjustments for very high resolution displays
    if (getCurrentScreenSize() == ScreenSize::UHD_8K)
    {
        // For 8K displays, we might want even more spacing
        auto bounds = getLocalBounds();
        auto extraMargin = bounds.getWidth() * 0.05f; // 5% extra margin on 8K
        
        // Apply extra margin to all components (this is illustrative)
        if (titleLabel)
        {
            auto titleBounds = titleLabel->getBounds();
            titleBounds.reduce(static_cast<int>(extraMargin * 0.5f), 0);
            titleLabel->setBounds(titleBounds);
        }
    }
}

//==============================================================================
void MainComponent::updateUIForScreenSize()
{
    ResponsiveLayout::updateUIForScreenSize();
    
    // Apply font scaling based on screen size
    if (titleLabel != nullptr)
    {
        auto baseFont = 28.0f * getScaleFactor();
        titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(baseFont)).boldened());
    }
    
    if (languageButton != nullptr)
    {
        // TextButton font is handled by LookAndFeel, not setFont
    }
    
    if (statusLabel != nullptr)
    {
        auto baseFont = 14.0f * getScaleFactor();
        statusLabel->setFont(juce::Font(juce::FontOptions().withHeight(baseFont)));
    }
    
    #if JUCE_DEBUG
    if (debugLabel != nullptr)
    {
        auto baseFont = 12.0f * getScaleFactor();
        debugLabel->setFont(juce::Font(juce::FontOptions().withHeight(baseFont)));
    }
    #endif
}

//==============================================================================
void MainComponent::changeLanguage(const juce::String& languageCode)
{
    auto& lm = LocalizationManager::getInstance();
    
    lm.setLanguage(languageCode);
    updateAllText();
    repaint();
    
    DBG("Language changed to: " + languageCode);
        
    // Save preference (in a real app, this would go to settings)
    // For now, just log it
    auto message = lm.getText("status.language_changed");
    message = message.replace("{language}", lm.getCurrentLanguageDisplayName());
    statusLabel->setText(message, juce::dontSendNotification);
}

void MainComponent::showLanguageSelector()
{
    auto& lm = LocalizationManager::getInstance();
    
    languageMenu = std::make_unique<juce::PopupMenu>();
    
    // Add available languages
    auto availableLanguages = lm.getAvailableLanguages();
    auto currentLang = lm.getCurrentLanguage();
    
    for (int i = 0; i < availableLanguages.size(); ++i)
    {
        auto langCode = availableLanguages[i];
        auto displayName = lm.getLanguageDisplayName(langCode);
        
        languageMenu->addItem(i + 1, displayName, true, langCode == currentLang);
    }
    
    languageMenu->showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetComponent(languageButton.get())
            .withStandardItemHeight(30),
        [this, availableLanguages](int result)
        {
            if (result > 0 && result <= availableLanguages.size())
            {
                changeLanguage(availableLanguages[result - 1]);
            }
        }
    );
}

//==============================================================================
void MainComponent::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    
    if (titleLabel != nullptr)
        titleLabel->setText(lm.getText("app.title"), juce::dontSendNotification);
        
    if (languageButton != nullptr)
        languageButton->setButtonText(lm.getText("settings.language"));
        
    if (statusLabel != nullptr && statusLabel->getText().isEmpty())
        statusLabel->setText(lm.getText("status.ready"), juce::dontSendNotification);
}

//==============================================================================
void MainComponent::detectAndConfigureScreens()
{
    auto& desktop = juce::Desktop::getInstance();
    auto primaryDisplay = desktop.getDisplays().getPrimaryDisplay();
    
    // Simple implementation - just detect if there might be multiple displays
    // by checking if the primary display is not the main screen boundary
    auto mainScreenBounds = desktop.getDisplays().getTotalBounds(true);
    bool hasMultipleDisplays = mainScreenBounds.getWidth() > primaryDisplay->totalArea.getWidth() || 
                               mainScreenBounds.getHeight() > primaryDisplay->totalArea.getHeight();
    
    DBG("Primary display: " + 
        juce::String(primaryDisplay->totalArea.getWidth()) + "x" + 
        juce::String(primaryDisplay->totalArea.getHeight()));
        
    if (hasMultipleDisplays)
    {
        setupDualScreenLayout();
        DBG("Multiple displays detected");
    }
    else
    {
        DBG("Single display detected");
    }
}

void MainComponent::setupDualScreenLayout()
{
    // In a full karaoke system, this would setup:
    // - Primary display: DJ/operator interface
    // - Secondary display: Public song display with lyrics
    
    DBG("Setting up dual-screen karaoke layout");
    
    // For now, just log that we detected multiple screens
    auto& lm = LocalizationManager::getInstance();
    statusLabel->setText(lm.getText("status.dual_screen_detected"), juce::dontSendNotification);
}

//==============================================================================
void MainComponent::timerCallback()
{
    updateConnectionStatus();
    updateDebugInfo();
}

void MainComponent::updateConnectionStatus()
{
    auto& lm = LocalizationManager::getInstance();
    
    // Simulate connection status (in real app, check Firebase connection)
    static int statusCounter = 0;
    statusCounter++;
    
    juce::String statusText;
    if (statusCounter % 3 == 0)
    {
        statusText = lm.getText("status.connected");
        isConnectedToFirebase = true;
    }
    else if (statusCounter % 3 == 1)
    {
        statusText = lm.getText("status.reconnecting");
        isConnectedToFirebase = false;
    }
    else
    {
        statusText = lm.getText("status.ready");
        isConnectedToFirebase = true;
    }
    
    statusLabel->setText(statusText, juce::dontSendNotification);
}

void MainComponent::updateDebugInfo()
{
    #if JUCE_DEBUG
    auto& lm = LocalizationManager::getInstance();
    
    juce::String debugText;
    auto screenSize = getCurrentScreenSize();
    debugText += getScreenSizeText(screenSize);
    debugText += " (" + getScreenResolutionText(screenSize) + ")";
    debugText += " | " + juce::String(getAspectRatio(screenSize), 2) + ":1";
    debugText += " | Scale: " + juce::String(getScaleFactor(), 2);
    debugText += " | Lang: " + lm.getCurrentLanguage();
    debugText += " | Size: " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    
    if (isUltraWideScreen(screenSize))
        debugText += " | Ultra-Wide";
    
    debugLabel->setText(debugText, juce::dontSendNotification);
    #endif
}

//==============================================================================
void MainComponent::setHighContrastMode(bool enabled)
{
    highContrastMode = enabled;
    
    // Apply high contrast colors
    if (enabled)
    {
        titleLabel->setColour(juce::Label::textColourId, juce::Colours::white);
        statusLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    }
    else
    {
        titleLabel->setColour(juce::Label::textColourId, juce::Colours::darkblue);
        statusLabel->setColour(juce::Label::textColourId, juce::Colours::black);
    }
    
    repaint();
}

void MainComponent::setLargeTextMode(bool enabled)
{
    largeTextMode = enabled;
    
    // Increase font sizes for accessibility
    float textScale = enabled ? 1.25f : 1.0f;
    
    titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(28.0f * textScale * getScaleFactor())).boldened());
    // TextButton font is handled by LookAndFeel, not setFont
    statusLabel->setFont(juce::Font(juce::FontOptions().withHeight(14.0f * textScale * getScaleFactor())));
    
    #if JUCE_DEBUG
    debugLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f * textScale * getScaleFactor())));
    #endif
    
    resized(); // Relayout with new font sizes
}