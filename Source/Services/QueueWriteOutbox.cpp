/*
  ==============================================================================

    QueueWriteOutbox.cpp

  ==============================================================================
*/

#include "QueueWriteOutbox.h"

#include "QueueService.h"

#include <cmath>

namespace
{
    constexpr int         kRetryTimerMs   = 10 * 1000;
    constexpr juce::int64 kInitialRetryMs = 5 * 1000;
    constexpr juce::int64 kMaximumRetryMs = 5 * 60 * 1000;

    // A queue write is only meaningful for the show it was made during --
    // replaying last night's "add song" into tonight's rotation would be a
    // bug, not a recovery. Anything older than this is dropped.
    constexpr juce::int64 kEntryLifetimeMs = 8 * 60 * 60 * 1000;

    juce::String sqliteError (sqlite3* database)
    {
        return database != nullptr ? juce::String::fromUTF8 (sqlite3_errmsg (database))
                                   : juce::String ("database unavailable");
    }

    juce::String opToString (QueueWriteOutbox::Op op)
    {
        return op == QueueWriteOutbox::Op::removeSong ? "removeSong" : "appendSong";
    }

    QueueWriteOutbox::Op opFromString (const juce::String& s)
    {
        return s == "removeSong" ? QueueWriteOutbox::Op::removeSong
                                 : QueueWriteOutbox::Op::appendSong;
    }

    // QueueItem::toJson() drops profileId/foxId, and appendSong's whole
    // doc-ID policy keys off profileId -- so this serialises the fields the
    // replay actually needs rather than reusing that.
    juce::String itemToJson (const QueueItem& item)
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty ("id",             juce::String (item.id));
        obj->setProperty ("deviceId",       juce::String (item.deviceId));
        obj->setProperty ("devicePlatform", juce::String (item.devicePlatform));
        obj->setProperty ("profileId",      juce::String (item.profileId));
        obj->setProperty ("foxId",          juce::String (item.foxId));
        obj->setProperty ("singerName",     juce::String (item.singerName));
        obj->setProperty ("singerAvatar",   juce::String (item.singerAvatar));
        obj->setProperty ("songId",         juce::String (item.songId));
        obj->setProperty ("songName",       juce::String (item.songName));
        obj->setProperty ("songArtist",     juce::String (item.songArtist));
        obj->setProperty ("songVersion",    juce::String (item.songVersion));
        obj->setProperty ("status",         juce::String (item.status));
        obj->setProperty ("time",           juce::String (item.time));
        obj->setProperty ("duration",       item.duration);
        obj->setProperty ("order",          item.order);
        obj->setProperty ("songOrder",      item.songOrder);
        obj->setProperty ("pitch",          (double) item.pitch);
        obj->setProperty ("dateAdded",      (juce::int64) item.dateAdded);
        return juce::JSON::toString (juce::var (obj.get()), false);
    }

    QueueItem itemFromJson (const juce::String& json)
    {
        QueueItem item;
        const auto parsed = juce::JSON::parse (json);
        if (! parsed.isObject())
            return item;

        auto str = [&parsed] (const char* key)
        {
            return parsed.getProperty (key, "").toString().toStdString();
        };

        item.id             = str ("id");
        item.deviceId       = str ("deviceId");
        item.devicePlatform = str ("devicePlatform");
        item.profileId      = str ("profileId");
        item.foxId          = str ("foxId");
        item.singerName     = str ("singerName");
        item.singerAvatar   = str ("singerAvatar");
        item.songId         = str ("songId");
        item.songName       = str ("songName");
        item.songArtist     = str ("songArtist");
        item.songVersion    = str ("songVersion");
        item.status         = str ("status");
        item.time           = str ("time");
        item.duration       = (int) parsed.getProperty ("duration", 0);
        item.order          = (int) parsed.getProperty ("order", 0);
        item.songOrder      = (int) parsed.getProperty ("songOrder", 0);
        item.pitch          = (float) (double) parsed.getProperty ("pitch", 0.0);
        item.dateAdded      = (juce::int64) parsed.getProperty ("dateAdded", 0);
        return item;
    }
}

//==============================================================================
QueueWriteOutbox& QueueWriteOutbox::getInstance()
{
    static QueueWriteOutbox instance;
    return instance;
}

QueueWriteOutbox::~QueueWriteOutbox()
{
    stop();
    close();
}

void QueueWriteOutbox::start()
{
    if (! openIfNeeded())
        return;

    purgeExpired();
    startTimer (kRetryTimerMs);
    notifyPendingCount();
    flushAsync();
}

void QueueWriteOutbox::stop()
{
    stopTimer();
}

