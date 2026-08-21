/*
  ==============================================================================

    UserPreferences.cpp

  ==============================================================================
*/

#include "UserPreferences.h"
#include <algorithm>

//==============================================================================
UserPreferences& UserPreferences::getInstance()
{
    static UserPreferences instance;
    return instance;
}

//==============================================================================
juce::File UserPreferences::getPreferencesFile()
{
    // Stored alongside songbook.json / meta_data.json in the EncoreKaraoke
    // app-data folder:
    //   macOS:   ~/Library/Application Support/EncoreKaraoke/user-preferences.json
    //   Windows: %AppData%/EncoreKaraoke/user-preferences.json
    //   Linux:   ~/.config/EncoreKaraoke/user-preferences.json
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("EncoreKaraoke");
    if (! dir.exists())
        dir.createDirectory();

    auto file = dir.getChildFile("user-preferences.json");

    // One-time migration from the old "encore" folder (case-sensitive) used by
    // the initial JUCE build, and from the Electron app's location.
    if (! file.existsAsFile())
    {
        juce::File legacy[] = {
            juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               #if JUCE_MAC
                .getChildFile("Application Support")
               #endif
                .getChildFile("encore").getChildFile("user-preferences.json"),
            juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("encore").getChildFile("user-preferences.json"),
        };
        for (auto& legacyFile : legacy)
        {
            if (legacyFile.existsAsFile())
            {
                legacyFile.copyFileTo(file);
                break;
            }
        }
    }
    return file;
}

//==============================================================================
UserPreferences::UserPreferences()
{
    load();
}

void UserPreferences::load()
{
    const juce::ScopedLock sl(lock_);
    auto file = getPreferencesFile();
    if (file.existsAsFile())
    {
        auto parsed = juce::JSON::parse(file);
        if (parsed.isObject())
        {
            root_ = parsed;
            return;
        }
    }
    // Default to an empty object so setters can add keys.
    root_ = juce::var(new juce::DynamicObject());
}

void UserPreferences::reload()
{
    load();
}

void UserPreferences::save()
{
    const juce::ScopedLock sl(lock_);
    auto file = getPreferencesFile();
    file.replaceWithText(juce::JSON::toString(root_, /*allOnOneLine*/ false));
}

//==============================================================================
static juce::DynamicObject* asObj(juce::var& v)
{
    if (! v.isObject())
        v = juce::var(new juce::DynamicObject());
    return v.getDynamicObject();
}

//==============================================================================
juce::Rectangle<int> UserPreferences::getWindowBounds() const
{
    const juce::ScopedLock sl(lock_);
    auto wb = root_.getProperty("windowBounds", juce::var());
    if (! wb.isObject()) return {};

    int w = (int) wb.getProperty("width",  0);
    int h = (int) wb.getProperty("height", 0);
    int x = (int) wb.getProperty("x", -1);
    int y = (int) wb.getProperty("y", -1);

    if (w <= 0 || h <= 0) return {};
    if (x < 0 || y < 0)   return juce::Rectangle<int>(w, h);   // size only
    return juce::Rectangle<int>(x, y, w, h);
}

void UserPreferences::setWindowBounds(const juce::Rectangle<int>& bounds)
{
    const juce::ScopedLock sl(lock_);
    auto* rootObj = asObj(root_);
    auto* wb = new juce::DynamicObject();
    wb->setProperty("x",      bounds.getX());
    wb->setProperty("y",      bounds.getY());
    wb->setProperty("width",  bounds.getWidth());
    wb->setProperty("height", bounds.getHeight());
    rootObj->setProperty("windowBounds", juce::var(wb));
    save();
}

//==============================================================================
juce::Rectangle<int> UserPreferences::getLyricWindowBounds() const
{
    const juce::ScopedLock sl(lock_);
    auto wb = root_.getProperty("lyricWindowBounds", juce::var());
    if (! wb.isObject()) return {};

    int w = (int) wb.getProperty("width",  0);
    int h = (int) wb.getProperty("height", 0);
    int x = (int) wb.getProperty("x", -1);
    int y = (int) wb.getProperty("y", -1);

    if (w <= 0 || h <= 0) return {};
    if (x < 0 || y < 0)   return juce::Rectangle<int>(w, h);
    return juce::Rectangle<int>(x, y, w, h);
}

