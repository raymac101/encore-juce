/*
  ==============================================================================

    LibraryScanner.cpp
    Created: 22 Apr 2026
    Author:  GitHub Copilot

  ==============================================================================
*/

#include "LibraryScanner.h"
#include <algorithm>
#include <cctype>

//==============================================================================
// Helpers
//==============================================================================
namespace
{
    // Known audio-companion extensions for CDG files
    static const juce::StringArray kAudioExts  { "mp3", "wav", "ogg", "flac", "aac" };
    // Karaoke file extensions that we count and index
    static const juce::StringArray kVideoExts  { "mp4", "m4a" };
    static const juce::StringArray kCdgExts    { "cdg" };
    static const juce::StringArray kZipExts    { "zip" };
    static const juce::StringArray kXmlExts    { "xml" };

    bool isAudioCompanion(const juce::String& ext)
    {
        return kAudioExts.contains(ext.toLowerCase());
    }

    // Returns the "type bucket" for a file extension: cdg/zip/mp4/m4a/xml/audio/unknown
    juce::String bucketForExt(const juce::String& ext)
    {
        juce::String e = ext.toLowerCase();
        if (kCdgExts.contains(e))   return "cdg";
        if (kZipExts.contains(e))   return "zip";
        if (e == "mp4")             return "mp4";
        if (e == "m4a")             return "m4a";
        if (kXmlExts.contains(e))   return "xml";
        if (isAudioCompanion(e))    return "audio";
        return "unknown";
    }

    // Karaoke-relevant extensions (everything except pure audio companions)
    bool isKaraoke(const juce::File& f)
    {
        juce::String e = f.getFileExtension().trimCharactersAtStart(".").toLowerCase();
        return kCdgExts.contains(e) || kZipExts.contains(e) ||
               kVideoExts.contains(e) || kXmlExts.contains(e) ||
               isAudioCompanion(e);
    }
}

//==============================================================================
LibraryScanner::LibraryScanner()
    : juce::Thread("LibraryScanner")
{}

LibraryScanner::~LibraryScanner()
{
    stopScan();
}

//==============================================================================
// static helpers
//==============================================================================

/** Normalise artist + song into a lowercase, punctuation-stripped key so that
    vendor variants of the same song map to a single entry. */
juce::String LibraryScanner::normaliseSongKey(const juce::String& artist,
                                               const juce::String& song)
{
    auto normalise = [](juce::String s) -> juce::String
    {
        s = s.toLowerCase().trim();

        // Normalise "The" article so all three forms produce the same key:
        //   "The House of the Rising Sun"
        //   "House of the Rising Sun"
        //   "House of the Rising Sun, The"
        // → all become "house of the rising sun"

        // Strip trailing ", the" (with optional space variants)
        if (s.endsWith(", the"))
            s = s.dropLastCharacters(5).trim();
        else if (s.endsWith(",the"))
            s = s.dropLastCharacters(4).trim();

        // Strip leading "the "
        if (s.startsWith("the "))
            s = s.substring(4).trim();

        // Keep only alphanumeric + single spaces
        juce::String r;
        for (int i = 0; i < s.length(); ++i)
        {
            auto c = s[i];
            if (juce::CharacterFunctions::isLetterOrDigit(c))
                r += c;
            else if (c == ' ' && !r.isEmpty() && r[r.length() - 1] != ' ')
                r += ' ';
        }
        r = r.trim();

        // Normalise contracted "-in'" forms to "-ing" so that
        // "Believin'", "Believing", "Believin" all produce the same key.
        // We scan word-by-word and expand any word ending in "in" to "ing".
        {
            juce::StringArray words;
            words.addTokens(r, " ", "");
            for (auto& w : words)
                if (w.endsWith("in") && w.length() > 2)
                    w += "g";
            r = words.joinIntoString(" ");
        }

        return r;
    };

    // Artist alias map — maps any alias to the canonical normalised form.
    // Keys and values must already be in the normalised form (lowercase,
    // alphanumeric + spaces only, "the" stripped, "-in" → "-ing" applied).
    static const struct { const char* alias; const char* canonical; } kArtistAliases[] = {
        { "jon bon jovi",          "bon jovi"          },
        { "john bon jovi",         "bon jovi"          },
        { "ac dc",                 "acdc"               },
        { "the rolling stones",    "rolling stones"     },
        { nullptr, nullptr }
    };

    juce::String normArtist = normalise(artist);
    for (int i = 0; kArtistAliases[i].alias != nullptr; ++i)
        if (normArtist == kArtistAliases[i].alias)
        { normArtist = kArtistAliases[i].canonical; break; }

    return normArtist + "|" + normalise(song);
}

