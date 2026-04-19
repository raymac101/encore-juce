/*
  ==============================================================================

    MainComponent.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Main application component implementation

  ==============================================================================
*/

#include "MainComponent.h"
#include <cmath>

//==============================================================================
MainComponent::MainComponent()
{
    // Set initial size (will be adjusted by responsive system)
    setSize(1200, 800);
    
    try
    {
        // Setup UI components carefully
        setupUI();
        
        // Force initial layout after components are created
        resized();
        
        // Ensure components are visible and painted
        repaint();
        
        // Start timer for periodic updates - disabled until safer implementation
        // startTimer(2000);
        
        // Initial screen configuration - simplified
        // detectAndConfigureScreens(); // Commenting out temporarily
        
        DBG("MainComponent initialized successfully");
    }
    catch (const std::exception& e)
    {
        DBG("Error in MainComponent constructor: " + juce::String(e.what()));
        // Fallback: create a simple label
        titleLabel = std::make_unique<juce::Label>("title", "Encore Karaoke"); 
        titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(28.0f)).boldened());
        titleLabel->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel.get());
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
}

void MainComponent::setupUI()
{
    // Setup UI with LocalizationManager integration
    DBG("Starting setupUI with LocalizationManager");
    
    // Create TopBar first
    topBar = std::make_unique<TopBar>();
    addAndMakeVisible(topBar.get());

    // Create BottomBar (music transport and waveform controls)
    bottomBar = std::make_unique<BottomBar>();
    addAndMakeVisible(bottomBar.get());
    
    // Set up TopBar callbacks
    topBar->onLogoClicked = [this]() {
        DBG("Encore logo clicked");
        // TODO: Implement logo click action (about dialog, etc.)
    };
    
    topBar->onUserButtonClicked = [this]() {
        DBG("User button clicked");
        // TODO: Implement user menu/account access
    };
    
    topBar->onHeightChanged = [this](int newHeight) {
        DBG("TopBar height changed to: " + juce::String(newHeight));
        resized(); // Re-layout when TopBar height changes
    };

    bottomBar->onHeightChanged = [this](int newHeight) {
        DBG("BottomBar height changed to: " + juce::String(newHeight));
        resized(); // Re-layout when BottomBar height changes
    };
    
    // Set sample data for TopBar
    topBar->setTrackInfo("My Way", "Frank Sinatra", "Karaoke Version");
    topBar->setMusicInfo("Bb", 120);
    topBar->setOnlineStatus(true); // Start with online status
    topBar->setUserInfo("Demo User", juce::Image());

    // BottomBar callbacks (placeholder transport logic for now)
    bottomBar->onReturnToZero = [this]() {
        DBG("BottomBar: Return to 0");
    };

    bottomBar->onStopAndReturnToZero = [this]() {
        DBG("BottomBar: Stop and return to zero");
    };

    bottomBar->onPlayPause = [this](bool isNowPlaying) {
        DBG("BottomBar: " + juce::String(isNowPlaying ? "Play" : "Pause"));
    };

    bottomBar->onJumpToEnd = [this]() {
        DBG("BottomBar: Jump to end");
    };

    bottomBar->onSeek = [this](float newProgress) {
        DBG("BottomBar: Seek to " + juce::String(newProgress * 100.0f, 1) + "%");
    };

    bottomBar->onPitchChanged = [this](int semitones) {
        DBG("BottomBar: Pitch set to " + juce::String(semitones) + " semitones");
    };

    bottomBar->onVolumeChanged = [this](int volumeStep) {
        DBG("BottomBar: Volume set to " + juce::String(volumeStep));
    };

    // Demo duration until real audio metadata is wired in.
    bottomBar->setDurationSeconds(210.0);
    
    // Start timer to simulate audio levels for VU meter
    startTimer(100); // Update every 100ms for smooth audio levels
    
    DBG("TopBar created and configured");
    
    // Title label - using LocalizationManager
    titleLabel = std::make_unique<juce::Label>("title", LocalizationManager::getInstance().getText("app.name"));
    titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(28.0f)).boldened());
    titleLabel->setJustificationType(juce::Justification::centred);
    titleLabel->setColour(juce::Label::textColourId, juce::Colours::darkblue);
    addAndMakeVisible(titleLabel.get());
    DBG("Title label created with localized text");

    // Language selection button - using LocalizationManager
    languageButton = std::make_unique<juce::TextButton>(LocalizationManager::getInstance().getText("language.english"));
    languageButton->onClick = [this]() { 
        DBG("Language button clicked");
        showLanguageSelector(); 
    };
    addAndMakeVisible(languageButton.get());
    DBG("Language button created with localized text");
    
    // Status display - using LocalizationManager
    statusLabel = std::make_unique<juce::Label>("status", LocalizationManager::getInstance().getText("status.ready"));
    statusLabel->setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
    statusLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel.get());
    DBG("Status label created with localized text");
    
    // Debug information (always visible for now)
    debugLabel = std::make_unique<juce::Label>("debug", "Debug: Application Running");
    debugLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    debugLabel->setJustificationType(juce::Justification::bottomLeft);
    debugLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(debugLabel.get());
    DBG("Debug label created");
    
    // Force initial bounds for all components to ensure visibility
    auto tempBounds = juce::Rectangle<int>(20, 70, 1160, 710);  // Reserve top 50px for TopBar
    if (titleLabel) titleLabel->setBounds(tempBounds.removeFromTop(60));
    if (languageButton) languageButton->setBounds(tempBounds.removeFromTop(40).withWidth(200));
    if (statusLabel) statusLabel->setBounds(tempBounds.removeFromTop(30));
    if (debugLabel) debugLabel->setBounds(tempBounds.removeFromTop(30));
    
    DBG("setupUI completed successfully with initial bounds set");
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
    
    // Simplified debug info to avoid getCurrentScreenSize() issues
    juce::String debugInfo = "Debug Mode - " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    g.setColour(juce::Colours::yellow);
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    g.drawText(debugInfo, getLocalBounds().reduced(5), 
               juce::Justification::topRight, true);
    #endif
}