void UserPreferences::setLyricWindowBounds(const juce::Rectangle<int>& bounds)
{
    const juce::ScopedLock sl(lock_);
    auto* rootObj = asObj(root_);
    auto* wb = new juce::DynamicObject();
    wb->setProperty("x",      bounds.getX());
    wb->setProperty("y",      bounds.getY());
    wb->setProperty("width",  bounds.getWidth());
    wb->setProperty("height", bounds.getHeight());
    rootObj->setProperty("lyricWindowBounds", juce::var(wb));
    save();
}

bool UserPreferences::getLyricWindowFullScreen() const
{
    const juce::ScopedLock sl(lock_);
    return (bool) root_.getProperty("lyricWindowFullScreen", juce::var(false));
}

void UserPreferences::setLyricWindowFullScreen(bool fullScreen)
{
    const juce::ScopedLock sl(lock_);
    auto* rootObj = asObj(root_);
    rootObj->setProperty("lyricWindowFullScreen", juce::var(fullScreen));
    save();
}

//==============================================================================
bool UserPreferences::getShowTitleBar() const
{
    const juce::ScopedLock sl(lock_);
    // Default: title bars visible.
    return (bool) root_.getProperty("showTitleBar", juce::var(true));
}

void UserPreferences::setShowTitleBar(bool show)
{
    const juce::ScopedLock sl(lock_);
    auto* rootObj = asObj(root_);
    rootObj->setProperty("showTitleBar", juce::var(show));
    save();
}

//==============================================================================
int UserPreferences::getTopBarHeight() const
{
    const juce::ScopedLock sl(lock_);
    return (int) root_.getProperty("topBarHeight", -1);
}

void UserPreferences::setTopBarHeight(int height)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("topBarHeight", height);
    save();
}

int UserPreferences::getBottomBarHeight() const
{
    const juce::ScopedLock sl(lock_);
    return (int) root_.getProperty("bottomBarHeight", -1);
}

void UserPreferences::setBottomBarHeight(int height)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("bottomBarHeight", height);
    save();
}

int UserPreferences::getNavBarWidth() const
{
    const juce::ScopedLock sl(lock_);
    return (int) root_.getProperty("navBarWidth", -1);
}

void UserPreferences::setNavBarWidth(int width)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("navBarWidth", width);
    save();
}

int UserPreferences::getQueueBarWidth() const
{
    const juce::ScopedLock sl(lock_);
    return (int) root_.getProperty("queueBarWidth", -1);
}

void UserPreferences::setQueueBarWidth(int width)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("queueBarWidth", width);
    save();
}

//==============================================================================
juce::String UserPreferences::getLanguage() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("language", juce::var()).toString();
}

void UserPreferences::setLanguage(const juce::String& languageCode)
{
    const juce::ScopedLock sl(lock_);
    auto* rootObj = asObj(root_);
    rootObj->setProperty("language", juce::var(languageCode));
    save();
}

//==============================================================================
juce::String UserPreferences::getVenueId() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("venueId", juce::var()).toString();
}

void UserPreferences::setVenueId(const juce::String& id)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("venueId", id);
    save();
}

//==============================================================================
juce::String UserPreferences::getSavedLoginEmail() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("savedLoginEmail", juce::var()).toString();
}

void UserPreferences::setSavedLoginEmail(const juce::String& email)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("savedLoginEmail", email);
    save();
}

juce::String UserPreferences::getSavedLoginRefreshToken() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("savedLoginRefreshToken", juce::var()).toString();
}

void UserPreferences::setSavedLoginRefreshToken(const juce::String& refreshToken)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("savedLoginRefreshToken", refreshToken);
    save();
}

void UserPreferences::clearSavedLogin()
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("savedLoginEmail", juce::String());
    asObj(root_)->setProperty("savedLoginRefreshToken", juce::String());
    save();
}

//==============================================================================
juce::String UserPreferences::getLibraryPath() const
{
    const juce::ScopedLock sl(lock_);
    // Old Electron key was "addPath"
    return root_.getProperty("addPath", juce::var()).toString();
}

void UserPreferences::setLibraryPath(const juce::String& path)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("addPath", path);
    save();
}

//==============================================================================
juce::String UserPreferences::getAnthropicApiKey() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("anthropicApiKey", juce::var()).toString();
}

void UserPreferences::setAnthropicApiKey(const juce::String& key)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("anthropicApiKey", key);
    save();
}

//==============================================================================
juce::int64 UserPreferences::getLastMetadataSyncAtMs() const
{
    const juce::ScopedLock sl(lock_);
    return (juce::int64) root_.getProperty("lastMetadataSyncAtMs", 0);
}

