/*
  ==============================================================================

    ApiService.cpp

  ==============================================================================
*/

#include "ApiService.h"

namespace
{
    //--- Vendor disc-code suffixes stripped by fixBrackets (mirror of the
    //    Angular if/else ladder). Listed once so we only loop the array.
    const char* const kBracketCodes[] = {
        "(Ask)", "(Auz Nz)", "(Bs)", "(Bc)", "(Cb)", "(CB)", "(Dg)", "(Dk)",
        "(Dis)", "(DKM)", "(Dkm)", "(dkm)", "(Dm)", "(EZH)", "(Gm)", "(Hspk)",
        "(Kar)", "(Kjt)", "(KK)", "(Kk)", "(kk)", "(KV)", "(Qh)", "(qh)",
        "(Sbi)", "(sbi)", "(Lbl)", "(lbl)", "(Mm)", "(Pr)", "(Llc)", "(Sk)",
        "(Leg)", "(Pi)", "(Mh)", "(MHR)", "(Par)", "(Rsz)", "(Sav)", "(Sc)",
        "(Scs)", "(Sdp)", "(Sdc)", "(Sfmw)", "(Sgb)", "(Sfpldu)", "(SF)",
        "(Sfg)", "(Thm)", "(Tt)", "(Mre)", "(Phm)", "(PHM)", "(Sf)", "(Ss)",
        "(SS)", "(Syb)", "(Wbgv)", "(Zoom)", "(1)", "(2)"
    };

    //--- Firestore-style "value" wrapper helpers. The cloud function returns
    //    plain JSON (not Firestore), so these are the simple reader helpers.
    juce::var getProp(const juce::var& obj, const char* key)
    {
        return obj.getProperty(juce::Identifier(key), juce::var());
    }

    juce::String getString(const juce::var& obj, const char* key)
    {
        auto v = getProp(obj, key);
        if (v.isString())  return v.toString();
        if (v.isInt() || v.isInt64() || v.isDouble())
            return v.toString();
        return {};
    }

    int getInt(const juce::var& obj, const char* key)
    {
        auto v = getProp(obj, key);
        if (v.isInt() || v.isInt64()) return (int) v;
        if (v.isDouble())             return (int) (double) v;
        if (v.isString())             return v.toString().getIntValue();
        return 0;
    }

    double getDouble(const juce::var& obj, const char* key)
    {
        auto v = getProp(obj, key);
        if (v.isDouble())             return (double) v;
        if (v.isInt() || v.isInt64()) return (double) (int64_t) v;
        if (v.isString())             return v.toString().getDoubleValue();
        return 0.0;
    }

    //--- Walk a chain of object → property → object → property safely.
    juce::var path(juce::var v, std::initializer_list<const char*> keys)
    {
        for (auto* k : keys)
        {
            if (! v.isObject()) return {};
            v = v.getProperty(juce::Identifier(k), juce::var());
        }
        return v;
    }

    //--- Pull the first array element if `v` is an array, else return v.
    juce::var firstElement(juce::var v)
    {
        if (auto* a = v.getArray())
            return a->isEmpty() ? juce::var() : a->getFirst();
        return v;
    }

    //--- Convert an arrayValue of strings to std::vector<std::string>.
    std::vector<std::string> arrayToStrings(const juce::var& v)
    {
        std::vector<std::string> out;
        if (auto* a = v.getArray())
        {
            out.reserve((size_t) a->size());
            for (auto& e : *a)
                if (e.isString()) out.push_back(e.toString().toStdString());
        }
        return out;
    }
}

//==============================================================================
ApiService& ApiService::getInstance()
{
    static ApiService instance;
    return instance;
}

ApiService::ApiService() = default;

//==============================================================================
// fixBrackets — single-pass replacement, returns the cleaned string.
juce::String ApiService::fixBrackets(const juce::String& s)
{
    if (s.isEmpty()) return s;

    juce::String out = s;
    for (auto* code : kBracketCodes)
    {
        if (out.contains(code))
        {
            out = out.replace(code, juce::String());
            break;  // Angular does only one replacement per call
        }
    }
    return out;
}