//==============================================================================
void MainComponent::resized()
{
    // Basic responsive resized method
    auto bounds = getLocalBounds();

    // Reserve bottom area for BottomBar
    if (bottomBar)
    {
        auto bottomBarBounds = bounds.removeFromBottom(bottomBar->getBarHeight());
        bottomBar->setBounds(bottomBarBounds);
    }
    
    // Reserve top area for TopBar
    if (topBar)
    {
        auto topBarBounds = bounds.removeFromTop(topBar->getBarHeight());
        topBar->setBounds(topBarBounds);
    }
    
    int margin = 20;
    bounds.reduce(margin, margin);
    
    // Simple fixed layout with basic responsiveness
    if (titleLabel)
    {
        auto titleBounds = bounds.removeFromTop(60);
        titleLabel->setBounds(titleBounds);
    }
    
    if (languageButton)
    {
        auto buttonBounds = bounds.removeFromTop(40);
        languageButton->setBounds(buttonBounds.withWidth(200).withX(bounds.getX()));
    }
    
    if (statusLabel)
    {
        auto statusBounds = bounds.removeFromTop(30);
        statusLabel->setBounds(statusBounds);
    }
    
    if (debugLabel)
    {
        auto debugBounds = bounds.removeFromTop(30);
        debugLabel->setBounds(debugBounds);
    }
    
    // Basic responsive features (commented out until safer)
    // updateUIForScreenSize();
}

// Simple implementations to satisfy linker requirements
void MainComponent::showLanguageSelector()
{
    // Enhanced language selector using LocalizationManager
    DBG("Language selector clicked - using LocalizationManager");
    
    auto& lm = LocalizationManager::getInstance();
    
    // Create popup menu with available languages
    juce::PopupMenu languageMenu;
    juce::StringArray languages = lm.getAvailableLanguages();
    
    // Add fallback languages if none loaded from files
    if (languages.isEmpty())
    {
        languageMenu.addItem(1, "English (English)", true, lm.getCurrentLanguage() == "en_US");
        languageMenu.addItem(2, "Español (Spanish)", true, lm.getCurrentLanguage() == "es_ES");  
        languageMenu.addItem(3, "Français (French)", true, lm.getCurrentLanguage() == "fr_FR");
        languageMenu.addItem(4, "Deutsch (German)", true, lm.getCurrentLanguage() == "de_DE");
    }
    else
    {
        // Use languages from LocalizationManager
        for (int i = 0; i < languages.size(); ++i)
        {
            juce::String langInfo = languages[i];
            languageMenu.addItem(i + 1, langInfo, true, false);
        }
    }
    
    languageMenu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetComponent(languageButton.get())
            .withStandardItemHeight(30),
        [this, &lm](int result)
        {
            if (result > 0)
            {
                juce::String selectedLang;
                
                // Map menu result to language codes
                switch (result)
                {
                    case 1: selectedLang = "en_US"; break;
                    case 2: selectedLang = "es_ES"; break; 
                    case 3: selectedLang = "fr_FR"; break;
                    case 4: selectedLang = "de_DE"; break;
                    default: selectedLang = "en_US"; break;
                }
                
                // Change language using LocalizationManager
                lm.setLanguage(selectedLang);
                
                // Update UI text
                updateAllText();
                
                DBG("Language changed to: " + selectedLang);
            }
        }
    );
}

