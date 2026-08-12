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
#include <array>
#include "../Localization/LocalizationManager.h"
#include "../Audio/SpectrumAnalyzer.h"

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
    static constexpr int DEFAULT_BAR_HEIGHT = 100;   // Raised from 80 so the logo+version stack (see
                                                       // getLogoArea()/resized()) has room without clipping
    
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
    // Audio level updates -- real stereo, replacing the old single-value
    // setAudioLevel(). 0.0 to 1.0 each.
    void setAudioLevels(float left, float right);

    /** Real per-band FFT levels (see SpectrumAnalyzer.h), 0.0-1.0 each, for
        the "Spectrum Band" meter style. */
    void setSpectrumLevels(const std::array<float, SpectrumAnalyzer::kNumBands>& levels);
    
    //==============================================================================
    // Connection status
    void setOnlineStatus(bool isOnline);
    
    //==============================================================================
    // User information
    void setUserInfo(const juce::String& userName, const juce::Image& avatar = {});

    //==============================================================================
    // Software update
    /** Shows/hides a small persistent "Update" pill just left of the
        avatar (mirrors VS Code's title-bar update button) -- stays
        visible until the host clicks it or the app restarts, rather than
        a one-off pop-up that can be missed or that interrupts a show. */
    void setUpdateAvailable(bool available, const juce::String& version = {});

    //==============================================================================
    // Callbacks
    std::function<void()> onUserButtonClicked;
    std::function<void()> onLogoClicked;
    std::function<void()> onUpdateButtonClicked;
    
    //==============================================================================
    // Localization
    /** Re-read all translatable strings from LocalizationManager. */
    void updateAllText();

private:
    //==============================================================================
    // Timer callback for VU meter animation
    void timerCallback() override;

    //==============================================================================
    // Pure click target -- paints nothing itself. juce::TextButton's
    // paintButton() always calls LookAndFeel_V4::drawButtonBackground(),
    // which draws its own rounded-rect fill/outline regardless of
    // buttonColourId being transparent -- that was the unwanted "box"
    // around the avatar/name. userButton uses this instead since its only
    // job is detecting the click; the avatar image (paint()) and
    // userNameLabel already provide the visuals.
    class InvisibleClickTarget : public juce::Button
    {
    public:
        InvisibleClickTarget() : juce::Button({}) {}
        void paintButton (juce::Graphics&, bool, bool) override {}
    };

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
    std::unique_ptr<InvisibleClickTarget> userButton;
    std::unique_ptr<juce::ImageButton> logoButton;
    std::unique_ptr<juce::TextButton> updateButton_;
    juce::String pendingUpdateVersion_;
    
    //==============================================================================
    // Images
    juce::Image logoImage;
    juce::Image coverArtImage;
    juce::Image userAvatarImage;
    juce::Image defaultAvatarImage;
    juce::Image defaultAlbumArtImage;
    juce::Image vuMeterBackImage;  // Analog Needle style's dial-face background (both L/R gauges in one image)

    //==============================================================================
    // State variables
    juce::String currentTrackName;
    juce::String currentArtistName;
    juce::String currentVersion;
    juce::String currentKey;
    int currentBpm = 0;
    bool isOnlineStatus = true;
    juce::String userName;

    //==============================================================================
    // VU meter -- vector-drawn, real stereo L/R, four selectable styles
    // cycled by clicking the meter (see mouseUp()/cycleVuMeterStyle()).
    enum class VuMeterStyle { GradientBar = 0, SegmentedLed = 1, AnalogNeedle = 2, SpectrumBand = 3 };
    static constexpr int kNumVuMeterStyles = 4;

    VuMeterStyle vuMeterStyle_ = VuMeterStyle::GradientBar;
    float currentLevelL_ = 0.0f, currentLevelR_ = 0.0f;
    float targetLevelL_  = 0.0f, targetLevelR_  = 0.0f;
    // Slow-decaying peak-hold marker (classic VU meter ballistics) drawn as
    // a thin tick on the gradient-bar and segmented-LED styles.
    float peakL_ = 0.0f, peakR_ = 0.0f;
    std::array<float, SpectrumAnalyzer::kNumBands> spectrumLevels_ {};

    void cycleVuMeterStyle();
    void drawGradientBarMeter(juce::Graphics& g, juce::Rectangle<int> bounds) const;
    void drawSegmentedLedMeter(juce::Graphics& g, juce::Rectangle<int> bounds) const;
    void drawAnalogNeedleMeter(juce::Graphics& g, juce::Rectangle<int> bounds) const;
    void drawSpectrumBandMeter(juce::Graphics& g, juce::Rectangle<int> bounds) const;
    void drawSingleNeedleGauge(juce::Graphics& g, juce::Rectangle<int> bounds, float level) const;
    static juce::Colour vuLevelColour(float level01);
    /** "-10.8"-style dB readout for the left-side value labels on the
        gradient-bar and segmented-LED styles. */
    static juce::String formatLevelDb(float level01);
    /** "600" / "1k" / "1.2k"-style compact readout for the spectrum-band
        style's per-band frequency labels. */
    static juce::String formatBandFrequency(double freqHz);
    /** Draws `text` in a reserved top strip and shrinks `bounds`
        accordingly -- shared by all four styles' titles. */
    void drawMeterTitle(juce::Graphics& g, juce::Rectangle<int>& bounds, const juce::String& text) const;
    
    //==============================================================================
    // Layout constants
    // COVER_ART_SIZE is now dynamic based on currentBarHeight
    // Logo is no longer a fixed square: resized() sizes it from the actual
    // asset's aspect ratio (a wide wordmark) so it fills the available
    // column width instead of being letterboxed inside a square button --
    // see resized()'s kMaxLogoHeight for the effective size cap.
    static constexpr int AVATAR_SIZE = 40;
    static constexpr int VU_METER_WIDTH = 240;   // Widened for the spectrum-band style's bars (was 180/150, originally 100)
    static constexpr int VU_METER_HEIGHT = 64;   // Taller to comfortably fit two stereo rows (was 40/30, originally 20)
    
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
    /** Slim pill area immediately left of getUserArea(), for updateButton_. */
    juce::Rectangle<int> getUpdateButtonArea() const;
    int getCurrentResizeHandleHeight() const;  // Dynamic height based on state
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBar)
};