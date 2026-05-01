/*
  ==============================================================================

    LibraryScanner.h
    Created: 22 Apr 2026
    Author:  GitHub Copilot

    Background service that:
      - Recursively scans a directory for karaoke files (.cdg, .zip, .mp4,
        .m4a, .xml)
      - Parses CDG filenames to extract artist, song name, vendor disc-code,
        and version info
      - Groups CDG+MP3 file pairs by base filename into a single CdgSong
      - Loads / saves the songbook as JSON from the user app-data folder
      - Applies locally-cached metadata to enrich the songbook
      - Reports scan progress via atomic integers (safe to poll from the UI
        thread) and fires completion/progress callbacks on the message thread

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <functional>
#include <map>
#include "../Models/CdgSong.h"
#include "SongDatabase.h"

//==============================================================================
class LibraryScanner : public juce::Thread
{
public:
    //==========================================================================
    struct ScanStats
    {
        int numSongs   = 0;
        int numCDG     = 0;
        int numZip     = 0;
        int numMP4     = 0;
        int numM4A     = 0;
        int numXML     = 0;
        int numUnknown = 0;
        int numGroups  = 0;
        int numMeta    = 0;  // Songs with metadata matched
    };

    //==========================================================================
    LibraryScanner();
    ~LibraryScanner() override;

    //==========================================================================
    // Scan operations

    /** Full scan — replaces the current songbook. */
    void startInitialScan(const juce::File& rootDir);

    /** Append scan — merges new files with the existing songbook. */
    void startAppendScan(const juce::File& rootDir,
                         const std::vector<CdgSong>& existingSongs);

    /** Stop any running scan (waits up to 3 s for the thread to exit). */
    void stopScan();

    //==========================================================================
    // SQLite database (optional — set before first scan)
    //
    // When set, scan results are written to (and loaded from) the SQLite index
    // instead of songbook.json.  The JSON file is kept as a migration fallback.
    void setSongDatabase(SongDatabase* db) noexcept { songDb_ = db; }

    //==========================================================================
    // Songbook persistence

    bool                    saveSongbook(const std::vector<CdgSong>& songs);
    std::vector<CdgSong>    loadSongbook();
    juce::File              getSongbookFile() const;
    juce::File              getScanRoot()     const { return scanRoot_; }

    //==========================================================================
    // Metadata enrichment
    //  Reads metadata.json from the app-data folder, matches by
    //  "artist|song" key, updates the supplied songs in-place, and
    //  returns the number of matches found.
    int applyLocalMetadata(std::vector<CdgSong>& songs);

    //==========================================================================
    // Stats

    static ScanStats computeStats(const std::vector<CdgSong>& songs);

    //==========================================================================
    // Filename parser — public for testing
    //
    //  Parses a filename (without extension) and extracts:
    //    outArtist   — artist name
    //    outSong     — song title
    //    outCode     — vendor disc-code (e.g. "SC1234", "SF0001"), or ""
    //    outVersion  — version string from trailing parentheses, or ""
    //
    //  Returns true if a " - " separator was found (artist/song split OK).
    static bool parseFilename(const juce::String& fileNameNoExt,
                              juce::String& outArtist,
                              juce::String& outSong,
                              juce::String& outCode,
                              juce::String& outVersion);

    //==========================================================================
    // Thread-safe progress state (poll freely from the UI thread)
    std::atomic<int>  progressCurrent { 0 };
    std::atomic<int>  progressTotal   { 0 };
    std::atomic<bool> isScanning      { false };

    // Current song being processed — written by scan thread, read by UI.
    // Access is best-effort; use progressCurrent/Total for reliable progress.
    juce::String currentSong;   // guarded by currentSongLock
    juce::CriticalSection currentSongLock;

    //==========================================================================
    // Callbacks — always invoked on the JUCE message thread
    std::function<void(int current, int total, juce::String songName)> onProgress;
    std::function<void(std::vector<CdgSong>, ScanStats)>               onComplete;
    std::function<void(juce::String)>                                   onError;

private:
    //==========================================================================
    void run() override;

    // Recursively collect all karaoke-relevant files
    void collectFiles(const juce::File& dir,
                      std::vector<juce::File>& outFiles,
                      int& outGroupCount);

    // Build a CdgSong from a group of files sharing the same base name
    CdgSong buildSong(const juce::String& baseName,
                      const std::vector<juce::File>& files);

    // Generate a stable ID from artist + song name (code ignored — same song
    // from different vendors must produce the same ID)
    static juce::String makeSongId(const juce::String& artist,
                                   const juce::String& song,
                                   const juce::String& code);

    // Normalise artist+song into a lowercase punctuation-stripped lookup key
    // used to group vendor versions of the same song together.
    static juce::String normaliseSongKey(const juce::String& artist,
                                         const juce::String& song);

    // Map a catalog disc-code prefix (e.g. "SC1234") to a human-readable
    // vendor name (e.g. "Sound Choice").  Returns "" if unrecognised.
    static juce::String vendorVersionFromCode(const juce::String& code);

    // Return a quality rating 0.0–5.0 for a given catalog disc-code prefix.
    static double ratingForCode(const juce::String& code);

    //==========================================================================
    // Recognised karaoke extensions
    static bool isKaraokeFile(const juce::File& f);
    static juce::String normaliseExt(const juce::String& ext);

    //==========================================================================
    juce::File           scanRoot_;
    bool                 appendMode_   = false;
    std::vector<CdgSong> existing_;
    SongDatabase*        songDb_       = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryScanner)
};
