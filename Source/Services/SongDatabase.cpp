/*
  ==============================================================================

    SongDatabase.cpp

  ==============================================================================
*/

#include "SongDatabase.h"
#include <sqlite3.h>
#include <cstring>

//==============================================================================
// Schema DDL
//==============================================================================

static const char* kCreateSongs = R"SQL(
CREATE TABLE IF NOT EXISTS songs (
    id            TEXT    PRIMARY KEY,
    song_name     TEXT    NOT NULL DEFAULT '',
    artist_name   TEXT    NOT NULL DEFAULT '',
    image_url     TEXT    DEFAULT '',
    key_signature TEXT    DEFAULT '',
    tempo         REAL    DEFAULT 0,
    duration_ms   INTEGER DEFAULT 0,
    release_date  TEXT    DEFAULT '',
    file_date     INTEGER DEFAULT 0,
    file_size     INTEGER DEFAULT 0,
    full_paths    TEXT    DEFAULT '[]',
    file_names    TEXT    DEFAULT '[]',
    file_paths    TEXT    DEFAULT '[]',
    file_types    TEXT    DEFAULT '[]',
    genres        TEXT    DEFAULT '[]',
    versions      TEXT    DEFAULT '[]',
    codes         TEXT    DEFAULT '[]',
    ratings       TEXT    DEFAULT '[]',
    added_at      INTEGER DEFAULT 0
);
)SQL";

// FTS5 virtual table — content= links it to the songs table.
static const char* kCreateFts = R"SQL(
CREATE VIRTUAL TABLE IF NOT EXISTS songs_fts
    USING fts5(
        id        UNINDEXED,
        song_name,
        artist_name,
        content='songs',
        content_rowid='rowid'
    );
)SQL";

// Triggers to keep songs_fts in sync with songs.
static const char* kCreateTriggers = R"SQL(
CREATE TRIGGER IF NOT EXISTS songs_ai AFTER INSERT ON songs BEGIN
    INSERT INTO songs_fts(rowid, id, song_name, artist_name)
    VALUES (new.rowid, new.id, new.song_name, new.artist_name);
END;

CREATE TRIGGER IF NOT EXISTS songs_au AFTER UPDATE ON songs BEGIN
    INSERT INTO songs_fts(songs_fts, rowid, id, song_name, artist_name)
    VALUES ('delete', old.rowid, old.id, old.song_name, old.artist_name);
    INSERT INTO songs_fts(rowid, id, song_name, artist_name)
    VALUES (new.rowid, new.id, new.song_name, new.artist_name);
END;

CREATE TRIGGER IF NOT EXISTS songs_ad AFTER DELETE ON songs BEGIN
    INSERT INTO songs_fts(songs_fts, rowid, id, song_name, artist_name)
    VALUES ('delete', old.rowid, old.id, old.song_name, old.artist_name);
END;
)SQL";

// Indices for fast sorted browse.
static const char* kCreateIndices = R"SQL(
CREATE INDEX IF NOT EXISTS idx_songs_artist ON songs(artist_name COLLATE NOCASE);
CREATE INDEX IF NOT EXISTS idx_songs_name   ON songs(song_name   COLLATE NOCASE);
)SQL";

//==============================================================================
// Prepared statement SQL
//==============================================================================

static const char* kInsertSql = R"SQL(
INSERT INTO songs
    (id, song_name, artist_name, image_url, key_signature,
     tempo, duration_ms, release_date, file_date, file_size,
     full_paths, file_names, file_paths, file_types,
     genres, versions, codes, ratings, added_at)
