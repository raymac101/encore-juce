/*
  ==============================================================================

    CdgSong.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    CDG Song model implementation

  ==============================================================================
*/

#include "CdgSong.h"
#include <algorithm>

//==============================================================================
static juce::Array<juce::var> strVecToVar(const std::vector<std::string>& v)
{
    juce::Array<juce::var> a;
    for (auto& s : v) a.add(juce::String(s));
    return a;
}

static std::vector<std::string> varToStrVec(const juce::var& v)
{
    std::vector<std::string> r;
    if (v.isArray())
        for (int i = 0; i < v.size(); ++i)
            r.push_back(v[i].toString().toStdString());
    return r;
}

static juce::Array<juce::var> dblVecToVar(const std::vector<double>& v)
{
    juce::Array<juce::var> a;
    for (auto& n : v) a.add(n);
    return a;
}

static std::vector<double> varToDblVec(const juce::var& v)
{
    std::vector<double> r;
    if (v.isArray())
        for (int i = 0; i < v.size(); ++i)
            r.push_back((double)v[i]);
    return r;
}

//==============================================================================
juce::String CdgSong::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    obj->setProperty("id", juce::String(id));
    obj->setProperty("imageUrl", juce::String(imageUrl));
    obj->setProperty("durationMS", durationMS);
    obj->setProperty("durationVerified", durationVerified);
    obj->setProperty("keySignature", juce::String(keySignature));
    obj->setProperty("tempo", tempo);
    obj->setProperty("songName", juce::String(songName));
    obj->setProperty("artistName", juce::String(artistName));
    obj->setProperty("fullPath", juce::var(strVecToVar(fullPath)));
    obj->setProperty("fileName", juce::var(strVecToVar(fileName)));
    obj->setProperty("filePath", juce::var(strVecToVar(filePath)));
    obj->setProperty("fileType", juce::var(strVecToVar(fileType)));
    obj->setProperty("fileDate",  (juce::int64)fileDate);
    obj->setProperty("fileSize",  (juce::int64)fileSize);
    obj->setProperty("addedAt",   (juce::int64)addedAt);
    obj->setProperty("releaseDate", juce::String(releaseDate));
    obj->setProperty("genres", juce::var(strVecToVar(genres)));
    obj->setProperty("version", juce::var(strVecToVar(version)));
    obj->setProperty("code", juce::var(strVecToVar(code)));
    obj->setProperty("rating", juce::var(dblVecToVar(rating)));

    return juce::JSON::toString(juce::var(obj.get()));
}

//==============================================================================
CdgSong CdgSong::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

CdgSong CdgSong::fromJsonObject(juce::DynamicObject* obj)
{
    CdgSong s;
    if (obj == nullptr) return s;

    s.id           = obj->getProperty("id").toString().toStdString();
    s.imageUrl     = obj->getProperty("imageUrl").toString().toStdString();
    s.durationMS   = (int)obj->getProperty("durationMS");
    s.durationVerified = (bool)obj->getProperty("durationVerified");
    s.keySignature = obj->getProperty("keySignature").toString().toStdString();
    s.tempo        = (double)obj->getProperty("tempo");
    s.songName     = obj->getProperty("songName").toString().toStdString();
    s.artistName   = obj->getProperty("artistName").toString().toStdString();
    s.fullPath     = varToStrVec(obj->getProperty("fullPath"));
    s.fileName     = varToStrVec(obj->getProperty("fileName"));
    s.filePath     = varToStrVec(obj->getProperty("filePath"));
    s.fileType     = varToStrVec(obj->getProperty("fileType"));
    s.fileDate     = (int64_t)obj->getProperty("fileDate");
    s.fileSize     = (int64_t)obj->getProperty("fileSize");
    s.addedAt      = (int64_t)obj->getProperty("addedAt");
    s.releaseDate  = obj->getProperty("releaseDate").toString().toStdString();
    s.genres       = varToStrVec(obj->getProperty("genres"));
    s.version      = varToStrVec(obj->getProperty("version"));
    s.code         = varToStrVec(obj->getProperty("code"));
    s.rating       = varToDblVec(obj->getProperty("rating"));

    return s;
}

//==============================================================================
bool CdgSong::isValid() const
{
    return !id.empty() && !songName.empty() && !artistName.empty();
}

bool CdgSong::hasMetadata() const
{
    return !imageUrl.empty()
        && durationMS > 0
        && !releaseDate.empty();
}

juce::String CdgSong::getFormattedDuration() const
{
    int totalSeconds = durationMS / 1000;
    return juce::String::formatted("%d:%02d", totalSeconds / 60, totalSeconds % 60);
}

std::vector<int> CdgSong::getVersionIndicesByRating() const
{
    std::vector<int> indices (version.size());
    for (size_t i = 0; i < indices.size(); ++i)
        indices[i] = (int) i;

    std::stable_sort (indices.begin(), indices.end(), [this] (int a, int b)
    {
        const double ra = (a >= 0 && (size_t) a < rating.size()) ? rating[(size_t) a] : 0.0;
        const double rb = (b >= 0 && (size_t) b < rating.size()) ? rating[(size_t) b] : 0.0;
        return ra > rb; // descending -- highest-rated first
    });

    return indices;
}

juce::String CdgSong::getVersionLabel (int versionIndex) const
{
    if (versionIndex < 0 || (size_t) versionIndex >= version.size())
        return {};

    const juce::String codeStr = (size_t) versionIndex < code.size()
        ? juce::String (code[(size_t) versionIndex]) : juce::String();
    const juce::String verStr = juce::String (version[(size_t) versionIndex]);

    juce::String label = codeStr.isNotEmpty() && verStr.isNotEmpty() ? codeStr + " - " + verStr
                        : (codeStr.isNotEmpty() ? codeStr : verStr);
    if (label.isEmpty())
        label = "Version " + juce::String (versionIndex + 1);

    if ((size_t) versionIndex < rating.size() && rating[(size_t) versionIndex] > 0.0)
        label += juce::String::formatted (" (%.1f)", rating[(size_t) versionIndex]);

    return label;
}