/** Map a catalog disc-code (e.g. "SC1234" or "Leg12345") to a human-readable
    vendor name.  Synced with findVersion() in karaoke-parser.js. */
juce::String LibraryScanner::vendorVersionFromCode(const juce::String& code)
{
    if (code.isEmpty()) return {};

    // Extract the full alphabetic prefix regardless of case
    // e.g. "SC1234"->"sc", "Leg12345"->"leg", "SFMW001"->"sfmw"
    juce::String prefix;
    for (int i = 0; i < code.length(); ++i)
    {
        juce::juce_wchar c = code[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            prefix += c;
        else
            break;
    }
    prefix = prefix.toLowerCase();

    // Ordered longest-first so e.g. "sfpldu" matches before "sf"
    static const struct { const char* key; const char* name; } kMap[] = {
        {"sfpldu", "SunFly"},
        {"zpcp",   "Zoom Platinum Country & Pop"},
        {"hspk",   "Hot Stuff Pack"},
        {"sfmw",   "SunFly"},
        {"sfmz",   "SunFly"},
        {"dkm",    "DAIICHIKOSHO"},
        {"llc",    "Legends Lost Classics"},
        {"mhr",    "Mr. Entertainer Hits"},
        {"ask",    "All Star Karaoke"},
        {"lbl",    "Legend Baseline"},
        {"phm",    "Pop Hits Monthly"},
        {"rsv",    "Radio Stars"},
        {"rsz",    "Radio Stars"},
        {"sav",    "BMB"},
        {"sbi",    "SBI Karaoke"},
        {"scs",    "Sound Choice"},
        {"sdc",    "Star Disc"},
        {"sdp",    "Star Disc Pop"},
        {"sgb",    "Sweet Georgia Brown"},
        {"thm",    "Top Hits Monthly"},
        {"kjt",    "KJ Tools"},
        {"ezh",    "Easy Karaoke"},
        {"sfg",    "SunFly"},
        {"ac",     "All Country"},
        {"bc",     "No Name"},
        {"bs",     "Back Stage"},
        {"cb",     "Chartbuster"},
        {"dg",     "Dangerous"},
        {"dis",    "Disney"},
        {"dk",     "DAIICHIKOSHO"},
        {"dm",     "Doctor Music"},
        {"gm",     "Gamesman Can/Con"},
        {"kar",    "Karaoke Can/Con"},
        {"kk",     "Karaoke Kurrents"},
        {"kv",     "Karaoke Version"},
        {"leg",    "Legends"},
        {"mh",     "Monster Hit"},
        {"mm",     "Music Maestro"},
        {"par",    "Parody"},
        {"phm",    "Pop Hits Monthly"},
        {"pi",     "Pioneer"},
        {"pr",     "Pridis Music"},
        {"qh",     "Quick Hits"},
        {"sc",     "Sound Choice"},
        {"sf",     "SunFly"},
        {"sk",     "Sing King"},
        {"ss",     "Singer Solution"},
        {"tt",     "Top Tunes"},
        {"zp",     "Zoom Platinum"},
        {nullptr,  nullptr}
    };

    for (int i = 0; kMap[i].key != nullptr; ++i)
        if (prefix == kMap[i].key)
            return kMap[i].name;
    return {};
}

/** Return a quality rating 0.0–5.0 for a vendor catalog disc-code.
    Synced with getRating() in karaoke-parser.js. */
double LibraryScanner::ratingForCode(const juce::String& code)
{
    if (code.isEmpty()) return 1.0;

    // Extract full alphabetic prefix, mixed-case safe
    juce::String prefix;
    for (int i = 0; i < code.length(); ++i)
    {
        juce::juce_wchar c = code[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            prefix += c;
        else
            break;
    }
    prefix = prefix.toLowerCase();

    // Ordered longest-first so "sfpldu" matches before "sf"
    static const struct { const char* key; double rating; } kRatings[] = {
        {"sfpldu", 4.0 }, {"zpcp",   4.0 }, {"hspk",   4.0 },
        {"sfmw",   4.0 }, {"sfmz",   4.0 }, {"dkm",    4.25},
        {"llc",    4.0 }, {"mhr",    4.0 }, {"ask",    3.5 },
        {"lbl",    4.0 }, {"phm",    4.5 }, {"rsv",    4.25},
        {"rsz",    4.25}, {"sav",    1.0 }, {"sbi",    4.0 },
        {"scs",    5.0 }, {"sdc",    4.5 }, {"sdp",    4.5 },
        {"sgb",    3.5 }, {"thm",    4.5 }, {"kjt",    3.0 },
        {"ezh",    3.75}, {"sfg",    4.25},
        {"bc",     3.0 }, {"bs",     2.5 }, {"cb",     4.0 },
        {"dg",     3.0 }, {"dis",    4.5 }, {"dk",     4.25},
        {"dm",     3.5 }, {"gm",     4.5 }, {"kar",    4.5 },
        {"kk",     4.5 }, {"kv",     4.0 }, {"leg",    4.0 },
        {"mh",     3.75}, {"mm",     3.75}, {"par",    3.5 },
        {"pi",     2.5 }, {"pr",     4.5 }, {"qh",     3.5 },
        {"sc",     5.0 }, {"sf",     4.25}, {"sk",     4.5 },
        {"ss",     3.5 }, {"tt",     1.0 }, {"zp",     3.5 },
        {nullptr,  0.0 }
    };

    for (int i = 0; kRatings[i].key != nullptr; ++i)
        if (prefix == kRatings[i].key)
            return kRatings[i].rating;
    return 1.0;
}

//==============================================================================
void LibraryScanner::startInitialScan(const juce::File& rootDir)
{
    stopScan();
    scanRoot_    = rootDir;
    appendMode_  = false;
    existing_.clear();
    startThread(juce::Thread::Priority::low);
}

void LibraryScanner::startAppendScan(const juce::File& rootDir,
                                     const std::vector<CdgSong>& existingSongs)
{
    stopScan();
    scanRoot_    = rootDir;
    appendMode_  = true;
    existing_    = existingSongs;
    startThread(juce::Thread::Priority::low);
}

void LibraryScanner::stopScan()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
        waitForThreadToExit(3000);
    }
}