VALUES
    (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
ON CONFLICT(id) DO UPDATE SET
    song_name    = excluded.song_name,
    artist_name  = excluded.artist_name,
    image_url    = excluded.image_url,
    key_signature= excluded.key_signature,
    tempo        = excluded.tempo,
    duration_ms  = excluded.duration_ms,
    release_date = excluded.release_date,
    file_date    = excluded.file_date,
    file_size    = excluded.file_size,
    full_paths   = excluded.full_paths,
    file_names   = excluded.file_names,
    file_paths   = excluded.file_paths,
    file_types   = excluded.file_types,
    genres       = excluded.genres,
    versions     = excluded.versions,
    codes        = excluded.codes,
    ratings      = excluded.ratings,
    added_at     = CASE WHEN songs.added_at > 0 THEN songs.added_at ELSE excluded.added_at END;
)SQL";

static const char* kSelectAll = R"SQL(
SELECT id, song_name, artist_name, image_url, key_signature,
       tempo, duration_ms, release_date, file_date, file_size,
       full_paths, file_names, file_paths, file_types,
       genres, versions, codes, ratings, added_at
FROM songs
ORDER BY artist_name COLLATE NOCASE ASC,
         song_name   COLLATE NOCASE ASC;
)SQL";

static const char* kSelectById = R"SQL(
SELECT id, song_name, artist_name, image_url, key_signature,
       tempo, duration_ms, release_date, file_date, file_size,
       full_paths, file_names, file_paths, file_types,
       genres, versions, codes, ratings, added_at
FROM songs WHERE id = ?;
)SQL";

// FTS5 MATCH — ranks by BM25 (lower rank = better match).
static const char* kSearchFts = R"SQL(
SELECT s.id, s.song_name, s.artist_name, s.image_url, s.key_signature,
       s.tempo, s.duration_ms, s.release_date, s.file_date, s.file_size,
       s.full_paths, s.file_names, s.file_paths, s.file_types,
       s.genres, s.versions, s.codes, s.ratings, s.added_at
FROM songs_fts f
JOIN songs s ON s.rowid = f.rowid
WHERE songs_fts MATCH ?
ORDER BY rank
LIMIT ?;
)SQL";

//==============================================================================
// Construction / lifecycle
//==============================================================================

SongDatabase::SongDatabase()  {}
SongDatabase::~SongDatabase() { close(); }

bool SongDatabase::open()
{
    juce::File dir = juce::File::getSpecialLocation(
                         juce::File::userApplicationDataDirectory)
                         .getChildFile("EncoreKaraoke");
    dir.createDirectory();
    return open(dir.getChildFile("songs.db"));
}

bool SongDatabase::open(const juce::File& file)
{
    close();
    dbFile_ = file;

    int rc = sqlite3_open(file.getFullPathName().toRawUTF8(), &db_);
    if (rc != SQLITE_OK)
    {
        DBG("SongDatabase::open failed: " + juce::String(sqlite3_errmsg(db_)));
        db_ = nullptr;
        return false;
    }

    // WAL mode: readers don't block writers, good for background scan writes.
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    exec("PRAGMA foreign_keys=ON;");

    return createSchema();
}

void SongDatabase::close()
{
    if (db_ != nullptr)
    {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

bool SongDatabase::createSchema()
{
    if (! exec(kCreateSongs))   return false;
    if (! exec(kCreateFts))     return false;
    if (! exec(kCreateTriggers)) return false;
    if (! exec(kCreateIndices)) return false;

    // Non-destructive migration: add added_at to databases created before this
    // column existed.  SQLite returns an error if the column already exists —
    // that's expected and safe to ignore.
    sqlite3_exec(db_,
        "ALTER TABLE songs ADD COLUMN added_at INTEGER DEFAULT 0;",
        nullptr, nullptr, nullptr);

    return true;
}

//==============================================================================
// Helpers
//==============================================================================

bool SongDatabase::exec(const char* sql)
{
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        DBG("SongDatabase SQL error: " + juce::String(err ? err : "unknown"));
        sqlite3_free(err);
        return false;
    }
    return true;
}

std::string SongDatabase::vecToJson(const std::vector<std::string>& v)
{
    // Simple manual JSON array — avoids pulling in a full JSON lib in this helper.
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i > 0) out += ",";
        out += "\"";
        for (char c : v[i])
        {
            if (c == '"')  out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else            out += c;
        }
        out += "\"";
    }
    return out + "]";
}

