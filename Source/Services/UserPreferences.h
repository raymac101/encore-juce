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

    //--- Setup flag ------------------------------------------------------------
    bool getSetupCompleted() const;
    void setSetupCompleted(bool completed);

    //--- Search column widths --------------------------------------------------
    // Stored as a JSON array of 7 numbers (fractions that sum to ~1.0):
    // art, song, artist, version, year, genre, edit. Empty vector if not set.
    std::vector<float> getSearchColumnFractions() const;
    void setSearchColumnFractions(const std::vector<float>& fractions);

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