//==============================================================================
void LibraryScanner::run()
{
    isScanning.store(true);
    progressCurrent.store(0);
    progressTotal.store(0);

    if (! scanRoot_.isDirectory())
    {
        juce::MessageManager::callAsync([this]() {
            isScanning.store(false);
            if (onError) onError("Invalid directory: " + scanRoot_.getFullPathName());
        });
        return;
    }

    // --- Step 1: collect all files ------------------------------------------
    std::vector<juce::File> allFiles;
    int groupCount = 0;
    collectFiles(scanRoot_, allFiles, groupCount);

    if (threadShouldExit()) { isScanning.store(false); return; }

    // --- Step 2: group by dir+baseName to pair CDG+MP3 from the same disc ---
    std::map<juce::String, std::vector<juce::File>> fileGroups;
    for (auto& f : allFiles)
    {
        juce::String key = f.getParentDirectory().getFullPathName()
                         + "/" + f.getFileNameWithoutExtension().toLowerCase();
        fileGroups[key].push_back(f);
    }

    progressTotal.store((int)fileGroups.size());

    // --- Step 3: build CdgSongs, grouping by normalised artist|song ----------
    //
    // Multiple vendor files for the same song (e.g. Sound Choice, SunFly)
    // are merged into one CdgSong entry.  Each vendor file becomes an
    // additional entry in the fullPath / fileName / filePath / fileType /
    // version / code / rating arrays — exactly as in karaoke-parser.js.
    std::vector<CdgSong> songs;
    std::map<juce::String, size_t> songLookup;  // normalised key → index

    int idx = 0;
    for (auto& [key, files] : fileGroups)
    {
        if (threadShouldExit()) { isScanning.store(false); return; }

        // Skip pure-audio groups (e.g. mp3 without an accompanying cdg)
        bool hasKaraoke = false;
        for (auto& f : files)
        {
            juce::String e = f.getFileExtension().trimCharactersAtStart(".").toLowerCase();
            if (! isAudioCompanion(e)) { hasKaraoke = true; break; }
        }
        if (! hasKaraoke) { progressCurrent.store(++idx); continue; }

        // Parse artist / song / code from the primary filename
        juce::String baseName = files[0].getFileNameWithoutExtension();
        juce::String artist, songTitle, code, version;
        parseFilename(baseName, artist, songTitle, code, version);

        // If no front catalog code, the parenthetical tag IS the vendor identifier
        // e.g. "Acdc - Back In Black (Leg).cdg" → version="Leg", code=""
        juce::String rawTag = code.isEmpty() ? version : code;

        // Resolve the human-readable vendor version string
        juce::String versionStr = vendorVersionFromCode(rawTag);
        if (versionStr.isEmpty())
            versionStr = rawTag.isEmpty() ? "Unknown" : rawTag;

        juce::String normKey = normaliseSongKey(artist, songTitle);

        auto it = songLookup.find(normKey);
        if (it != songLookup.end())
        {
            // ---- Existing song: append as another vendor version -----------
            CdgSong& existing = songs[it->second];

            // Guard against scanning the same file twice (re-index scenario)
            bool alreadyPresent = false;
            for (auto& fp : existing.fullPath)
                if (juce::String(fp) == files[0].getFullPathName())
                { alreadyPresent = true; break; }

            if (! alreadyPresent)
            {
                // Sort so .cdg comes first
                std::vector<juce::File> sorted = files;
                std::sort(sorted.begin(), sorted.end(),
                    [](const juce::File& a, const juce::File& b)
                    {
                        auto rank = [](const juce::File& f)
                        {
                            juce::String e = f.getFileExtension()
                                              .trimCharactersAtStart(".").toLowerCase();
                            if (e == "cdg") return 0; if (e == "zip") return 1;
                            if (e == "mp4") return 2; if (e == "m4a") return 3;
                            if (e == "xml") return 4; return 5;
                        };
                        return rank(a) < rank(b);
                    });

                for (auto& f : sorted)
                {
                    juce::String ext = f.getFileExtension()
                                        .trimCharactersAtStart(".").toLowerCase();
                    if (isAudioCompanion(ext)) continue;
                    existing.fullPath.push_back(f.getFullPathName().toStdString());
                    existing.fileName.push_back(f.getFileName().toStdString());
                    existing.filePath.push_back(
                        f.getParentDirectory().getFullPathName().toStdString());
                    existing.fileType.push_back(ext.toStdString());
                }
                existing.version.push_back(versionStr.toStdString());
                existing.code.push_back(rawTag.toStdString());
                existing.rating.push_back(ratingForCode(rawTag));
            }
        }
        else
        {
            // ---- New song ---------------------------------------------------
            CdgSong song = buildSong(baseName, files);
            songLookup[normKey] = songs.size();
            songs.push_back(std::move(song));
        }

        {
            juce::ScopedLock sl(currentSongLock);
            currentSong = songs.back().songName.empty()
                              ? juce::String(songs.back().fileName.empty()
                                             ? "" : songs.back().fileName[0])
                              : juce::String(songs.back().artistName)
                                + " - " + juce::String(songs.back().songName);
        }
        progressCurrent.store(++idx);

        if (idx % 50 == 0 || idx == (int)fileGroups.size())
        {
            int cur = idx;
            int tot = (int)fileGroups.size();
            juce::String songName;
            {
                juce::ScopedLock sl(currentSongLock);
                songName = currentSong;
            }
            juce::MessageManager::callAsync([this, cur, tot, songName]() {
                if (onProgress) onProgress(cur, tot, songName);
            });
        }
    }

    // --- Step 4: merge with existing (append mode) ---------------------------
    if (appendMode_ && ! existing_.empty())
    {
        // Build a lookup of existing songs by normalised artist|song key
        std::map<juce::String, size_t> existingLookup;
        for (size_t i = 0; i < existing_.size(); ++i)
        {
            juce::String k = normaliseSongKey(
                juce::String(existing_[i].artistName),
                juce::String(existing_[i].songName));
            existingLookup[k] = i;
        }

        std::vector<CdgSong> merged = existing_;
        for (auto& s : songs)
        {
            juce::String k = normaliseSongKey(
                juce::String(s.artistName),
                juce::String(s.songName));
            auto it2 = existingLookup.find(k);
            if (it2 == existingLookup.end())
            {
                merged.push_back(std::move(s));  // Truly new song
            }
            else
            {
                // Merge new file versions into the existing entry
                CdgSong& ex = merged[it2->second];
                for (size_t vi = 0; vi < s.fullPath.size(); ++vi)
                {
                    bool dup = false;
                    for (auto& fp : ex.fullPath)
                        if (fp == s.fullPath[vi]) { dup = true; break; }
                    if (! dup)
                    {
                        ex.fullPath.push_back(s.fullPath[vi]);
                        ex.fileName.push_back(vi < s.fileName.size() ? s.fileName[vi] : "");
                        ex.filePath.push_back(vi < s.filePath.size() ? s.filePath[vi] : "");
                        ex.fileType.push_back(vi < s.fileType.size() ? s.fileType[vi] : "");
                        if (vi < s.version.size()) ex.version.push_back(s.version[vi]);
                        if (vi < s.code.size())    ex.code.push_back(s.code[vi]);
                        if (vi < s.rating.size())  ex.rating.push_back(s.rating[vi]);
                    }
                }
            }
        }
        songs = std::move(merged);
    }

    // --- Step 5: enrich with local metadata (imageUrl, tempo, genres, etc.) ---
    applyLocalMetadata(songs);

    ScanStats stats = computeStats(songs);
    stats.numGroups = groupCount;

    // Persist to SQLite (primary) and JSON (fallback / legacy).
    // Both calls are synchronous here on the scan thread — cheap vs. the scan.
    saveSongbook(songs);

    isScanning.store(false);

    // Fire completion on message thread
    juce::MessageManager::callAsync([this, songs, stats]() mutable {
        if (onComplete) onComplete(std::move(songs), stats);
    });
}