void UserPreferences::setLastMetadataSyncAtMs(juce::int64 epochMs)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lastMetadataSyncAtMs", epochMs);
    save();
}

//==============================================================================
juce::String UserPreferences::getPreferredAudioOutputDevice() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("preferredAudioOutputDevice", juce::var()).toString();
}

void UserPreferences::setPreferredAudioOutputDevice(const juce::String& deviceName)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("preferredAudioOutputDevice", deviceName);
    save();
}

//==============================================================================
bool UserPreferences::getLiveVocalInputEnabled() const
{
    const juce::ScopedLock sl(lock_);
    return (bool) root_.getProperty("liveVocalInputEnabled", false);
}

void UserPreferences::setLiveVocalInputEnabled(bool enabled)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("liveVocalInputEnabled", enabled);
    save();
}

int UserPreferences::getMicInputChannel(int micIndex) const
{
    const juce::ScopedLock sl(lock_);
    const auto key = "micInputChannel" + juce::String(micIndex);
    return (int) root_.getProperty(key, juce::var(-1));
}

void UserPreferences::setMicInputChannel(int micIndex, int deviceChannelIndex)
{
    const juce::ScopedLock sl(lock_);
    const auto key = "micInputChannel" + juce::String(micIndex);
    asObj(root_)->setProperty(key, deviceChannelIndex);
    save();
}

float UserPreferences::getMicGain(int micIndex) const
{
    const juce::ScopedLock sl(lock_);
    const auto key = "micGain" + juce::String(micIndex);
    auto gain = (float) (double) root_.getProperty(key, juce::var(0.8));
    return juce::jlimit(0.0f, 1.0f, gain);
}

void UserPreferences::setMicGain(int micIndex, float gain)
{
    const juce::ScopedLock sl(lock_);
    const auto key = "micGain" + juce::String(micIndex);
    asObj(root_)->setProperty(key, juce::jlimit(0.0f, 1.0f, gain));
    save();
}

//==============================================================================
bool UserPreferences::getSetupCompleted() const
{
    const juce::ScopedLock sl(lock_);
    return (bool) root_.getProperty("setupCompleted", false);
}

void UserPreferences::setSetupCompleted(bool completed)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("setupCompleted", completed);
    save();
}

//==============================================================================
int UserPreferences::getNightlyCleanupHour() const
{
    const juce::ScopedLock sl(lock_);
    // Default: 4:00 AM. Clamp on read so a corrupted value can't escape.
    int h = (int) root_.getProperty("nightlyCleanupHour", juce::var(4));
    return juce::jlimit(0, 23, h);
}

void UserPreferences::setNightlyCleanupHour(int hour)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("nightlyCleanupHour", juce::jlimit(0, 23, hour));
    save();
}

//==============================================================================
float UserPreferences::getTrailingSilenceThresholdDb() const
{
    const juce::ScopedLock sl(lock_);
    // Default: -50 dBFS. Clamp to sane range for simple UI controls.
    auto db = (float) (double) root_.getProperty("trailingSilenceThresholdDb", juce::var(-50.0));
    return juce::jlimit(-80.0f, -20.0f, db);
}

void UserPreferences::setTrailingSilenceThresholdDb(float db)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("trailingSilenceThresholdDb", juce::jlimit(-80.0f, -20.0f, db));
    save();
}

int UserPreferences::getLyricAdTransitionLeadSeconds() const
{
    const juce::ScopedLock sl(lock_);
    // Default: 7 seconds before audible end.
    int s = (int) root_.getProperty("lyricAdTransitionLeadSeconds", juce::var(7));
    return juce::jlimit(1, 30, s);
}

void UserPreferences::setLyricAdTransitionLeadSeconds(int seconds)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricAdTransitionLeadSeconds", juce::jlimit(1, 30, seconds));
    save();
}

int UserPreferences::getLyricVenueCodeBarHeightPercent() const
{
    const juce::ScopedLock sl(lock_);
    // Default: 11% (roughly previous 1/9 behavior).
    int p = (int) root_.getProperty("lyricVenueCodeBarHeightPercent", juce::var(11));
    return juce::jlimit(6, 20, p);
}

void UserPreferences::setLyricVenueCodeBarHeightPercent(int percent)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricVenueCodeBarHeightPercent", juce::jlimit(6, 20, percent));
    save();
}

