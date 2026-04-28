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
#include "../Audio/AudioEngine.h"
#include "../Services/ImageCache.h"
#include "../Models/CdgSong.h"
#include "TopBar.h"
#include "BottomBar.h"
#include "NavBar.h"
#include "MainArea.h"
#include "QueueBar.h"
#include "SongSelectionDialog.h"
#include "LyricDisplayWindow.h"

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

    /** Accessor for the secondary (singer-facing) lyric window. May be null
        if the window couldn't be constructed. */
    LyricDisplayWindow* getLyricWindow() noexcept { return lyricWindow_.get(); }

    /** Install (or remove) the application's MenuBarModel. On Windows/Linux
        this embeds a MenuBarComponent at the top of the window. On macOS the
        system menu bar is used instead so this is a no-op. */
    void installMenuBarModel (juce::MenuBarModel* model);

    /** Load the active venue from Firestore and propagate its name/code to
        the queue bar and its logo + code to the lyric display window. Safe
        to call from the message thread; network work happens in background.
        If `requestInitialScan` is true, the user is switching to a venue
        that wasn't configured on this PC — once the venue loads we switch
        to the Library page and start the initial song-load flow. */
    void setVenueId (const juce::String& venueId, bool requestInitialScan = false);
    
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
    // Audio playback
    std::unique_ptr<AudioEngine> audioEngine;
    CdgSong      currentSong;
    juce::String currentSongImageUrl;
    double       currentSongDuration = 0.0;

    //==============================================================================
    // Secondary-monitor lyric / CDG display
    std::unique_ptr<LyricDisplayWindow> lyricWindow_;

    //==============================================================================
    // Embedded menu bar (Windows/Linux only — macOS uses the system bar).
    std::unique_ptr<juce::MenuBarComponent> menuBar_;

    /** Resolve the .cdg file that pairs with a given audio file (typically
        a sibling with the same base name). Returns juce::File{} if none is
        found. */
    juce::File resolveCdgFileFor (const juce::File& audioFile) const;

    /** Begin playback of the chosen song (PlayNow action). */
    void loadAndPlaySong(const CdgSong& song, int versionIndex, int pitchSemitones);

    /** Show / hide a full-window "Loading song..." overlay. */
    void showLoadingOverlay(const juce::String& message);
    void hideLoadingOverlay();

    class LoadingOverlay;
    std::unique_ptr<LoadingOverlay> loadingOverlay_;
    
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

    juce::String activeVenueId_;
    
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