//==============================================================================
void LibraryScanner::collectFiles(const juce::File& dir,
                                  std::vector<juce::File>& outFiles,
                                  int& outGroupCount)
{
    if (threadShouldExit()) return;

    juce::Array<juce::File> children;
    dir.findChildFiles(children, juce::File::findFilesAndDirectories, false);

    bool hasSubDirs = false;
    for (auto& child : children)
    {
        if (threadShouldExit()) return;
        if (child.isDirectory())
        {
            hasSubDirs = true;
            collectFiles(child, outFiles, outGroupCount);
        }
        else if (isKaraoke(child))
        {
            outFiles.push_back(child);
        }
    }

    // Count this directory as a group if it contained files
    if (hasSubDirs) ++outGroupCount;
}

//==============================================================================
CdgSong LibraryScanner::buildSong(const juce::String& baseName,
                                  const std::vector<juce::File>& files)
{
    CdgSong song;

    // Sort so .cdg comes first (gives best filename for parsing)
    std::vector<juce::File> sorted = files;
    std::sort(sorted.begin(), sorted.end(), [](const juce::File& a, const juce::File& b) {
        auto rank = [](const juce::File& f) {
            juce::String e = f.getFileExtension().trimCharactersAtStart(".").toLowerCase();
            if (e == "cdg")  return 0;
            if (e == "zip")  return 1;
            if (e == "mp4")  return 2;
            if (e == "m4a")  return 3;
            if (e == "xml")  return 4;
            return 5;
        };
        return rank(a) < rank(b);
    });

    // Parse artist/song from the primary file's base name
    juce::String artist, songTitle, code, version;
    parseFilename(baseName, artist, songTitle, code, version);

    song.artistName = artist.toStdString();
    song.songName   = songTitle.toStdString();

    // Populate file arrays (skip pure audio companions — they're implicit)
    for (auto& f : sorted)
    {
        juce::String ext = f.getFileExtension().trimCharactersAtStart(".").toLowerCase();
        if (isAudioCompanion(ext)) continue;  // don't list .mp3 separately

        song.fullPath.push_back(f.getFullPathName().toStdString());
        song.fileName.push_back(f.getFileName().toStdString());
        song.filePath.push_back(f.getParentDirectory().getFullPathName().toStdString());
        song.fileType.push_back(ext.toStdString());
    }

    // Version & disc code — resolve vendor name from catalog code
    // If no front catalog code, the parenthetical tag IS the vendor identifier
    juce::String rawTag = code.isEmpty() ? version : code;
    juce::String versionStr = vendorVersionFromCode(rawTag);
    if (versionStr.isEmpty())
        versionStr = rawTag.isEmpty() ? "Unknown" : rawTag;
    song.version.push_back(versionStr.toStdString());
    song.code.push_back(rawTag.toStdString());
    song.rating.push_back(ratingForCode(rawTag));

    // File size / date from primary file
    song.fileSize = sorted[0].getSize();
    song.fileDate = sorted[0].getLastModificationTime().toMilliseconds();

    // Stable ID — does NOT include code so same song from different vendors
    // always produces the same ID and maps to one songbook entry.
    song.id = makeSongId(artist, songTitle, "").toStdString();

    return song;
}

