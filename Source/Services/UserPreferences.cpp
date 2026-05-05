/*
  ==============================================================================

    UserPreferences.cpp

  ==============================================================================
*/

#include "UserPreferences.h"

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
