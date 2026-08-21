#include "PerformanceEventOutbox.h"

#include "FirestoreClient.h"
#include "UserPreferences.h"
#include "../Firebase/FirebaseConfig.h"

#include <cmath>

namespace
{
    constexpr int kRetryTimerMs = 10 * 1000;
    constexpr juce::int64 kInitialRetryMs = 5 * 1000;
    constexpr juce::int64 kMaximumRetryMs = 15 * 60 * 1000;

    juce::String functionUrl()
    {
        return "https://us-central1-" + FirebaseConfig::projectId
             + ".cloudfunctions.net/recordPerformanceEventV2";
    }

    juce::String canonicalPlatform(const QueueItem& item)
    {
        const auto supplied = juce::String(item.devicePlatform).trim().toLowerCase();
        if (supplied.contains("ios") || supplied.contains("iphone") || supplied.contains("ipad"))
            return "ios";
        if (supplied.contains("android"))
            return "android";
        if (supplied.contains("windows"))
            return "windows";
        if (supplied.contains("mac"))
            return "macos";

        const auto deviceId = juce::String(item.deviceId).trim().toLowerCase();
        if (deviceId == "local" || deviceId.contains("host") || deviceId.contains("encore"))
        {
           #if JUCE_MAC
            return "macos";
           #elif JUCE_WINDOWS
            return "windows";
           #else
            return "unknown";
           #endif
        }

        return "unknown";
    }

    juce::String canonicalSource(const QueueItem& item)
    {
        const auto deviceId = juce::String(item.deviceId).trim().toLowerCase();
        if (deviceId == "local" || deviceId.contains("host") || deviceId.contains("encore"))
            return "encore";
        return deviceId.isNotEmpty() ? "mobile" : "unknown";
    }

    juce::String venueGuestId(const juce::String& venueId,
                              const Singers& singer,
                              const QueueItem& item)
    {
        auto identifier = juce::String(singer.id).trim();
        if (identifier.isEmpty())
            identifier = juce::String(item.foxId).trim();
        if (identifier.isNotEmpty())
            return identifier;

        const auto identitySeed = venueId + "|" + juce::String(singer.name).trim().toLowerCase();
        return "guest-" + juce::MD5(identitySeed.toUTF8()).toHexString();
    }

    juce::String sqliteError(sqlite3* database)
    {
        return database != nullptr ? juce::String::fromUTF8(sqlite3_errmsg(database))
                                   : juce::String("database unavailable");
    }

    struct DeliveryResult
    {
        bool ok = false;
        juce::String error;
    };

    DeliveryResult deliverPayload(const juce::String& payloadJson)
    {
        DeliveryResult result;
        const auto idToken = FirestoreClient::getInstance().getFreshIdToken();
        if (idToken.isEmpty())
        {
            result.error = "No authenticated Firebase session";
            return result;
        }

        const auto payload = juce::JSON::parse(payloadJson);
        if (!payload.isObject())
        {
            result.error = "Outbox payload is invalid JSON";
            return result;
        }

        juce::DynamicObject::Ptr requestBody = new juce::DynamicObject();
        requestBody->setProperty("data", payload);
        const auto body = juce::JSON::toString(juce::var(requestBody.get()), false);

        int statusCode = 0;
        auto stream = juce::URL(functionUrl()).withPOSTData(body).createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                .withConnectionTimeoutMs(20000)
                .withHttpRequestCmd("POST")
                .withExtraHeaders("Content-Type: application/json\r\n"
                                  "Accept: application/json\r\n"
                                  "Authorization: Bearer " + idToken)
                .withStatusCode(&statusCode));

        if (stream == nullptr)
        {
            result.error = "Network error contacting recordPerformanceEventV2";
            return result;
        }

        const auto response = juce::JSON::parse(stream->readEntireStreamAsString());
        if (statusCode >= 200 && statusCode < 300 && response.isObject()
            && response.getProperty("result", {}).isObject())
        {
            const auto status = response.getProperty("result", {})
                                      .getProperty("status", "").toString();
            result.ok = status == "recorded" || status == "already_recorded";
            if (!result.ok)
                result.error = "Unexpected callable result: " + status;
            return result;
        }

        if (response.isObject() && response.hasProperty("error"))
        {
            const auto error = response.getProperty("error", {});
            result.error = error.getProperty("status", "UNKNOWN").toString()
                         + ": " + error.getProperty("message", "Request failed").toString();
        }
        else
        {
            result.error = "Callable failed with HTTP " + juce::String(statusCode);
        }
        return result;
    }
}

PerformanceEventOutbox& PerformanceEventOutbox::getInstance()
{
    static PerformanceEventOutbox instance;
    return instance;
}

PerformanceEventOutbox::~PerformanceEventOutbox()
{
    stop();
    close();
}