//==============================================================================
// static
bool LibraryScanner::parseFilename(const juce::String& fileNameNoExt,
                                   juce::String& outArtist,
                                   juce::String& outSong,
                                   juce::String& outCode,
                                   juce::String& outVersion)
{
    outArtist  = "";
    outSong    = "";
    outCode    = "";
    outVersion = "";

    juce::String name = fileNameNoExt.trim();
    if (name.isEmpty()) return false;

    // ----------------------------------------------------------------
    // Step 1 — try to strip a vendor disc-code from the front.
    //
    //  Pattern: 1-4 uppercase letters, optional single space, 1-6 digits,
    //           followed by a space (or end of code region).
    //
    //  Examples: "SC1234 ", "SF 0001 ", "CBX1234 ", "MM 002 "
    // ----------------------------------------------------------------
    juce::String remaining = name;
    {
        int i = 0;
        int len = name.length();

        // Collect leading uppercase letters (1-4)
        while (i < len && name[i] >= 'A' && name[i] <= 'Z') ++i;
        int lettersEnd = i;

        if (lettersEnd >= 1 && lettersEnd <= 4)
        {
            // Optional single space between letters and digits
            int j = i;
            if (j < len && name[j] == ' ') ++j;

            // Collect digits (1-6)
            int digitsStart = j;
            while (j < len && name[j] >= '0' && name[j] <= '9') ++j;
            int digitsEnd = j;

            if (digitsEnd > digitsStart)
            {
                // Require a trailing space to avoid "ABBADancing" misparse
                if (j < len && name[j] == ' ')
                {
                    outCode = name.substring(0, lettersEnd) + name.substring(digitsStart, digitsEnd);
                    remaining = name.substring(j + 1).trim();   // skip the trailing space

                    // A catalog code is always followed by " - " before the artist.
                    // Strip that separator so "remaining" starts directly with the artist.
                    // e.g. "SC1234 - Artist - Song" → code="SC1234", remaining="Artist - Song"
                    if (remaining.startsWith("- "))
                        remaining = remaining.substring(2).trim();
                }
            }
        }
    }

    // ----------------------------------------------------------------
    // Step 2 — split on " - " to get artist / song
    // ----------------------------------------------------------------
    int dashPos = remaining.indexOf(" - ");
    if (dashPos < 0)
    {
        // Fallback: try "- " without leading space
        dashPos = remaining.indexOf("- ");
        if (dashPos < 0)
        {
            // No separator: treat the whole thing as song name only
            outSong = remaining;
            return false;
        }
        outArtist  = remaining.substring(0, dashPos).trim();
        remaining  = remaining.substring(dashPos + 2).trim();
    }
    else
    {
        outArtist  = remaining.substring(0, dashPos).trim();
        remaining  = remaining.substring(dashPos + 3).trim();
    }

    // ----------------------------------------------------------------
    // Step 3 — extract version from trailing parentheses, e.g. "(Male)"
    // ----------------------------------------------------------------
    if (remaining.endsWithChar(')'))
    {
        int parenOpen = remaining.lastIndexOf("(");
        if (parenOpen > 0)
        {
            outVersion = remaining.substring(parenOpen + 1,
                                             remaining.length() - 1).trim();
            remaining  = remaining.substring(0, parenOpen).trim();
        }
    }

    outSong = remaining;
    return true;
}

