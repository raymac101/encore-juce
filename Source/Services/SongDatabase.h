/*
  ==============================================================================

    SongDatabase.h

    SQLite-backed song library index.

    Schema
    ──────
    songs          — one row per CdgSong, multi-value fields stored as JSON
    songs_fts      — FTS5 virtual table (song_name + artist_name) for search
    Triggers keep songs_fts in sync automatically on INSERT / UPDATE / DELETE.

    Threading
    ─────────
    All public methods are safe to call from any thread.  Writes serialise
    through a juce::CriticalSection; reads acquire a shared lock.  Heavy
    bulk operations (insertOrReplace for a full scan result) block the caller —
    invoke from a background thread (e.g. inside LibraryScanner::run()).

    Dependency
    ──────────
    System SQLite:
      macOS   — pre-installed: -lsqlite3 (no install required)
      Windows — vcpkg install sqlite3   (adds sqlite3.h + sqlite3.lib)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/CdgSong.h"
#include <sqlite3.h>
#include <vector>
#include <functional>

//==============================================================================
class SongDatabase
{
public:
    SongDatabase();
    ~SongDatabase();

    //==========================================================================
    // Lifecycle

    /** Open (or create) the database at the default location:
        <userApplicationDataDirectory>/EncoreKaraoke/songs.db
        Returns true on success. */
    bool open();

    /** Open at an explicit path — useful for testing. */
    bool open(const juce::File& dbFile);

    void close();
    bool isOpen() const noexcept { return db_ != nullptr; }

    //==========================================================================
    // Writes

    /** Insert or replace a song.  On conflict (same id) the row is fully
        replaced.  Returns true on success. */
    bool insertOrReplace(const CdgSong& song);

    /** Bulk insert.  Runs inside a single transaction — much faster than
        calling insertOrReplace() in a loop.  Returns the number of rows
        written, or -1 on error. */
    int  bulkInsert(const std::vector<CdgSong>& songs);

    /** Replace the entire library: drops all rows then bulk-inserts.
        Runs in a single transaction. */
    bool replaceAll(const std::vector<CdgSong>& songs);

    /** Remove a song by id.  Returns true if a row was actually deleted. */
    bool remove(const juce::String& songId);

    /** Delete all rows. */
    bool clear();

    //==========================================================================
    // Reads

    /** Return every song, sorted by artistName ASC, songName ASC. */
    std::vector<CdgSong> getAll() const;

    /** Return a single song by id, or an invalid CdgSong (id empty) if not
        found. */
    CdgSong getById(const juce::String& songId) const;

    /** Full-text search on song_name + artist_name using FTS5 MATCH.
        The query string may include FTS5 operators ("rolling NEAR stones").
        Results are returned ranked by relevance (BM25). */
    std::vector<CdgSong> search(const juce::String& query,
                                int maxResults = 200) const;

    /** Simple prefix search: prepends a wildcard so "hel" matches "Hello". */
    std::vector<CdgSong> searchPrefix(const juce::String& prefix,
                                      int maxResults = 200) const;

    /** Return the total row count. */
    int count() const;

    //==========================================================================
    // Maintenance

    /** Rebuild the FTS5 index (call after external db modifications). */
    bool rebuildFts();

    /** Run VACUUM to reclaim space after bulk deletes. */
    bool vacuum();

    /** Path to the open database file. */
    juce::File getFile() const { return dbFile_; }

private:
    //==========================================================================
    bool  createSchema();
    bool  exec(const char* sql);
    bool  execBind(sqlite3_stmt* stmt) const;

    // Bind all CdgSong fields onto a prepared INSERT OR REPLACE statement.
    void  bindSong(sqlite3_stmt* stmt, const CdgSong& song) const;

    // Read a CdgSong from the current row of a SELECT statement.
    static CdgSong rowToSong(sqlite3_stmt* stmt);

    // Serialise a string vector to a compact JSON array string, and back.
    static std::string vecToJson(const std::vector<std::string>& v);
    static std::vector<std::string> jsonToStrVec(const char* json);
    static std::string dblVecToJson(const std::vector<double>& v);
    static std::vector<double> jsonToDblVec(const char* json);

    //==========================================================================
    sqlite3*   db_     = nullptr;
    juce::File dbFile_;

    mutable juce::ReadWriteLock lock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongDatabase)
};