//==============================================================================
// getKeySignature — convert "<key>:<mode>" (Spotify integer keys) to a
// readable label.  The Angular code intentionally appends " minor" only for
// mode==0 (Spotify uses 1=major, 0=minor).
juce::String ApiService::getKeySignature(const juce::String& spotifyKeyMode)
{
    if (spotifyKeyMode.isEmpty()) return {};

    auto colon = spotifyKeyMode.indexOfChar(':');
    juce::String keyTok  = colon < 0 ? spotifyKeyMode : spotifyKeyMode.substring(0, colon);
    juce::String modeTok = colon < 0 ? juce::String() : spotifyKeyMode.substring(colon + 1);

    juce::String letter;
    switch (keyTok.getIntValue())
    {
        case 0:  letter = "C";  break;
        case 1:  letter = "C#"; break;
        case 2:  letter = "D";  break;
        case 3:  letter = "Eb"; break;
        case 4:  letter = "E";  break;
        case 5:  letter = "F";  break;
        case 6:  letter = "F#"; break;
        case 7:  letter = "G";  break;
        case 8:  letter = "Ab"; break;
        case 9:  letter = "A";  break;
        case 10: letter = "Bb"; break;
        case 11: letter = "B";  break;
        default: return {};
    }
    if (modeTok == "0") letter += " minor";
    return letter;
}

//==============================================================================
juce::String ApiService::makeCacheKey(const juce::String& artist,
                                      const juce::String& song)
{
    auto clean = [](juce::String x)
    {
        x = x.toLowerCase().trim();
        // Keep alphanumerics and spaces; drop everything else
        juce::String out;
        out.preallocateBytes((size_t) x.length());
        for (auto c : x)
        {
            if (juce::CharacterFunctions::isLetterOrDigit(c) || c == ' ')
                out += juce::String::charToString(c);
        }
        return out.trim();
    };
    return clean(artist) + "|" + clean(song);
}

//==============================================================================
juce::File ApiService::getSharedMetadataFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
              .getChildFile("EncoreKaraoke")
              .getChildFile("shared_metadata.json");
}

juce::var ApiService::getSharedMetadata(const juce::String& artist,
                                        const juce::String& song)
{
    auto key = makeCacheKey(artist, song);
    if (key == "|") return {};

    const juce::ScopedLock sl(cacheLock_);
    auto file = getSharedMetadataFile();
    if (! file.existsAsFile()) return {};

    auto parsed = juce::JSON::parse(file);
    if (! parsed.isObject()) return {};
    return parsed.getProperty(juce::Identifier(key), juce::var());
}

bool ApiService::saveSharedMetadata(const CdgSong& song)
{
    auto key = makeCacheKey(juce::String(song.artistName),
                            juce::String(song.songName));
    if (key == "|") return false;

    const juce::ScopedLock sl(cacheLock_);
    auto file = getSharedMetadataFile();
    file.getParentDirectory().createDirectory();

    juce::var existing;
    if (file.existsAsFile())
        existing = juce::JSON::parse(file);

    auto* root = existing.isObject()
                     ? existing.getDynamicObject()
                     : (existing = juce::var(new juce::DynamicObject()),
                        existing.getDynamicObject());

    // Build the entry — only include non-empty fields so a fresh API hit can
    // overwrite individual blanks without nuking previously-cached values.
    juce::DynamicObject::Ptr entry = new juce::DynamicObject();
    if (! song.imageUrl.empty())     entry->setProperty("imageUrl",     juce::String(song.imageUrl));
    if (song.durationMS > 0)         entry->setProperty("durationMS",   song.durationMS);
    if (! song.keySignature.empty()) entry->setProperty("keySignature", juce::String(song.keySignature));
    if (song.tempo > 0.0)            entry->setProperty("tempo",        song.tempo);
    if (! song.songName.empty())     entry->setProperty("songName",     juce::String(song.songName));
    if (! song.artistName.empty())   entry->setProperty("artistName",   juce::String(song.artistName));
    if (! song.releaseDate.empty())  entry->setProperty("releaseDate",  juce::String(song.releaseDate));

    if (! song.genres.empty())
    {
        juce::Array<juce::var> arr;
        for (auto& g : song.genres) arr.add(juce::String(g));
        entry->setProperty("genres", juce::var(arr));
    }
    if (! song.version.empty())
    {
        juce::Array<juce::var> arr;
        for (auto& v : song.version) arr.add(juce::String(v));
        entry->setProperty("version", juce::var(arr));
    }
    entry->setProperty("cachedAt", juce::Time::getCurrentTime().toMilliseconds());

    root->setProperty(juce::Identifier(key), juce::var(entry.get()));

    auto json = juce::JSON::toString(existing, true);
    return file.replaceWithText(json);
}