//==============================================================================
// static
juce::String LibraryScanner::makeSongId(const juce::String& artist,
                                         const juce::String& song,
                                         const juce::String& /*code*/)
{
    // Code intentionally excluded: different vendor versions of the same song
    // must share a single ID so they resolve to the same songbook entry.
    juce::String key = artist.toLowerCase().trim()
                     + "|" + song.toLowerCase().trim();
    return juce::String::toHexString(key.hashCode64()).paddedLeft('0', 16);
}

//==============================================================================
// static
bool LibraryScanner::isKaraokeFile(const juce::File& f)
{
    return isKaraoke(f);
}

juce::String LibraryScanner::normaliseExt(const juce::String& ext)
{
    return ext.trimCharactersAtStart(".").toLowerCase();
}

//==============================================================================
// Songbook persistence
//==============================================================================
juce::File LibraryScanner::getSongbookFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("EncoreKaraoke")
               .getChildFile("songbook.json");
}

bool LibraryScanner::saveSongbook(const std::vector<CdgSong>& songs)
{
    // Persist the scan root for next-launch restoration.
    if (scanRoot_.isDirectory())
    {
        juce::File rootFile = getSongbookFile().getSiblingFile("scanRoot.txt");
        rootFile.getParentDirectory().createDirectory();
        rootFile.replaceWithText(scanRoot_.getFullPathName());
    }

    // ── Primary path: SQLite ─────────────────────────────────────────────────
    if (songDb_ != nullptr && songDb_->isOpen())
    {
        bool ok = songDb_->replaceAll(songs);
        if (!ok)
            DBG("LibraryScanner::saveSongbook: SQLite write failed");
        // Fall through to JSON backup regardless of SQLite result.
    }

    // ── Fallback / backup: JSON ──────────────────────────────────────────────
    juce::File file = getSongbookFile();
    file.getParentDirectory().createDirectory();

    juce::Array<juce::var> arr;
    for (auto& s : songs)
    {
        juce::DynamicObject* obj = new juce::DynamicObject();
        obj->setProperty("id",           juce::String(s.id));
        obj->setProperty("songName",     juce::String(s.songName));
        obj->setProperty("artistName",   juce::String(s.artistName));
        obj->setProperty("imageUrl",     juce::String(s.imageUrl));
        obj->setProperty("keySignature", juce::String(s.keySignature));
        obj->setProperty("releaseDate",  juce::String(s.releaseDate));
        obj->setProperty("durationMS",   s.durationMS);
        obj->setProperty("tempo",        s.tempo);
        obj->setProperty("fileDate",     (juce::int64)s.fileDate);
        obj->setProperty("fileSize",     (juce::int64)s.fileSize);

        auto toArray = [](const std::vector<std::string>& v) {
            juce::Array<juce::var> a;
            for (auto& x : v) a.add(juce::String(x));
            return a;
        };
        obj->setProperty("fullPath", toArray(s.fullPath));
        obj->setProperty("fileName", toArray(s.fileName));
        obj->setProperty("filePath", toArray(s.filePath));
        obj->setProperty("fileType", toArray(s.fileType));
        obj->setProperty("genres",   toArray(s.genres));
        obj->setProperty("version",  toArray(s.version));
        obj->setProperty("code",     toArray(s.code));

        juce::Array<juce::var> ratingArr;
        for (auto r : s.rating) ratingArr.add(r);
        obj->setProperty("rating", ratingArr);

        arr.add(juce::var(obj));
    }

    return file.replaceWithText(juce::JSON::toString(juce::var(arr), false));
}