//==============================================================================
// Lyric screen element scaling -- shared getter/setter shape, one JSON key
// each, clamped 50-200, default 100.
namespace
{
    int getScalePercent(const juce::var& root, const juce::CriticalSection& lock, const char* key)
    {
        const juce::ScopedLock sl(lock);
        int p = (int) root.getProperty(key, juce::var(100));
        return juce::jlimit(50, 200, p);
    }
}

int UserPreferences::getLyricLogoScalePercent() const
{
    return getScalePercent(root_, lock_, "lyricLogoScalePercent");
}
void UserPreferences::setLyricLogoScalePercent(int percent)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricLogoScalePercent", juce::jlimit(50, 200, percent));
    save();
}

int UserPreferences::getLyricBrandTextScalePercent() const
{
    return getScalePercent(root_, lock_, "lyricBrandTextScalePercent");
}
void UserPreferences::setLyricBrandTextScalePercent(int percent)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricBrandTextScalePercent", juce::jlimit(50, 200, percent));
    save();
}

int UserPreferences::getLyricNowSingingTextScalePercent() const
{
    return getScalePercent(root_, lock_, "lyricNowSingingTextScalePercent");
}
void UserPreferences::setLyricNowSingingTextScalePercent(int percent)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricNowSingingTextScalePercent", juce::jlimit(50, 200, percent));
    save();
}

int UserPreferences::getLyricNowSingingInfoScalePercent() const
{
    return getScalePercent(root_, lock_, "lyricNowSingingInfoScalePercent");
}
void UserPreferences::setLyricNowSingingInfoScalePercent(int percent)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricNowSingingInfoScalePercent", juce::jlimit(50, 200, percent));
    save();
}

int UserPreferences::getLyricUpNextTextScalePercent() const
{
    return getScalePercent(root_, lock_, "lyricUpNextTextScalePercent");
}
void UserPreferences::setLyricUpNextTextScalePercent(int percent)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricUpNextTextScalePercent", juce::jlimit(50, 200, percent));
    save();
}

int UserPreferences::getLyricUpNextInfoScalePercent() const
{
    return getScalePercent(root_, lock_, "lyricUpNextInfoScalePercent");
}
void UserPreferences::setLyricUpNextInfoScalePercent(int percent)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricUpNextInfoScalePercent", juce::jlimit(50, 200, percent));
    save();
}

int UserPreferences::getLyricBottomBarTextScalePercent() const
{
    return getScalePercent(root_, lock_, "lyricBottomBarTextScalePercent");
}
void UserPreferences::setLyricBottomBarTextScalePercent(int percent)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricBottomBarTextScalePercent", juce::jlimit(50, 200, percent));
    save();
}

//==============================================================================
// Lyric screen theme -- a sibling 0-100 clamp helper (the size sliders above
// use 50-200, which doesn't fit Color/Motion Intensity's 0-100 range), plus
// a 0-7 clamp for the theme index.
namespace
{
    int getPercent01to100(const juce::var& root, const juce::CriticalSection& lock, const char* key, int defaultValue)
    {
        const juce::ScopedLock sl(lock);
        int p = (int) root.getProperty(key, juce::var(defaultValue));
        return juce::jlimit(0, 100, p);
    }
}

int UserPreferences::getLyricThemeIndex() const
{
    const juce::ScopedLock sl(lock_);
    int idx = (int) root_.getProperty("lyricThemeIndex", juce::var(0));
    return juce::jlimit(0, 7, idx);
}
void UserPreferences::setLyricThemeIndex(int index)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricThemeIndex", juce::jlimit(0, 7, index));
    save();
}

int UserPreferences::getLyricColorIntensityPercent() const
{
    return getPercent01to100(root_, lock_, "lyricColorIntensityPercent", 100);
}
void UserPreferences::setLyricColorIntensityPercent(int percent)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricColorIntensityPercent", juce::jlimit(0, 100, percent));
    save();
}

int UserPreferences::getLyricMotionIntensityPercent() const
{
    return getPercent01to100(root_, lock_, "lyricMotionIntensityPercent", 100);
}
void UserPreferences::setLyricMotionIntensityPercent(int percent)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("lyricMotionIntensityPercent", juce::jlimit(0, 100, percent));
    save();
}

