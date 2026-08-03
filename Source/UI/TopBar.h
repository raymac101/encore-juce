/*
  ==============================================================================

    TopBar.h
    Created: 16 Apr 2026 11:00:00am
    Author:  GitHub Copilot

    Top bar component for Encore Karaoke application
    Displays logo, version, song info, VU meter, and user controls

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Localization/LocalizationManager.h"

//==============================================================================
/**
    TopBar component that displays application status and current song information
*/
class TopBar : public juce::Component, public juce::Timer
{
public:
    //==============================================================================
    // Public constants
    static constexpr int MIN_BAR_HEIGHT = 60;   // Minimum height
    static constexpr int MAX_BAR_HEIGHT = 200;  // Maximum height
    static constexpr int DEFAULT_BAR_HEIGHT = 80;   // Default height changed to 80px
    
    // Current height (adjustable)
    int currentBarHeight = DEFAULT_BAR_HEIGHT;
    
    // Callback for height changes
    std::function<void(int newHeight)> onHeightChanged;
    
    //==============================================================================
    TopBar();
    ~TopBar() override;

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Mouse handling for resizing
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    
    // Height management
    void setBarHeight(int newHeight);
    int getBarHeight() const { return currentBarHeight; }
    
    //==============================================================================
    // Song information updates
    void setTrackInfo(const juce::String& trackName, 
                     const juce::String& artistName,
                     const juce::String& version = "");
    void setMusicInfo(const juce::String& key, int bpm);
    void setCoverArt(const juce::Image& coverImage);
    
    //==============================================================================
    // Audio level updates
    void setAudioLevel(float level); // 0.0 to 1.0
    
    //==============================================================================
    // Connection status
    void setOnlineStatus(bool isOnline);
    
    //==============================================================================
    // User information
    void setUserInfo(const juce::String& userName, const juce::Image& avatar = {});
    
    //==============================================================================
    // Callbacks
    std::function<void()> onUserButtonClicked;
    std::function<void()> onLogoClicked;
    
    //==============================================================================
    // Localization
    /** Re-read all translatable strings from LocalizationManager. */
    void updateAllText();

private:
    //==============================================================================
    // Timer callback for VU meter animation
    void timerCallback() override;
    
    //==============================================================================
    // UI Elements
    std::unique_ptr<juce::Label> versionLabel;
    std::unique_ptr<juce::Label> trackNameLabel;
    std::unique_ptr<juce::Label> artistNameLabel;
    std::unique_ptr<juce::Label> keyTitleLabel;
    std::unique_ptr<juce::Label> keyValueLabel;
    std::unique_ptr<juce::Label> bpmTitleLabel;
    std::unique_ptr<juce::Label> bpmValueLabel;
    std::unique_ptr<juce::Label> offlineWarningLabel;
    std::unique_ptr<juce::Label> userNameLabel;
    std::unique_ptr<juce::TextButton> userButton;
    std::unique_ptr<juce::ImageButton> logoButton;
    
    //==============================================================================
    // Images
    juce::Image logoImage;
    juce::Image coverArtImage;
    juce::Image userAvatarImage;
    juce::Image vuMeterOffImage;
    juce::Image vuMeterOnImage;
    juce::Image defaultAvatarImage;
    juce::Image defaultAlbumArtImage;
    
    //==============================================================================
    // State variables
    juce::String currentTrackName;
    juce::String currentArtistName; 
    juce::String currentVersion;
    juce::String currentKey;
    int currentBpm = 0;
    float currentAudioLevel = 0.0f;
    float targetAudioLevel = 0.0f;
    bool isOnlineStatus = true;
    juce::String userName;
    
    //==============================================================================
    // Layout constants
    // COVER_ART_SIZE is now dynamic based on currentBarHeight
    static constexpr int LOGO_SIZE = 64;  // Increased logo size (was 48)
    static constexpr int AVATAR_SIZE = 40;
    static constexpr int VU_METER_WIDTH = 150;  // Increased width (was 100)
    static constexpr int VU_METER_HEIGHT = 30;  // Increased height (was 20)
    
    //==============================================================================
    // Resize handle - Visual Studio style with highlight
    static constexpr int RESIZE_HANDLE_INACTIVE_HEIGHT = 1;  // Subtle when inactive
    static constexpr int RESIZE_HANDLE_ACTIVE_HEIGHT = 6;    // Prominent when active
    bool isResizing = false;
    bool isOverResizeHandle = false;
    int resizeStartHeight = 0;
    juce::Point<int> resizeStartPosition;
    
    //==============================================================================
    // Colors from original app
    static const juce::Colour BACKGROUND_COLOR;
    static const juce::Colour OFFLINE_COLOR;
    static const juce::Colour TEXT_COLOR;
    static const juce::Colour ACCENT_COLOR;
    
    //==============================================================================
    // Helper methods
    void setupUI();
    void loadAssets();
    void updateOfflineDisplay();
    void drawVUMeter(juce::Graphics& g, juce::Rectangle<int> bounds);
    juce::Rectangle<int> getLogoArea() const;
    juce::Rectangle<int> getCoverArtArea() const;
    juce::Rectangle<int> getTrackInfoArea() const;
    juce::Rectangle<int> getVUMeterArea() const;
    juce::Rectangle<int> getUserArea() const;
    int getCurrentResizeHandleHeight() const;  // Dynamic height based on state
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBar)
};