std::vector<std::string> SongDatabase::jsonToStrVec(const char* json)
{
    // Use JUCE JSON parser for robustness.
    std::vector<std::string> r;
    if (json == nullptr) return r;
    juce::var v = juce::JSON::parse(juce::String::fromUTF8(json));
    if (auto* arr = v.getArray())
        for (auto& item : *arr)
            r.push_back(item.toString().toStdString());
    return r;
}

std::string SongDatabase::dblVecToJson(const std::vector<double>& v)
{
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i > 0) out += ",";
        out += std::to_string(v[i]);
    }
    return out + "]";
}

std::vector<double> SongDatabase::jsonToDblVec(const char* json)
{
    std::vector<double> r;
    if (json == nullptr) return r;
    juce::var v = juce::JSON::parse(juce::String::fromUTF8(json));
    if (auto* arr = v.getArray())
        for (auto& item : *arr)
            r.push_back(static_cast<double>(item));
    return r;
}

void SongDatabase::bindSong(sqlite3_stmt* stmt, const CdgSong& song) const
{
    auto bindText = [&](int col, const std::string& s) {
        sqlite3_bind_text(stmt, col, s.c_str(), static_cast<int>(s.size()), SQLITE_TRANSIENT);
    };

    bindText(1,  song.id);
    bindText(2,  song.songName);
    bindText(3,  song.artistName);
    bindText(4,  song.imageUrl);
    bindText(5,  song.keySignature);
    sqlite3_bind_double (stmt, 6, song.tempo);
    sqlite3_bind_int    (stmt, 7, song.durationMS);
    bindText(8,  song.releaseDate);
    sqlite3_bind_int64  (stmt, 9,  static_cast<sqlite3_int64>(song.fileDate));
    sqlite3_bind_int64  (stmt, 10, static_cast<sqlite3_int64>(song.fileSize));
    bindText(11, vecToJson(song.fullPath));
    bindText(12, vecToJson(song.fileName));
    bindText(13, vecToJson(song.filePath));
    bindText(14, vecToJson(song.fileType));
    bindText(15, vecToJson(song.genres));
    bindText(16, vecToJson(song.version));
    bindText(17, vecToJson(song.code));
    bindText(18, dblVecToJson(song.rating));
    sqlite3_bind_int64  (stmt, 19, static_cast<sqlite3_int64>(song.addedAt));
}

// static
CdgSong SongDatabase::rowToSong(sqlite3_stmt* stmt)
{
    CdgSong s;
    auto getText = [&](int col) -> std::string {
        const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
        return t ? t : "";
    };

    s.id           = getText(0);
    s.songName     = getText(1);
    s.artistName   = getText(2);
    s.imageUrl     = getText(3);
    s.keySignature = getText(4);
    s.tempo        = sqlite3_column_double(stmt, 5);
    s.durationMS   = sqlite3_column_int   (stmt, 6);
    s.releaseDate  = getText(7);
    s.fileDate     = static_cast<int64_t>(sqlite3_column_int64(stmt, 8));
    s.fileSize     = static_cast<int64_t>(sqlite3_column_int64(stmt, 9));
    s.fullPath     = jsonToStrVec(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10)));
    s.fileName     = jsonToStrVec(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11)));
    s.filePath     = jsonToStrVec(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12)));
    s.fileType     = jsonToStrVec(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13)));
    s.genres       = jsonToStrVec(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14)));
    s.version      = jsonToStrVec(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15)));
    s.code         = jsonToStrVec(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 16)));
    s.rating       = jsonToDblVec(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 17)));
    s.addedAt      = static_cast<int64_t>(sqlite3_column_int64(stmt, 18));
    return s;
}

//==============================================================================
// Writes
//==============================================================================