//==============================================================================
juce::String UserPreferences::getDeviceId()
{
    const juce::ScopedLock sl(lock_);
    auto existing = root_.getProperty("deviceId", juce::var()).toString();
    if (existing.isNotEmpty())
        return existing;

    // Generated lazily, once, and persisted forever after -- not tied to
    // hardware, just needs to be stable for the lifetime of this install.
    const auto fresh = juce::Uuid().toString();
    asObj(root_)->setProperty("deviceId", fresh);
    save();
    return fresh;
}

bool UserPreferences::hasConfirmedVenueOnThisDevice(const juce::String& venueId) const
{
    const juce::ScopedLock sl(lock_);
    auto arr = root_.getProperty("confirmedVenueIds", juce::var());
    if (! arr.isArray())
        return false;

    for (int i = 0; i < arr.size(); ++i)
        if (arr[i].toString() == venueId)
            return true;
    return false;
}

void UserPreferences::markVenueConfirmedOnThisDevice(const juce::String& venueId)
{
    const juce::ScopedLock sl(lock_);
    if (hasConfirmedVenueOnThisDevice(venueId))
        return;

    auto existing = root_.getProperty("confirmedVenueIds", juce::var());
    juce::Array<juce::var> arr;
    if (existing.isArray())
        arr = *existing.getArray();
    arr.add(venueId);

    asObj(root_)->setProperty("confirmedVenueIds", arr);
    save();
}

//==============================================================================
juce::StringArray UserPreferences::getSfxSlotAssignments() const
{
    static const juce::StringArray defaults {
        "Are You Ready", "Chicken", "Burp", "Bruh",
        "Buzzer", "Drum Fill", "Drum Roll", "WooHoo"
    };

    const juce::ScopedLock sl(lock_);
    auto arr = root_.getProperty("sfxSlotAssignments", juce::var());
    if (! arr.isArray() || arr.size() != 8)
        return defaults;

    juce::StringArray out;
    for (int i = 0; i < 8; ++i)
        out.add(arr[i].toString());
    return out;
}

void UserPreferences::setSfxSlotAssignment(int slotIndex, const juce::String& soundName)
{
    if (slotIndex < 0 || slotIndex >= 8)
        return;

    const juce::ScopedLock sl(lock_);
    auto current = getSfxSlotAssignments();
    current.set(slotIndex, soundName);

    juce::Array<juce::var> arr;
    for (auto& name : current)
        arr.add(name);

    asObj(root_)->setProperty("sfxSlotAssignments", arr);
    save();
}

//==============================================================================
juce::String UserPreferences::getBackgroundMusicFolder() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("backgroundMusicFolder", juce::var()).toString();
}

void UserPreferences::setBackgroundMusicFolder(const juce::String& path)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("backgroundMusicFolder", path);
    save();
}

juce::StringArray UserPreferences::getBackgroundMusicSelectedTracks() const
{
    const juce::ScopedLock sl(lock_);
    auto arr = root_.getProperty("backgroundMusicSelectedTracks", juce::var());
    juce::StringArray out;
    if (! arr.isArray())
        return out;
    for (int i = 0; i < arr.size(); ++i)
        out.add(arr[i].toString());
    return out;
}

void UserPreferences::setBackgroundMusicSelectedTracks(const juce::StringArray& filenames)
{
    const juce::ScopedLock sl(lock_);
    juce::Array<juce::var> arr;
    for (auto& name : filenames)
        arr.add(name);
    asObj(root_)->setProperty("backgroundMusicSelectedTracks", arr);
    save();
}

juce::String UserPreferences::getBackgroundMusicSource() const
{
    const juce::ScopedLock sl(lock_);
    const auto v = root_.getProperty("backgroundMusicSource", juce::var()).toString();
    return v.isNotEmpty() ? v : juce::String("local");
}

void UserPreferences::setBackgroundMusicSource(const juce::String& source)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("backgroundMusicSource", source);
    save();
}

bool UserPreferences::getBackgroundMusicEnabled() const
{
    const juce::ScopedLock sl(lock_);
    return (bool) root_.getProperty("backgroundMusicEnabled", true);
}

void UserPreferences::setBackgroundMusicEnabled(bool enabled)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("backgroundMusicEnabled", enabled);
    save();
}

juce::String UserPreferences::getSpotifyClientId() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("spotifyClientId", juce::var()).toString();
}

void UserPreferences::setSpotifyClientId(const juce::String& clientId)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("spotifyClientId", clientId);
    save();
}

juce::String UserPreferences::getSpotifyRefreshToken() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("spotifyRefreshToken", juce::var()).toString();
}

void UserPreferences::setSpotifyRefreshToken(const juce::String& refreshToken)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("spotifyRefreshToken", refreshToken);
    save();
}