//==============================================================================
ApiService::Result ApiService::tryCachedLookup(const CdgSong& currentSong,
                                               const juce::String& artist,
                                               const juce::String& song)
{
    Result r;
    auto entry = getSharedMetadata(artist, song);
    if (! entry.isObject())
        return r;

    CdgSong out = currentSong;

    auto img = getString(entry, "imageUrl");
    if (img.isNotEmpty()) out.imageUrl = img.toStdString();

    auto dur = getInt(entry, "durationMS");
    if (dur > 0)          out.durationMS = dur;

    auto key = getString(entry, "keySignature");
    if (key.isNotEmpty()) out.keySignature = key.toStdString();

    auto tempo = getDouble(entry, "tempo");
    if (tempo > 0.0)      out.tempo = tempo;

    auto sn = getString(entry, "songName");
    if (sn.isNotEmpty())  out.songName = sn.toStdString();

    auto an = getString(entry, "artistName");
    if (an.isNotEmpty())  out.artistName = an.toStdString();

    auto rd = getString(entry, "releaseDate");
    if (rd.isNotEmpty())  out.releaseDate = rd.toStdString();

    auto gs = arrayToStrings(getProp(entry, "genres"));
    if (! gs.empty())     out.genres = gs;

    auto vs = arrayToStrings(getProp(entry, "version"));
    if (! vs.empty())     out.version = vs;

    r.ok = true;
    r.fromCache = true;
    r.song = std::move(out);
    return r;
}