bool SongDatabase::insertOrReplace(const CdgSong& song)
{
    juce::ScopedWriteLock wl(lock_);
    if (!isOpen()) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bindSong(stmt, song);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

int SongDatabase::bulkInsert(const std::vector<CdgSong>& songs)
{
    juce::ScopedWriteLock wl(lock_);
    if (!isOpen() || songs.empty()) return 0;

    sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return -1;
    }

    int written = 0;
    for (const auto& song : songs)
    {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bindSong(stmt, song);
        if (sqlite3_step(stmt) == SQLITE_DONE)
            ++written;
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return written;
}

bool SongDatabase::replaceAll(const std::vector<CdgSong>& songs)
{
    juce::ScopedWriteLock wl(lock_);
    if (!isOpen()) return false;

    sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr);

    // Drop and recreate FTS to avoid stale entries (triggers handle incremental).
    sqlite3_exec(db_, "DELETE FROM songs;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "DELETE FROM songs_fts;", nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& song : songs)
    {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bindSong(stmt, song);
        sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

bool SongDatabase::remove(const juce::String& songId)
{
    juce::ScopedWriteLock wl(lock_);
    if (!isOpen()) return false;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "DELETE FROM songs WHERE id=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, songId.toRawUTF8(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

bool SongDatabase::clear()
{
    juce::ScopedWriteLock wl(lock_);
    if (!isOpen()) return false;
    return exec("DELETE FROM songs;") && exec("DELETE FROM songs_fts;");
}

//==============================================================================
// Reads
//==============================================================================

std::vector<CdgSong> SongDatabase::getAll() const
{
    juce::ScopedReadLock rl(lock_);
    std::vector<CdgSong> result;
    if (!isOpen()) return result;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kSelectAll, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW)
        result.push_back(rowToSong(stmt));

    sqlite3_finalize(stmt);
    return result;
}

CdgSong SongDatabase::getById(const juce::String& songId) const
{
    juce::ScopedReadLock rl(lock_);
    if (!isOpen()) return {};

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kSelectById, -1, &stmt, nullptr) != SQLITE_OK)
        return {};

    sqlite3_bind_text(stmt, 1, songId.toRawUTF8(), -1, SQLITE_TRANSIENT);

    CdgSong result;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = rowToSong(stmt);

    sqlite3_finalize(stmt);
    return result;
}

std::vector<CdgSong> SongDatabase::search(const juce::String& query,
                                           int maxResults) const
{
    juce::ScopedReadLock rl(lock_);
    std::vector<CdgSong> result;
    if (!isOpen() || query.isEmpty()) return result;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kSearchFts, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    sqlite3_bind_text(stmt, 1, query.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 2, maxResults);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        result.push_back(rowToSong(stmt));

    sqlite3_finalize(stmt);
    return result;
}

std::vector<CdgSong> SongDatabase::searchPrefix(const juce::String& prefix,
                                                  int maxResults) const
{
    // FTS5 prefix query: append * to each word so "hel wor" → "hel* wor*"
    juce::StringArray words;
    words.addTokens(prefix.trim(), " ", "");

    juce::String ftsQuery;
    for (auto& w : words)
    {
        if (w.isNotEmpty())
        {
            if (ftsQuery.isNotEmpty()) ftsQuery += " ";
            ftsQuery += w + "*";
        }
    }

    return ftsQuery.isEmpty() ? std::vector<CdgSong>{} : search(ftsQuery, maxResults);
}

int SongDatabase::count() const
{
    juce::ScopedReadLock rl(lock_);
    if (!isOpen()) return 0;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM songs;", -1, &stmt, nullptr);
    int n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        n = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return n;
}

//==============================================================================
// Maintenance
//==============================================================================

bool SongDatabase::rebuildFts()
{
    juce::ScopedWriteLock wl(lock_);
    if (!isOpen()) return false;
    // FTS5 rebuild: delete shadow tables and repopulate from content table.
    return exec("INSERT INTO songs_fts(songs_fts) VALUES('rebuild');");
}

bool SongDatabase::vacuum()
{
    juce::ScopedWriteLock wl(lock_);
    if (!isOpen()) return false;
    return exec("VACUUM;");
}