juce::String UserPreferences::getSpotifyAccountName() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("spotifyAccountName", juce::var()).toString();
}

void UserPreferences::setSpotifyAccountName(const juce::String& name)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("spotifyAccountName", name);
    save();
}

juce::String UserPreferences::getSpotifySelectedPlaylistUri() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("spotifySelectedPlaylistUri", juce::var()).toString();
}

juce::String UserPreferences::getSpotifySelectedPlaylistName() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("spotifySelectedPlaylistName", juce::var()).toString();
}

void UserPreferences::setSpotifySelectedPlaylist(const juce::String& uri, const juce::String& name)
{
    const juce::ScopedLock sl(lock_);
    auto* obj = asObj(root_);
    obj->setProperty("spotifySelectedPlaylistUri", uri);
    obj->setProperty("spotifySelectedPlaylistName", name);
    save();
}

//==============================================================================
juce::String UserPreferences::getElevenLabsApiKey() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("elevenLabsApiKey", juce::var()).toString();
}

void UserPreferences::setElevenLabsApiKey(const juce::String& key)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("elevenLabsApiKey", key);
    save();
}

juce::String UserPreferences::getIntroScript() const
{
    static const juce::String defaultScript =
        "Welcome to Karaoke Night at {venue}, with your host {host}! Let's get this party started!";

    const juce::ScopedLock sl(lock_);
    const auto value = root_.getProperty("introScript", juce::var()).toString();
    return value.isNotEmpty() ? value : defaultScript;
}

void UserPreferences::setIntroScript(const juce::String& script)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("introScript", script);
    save();
}

juce::String UserPreferences::getIntroVoiceId() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("introVoiceId", juce::var()).toString();
}

void UserPreferences::setIntroVoiceId(const juce::String& voiceId)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("introVoiceId", voiceId);
    save();
}

juce::String UserPreferences::getIntroMusicFilename() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("introMusicFilename", juce::var()).toString();
}

void UserPreferences::setIntroMusicFilename(const juce::String& filename)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("introMusicFilename", filename);
    save();
}

std::vector<UserPreferences::SavedIntro> UserPreferences::getSavedIntros() const
{
    const juce::ScopedLock sl(lock_);
    std::vector<SavedIntro> out;

    auto arr = root_.getProperty("savedIntros", juce::var());
    if (! arr.isArray())
        return out;

    for (int i = 0; i < arr.size(); ++i)
    {
        auto entry = arr[i];
        SavedIntro s;
        s.id             = entry.getProperty("id", "").toString();
        s.label          = entry.getProperty("label", "").toString();
        s.fileName       = entry.getProperty("fileName", "").toString();
        s.script         = entry.getProperty("script", "").toString();
        s.voiceId        = entry.getProperty("voiceId", "").toString();
        s.musicFilename  = entry.getProperty("musicFilename", "").toString();
        s.createdAtMs    = (juce::int64) entry.getProperty("createdAtMs", 0);

        if (s.id.isNotEmpty() && s.fileName.isNotEmpty())
            out.push_back(std::move(s));
    }

    // Newest first.
    std::sort(out.begin(), out.end(), [](const SavedIntro& a, const SavedIntro& b)
    {
        return a.createdAtMs > b.createdAtMs;
    });

    return out;
}

void UserPreferences::addSavedIntro(const SavedIntro& intro)
{
    const juce::ScopedLock sl(lock_);

    juce::Array<juce::var> arr;
    auto existing = root_.getProperty("savedIntros", juce::var());
    if (existing.isArray())
        for (int i = 0; i < existing.size(); ++i)
            arr.add(existing[i]);

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id",             intro.id);
    obj->setProperty("label",          intro.label);
    obj->setProperty("fileName",       intro.fileName);
    obj->setProperty("script",         intro.script);
    obj->setProperty("voiceId",        intro.voiceId);
    obj->setProperty("musicFilename",  intro.musicFilename);
    obj->setProperty("createdAtMs",    intro.createdAtMs);
    arr.add(juce::var(obj.get()));

    asObj(root_)->setProperty("savedIntros", arr);
    save();
}

void UserPreferences::deleteSavedIntro(const juce::String& id)
{
    const juce::ScopedLock sl(lock_);

    juce::Array<juce::var> arr;
    auto existing = root_.getProperty("savedIntros", juce::var());
    if (existing.isArray())
        for (int i = 0; i < existing.size(); ++i)
        {
            auto entry = existing[i];
            if (entry.getProperty("id", "").toString() != id)
                arr.add(entry);
        }

    asObj(root_)->setProperty("savedIntros", arr);
    save();
}