//==============================================================================
bool QueueWriteOutbox::openIfNeeded()
{
    const juce::ScopedLock lock (databaseLock_);
    if (database_ != nullptr)
        return true;

    auto directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("EncoreKaraoke");
    if (! directory.createDirectory())
    {
        DBG ("[QueueOutbox] could not create application data directory");
        return false;
    }

    const auto databaseFile = directory.getChildFile ("queue-write-outbox.db");
    if (sqlite3_open (databaseFile.getFullPathName().toRawUTF8(), &database_) != SQLITE_OK)
    {
        DBG ("[QueueOutbox] SQLite open failed: " + sqliteError (database_));
        close();
        return false;
    }

    sqlite3_busy_timeout (database_, 5000);
    sqlite3_exec (database_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec (database_, "PRAGMA synchronous=FULL;", nullptr, nullptr, nullptr);

    const char* schema = R"SQL(
CREATE TABLE IF NOT EXISTS queue_write_outbox (
    entry_id        TEXT PRIMARY KEY,
    op              TEXT NOT NULL,
    venue_id        TEXT NOT NULL,
    payload_json    TEXT NOT NULL,
    created_at_ms   INTEGER NOT NULL,
    attempts        INTEGER NOT NULL DEFAULT 0,
    next_attempt_ms INTEGER NOT NULL DEFAULT 0,
    last_error      TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_queue_write_outbox_due
    ON queue_write_outbox(next_attempt_ms, created_at_ms);
)SQL";

    char* error = nullptr;
    if (sqlite3_exec (database_, schema, nullptr, nullptr, &error) != SQLITE_OK)
    {
        DBG ("[QueueOutbox] schema creation failed: "
             + juce::String::fromUTF8 (error != nullptr ? error : "unknown"));
        sqlite3_free (error);
        close();
        return false;
    }

    return true;
}

void QueueWriteOutbox::close()
{
    const juce::ScopedLock lock (databaseLock_);
    if (database_ != nullptr)
    {
        sqlite3_close_v2 (database_);
        database_ = nullptr;
    }
}

//==============================================================================
bool QueueWriteOutbox::insertEntry (const juce::String& entryId, Op op,
                                    const juce::String& venueId,
                                    const juce::String& payloadJson)
{
    const juce::ScopedLock lock (databaseLock_);
    if (database_ == nullptr)
        return false;

    sqlite3_stmt* statement = nullptr;
    const char* sql = "INSERT OR IGNORE INTO queue_write_outbox "
                      "(entry_id, op, venue_id, payload_json, created_at_ms, next_attempt_ms) "
                      "VALUES (?, ?, ?, ?, ?, 0);";
    if (sqlite3_prepare_v2 (database_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return false;

    const auto opText = opToString (op);
    sqlite3_bind_text  (statement, 1, entryId.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (statement, 2, opText.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (statement, 3, venueId.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (statement, 4, payloadJson.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (statement, 5, (sqlite3_int64) juce::Time::currentTimeMillis());

    const bool ok = sqlite3_step (statement) == SQLITE_DONE;
    sqlite3_finalize (statement);
    return ok;
}

bool QueueWriteOutbox::enqueue (Op op, const juce::String& venueId, const QueueItem& item)
{
    if (venueId.isEmpty() || ! openIfNeeded())
        return false;

    // Keyed on the song's own UUID so the same failed write retried by the
    // caller can't stack up multiple copies of itself in here.
    auto entryId = juce::String (item.id).trim();
    if (entryId.isEmpty())
        entryId = juce::Uuid().toString();
    entryId = opToString (op) + ":" + entryId;

    const bool inserted = insertEntry (entryId, op, venueId, itemToJson (item));
    if (inserted)
    {
        DBG ("[QueueOutbox] queued " + opToString (op) + " for '"
             + juce::String (item.singerName) + "' -- will retry when the link returns");
        notifyPendingCount();
        flushAsync();
    }
    else
    {
        DBG ("[QueueOutbox] FAILED to persist " + opToString (op)
             + " for '" + juce::String (item.singerName) + "'");
    }
    return inserted;
}

int QueueWriteOutbox::getPendingCount() const
{
    const juce::ScopedLock lock (databaseLock_);
    if (database_ == nullptr)
        return 0;

    sqlite3_stmt* statement = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2 (database_, "SELECT COUNT(*) FROM queue_write_outbox;",
                            -1, &statement, nullptr) == SQLITE_OK
        && sqlite3_step (statement) == SQLITE_ROW)
        count = sqlite3_column_int (statement, 0);
    sqlite3_finalize (statement);
    return count;
}

void QueueWriteOutbox::notifyPendingCount()
{
    const int pending = getPendingCount();
    juce::MessageManager::callAsync ([this, pending]
    {
        if (onPendingCountChanged)
            onPendingCountChanged (pending);
    });
}

//==============================================================================
void QueueWriteOutbox::timerCallback()
{
    purgeExpired();
    flushAsync();
}

void QueueWriteOutbox::flushAsync()
{
    if (deliveryInFlight_.exchange (true))
        return;

    juce::Thread::launch ([this]
    {
        deliverNext();
        deliveryInFlight_ = false;
    });
}

bool QueueWriteOutbox::readNextPending (PendingWrite& write)
{
    const juce::ScopedLock lock (databaseLock_);
    if (database_ == nullptr)
        return false;

    sqlite3_stmt* statement = nullptr;
    const char* sql = "SELECT entry_id, op, venue_id, payload_json, attempts "
                      "FROM queue_write_outbox WHERE next_attempt_ms <= ? "
                      "ORDER BY created_at_ms LIMIT 1;";
    if (sqlite3_prepare_v2 (database_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64 (statement, 1, (sqlite3_int64) juce::Time::currentTimeMillis());

    const bool found = sqlite3_step (statement) == SQLITE_ROW;
    if (found)
    {
        write.entryId  = juce::String::fromUTF8 ((const char*) sqlite3_column_text (statement, 0));
        write.op       = opFromString (juce::String::fromUTF8 ((const char*) sqlite3_column_text (statement, 1)));
        write.venueId  = juce::String::fromUTF8 ((const char*) sqlite3_column_text (statement, 2));
        write.item     = itemFromJson (juce::String::fromUTF8 ((const char*) sqlite3_column_text (statement, 3)));
        write.attempts = sqlite3_column_int (statement, 4);
    }
    sqlite3_finalize (statement);
    return found;
}

void QueueWriteOutbox::deliverNext()
{
    PendingWrite write;
    if (! readNextPending (write))
        return;

    juce::String error;
    const bool ok = write.op == Op::appendSong
                  ? QueueService::getInstance().appendSongSync (write.venueId, write.item, true, error)
                  : QueueService::getInstance().removeSongSync (write.venueId, write.item, error);

    if (ok)
    {
        markDelivered (write.entryId);
        DBG ("[QueueOutbox] replayed " + write.entryId);
        notifyPendingCount();
        juce::MessageManager::callAsync ([this] { flushAsync(); });
    }
    else
    {
        markFailed (write, error);
        DBG ("[QueueOutbox] replay deferred for " + write.entryId + ": " + error);
    }
}

void QueueWriteOutbox::markDelivered (const juce::String& entryId)
{
    const juce::ScopedLock lock (databaseLock_);
    sqlite3_stmt* statement = nullptr;
    if (database_ != nullptr
        && sqlite3_prepare_v2 (database_, "DELETE FROM queue_write_outbox WHERE entry_id=?;",
                               -1, &statement, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_text (statement, 1, entryId.toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_step (statement);
    }
    sqlite3_finalize (statement);
}

void QueueWriteOutbox::markFailed (const PendingWrite& write, const juce::String& error)
{
    const auto exponent = juce::jmin (8, write.attempts);
    const auto baseDelay = juce::jmin (kMaximumRetryMs,
                                       kInitialRetryMs * (juce::int64) std::pow (2.0, exponent));
    const double jitter = 0.8 + juce::Random::getSystemRandom().nextDouble() * 0.4;
    const auto nextAttempt = juce::Time::currentTimeMillis() + (juce::int64) ((double) baseDelay * jitter);

    const juce::ScopedLock lock (databaseLock_);
    sqlite3_stmt* statement = nullptr;
    const char* sql = "UPDATE queue_write_outbox SET attempts=attempts+1, "
                      "next_attempt_ms=?, last_error=? WHERE entry_id=?;";
    if (database_ != nullptr && sqlite3_prepare_v2 (database_, sql, -1, &statement, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int64 (statement, 1, (sqlite3_int64) nextAttempt);
        sqlite3_bind_text  (statement, 2, error.substring (0, 1000).toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text  (statement, 3, write.entryId.toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_step (statement);
    }
    sqlite3_finalize (statement);
}

void QueueWriteOutbox::purgeExpired()
{
    int removed = 0;
    {
        const juce::ScopedLock lock (databaseLock_);
        if (database_ == nullptr)
            return;

        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2 (database_, "DELETE FROM queue_write_outbox WHERE created_at_ms < ?;",
                                -1, &statement, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_int64 (statement, 1,
                                (sqlite3_int64) (juce::Time::currentTimeMillis() - kEntryLifetimeMs));
            if (sqlite3_step (statement) == SQLITE_DONE)
                removed = sqlite3_changes (database_);
        }
        sqlite3_finalize (statement);
    }

    if (removed > 0)
    {
        DBG ("[QueueOutbox] dropped " + juce::String (removed) + " stale entr(ies)");
        notifyPendingCount();
    }
}