//==============================================================================
// Build the URL, hit the cloud function, parse the Spotify-shaped JSON, and
// fold the relevant fields into a copy of `currentSong`.
ApiService::Result ApiService::doSpotifyApiCall(const CdgSong& currentSong,
                                                const juce::String& artist,
                                                const juce::String& song)
{
    Result r;

    // Strip vendor brackets, then keep alphanumerics + spaces in lowercase
    // (matches the Angular regex).
    auto artistChecked = fixBrackets(artist);
    auto songChecked   = fixBrackets(song);

    auto stripPunctLower = [](const juce::String& s)
    {
        juce::String out;
        out.preallocateBytes((size_t) s.length());
        for (auto c : s)
        {
            if (juce::CharacterFunctions::isLetterOrDigit(c) || c == ' ')
                out += juce::String::charToString(c);
        }
        return out.toLowerCase();
    };

    auto artistFilter = stripPunctLower(artistChecked);
    auto songFilter   = stripPunctLower(songChecked);

    if (artistFilter == "acdc")
        artistFilter = "ac-dc";

    // URL-encode each path segment (handles spaces, accents, …)
    auto urlEncode = [](const juce::String& s) {
        return juce::URL::addEscapeChars(s, /*isParameter*/ false);
    };

    juce::String url = taggApiUrl_ + "searchArtistAndSong/"
                                   + urlEncode(artistFilter) + "/"
                                   + urlEncode(songFilter);

    DBG ("[API] GET " << url);

    juce::URL juceUrl(url);

    int statusCode = 0;
    juce::StringPairArray responseHeaders;

    auto stream = juceUrl.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(timeoutMs_)
            .withExtraHeaders("Content-Type: application/json\r\n"
                              "Accept: application/json\r\n"
                              "Authorization: Bearer " + bearerToken_)
            .withStatusCode(&statusCode)
            .withResponseHeaders(&responseHeaders));
    if (stream == nullptr)
    {
        r.errorMessage = "Could not connect to metadata service.";
        return r;
    }

    juce::String body = stream->readEntireStreamAsString();

    if (statusCode < 200 || statusCode >= 300)
    {
        r.errorMessage = "API HTTP " + juce::String(statusCode)
                       + (body.isNotEmpty() ? (" — " + body.substring(0, 200)) : juce::String());
        return r;
    }

    if (body.startsWith("Error"))
    {
        r.errorMessage = "Could not find info for '" + artist + " — " + song + "'.";
        return r;
    }

    auto parsed = juce::JSON::parse(body);
    if (! parsed.isObject())
    {
        r.errorMessage = "Malformed metadata response.";
        return r;
    }

    // The cloud function wraps the Spotify payload in `data` (mirror Angular).
    auto data = getProp(parsed, "data");
    if (! data.isObject())
        data = parsed;

    CdgSong out = currentSong;

    // Album image — search-result first, then artist fallback.
    auto firstTrack = firstElement(path(data, {"searchResult", "tracks", "items"}));
    juce::String imageUrl;
    {
        auto images = path(firstTrack, {"album", "images"});
        auto firstImg = firstElement(images);
        imageUrl = getString(firstImg, "url");
    }
    if (imageUrl.isEmpty())
    {
        auto artistImages = path(data, {"artist", "images"});
        auto firstImg = firstElement(artistImages);
        imageUrl = getString(firstImg, "url");
    }
    if (imageUrl.isNotEmpty())
        out.imageUrl = imageUrl.toStdString();

    // Duration
    int durationMS = currentSong.durationMS;
    auto trackDur = getInt(getProp(data, "track"), "duration_ms");
    if (trackDur > 0)
    {
        durationMS = trackDur;
    }
    else
    {
        auto srDur = getInt(firstTrack, "duration_ms");
        if (srDur > 0) durationMS = srDur;
    }

    // Song name
    auto trackName = getString(getProp(data, "track"), "name");
    if (trackName.isNotEmpty())
    {
        out.songName = trackName.toStdString();
    }
    else
    {
        auto srName = getString(firstTrack, "name");
        if (srName.isNotEmpty()) out.songName = srName.toStdString();
    }

    // Artist name
    auto trackArtists = getProp(getProp(data, "track"), "artists");
    auto firstTrackArtist = firstElement(trackArtists);
    auto trackArtistName = getString(firstTrackArtist, "name");
    if (trackArtistName.isNotEmpty())
    {
        out.artistName = trackArtistName.toStdString();
    }
    else
    {
        auto artistObjName = getString(getProp(data, "artist"), "name");
        if (artistObjName.isNotEmpty())
        {
            out.artistName = artistObjName.toStdString();
        }
        else
        {
            auto srArtists = getProp(firstTrack, "artists");
            auto firstSrArtist = firstElement(srArtists);
            auto srArtistName = getString(firstSrArtist, "name");
            if (srArtistName.isNotEmpty()) out.artistName = srArtistName.toStdString();
        }
    }

    // Release date
    auto trackRelease = getString(path(data, {"track", "album"}), "release_date");
    if (trackRelease.isNotEmpty())
    {
        out.releaseDate = trackRelease.toStdString();
    }
    else
    {
        auto srRelease = getString(getProp(firstTrack, "album"), "release_date");
        if (srRelease.isNotEmpty()) out.releaseDate = srRelease.toStdString();
    }

    // Genres
    auto genreArr = arrayToStrings(getProp(getProp(data, "artist"), "genres"));
    if (! genreArr.empty()) out.genres = std::move(genreArr);

    // Popularity → rating[0]
    auto popularity = getInt(getProp(data, "track"), "popularity");
    if (popularity == 0)
        popularity = getInt(firstTrack, "popularity");
    if (popularity > 0)
        out.rating = { (double) popularity };

    // Audio features → keySignature + tempo
    auto af = getProp(data, "audioFeatures");
    if (af.isObject())
    {
        auto keyRaw  = getProp(af, "key");
        auto modeRaw = getProp(af, "mode");
        if (! keyRaw.isVoid())
        {
            juce::String packed = keyRaw.toString() + ":" + modeRaw.toString();
            auto pretty = getKeySignature(packed);
            if (pretty.isNotEmpty()) out.keySignature = pretty.toStdString();
        }
        auto tempo = getDouble(af, "tempo");
        if (tempo > 0.0) out.tempo = tempo;
    }

    // Match Angular's "if durationMS > 1000, treat as ms and convert to s".
    // The cloud function returns Spotify's milliseconds; CdgSong.durationMS in
    // this codebase is stored in MILLISECONDS, so we keep ms as-is.
    out.durationMS = durationMS;

    // Round tempo
    if (out.tempo > 0.0)
        out.tempo = std::round(out.tempo);

    r.ok = true;
    r.song = std::move(out);
    return r;
}

//==============================================================================
void ApiService::searchArtistAndSong(const CdgSong& currentSong,
                                     const juce::String& artist,
                                     const juce::String& song,
                                     Callback onDone)
{
    if (artist.trim().isEmpty() || song.trim().isEmpty())
    {
        Result r;
        r.errorMessage = "Artist and song name are required.";
        if (onDone)
            juce::MessageManager::callAsync([onDone, r]() { onDone(r); });
        return;
    }

    juce::Thread::launch([this, currentSong, artist, song, onDone]()
    {
        // 1) Cache lookup.
        Result r = tryCachedLookup(currentSong, artist, song);
        if (r.ok)
        {
            DBG ("[API] cache hit: " << artist << " - " << song);
            if (onDone)
                juce::MessageManager::callAsync([onDone, r]() { onDone(r); });
            return;
        }

        // 2) Cloud function call.
        r = doSpotifyApiCall(currentSong, artist, song);

        if (r.ok)
        {
            // Persist successful API result to the shared cache.
            saveSharedMetadata(r.song);
        }

        if (onDone)
            juce::MessageManager::callAsync([onDone, r]() { onDone(r); });
    });
}