juce::String UserPreferences::getSelectedIntroId() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("selectedIntroId", juce::var()).toString();
}

void UserPreferences::setSelectedIntroId(const juce::String& id)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("selectedIntroId", id);
    save();
}

//==============================================================================
namespace
{
    juce::var roomEqBandsToVar(const std::vector<UserPreferences::RoomEqBandPref>& bands)
    {
        juce::Array<juce::var> arr;
        for (auto& b : bands)
        {
            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
            obj->setProperty("frequencyHz", b.frequencyHz);
            obj->setProperty("gainDb",      b.gainDb);
            obj->setProperty("q",           b.q);
            arr.add(juce::var(obj.get()));
        }
        return juce::var(arr);
    }

    std::vector<UserPreferences::RoomEqBandPref> roomEqBandsFromVar(const juce::var& arr)
    {
        std::vector<UserPreferences::RoomEqBandPref> out;
        if (! arr.isArray())
            return out;

        for (int i = 0; i < arr.size(); ++i)
        {
            auto entry = arr[i];
            UserPreferences::RoomEqBandPref b;
            b.frequencyHz = (double) entry.getProperty("frequencyHz", 1000.0);
            b.gainDb       = (float)  entry.getProperty("gainDb", 0.0);
            b.q             = (double) entry.getProperty("q", 1.4);
            out.push_back(b);
        }
        return out;
    }
}

std::vector<UserPreferences::RoomEqProfile> UserPreferences::getRoomEqProfiles() const
{
    const juce::ScopedLock sl(lock_);
    std::vector<RoomEqProfile> out;

    auto arr = root_.getProperty("roomEqProfiles", juce::var());
    if (! arr.isArray())
        return out;

    for (int i = 0; i < arr.size(); ++i)
    {
        auto entry = arr[i];
        RoomEqProfile p;
        p.id          = entry.getProperty("id", "").toString();
        p.label       = entry.getProperty("label", "").toString();
        p.venueId     = entry.getProperty("venueId", "").toString();
        p.micType     = entry.getProperty("micType", "").toString();
        p.bands       = roomEqBandsFromVar(entry.getProperty("bands", juce::var()));
        p.createdAtMs = (juce::int64) entry.getProperty("createdAtMs", 0);

        if (p.id.isNotEmpty())
            out.push_back(std::move(p));
    }

    // Newest first.
    std::sort(out.begin(), out.end(), [](const RoomEqProfile& a, const RoomEqProfile& b)
    {
        return a.createdAtMs > b.createdAtMs;
    });

    return out;
}

void UserPreferences::addRoomEqProfile(const RoomEqProfile& profile)
{
    const juce::ScopedLock sl(lock_);

    juce::Array<juce::var> arr;
    auto existing = root_.getProperty("roomEqProfiles", juce::var());
    if (existing.isArray())
        for (int i = 0; i < existing.size(); ++i)
            arr.add(existing[i]);

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id",          profile.id);
    obj->setProperty("label",       profile.label);
    obj->setProperty("venueId",     profile.venueId);
    obj->setProperty("micType",     profile.micType);
    obj->setProperty("bands",       roomEqBandsToVar(profile.bands));
    obj->setProperty("createdAtMs", profile.createdAtMs);
    arr.add(juce::var(obj.get()));

    asObj(root_)->setProperty("roomEqProfiles", arr);
    save();
}

void UserPreferences::deleteRoomEqProfile(const juce::String& id)
{
    const juce::ScopedLock sl(lock_);

    juce::Array<juce::var> arr;
    auto existing = root_.getProperty("roomEqProfiles", juce::var());
    if (existing.isArray())
        for (int i = 0; i < existing.size(); ++i)
        {
            auto entry = existing[i];
            if (entry.getProperty("id", "").toString() != id)
                arr.add(entry);
        }

    asObj(root_)->setProperty("roomEqProfiles", arr);
    save();
}

juce::String UserPreferences::getSelectedRoomEqProfileId() const
{
    const juce::ScopedLock sl(lock_);
    return root_.getProperty("selectedRoomEqProfileId", juce::var()).toString();
}

void UserPreferences::setSelectedRoomEqProfileId(const juce::String& id)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("selectedRoomEqProfileId", id);
    save();
}