std::vector<CdgSong> LibraryScanner::loadSongbook()
{
    // ── Primary path: SQLite ─────────────────────────────────────────────────
    if (songDb_ != nullptr && songDb_->isOpen() && songDb_->count() > 0)
        return songDb_->getAll();

    // ── Fallback: JSON (used on first run or when SQLite is not configured) ──
    std::vector<CdgSong> songs;
    juce::File file = getSongbookFile();
    if (! file.existsAsFile()) return songs;

    juce::var parsed = juce::JSON::parse(file.loadFileAsString());
    if (auto* arr = parsed.getArray())
    {
        songs.reserve((size_t)arr->size());
        for (auto& item : *arr)
            if (auto* obj = item.getDynamicObject())
                songs.push_back(CdgSong::fromJsonObject(obj));
    }

    // Migrate JSON → SQLite so subsequent loads use the fast path.
    if (songDb_ != nullptr && songDb_->isOpen() && !songs.empty())
    {
        songDb_->replaceAll(songs);
        DBG("LibraryScanner::loadSongbook: migrated " + juce::String((int)songs.size())
            + " songs from JSON to SQLite");
    }

    return songs;
}

//==============================================================================
// Metadata enrichment
//==============================================================================
int LibraryScanner::applyLocalMetadata(std::vector<CdgSong>& songs)
{
    // meta_data.json lives alongside songbook.json in the EncoreKaraoke app-data dir.
    // Copy it there from src/assets/data/meta_data.json if not already present.
    juce::File metaFile = getSongbookFile().getSiblingFile("meta_data.json");
    if (! metaFile.existsAsFile()) return 0;

    // meta_data.json is a JSON *object* keyed by Firestore document IDs:
    //   { "<docId>": { "artistName":"...", "songName":"...", ... }, ... }
    juce::var parsed = juce::JSON::parse(metaFile.loadFileAsString());
    if (! parsed.isObject()) return 0;

    // ------------------------------------------------------------------
    // Build a lookup from normalised artist|song key → metadata values.
    // We use normaliseSongKey so "The Beatles" and "Beatles, The" both map
    // to the same key as their scanned counterparts.
    // ------------------------------------------------------------------
    struct MetaEntry
    {
        std::string artistName, songName, imageUrl, keySignature, releaseDate;
        int    durationMS = 0;
        double tempo      = 0.0;
        std::vector<std::string> genres;
    };

    std::map<juce::String, MetaEntry> metaLookup;

    auto* rootObj = parsed.getDynamicObject();
    auto& props   = rootObj->getProperties();
    for (int i = 0; i < props.size(); ++i)
    {
        auto* entryObj = props.getValueAt(i).getDynamicObject();
        if (! entryObj) continue;

        juce::String mArtist = entryObj->getProperty("artistName").toString();
        juce::String mSong   = entryObj->getProperty("songName").toString();
        if (mArtist.isEmpty() || mSong.isEmpty()) continue;

        MetaEntry entry;
        entry.artistName   = mArtist.toStdString();
        entry.songName     = mSong.toStdString();
        entry.imageUrl     = entryObj->getProperty("imageUrl").toString().toStdString();
        entry.keySignature = entryObj->getProperty("keySignature").toString().toStdString();
        entry.releaseDate  = entryObj->getProperty("releaseDate").toString().toStdString();
        entry.durationMS   = (int)entryObj->getProperty("durationMS");
        entry.tempo        = (double)entryObj->getProperty("tempo");

        if (auto* ga = entryObj->getProperty("genres").getArray())
            for (auto& g : *ga)
                entry.genres.push_back(g.toString().toStdString());

        metaLookup[normaliseSongKey(mArtist, mSong)] = std::move(entry);
    }

    // ------------------------------------------------------------------
    // Walk each scanned song and try to find a metadata match.
    // Primary:  normKey(artistName, songName)  — normal case
    // Fallback: normKey(songName, artistName)  — handles filenames where
    //           artist and song were written in the wrong order
    // ------------------------------------------------------------------
    int matchCount = 0;
    for (auto& s : songs)
    {
        juce::String fwdKey = normaliseSongKey(
            juce::String(s.artistName), juce::String(s.songName));

        auto it = metaLookup.find(fwdKey);
        if (it == metaLookup.end())
        {
            // Try with artist and song swapped
            juce::String revKey = normaliseSongKey(
                juce::String(s.songName), juce::String(s.artistName));
            it = metaLookup.find(revKey);
        }

        if (it == metaLookup.end()) continue;

        const MetaEntry& meta = it->second;
        // Use the canonical spelling from the metadata database
        s.artistName   = meta.artistName;
        s.songName     = meta.songName;
        s.imageUrl     = meta.imageUrl;
        s.keySignature = meta.keySignature;
        s.releaseDate  = meta.releaseDate;
        s.durationMS   = meta.durationMS;
        s.tempo        = meta.tempo;
        if (! meta.genres.empty())
            s.genres = meta.genres;

        ++matchCount;
    }
    return matchCount;
}

//==============================================================================
// static
LibraryScanner::ScanStats LibraryScanner::computeStats(const std::vector<CdgSong>& songs)
{
    ScanStats stats;
    stats.numSongs = (int)songs.size();

    for (auto& s : songs)
    {
        for (auto& t : s.fileType)
        {
            juce::String ext = juce::String(t).toLowerCase();
            if      (ext == "cdg")     ++stats.numCDG;
            else if (ext == "zip")     ++stats.numZip;
            else if (ext == "mp4")     ++stats.numMP4;
            else if (ext == "m4a")     ++stats.numM4A;
            else if (ext == "xml")     ++stats.numXML;
            else if (ext != "audio")   ++stats.numUnknown;
        }
        if (! s.imageUrl.empty()) ++stats.numMeta;
    }
    return stats;
}