void MainComponent::updateUIForScreenSize()
{
    // Basic UI updates for different screen sizes
    DBG("updateUIForScreenSize called");
    
    // Simple font scaling based on window size  
    float baseScale = juce::jmin(getWidth() / 1200.0f, getHeight() / 800.0f);
    baseScale = juce::jmax(0.7f, juce::jmin(baseScale, 2.0f)); // Clamp between 0.7 and 2.0
    
    if (titleLabel != nullptr)
    {
        auto scaledFont = juce::Font(juce::FontOptions().withHeight(28.0f * baseScale)).boldened();
        titleLabel->setFont(scaledFont);
    }
    
    if (statusLabel != nullptr)
    {
        auto scaledFont = juce::Font(juce::FontOptions().withHeight(14.0f * baseScale));
        statusLabel->setFont(scaledFont);
    }
    
    #if JUCE_DEBUG
    if (debugLabel != nullptr)
    {
        auto scaledFont = juce::Font(juce::FontOptions().withHeight(12.0f * baseScale));
        debugLabel->setFont(scaledFont);
    }
    #endif
    
    resized(); // Update layout
}

void MainComponent::timerCallback()
{
    updateConnectionStatus();
    updateDebugInfo();
    
    // Simulate VU meter audio levels with a sine wave
    if (topBar != nullptr)
    {
        static float time = 0.0f;
        time += 0.1f;
        
        // Create a dynamic audio level simulation (0.0 to 1.0)
        float audioLevel = (std::sin(time * 2.0f) + 1.0f) * 0.5f; // Sine wave from 0 to 1
        audioLevel = audioLevel * audioLevel; // Square it for more realistic audio behavior
        
        topBar->setAudioLevel(audioLevel);
    }
}

void MainComponent::updateConnectionStatus()
{
    // Simple status updates without LocalizationManager
    static int statusCounter = 0;
    statusCounter++;
    
    juce::String statusText = "Status: Active (Update #" + juce::String(statusCounter) + ")";
    
    if (statusLabel != nullptr)
        statusLabel->setText(statusText, juce::dontSendNotification);
}

void MainComponent::updateDebugInfo()
{
    // Simple debug info updates
    juce::String debugText = "Size: " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    debugText += " | Timer Active | Memory: " + juce::String(juce::SystemStats::getMemorySizeInMegabytes()) + "MB";
    
    if (debugLabel != nullptr)
        debugLabel->setText(debugText, juce::dontSendNotification);
}

// Configuration methods temporarily disabled for debugging
/*
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
// Configuration methods temporarily disabled for debugging
/*void MainComponent::configureForWide()
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

/*
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
*/

//==============================================================================
void MainComponent::updateAllText()
{
    // Update all UI text using LocalizationManager 
    DBG("updateAllText called - refreshing localized text");
    
    auto& lm = LocalizationManager::getInstance();
    
    try
    {
        // Update title
        if (titleLabel != nullptr)
        {
            titleLabel->setText(lm.getText("app.name"), juce::dontSendNotification);
        }
        
        // Update language button with current language display name  
        if (languageButton != nullptr)
        {
            languageButton->setButtonText(lm.getCurrentLanguageDisplayName());
        }
        
        // Update status
        if (statusLabel != nullptr)
        {
            statusLabel->setText(lm.getText("status.ready"), juce::dontSendNotification);
        }
        
        // Update debug info
        if (debugLabel != nullptr)
        {
            juce::String debugText = "Debug: " + lm.getCurrentLanguage() + " Active";
            debugLabel->setText(debugText, juce::dontSendNotification);
        }
        
        // Trigger repaint to update any drawn text
        repaint();
        
        DBG("All UI text updated successfully");
    }
    catch (const std::exception& e)
    {
        DBG("Error updating UI text: " + juce::String(e.what()));
    }
    catch (...)
    {
        DBG("Unknown error updating UI text");
    }
}

// Methods temporarily disabled for debugging
/*
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
    // Simplified language selector - no LocalizationManager for now
    DBG("Language selector clicked - functionality temporarily disabled");
    
    // Show a simple popup menu with basic options
    juce::PopupMenu languageMenu;
    languageMenu.addItem(1, "English", true, true);
    languageMenu.addItem(2, "Español", true, false);
    languageMenu.addItem(3, "Français", true, false);
    
    languageMenu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetComponent(languageButton.get())
            .withStandardItemHeight(30),
        [this](int result)
        {
            DBG("Selected language option: " + juce::String(result));
            // Language change functionality disabled for now
        }
    );
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
    // Simplified - no LocalizationManager for now
    static int statusCounter = 0;
    statusCounter++;
    
    juce::String statusText = "Status Update #" + juce::String(statusCounter);
    
    if (statusLabel != nullptr)
        statusLabel->setText(statusText, juce::dontSendNotification);
}

void MainComponent::updateDebugInfo()
{
    // Simplified debug info - no LocalizationManager calls
    juce::String debugText = "Size: " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    debugText += " | Debug Mode Active";
    
    if (debugLabel != nullptr)
        debugLabel->setText(debugText, juce::dontSendNotification);
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
*/