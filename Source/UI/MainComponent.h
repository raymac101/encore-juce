/*
  ==============================================================================

    MainComponent.h
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Main application component with responsive design and localization

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ResponsiveLayout.h"
#include "../Localization/LocalizationManager.h"
#include "TopBar.h"
#include "BottomBar.h"
#include "NavBar.h"
#include "MainArea.h"
#include "QueueBar.h"

//==============================================================================
/**
    Main application component that serves as the root UI container.
    Provides responsive layout and coordinates between all major subsystems.
*/
class MainComponent : public ResponsiveLayout,
                      public juce::Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    // Component Overrides
    void paint(juce::Graphics&) override;
    void resized() override;
    
    //==============================================================================
    // Multi-language Support
    void changeLanguage(const juce::String& languageCode);
    void showLanguageSelector();
    void updateAllText();
    
    //==============================================================================
    // Screen Management
    void detectAndConfigureScreens();
    void setupDualScreenLayout();  // DJ screen + public display
    
    //==============================================================================
    // Accessibility
    void setHighContrastMode(bool enabled);
    void setLargeTextMode(bool enabled);

protected:
    //==============================================================================
    // ResponsiveLayout Overrides
    void updateUIForScreenSize() override;
    
    // Timer callback for periodic updates
    void timerCallback() override;

private:
    //==============================================================================
    // UI Components
    std::unique_ptr<TopBar> topBar;
    std::unique_ptr<BottomBar> bottomBar;
    std::unique_ptr<NavBar> navBar;
    std::unique_ptr<MainArea> mainArea;
    std::unique_ptr<QueueBar> queueBar;
    std::unique_ptr<juce::Label> titleLabel;
    std::unique_ptr<juce::TextButton> languageButton;
    std::unique_ptr<juce::Label> statusLabel;
    std::unique_ptr<juce::Label> debugLabel;
    
    // Language selector popup
    std::unique_ptr<juce::PopupMenu> languageMenu;
    
    //==============================================================================
    // Background Tile
    juce::Image backgroundTileImage_;
    int backgroundTileSize_ = 340;
    void loadBackgroundTile(const juce::String& path = "");

    //==============================================================================
    // Application State
    bool highContrastMode = false;
    bool largeTextMode = false;
    bool isConnectedToFirebase = false;
    
    //==============================================================================
    // UI Setup
    void setupUI();
    void setupLanguageButton();
    
    // Responsive configurations for different screen categories
    void configureForMobile();
    void configureForStandard();        // Standard laptop/desktop (HD, WXGA, etc.)
    void configureForWide();            // Wide screens (Full HD, QHD, etc.)
    void configureForUltraWide();       // Ultra-wide monitors (21:9, 32:9)
    void configureForHighResolution();  // 4K/5K/8K displays
    
    // Status updates
    void updateConnectionStatus();
    void updateDebugInfo();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};