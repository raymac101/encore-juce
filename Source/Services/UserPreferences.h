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

    //--- Resizable bar sizes ----------------------------------------------------
    // TopBar/BottomBar heights and NavBar/QueueBar widths, persisted
    // per-machine so a host's drag-to-resize choices survive logging out
    // and back in. -1 means "never saved" -- callers should leave the bar
    // at its own built-in default in that case rather than clamping to it.
    int getTopBarHeight() const;
    void setTopBarHeight(int height);
    int getBottomBarHeight() const;
    void setBottomBarHeight(int height);
    int getNavBarWidth() const;
    void setNavBarWidth(int width);
    int getQueueBarWidth() const;
    void setQueueBarWidth(int width);

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

    //--- Device identity / per-device venue confirmation ------------------------
    // A random ID generated once and persisted forever, identifying THIS
    // install (not tied to hardware). Used so the app can tell "the same PC
    // reopening a venue" apart from "a different PC opening it for the first
    // time" -- see LoginWindow's venue-confirmation screen and
    // VenueSessionService's session heartbeat.
    // Not const: lazily generates and persists the ID on first call.
    juce::String getDeviceId();

    // Tracks which venues THIS PC has already shown the "you're logging
    // into venue {name}" confirmation screen for, so a returning host on
    // their regular show machine only ever sees it once per venue, not on
    // every login.
    bool hasConfirmedVenueOnThisDevice(const juce::String& venueId) const;
    void markVenueConfirmedOnThisDevice(const juce::String& venueId);

    //--- Sound effects slot assignments (Ribbon > Sound Effects, full-screen) --
    // 8 entries, one per slot, each the display name of a SfxLibraryService
    // entry (or an empty string for an unassigned/cleared slot). Defaults to
    // the 8 sounds this app always used before slots became configurable,
    // so existing users see no change until they customize.
    juce::StringArray getSfxSlotAssignments() const;
    void setSfxSlotAssignment(int slotIndex, const juce::String& soundName);

    //--- Background music folder + track selection (Ribbon > Background Music,
    //    full-screen) --------------------------------------------------------
    // Empty string means "use the bundled default (assets/music)".
    juce::String getBackgroundMusicFolder() const;
    void setBackgroundMusicFolder(const juce::String& path);

    // Filenames (with extension, no path) of tracks selected to actually
    // play from the current folder. Empty array means "every track in the
    // folder is selected" -- this is what makes a freshly-picked folder
    // play everything by default with no extra step.
    juce::StringArray getBackgroundMusicSelectedTracks() const;
    void setBackgroundMusicSelectedTracks(const juce::StringArray& filenames);

    //--- Background music source (Ribbon > Background Music, full-screen) -----
    // "local" (default, zero-surprise on upgrade) or "spotify". See
    // Source/Services/SpotifyService.h -- Spotify is remote-controlled, not
    // played through this app's own audio engine.
    juce::String getBackgroundMusicSource() const;
    void setBackgroundMusicSource(const juce::String& source);

    // One-time setup: the Client ID from the host's own Spotify Developer
    // app (developer.spotify.com). Per-machine, same convention as the
    // ElevenLabs API key below.
    juce::String getSpotifyClientId() const;
    void setSpotifyClientId(const juce::String& clientId);

    // OAuth refresh token -- same security posture as the ElevenLabs API key
    // (this file, not additionally encrypted). Empty means not connected.
    juce::String getSpotifyRefreshToken() const;
    void setSpotifyRefreshToken(const juce::String& refreshToken);

    // Cached from connect()'s successful result purely so the "Connected
    // as ..." status can be shown on every UI refresh without an extra
    // GET /me call each time.
    juce::String getSpotifyAccountName() const;
    void setSpotifyAccountName(const juce::String& name);

    // The host's chosen playlist to start when switching to/selecting
    // Spotify as the source. Name is cached alongside the URI purely so the
    // picker can show it without a network round-trip.
    juce::String getSpotifySelectedPlaylistUri() const;
    juce::String getSpotifySelectedPlaylistName() const;
    void setSpotifySelectedPlaylist(const juce::String& uri, const juce::String& name);

    //--- "Start the Night" AI voice intro (Ribbon > Next Singer, full-screen) --
    // All per-machine, same as every other host preference above -- each
    // host supplies their own TTS provider API key.
    juce::String getElevenLabsApiKey() const;
    void setElevenLabsApiKey(const juce::String& key);

    // May contain the literal placeholders {venue} and {host}, substituted
    // at generation time. Defaults to a template using them so it works
    // out of the box.
    juce::String getIntroScript() const;
    void setIntroScript(const juce::String& script);

    juce::String getIntroVoiceId() const;
    void setIntroVoiceId(const juce::String& voiceId);

    // Filename (with extension, no path) of the chosen track under
    // assets/music/ (the same folder background music draws from). Empty
    // if none chosen yet.
    juce::String getIntroMusicFilename() const;
    void setIntroMusicFilename(const juce::String& filename);

    // One entry per successful IntroVoiceService::generateAndCache() call --
    // a host can build up a small library of intros and pick which one
    // "Start the Night" plays (see getSelectedIntroId() below), rather than
    // every generation overwriting the last.
    struct SavedIntro
    {
        juce::String id;             // stable, timestamp-based
        juce::String label;          // host-chosen display name
        juce::String fileName;       // under IntroVoiceService::getGeneratedDirectory()
        juce::String script;         // as generated (placeholders already substituted)
        juce::String voiceId;
        juce::String musicFilename;
        juce::int64  createdAtMs = 0;
    };

    // Newest first.
    std::vector<SavedIntro> getSavedIntros() const;
    void addSavedIntro(const SavedIntro& intro);
    void deleteSavedIntro(const juce::String& id);

    // Which saved intro (by id) "Start the Night" plays. Empty if none
    // selected yet, or if the selected one was since deleted.
    juce::String getSelectedIntroId() const;
    void setSelectedIntroId(const juce::String& id);

    //--- Room EQ Wizard (Source/UI/RoomEqWizard.h) ------------------------------
    // A correction curve derived from a Room EQ Wizard measurement run --
    // per-machine, since it's tied to wherever this PC's PA physically
    // lives (see RoomCorrectionEq.h). Mirrors SavedIntro's exact
    // named-list/JSON-array-of-objects shape above.
    struct RoomEqBandPref
    {
        double frequencyHz = 1000.0;
        float  gainDb       = 0.0f;
        double q             = 1.4;
    };

    struct RoomEqProfile
    {
        juce::String id;             // stable, timestamp-based
        juce::String label;          // host-chosen name, defaults to the active venue's name
        juce::String venueId;        // the venue active when this was measured, if any -- empty if none
        juce::String micType;        // "dynamic" / "condenser" / "flat"
        std::vector<RoomEqBandPref> bands;
        juce::int64  createdAtMs = 0;
    };

    // Newest first.
    std::vector<RoomEqProfile> getRoomEqProfiles() const;
    void addRoomEqProfile(const RoomEqProfile& profile);
    void deleteRoomEqProfile(const juce::String& id);

    // Which saved profile (by id) is currently applied. Empty if none
    // selected yet, or if the selected one was since deleted.
    juce::String getSelectedRoomEqProfileId() const;
    void setSelectedRoomEqProfileId(const juce::String& id);

    // The last-applied set of bands + on/off state, independent of any
    // saved profile -- so a manual tweak or an ad-hoc wizard run that
    // wasn't saved as a profile still survives an app restart.
    std::vector<RoomEqBandPref> getRoomEqBands() const;
    void setRoomEqBands(const std::vector<RoomEqBandPref>& bands);
    bool getRoomEqEnabled() const;
    void setRoomEqEnabled(bool enabled);

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

    //--- TopBar VU meter style ---------------------------------------------------
    // Cycled by clicking the meter itself (Source/UI/TopBar.cpp) -- stored
    // as a plain int (TopBar::VuMeterStyle's underlying value) rather than
    // a string, since it's an internal display choice, not something
    // hand-edited. Defaults to 0 (TopBar::VuMeterStyle::GradientBar).
    int getVuMeterStyle() const;
    void setVuMeterStyle(int style);

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
