/*
  ==============================================================================

    UserPreferences.h

    Persistent user preferences stored in the EncoreKaraoke app-data folder
    (alongside songbook.json / meta_data.json):
      macOS:   ~/Library/EncoreKaraoke/user-preferences.json
      Windows: %AppData%/EncoreKaraoke/user-preferences.json
      Linux:   ~/.config/EncoreKaraoke/user-preferences.json

    A one-time migration copies an existing file from the old Electron app
    location (~/Library/Application Support/encore/) if found. Unknown keys
    read from disk are preserved on save.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class UserPreferences
{
public:
    static UserPreferences& getInstance();

    //--- Window bounds ---------------------------------------------------------
    // Returns saved bounds, or an empty rectangle if none have been saved yet.
    juce::Rectangle<int> getWindowBounds() const;
    void setWindowBounds(const juce::Rectangle<int>& bounds);

    //--- Lyric (secondary) window bounds --------------------------------------
    juce::Rectangle<int> getLyricWindowBounds() const;
    void setLyricWindowBounds(const juce::Rectangle<int>& bounds);
    bool getLyricWindowFullScreen() const;
    void setLyricWindowFullScreen(bool fullScreen);

    //--- Title bar visibility (applies to both windows) -----------------------
    bool getShowTitleBar() const;
    void setShowTitleBar(bool show);

    //--- UI language (locale code, e.g. "en_US") -------------------------------
    juce::String getLanguage() const;
    void setLanguage(const juce::String& languageCode);

    //--- Venue -----------------------------------------------------------------
    juce::String getVenueId() const;
    void setVenueId(const juce::String& id);

    //--- Library path ----------------------------------------------------------
    juce::String getLibraryPath() const;
    void setLibraryPath(const juce::String& path);

    //--- Audio -----------------------------------------------------------------
    juce::String getPreferredAudioOutputDevice() const;
    void setPreferredAudioOutputDevice(const juce::String& deviceName);

    //--- Live vocal input (mic) -------------------------------------------------
    // Feature flag: disabled by default. When false, AudioEngine never opens
    // input channels or registers mic capture — existing playback behaviour is
    // completely unchanged.
    bool getLiveVocalInputEnabled() const;
    void setLiveVocalInputEnabled(bool enabled);

    // micIndex is 1 or 2. Returns -1 if unset (no channel mapped yet).
    int  getMicInputChannel(int micIndex) const;
    void setMicInputChannel(int micIndex, int deviceChannelIndex);

    // micIndex is 1 or 2. 0.0-1.0, default 0.8.
    float getMicGain(int micIndex) const;
    void  setMicGain(int micIndex, float gain);

    //--- Setup flag ------------------------------------------------------------
    bool getSetupCompleted() const;
    void setSetupCompleted(bool completed);

    //--- Nightly cleanup hour --------------------------------------------------
    // The hour-of-day (0-23) at which the automatic end-of-night archive +
    // queue clear should run. Default is 4 (4:00 AM).
    int  getNightlyCleanupHour() const;
    void setNightlyCleanupHour(int hour);

    //--- Lyric ad timing / silence detection ---------------------------------
    // dBFS threshold used when scanning for trailing silence (typically -35..-70).
    float getTrailingSilenceThresholdDb() const;
    void setTrailingSilenceThresholdDb(float db);

    // Seconds before detected audible end when lyric ads should transition in.
    int  getLyricAdTransitionLeadSeconds() const;
    void setLyricAdTransitionLeadSeconds(int seconds);

    // Height of the lyric lower-third venue-code bar as a percent of screen
    // height (default 11%).
    int  getLyricVenueCodeBarHeightPercent() const;
    void setLyricVenueCodeBarHeightPercent(int percent);

    //--- Lyric screen element scaling (Settings > Logo section sliders) -------
    // All are percentages (50-200, default 100) applied on top of
    // LyricDisplayComponent's existing size calculations, so 100% always
    // reproduces today's look exactly.
    int  getLyricLogoScalePercent() const;
    void setLyricLogoScalePercent(int percent);

    int  getLyricBrandTextScalePercent() const;
    void setLyricBrandTextScalePercent(int percent);

    int  getLyricNowSingingTextScalePercent() const;
    void setLyricNowSingingTextScalePercent(int percent);

    int  getLyricNowSingingInfoScalePercent() const;
    void setLyricNowSingingInfoScalePercent(int percent);

    int  getLyricUpNextTextScalePercent() const;
    void setLyricUpNextTextScalePercent(int percent);

    int  getLyricUpNextInfoScalePercent() const;
    void setLyricUpNextInfoScalePercent(int percent);

    int  getLyricBottomBarTextScalePercent() const;
    void setLyricBottomBarTextScalePercent(int percent);

    //--- Lyric screen theme (Settings > Lyric Screen section) -----------------
    // Index into the LyricTheme enum (Source/UI/LyricDisplayComponent.h),
    // 0-7, default 0 (Classic). Stored as a plain int here so this Services
    // class doesn't need to include a UI header.
    int  getLyricThemeIndex() const;
    void setLyricThemeIndex(int index);

    // 0-100, default 100. Scales every theme's accent/card saturation; at
    // 100% every theme (including Classic) renders at full design colour.
    int  getLyricColorIntensityPercent() const;
    void setLyricColorIntensityPercent(int percent);

    // 0-100, default 100. Scales every theme's animation speed/amplitude;
    // at 0% every theme is fully static, with no per-theme floor.
    int  getLyricMotionIntensityPercent() const;
    void setLyricMotionIntensityPercent(int percent);

    //--- Search column widths --------------------------------------------------
    // Stored as a JSON array of 7 numbers (fractions that sum to ~1.0):
    // art, song, artist, version, year, genre, edit. Empty vector if not set.
    std::vector<float> getSearchColumnFractions() const;
    void setSearchColumnFractions(const std::vector<float>& fractions);

    //--- Mixer plugin slots (Phase B) -------------------------------------------
    // One entry per (channelId, slotIndex) that currently has a plugin
    // loaded. channelId is one of "music"/"vocal1"/"vocal2"/"fx"/"bgmusic"/
    // "master"; slotIndex is 0 or 1. stateBase64 is the plugin's own
    // AudioProcessor::getStateInformation() blob, base64-encoded.
    struct PluginSlotState
    {
        juce::String channelId;
        int slotIndex = 0;
        // Full juce::PluginDescription::createXml() output, as a string —
        // the authoritative source for restore (handles VST3 shell
        // sub-plugins etc. correctly). pluginName is kept alongside purely
        // for a human-readable fallback/log message if XML parsing ever
        // fails on restore.
        juce::String descriptionXml;
        juce::String pluginName;
        juce::String stateBase64;
    };

    std::vector<PluginSlotState> getPluginSlotStates() const;

    /** Upserts by (channelId, slotIndex) — replaces any existing entry for
        that slot. */
    void setPluginSlotState(const PluginSlotState& state);

    /** Removes the saved entry for a slot, e.g. when the user picks "None". */
    void clearPluginSlotState(const juce::String& channelId, int slotIndex);

    //--- Low-level access ------------------------------------------------------
    /** Get the path of the preferences JSON file. */
    static juce::File getPreferencesFile();

    /** Force an immediate write to disk. Normally setters write automatically. */
    void save();

    /** Re-read the file from disk (useful after an external edit). */
    void reload();

private:
    UserPreferences();
    ~UserPreferences() = default;

    void load();

    juce::var root_;                // JSON root (DynamicObject)
    juce::CriticalSection lock_;

    JUCE_DECLARE_NON_COPYABLE(UserPreferences)
};
