/*
  ==============================================================================

    VenueItem.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Venue data model implementation

  ==============================================================================
*/

#include "VenueItem.h"

//==============================================================================
juce::String VenueItem::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    // Core identity
    obj->setProperty("id",              juce::String(id));
    obj->setProperty("name",            juce::String(name));
    obj->setProperty("address",         juce::String(address));
    obj->setProperty("city",            juce::String(city));
    obj->setProperty("country",         juce::String(country));
    obj->setProperty("adminEmail",      juce::String(adminEmail));
    obj->setProperty("password",        juce::String(password));
    obj->setProperty("registrationKey", juce::String(registrationKey));

    // Venue codes
    obj->setProperty("code",     juce::String(code));
    obj->setProperty("codePlus", juce::String(codePlus));

    // Queue policy – stored as strings in Firestore to match Angular app
    obj->setProperty("numSongs",   juce::String(numSongs));
    obj->setProperty("numSingers", juce::String(numSingers));
    obj->setProperty("numStrikes", juce::String(numStrikes));
    obj->setProperty("repeatSongs",  repeatSongs);
    obj->setProperty("autoapprove",  autoapprove);

    // Display
    obj->setProperty("background",          juce::String(background));  // stored as string
    obj->setProperty("encoreBackground",    juce::String(encoreBackground));
    obj->setProperty("encodeBackgroundSize",juce::String(encodeBackgroundSize));
    obj->setProperty("showOnlineSongs",        showOnlineSongs);
    obj->setProperty("showOnlineSongsEncore",  showOnlineSongsEncore);
    obj->setProperty("showMemoryStats",        showMemoryStats);

    // Media / assets
    obj->setProperty("logoUrl",           juce::String(logoUrl));
    obj->setProperty("songBookUrl",       juce::String(songBookUrl));
    obj->setProperty("staticMapImageUrl", juce::String(staticMapImageUrl));
    obj->setProperty("venueImageUrl",     juce::String(venueImageUrl));

    // Timestamps / location
    obj->setProperty("updateSongs", (int64)updateSongs);
    obj->setProperty("dateTime",    (int64)dateTime);
    obj->setProperty("lat",         (int64)lat);
    obj->setProperty("lng",         (int64)lng);

    // Version
    obj->setProperty("taggVersion", juce::String(taggVersion));
    obj->setProperty("version",     juce::String(version));

    // Localization
    obj->setProperty("language", juce::String(language));
    obj->setProperty("timezone", juce::String(timezone));

    return juce::JSON::toString(juce::var(obj.get()));
}

//==============================================================================
VenueItem VenueItem::fromJson(const juce::String& json)
{
    VenueItem venue;

    juce::var parsedJson = juce::JSON::parse(json);
    if (!parsedJson.isObject())
        return venue;

    juce::DynamicObject* obj = parsedJson.getDynamicObject();
    if (!obj)
        return venue;

    // Helper: safely parse a field that may be stored as string OR number
    auto intProp = [&](const char* key, int def) -> int
    {
        auto v = obj->getProperty(key);
        if (v.isVoid()) return def;
        return v.toString().isEmpty() ? def : v.toString().getIntValue();
    };
    auto int64Prop = [&](const char* key, int64_t def) -> int64_t
    {
        auto v = obj->getProperty(key);
        if (v.isVoid()) return def;
        return v.toString().isEmpty() ? def : (int64_t)v.toString().getLargeIntValue();
    };
    auto boolProp = [&](const char* key, bool def) -> bool
    {
        auto v = obj->getProperty(key);
        if (v.isVoid()) return def;
        return (bool)v;
    };
    auto strProp = [&](const char* key) -> std::string
    {
        return obj->getProperty(key).toString().toStdString();
    };

    // Core identity
    venue.id              = strProp("id");
    venue.name            = strProp("name");
    venue.address         = strProp("address");
    venue.city            = strProp("city");
    venue.country         = strProp("country");
    venue.adminEmail      = strProp("adminEmail");
    venue.password        = strProp("password");
    venue.registrationKey = strProp("registrationKey");

    // Codes
    venue.code     = strProp("code");
    venue.codePlus = strProp("codePlus");

    // Queue policy
    venue.numSongs   = intProp("numSongs",   5);
    venue.numSingers = intProp("numSingers", 3);
    venue.numStrikes = intProp("numStrikes", 3);
    venue.repeatSongs  = boolProp("repeatSongs",  false);
    venue.autoapprove  = boolProp("autoapprove",  true);

    // Display
    venue.background           = intProp("background", 0);
    venue.encoreBackground     = strProp("encoreBackground");
    venue.encodeBackgroundSize = strProp("encodeBackgroundSize");
    venue.showOnlineSongs        = boolProp("showOnlineSongs",        true);
    venue.showOnlineSongsEncore  = boolProp("showOnlineSongsEncore",  false);
    venue.showMemoryStats        = boolProp("showMemoryStats",        false);

    // Media / assets
    venue.logoUrl           = strProp("logoUrl");
    venue.songBookUrl       = strProp("songBookUrl");
    venue.staticMapImageUrl = strProp("staticMapImageUrl");
    venue.venueImageUrl     = strProp("venueImageUrl");

    // Timestamps / location
    venue.updateSongs = int64Prop("updateSongs", 0);
    venue.dateTime    = int64Prop("dateTime",    0);
    venue.lat         = int64Prop("lat",        -1);
    venue.lng         = int64Prop("lng",        -1);

    // Version
    venue.taggVersion = strProp("taggVersion");
    venue.version     = strProp("version");

    // Localization
    auto lang = obj->getProperty("language").toString();
    venue.language = lang.isEmpty() ? "en_US" : lang.toStdString();
    auto tz = obj->getProperty("timezone").toString();
    venue.timezone = tz.isEmpty() ? "UTC" : tz.toStdString();

    return venue;
}

//==============================================================================
bool VenueItem::isValid() const
{
    return !id.empty() && !name.empty() && !code.empty();
}

//==============================================================================
juce::String VenueItem::getLocalizedName() const
{
    // For now, return the standard name
    // TODO: Implement localization lookup when LocalizationManager is ready
    return juce::String(name);
}