void PerformanceEventOutbox::start()
{
    if (!openIfNeeded())
        return;

    startTimer(kRetryTimerMs);
    flushAsync();
}

void PerformanceEventOutbox::stop()
{
    stopTimer();
}

bool PerformanceEventOutbox::openIfNeeded()
{
    const juce::ScopedLock lock(databaseLock_);
    if (database_ != nullptr)
        return true;

    auto directory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("EncoreKaraoke");
    if (!directory.createDirectory())
    {
        DBG("[AuditV2] could not create application data directory");
        return false;
    }

    const auto databaseFile = directory.getChildFile("audit-v2-outbox.db");
    if (sqlite3_open(databaseFile.getFullPathName().toRawUTF8(), &database_) != SQLITE_OK)
    {
        DBG("[AuditV2] SQLite open failed: " + sqliteError(database_));
        close();
        return false;
    }

    sqlite3_busy_timeout(database_, 5000);
    sqlite3_exec(database_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(database_, "PRAGMA synchronous=FULL;", nullptr, nullptr, nullptr);

    const char* schema = R"SQL(
CREATE TABLE IF NOT EXISTS performance_event_outbox (
    event_id        TEXT PRIMARY KEY,
    payload_json    TEXT NOT NULL,
    created_at_ms   INTEGER NOT NULL,
    attempts        INTEGER NOT NULL DEFAULT 0,
    next_attempt_ms INTEGER NOT NULL DEFAULT 0,
    last_error      TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_performance_event_outbox_due
    ON performance_event_outbox(next_attempt_ms, created_at_ms);
)SQL";
    char* error = nullptr;
    if (sqlite3_exec(database_, schema, nullptr, nullptr, &error) != SQLITE_OK)
    {
        DBG("[AuditV2] schema creation failed: " + juce::String::fromUTF8(error != nullptr ? error : "unknown"));
        sqlite3_free(error);
        close();
        return false;
    }

    return true;
}

void PerformanceEventOutbox::close()
{
    const juce::ScopedLock lock(databaseLock_);
    if (database_ != nullptr)
    {
        sqlite3_close_v2(database_);
        database_ = nullptr;
    }
}

bool PerformanceEventOutbox::insertEvent(const juce::String& eventId,
                                         const juce::String& payloadJson)
{
    const juce::ScopedLock lock(databaseLock_);
    if (database_ == nullptr)
        return false;

    sqlite3_stmt* statement = nullptr;
    const char* sql = "INSERT OR IGNORE INTO performance_event_outbox "
                      "(event_id, payload_json, created_at_ms, next_attempt_ms) VALUES (?, ?, ?, 0);";
    if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(statement, 1, eventId.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, payloadJson.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement, 3, (sqlite3_int64)juce::Time::currentTimeMillis());
    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

bool PerformanceEventOutbox::enqueuePerformance(const CdgSong& song,
                                                const Singers& singer,
                                                const QueueItem& item,
                                                const juce::String& venueId,
                                                juce::int64 startedAtMs,
                                                juce::int64 endedAtMs,
                                                bool naturalEnd)
{
    if (!openIfNeeded() || venueId.isEmpty() || endedAtMs < startedAtMs)
        return false;

    const auto userId = juce::String(item.profileId).trim();
    juce::DynamicObject::Ptr event = new juce::DynamicObject();
    const auto eventId = juce::Uuid().toString();
    auto songName = juce::String(song.songName).trim();
    if (songName.isEmpty())
        songName = juce::String(item.songName).trim();
    if (songName.isEmpty())
        songName = "Unknown Song";

    auto artist = juce::String(song.artistName).trim();
    if (artist.isEmpty())
        artist = juce::String(item.songArtist).trim();
    if (artist.isEmpty())
        artist = "Unknown Artist";

    auto songId = juce::String(song.id).trim();
    if (songId.isEmpty())
        songId = juce::String(item.songId).trim();
    if (songId.isEmpty())
        songId = "unknown-" + juce::MD5((artist + "|" + songName).toUTF8()).toHexString();

    auto singerStageName = juce::String(singer.name).trim();
    if (singerStageName.isEmpty())
        singerStageName = juce::String(item.singerName).trim();
    if (singerStageName.isEmpty())
        singerStageName = "Unknown Singer";

    event->setProperty("eventId", eventId);
    event->setProperty("venueId", venueId);
    event->setProperty("songId", songId);
    event->setProperty("songName", songName);
    event->setProperty("artist", artist);
    event->setProperty("songVersion", juce::String(item.songVersion));
    const int songDurationMs = song.durationMS > 0 ? song.durationMS : juce::jmax(0, item.duration * 1000);
    event->setProperty("songDurationMs", songDurationMs);
    event->setProperty("actualPlayedDurationMs", endedAtMs - startedAtMs);
    event->setProperty("completionReason", naturalEnd ? "completed" : "stopped");
    event->setProperty("userId", userId);
    event->setProperty("guestSingerId", userId.isEmpty() ? venueGuestId(venueId, singer, item) : juce::String());
    event->setProperty("singerStageName", singerStageName);
    event->setProperty("source", canonicalSource(item));
    event->setProperty("platform", canonicalPlatform(item));
    event->setProperty("deviceId", juce::String(item.deviceId));
    event->setProperty("encoreInstallationId", UserPreferences::getInstance().getDeviceId());
    event->setProperty("clientStartedAt", startedAtMs);
    event->setProperty("clientEndedAt", endedAtMs);

    const auto payloadJson = juce::JSON::toString(juce::var(event.get()), false);
    const bool inserted = insertEvent(eventId, payloadJson);
    if (inserted)
    {
        DBG("[AuditV2] queued performance event " + eventId);
        flushAsync();
    }
    else
    {
        DBG("[AuditV2] failed to persist performance event " + eventId);
    }
    return inserted;
}

int PerformanceEventOutbox::getPendingCount() const
{
    const juce::ScopedLock lock(databaseLock_);
    if (database_ == nullptr)
        return 0;

    sqlite3_stmt* statement = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(database_, "SELECT COUNT(*) FROM performance_event_outbox;",
                           -1, &statement, nullptr) == SQLITE_OK
        && sqlite3_step(statement) == SQLITE_ROW)
        count = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return count;
}

void PerformanceEventOutbox::timerCallback()
{
    flushAsync();
}

void PerformanceEventOutbox::flushAsync()
{
    if (deliveryInFlight_.exchange(true))
        return;

    juce::Thread::launch([this]
    {
        deliverNext();
        deliveryInFlight_ = false;
    });
}

bool PerformanceEventOutbox::readNextPending(PendingEvent& event)
{
    const juce::ScopedLock lock(databaseLock_);
    if (database_ == nullptr)
        return false;

    sqlite3_stmt* statement = nullptr;
    const char* sql = "SELECT event_id, payload_json, attempts FROM performance_event_outbox "
                      "WHERE next_attempt_ms <= ? ORDER BY created_at_ms LIMIT 1;";
    if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(statement, 1, (sqlite3_int64)juce::Time::currentTimeMillis());
    const bool found = sqlite3_step(statement) == SQLITE_ROW;
    if (found)
    {
        event.eventId = juce::String::fromUTF8((const char*)sqlite3_column_text(statement, 0));
        event.payloadJson = juce::String::fromUTF8((const char*)sqlite3_column_text(statement, 1));
        event.attempts = sqlite3_column_int(statement, 2);
    }
    sqlite3_finalize(statement);
    return found;
}

void PerformanceEventOutbox::deliverNext()
{
    PendingEvent event;
    if (!readNextPending(event))
        return;

    const auto result = deliverPayload(event.payloadJson);
    if (result.ok)
    {
        markDelivered(event.eventId);
        DBG("[AuditV2] delivered performance event " + event.eventId);
        juce::MessageManager::callAsync([this] { flushAsync(); });
    }
    else
    {
        markFailed(event, result.error);
        DBG("[AuditV2] delivery deferred for " + event.eventId + ": " + result.error);
    }
}

void PerformanceEventOutbox::markDelivered(const juce::String& eventId)
{
    const juce::ScopedLock lock(databaseLock_);
    sqlite3_stmt* statement = nullptr;
    if (database_ != nullptr
        && sqlite3_prepare_v2(database_, "DELETE FROM performance_event_outbox WHERE event_id=?;",
                              -1, &statement, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_text(statement, 1, eventId.toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
}

void PerformanceEventOutbox::markFailed(const PendingEvent& event, const juce::String& error)
{
    const auto exponent = juce::jmin(8, event.attempts);
    const auto baseDelay = juce::jmin(kMaximumRetryMs,
                                      kInitialRetryMs * (juce::int64)std::pow(2.0, exponent));
    const double jitter = 0.8 + juce::Random::getSystemRandom().nextDouble() * 0.4;
    const auto retryDelay = (juce::int64)((double)baseDelay * jitter);
    const auto nextAttempt = juce::Time::currentTimeMillis() + retryDelay;

    const juce::ScopedLock lock(databaseLock_);
    sqlite3_stmt* statement = nullptr;
    const char* sql = "UPDATE performance_event_outbox SET attempts=attempts+1, "
                      "next_attempt_ms=?, last_error=? WHERE event_id=?;";
    if (database_ != nullptr && sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int64(statement, 1, (sqlite3_int64)nextAttempt);
        sqlite3_bind_text(statement, 2, error.substring(0, 1000).toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, event.eventId.toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
}