std::vector<UserPreferences::RoomEqBandPref> UserPreferences::getRoomEqBands() const
{
    const juce::ScopedLock sl(lock_);
    return roomEqBandsFromVar(root_.getProperty("roomEqBands", juce::var()));
}

void UserPreferences::setRoomEqBands(const std::vector<RoomEqBandPref>& bands)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("roomEqBands", roomEqBandsToVar(bands));
    save();
}

bool UserPreferences::getRoomEqEnabled() const
{
    const juce::ScopedLock sl(lock_);
    return (bool) root_.getProperty("roomEqEnabled", false);
}

void UserPreferences::setRoomEqEnabled(bool enabled)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("roomEqEnabled", enabled);
    save();
}

//==============================================================================
int UserPreferences::getVuMeterStyle() const
{
    const juce::ScopedLock sl(lock_);
    return (int) root_.getProperty("vuMeterStyle", 0);
}

void UserPreferences::setVuMeterStyle(int style)
{
    const juce::ScopedLock sl(lock_);
    asObj(root_)->setProperty("vuMeterStyle", style);
    save();
}

//==============================================================================
std::vector<float> UserPreferences::getSearchColumnFractions() const
{
    const juce::ScopedLock sl(lock_);
    auto arr = root_.getProperty("searchColumnFractions", juce::var());
    if (! arr.isArray()) return {};
    std::vector<float> out;
    out.reserve((size_t) arr.size());
    for (int i = 0; i < arr.size(); ++i)
        out.push_back((float)(double) arr[i]);
    return out;
}

void UserPreferences::setSearchColumnFractions(const std::vector<float>& fractions)
{
    const juce::ScopedLock sl(lock_);
    juce::Array<juce::var> arr;
    for (float f : fractions) arr.add(f);
    asObj(root_)->setProperty("searchColumnFractions", arr);
    save();
}

//==============================================================================
std::vector<UserPreferences::PluginSlotState> UserPreferences::getPluginSlotStates() const
{
    const juce::ScopedLock sl(lock_);
    std::vector<PluginSlotState> out;

    auto arr = root_.getProperty("pluginSlots", juce::var());
    if (! arr.isArray())
        return out;

    for (int i = 0; i < arr.size(); ++i)
    {
        auto entry = arr[i];
        PluginSlotState s;
        s.channelId             = entry.getProperty("channelId", "").toString();
        s.slotIndex             = (int) entry.getProperty("slotIndex", 0);
        s.descriptionXml        = entry.getProperty("descriptionXml", "").toString();
        s.pluginName            = entry.getProperty("pluginName", "").toString();
        s.stateBase64           = entry.getProperty("stateBase64", "").toString();

        if (s.channelId.isNotEmpty())
            out.push_back(std::move(s));
    }

    return out;
}

void UserPreferences::setPluginSlotState(const PluginSlotState& state)
{
    const juce::ScopedLock sl(lock_);

    juce::Array<juce::var> arr;
    auto existing = root_.getProperty("pluginSlots", juce::var());
    if (existing.isArray())
        for (int i = 0; i < existing.size(); ++i)
        {
            auto entry = existing[i];
            const bool sameSlot = entry.getProperty("channelId", "").toString() == state.channelId
                               && (int) entry.getProperty("slotIndex", 0) == state.slotIndex;
            if (! sameSlot)
                arr.add(entry);
        }

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("channelId", state.channelId);
    obj->setProperty("slotIndex", state.slotIndex);
    obj->setProperty("descriptionXml", state.descriptionXml);
    obj->setProperty("pluginName", state.pluginName);
    obj->setProperty("stateBase64", state.stateBase64);
    arr.add(juce::var(obj.get()));

    asObj(root_)->setProperty("pluginSlots", arr);
    save();
}

void UserPreferences::clearPluginSlotState(const juce::String& channelId, int slotIndex)
{
    const juce::ScopedLock sl(lock_);

    auto existing = root_.getProperty("pluginSlots", juce::var());
    if (! existing.isArray())
        return;

    juce::Array<juce::var> arr;
    for (int i = 0; i < existing.size(); ++i)
    {
        auto entry = existing[i];
        const bool sameSlot = entry.getProperty("channelId", "").toString() == channelId
                           && (int) entry.getProperty("slotIndex", 0) == slotIndex;
        if (! sameSlot)
            arr.add(entry);
    }

    asObj(root_)->setProperty("pluginSlots", arr);
    save();
}
