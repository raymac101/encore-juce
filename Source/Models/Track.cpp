/*
  ==============================================================================

    Track.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Track data model implementation

  ==============================================================================
*/

#include "Track.h"

//==============================================================================
static juce::Array<juce::var> stringVectorToVarArray(const std::vector<std::string>& vec)
{
    juce::Array<juce::var> arr;
    for (auto& s : vec)
        arr.add(juce::String(s));
    return arr;
}

static std::vector<std::string> varArrayToStringVector(const juce::var& v)
{
    std::vector<std::string> vec;
    if (v.isArray())
        for (int i = 0; i < v.size(); ++i)
            vec.push_back(v[i].toString().toStdString());
    return vec;
}

//==============================================================================
juce::String Track::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    obj->setProperty("id", juce::String(id));
    obj->setProperty("name", juce::String(name));
    obj->setProperty("artists", juce::String(artists));
    obj->setProperty("album", juce::String(album));

    obj->setProperty("appleUrl", juce::String(appleUrl));
    obj->setProperty("spotifyUrl", juce::String(spotifyUrl));
    obj->setProperty("youtubeUrl", juce::String(youtubeUrl));
    obj->setProperty("kwiUrl", juce::String(kwiUrl));
    obj->setProperty("href", juce::String(href));
    obj->setProperty("previewUrl", juce::String(previewUrl));

    obj->setProperty("imageUrl", juce::String(imageUrl));
    obj->setProperty("musicUrl", juce::String(musicUrl));
    obj->setProperty("lyricUrl", juce::String(lyricUrl));

    obj->setProperty("keySignature", juce::String(keySignature));
    obj->setProperty("releaseDate", juce::String(releaseDate));
    obj->setProperty("type", juce::String(type));
    obj->setProperty("gender", juce::String(gender));
    obj->setProperty("musicLabel", juce::String(musicLabel));
    obj->setProperty("genres", juce::var(stringVectorToVarArray(genres)));
    obj->setProperty("availableMarkets", juce::var(stringVectorToVarArray(availableMarkets)));

    obj->setProperty("discNumber", discNumber);
    obj->setProperty("durationMS", durationMS);
    obj->setProperty("trackNumber", trackNumber);
    obj->setProperty("tempo", tempo);
    obj->setProperty("popularity", popularity);
    obj->setProperty("quality", quality);
    obj->setProperty("key", key);
    obj->setProperty("mode", mode);
    obj->setProperty("timeSignature", timeSignature);

    obj->setProperty("explicit", isExplicit);
    obj->setProperty("isPlayable", isPlayable);

    obj->setProperty("isrc", juce::String(isrc));
    obj->setProperty("iswc", juce::String(iswc));

    obj->setProperty("performanceRO", juce::String(performanceRO));
    obj->setProperty("publisherMechanical", juce::String(publisherMechanical));
    obj->setProperty("publisherSync", juce::String(publisherSync));
    obj->setProperty("restrictions", juce::String(restrictions));

    obj->setProperty("acousticness", acousticness);
    obj->setProperty("danceability", danceability);
    obj->setProperty("energy", energy);
    obj->setProperty("instrumentalness", instrumentalness);
    obj->setProperty("liveness", liveness);
    obj->setProperty("loudness", loudness);
    obj->setProperty("speechiness", speechiness);
    obj->setProperty("valence", valence);

    obj->setProperty("difficulty", difficulty);

    return juce::JSON::toString(juce::var(obj.get()));
}

//==============================================================================
Track Track::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

Track Track::fromJsonObject(juce::DynamicObject* obj)
{
    Track t;
    if (obj == nullptr) return t;

    t.id                  = obj->getProperty("id").toString().toStdString();
    t.name                = obj->getProperty("name").toString().toStdString();
    t.artists             = obj->getProperty("artists").toString().toStdString();
    t.album               = obj->getProperty("album").toString().toStdString();

    t.appleUrl            = obj->getProperty("appleUrl").toString().toStdString();
    t.spotifyUrl          = obj->getProperty("spotifyUrl").toString().toStdString();
    t.youtubeUrl          = obj->getProperty("youtubeUrl").toString().toStdString();
    t.kwiUrl              = obj->getProperty("kwiUrl").toString().toStdString();
    t.href                = obj->getProperty("href").toString().toStdString();
    t.previewUrl          = obj->getProperty("previewUrl").toString().toStdString();

    t.imageUrl            = obj->getProperty("imageUrl").toString().toStdString();
    t.musicUrl            = obj->getProperty("musicUrl").toString().toStdString();
    t.lyricUrl            = obj->getProperty("lyricUrl").toString().toStdString();

    t.keySignature        = obj->getProperty("keySignature").toString().toStdString();
    t.releaseDate         = obj->getProperty("releaseDate").toString().toStdString();
    t.type                = obj->getProperty("type").toString().toStdString();
    t.gender              = obj->getProperty("gender").toString().toStdString();
    t.musicLabel          = obj->getProperty("musicLabel").toString().toStdString();
    t.genres              = varArrayToStringVector(obj->getProperty("genres"));
    t.availableMarkets    = varArrayToStringVector(obj->getProperty("availableMarkets"));

    t.discNumber          = (int)obj->getProperty("discNumber");
    t.durationMS          = (int)obj->getProperty("durationMS");
    t.trackNumber         = (int)obj->getProperty("trackNumber");
    t.tempo               = (double)obj->getProperty("tempo");
    t.popularity          = (int)obj->getProperty("popularity");
    t.quality             = (int)obj->getProperty("quality");
    t.key                 = (int)obj->getProperty("key");
    t.mode                = (int)obj->getProperty("mode");
    t.timeSignature       = (int)obj->getProperty("timeSignature");

    t.isExplicit          = (bool)obj->getProperty("explicit");
    t.isPlayable          = (bool)obj->getProperty("isPlayable");

    t.isrc                = obj->getProperty("isrc").toString().toStdString();
    t.iswc                = obj->getProperty("iswc").toString().toStdString();

    t.performanceRO       = obj->getProperty("performanceRO").toString().toStdString();
    t.publisherMechanical = obj->getProperty("publisherMechanical").toString().toStdString();
    t.publisherSync       = obj->getProperty("publisherSync").toString().toStdString();
    t.restrictions        = obj->getProperty("restrictions").toString().toStdString();

    t.acousticness        = (double)obj->getProperty("acousticness");
    t.danceability        = (double)obj->getProperty("danceability");
    t.energy              = (double)obj->getProperty("energy");
    t.instrumentalness    = (double)obj->getProperty("instrumentalness");
    t.liveness            = (double)obj->getProperty("liveness");
    t.loudness            = (double)obj->getProperty("loudness");
    t.speechiness         = (double)obj->getProperty("speechiness");
    t.valence             = (double)obj->getProperty("valence");

    t.difficulty          = (double)obj->getProperty("difficulty");

    return t;
}

//==============================================================================
bool Track::isValid() const
{
    return !id.empty() && !name.empty() && !artists.empty();
}

//==============================================================================
juce::String Track::getFormattedDuration() const
{
    int totalSeconds = durationMS / 1000;
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    return juce::String::formatted("%d:%02d", minutes, seconds);
}
