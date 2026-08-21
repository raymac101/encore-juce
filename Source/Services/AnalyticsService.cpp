/*
  ==============================================================================

    AnalyticsService.cpp

  ==============================================================================
*/

#include "AnalyticsService.h"
#include "FirestoreClient.h"
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <set>

namespace
{
    using FC = FirestoreClient;
    using AS = AnalyticsService;

    juce::String valueAsString(const juce::var& v)
    {
        if (v.hasProperty("stringValue"))  return v.getProperty("stringValue", "").toString();
        if (v.hasProperty("integerValue")) return v.getProperty("integerValue", "").toString();
        if (v.hasProperty("doubleValue"))  return juce::String((double) v.getProperty("doubleValue", 0.0));
        if (v.hasProperty("booleanValue")) return (bool) v.getProperty("booleanValue", false) ? "true" : "false";
        return {};
    }

    juce::int64 valueAsInt64(const juce::var& v)
    {
        if (v.hasProperty("integerValue"))
            return v.getProperty("integerValue", "0").toString().getLargeIntValue();
        if (v.hasProperty("doubleValue"))
            return (juce::int64) (double) v.getProperty("doubleValue", 0.0);
        if (v.hasProperty("stringValue"))
            return v.getProperty("stringValue", "0").toString().getLargeIntValue();
        return 0;
    }

    double valueAsDouble(const juce::var& v)
    {
        if (v.hasProperty("doubleValue"))
            return (double) v.getProperty("doubleValue", 0.0);
        if (v.hasProperty("integerValue"))
            return (double) v.getProperty("integerValue", "0").toString().getLargeIntValue();
        if (v.hasProperty("stringValue"))
            return v.getProperty("stringValue", "0").toString().getDoubleValue();
        return 0.0;
    }

    juce::String fieldStr(const juce::var& fields, const juce::String& name)
    {
        return valueAsString(fields.getProperty(name, juce::var()));
    }

    juce::String firstFieldStr(const juce::var& fields,
                               std::initializer_list<const char*> names)
    {
        for (auto* name : names)
        {
            auto value = fieldStr(fields, name).trim();
            if (value.isNotEmpty())
                return value;
        }
        return {};
    }

    juce::int64 fieldInt(const juce::var& fields, const juce::String& name)
    {
        return valueAsInt64(fields.getProperty(name, juce::var()));
    }

    bool inRange(juce::int64 ts, const AS::TimeRange& r)
    {
        const auto start = r.start.toMilliseconds();
        const auto end = r.end.toMilliseconds();
        return ts >= start && ts <= end;
    }

    juce::String determineSource(const juce::String& sourceField, const juce::String& deviceId)
    {
        if (sourceField.isNotEmpty())
            return sourceField.toLowerCase();

        const auto d = deviceId.trim().toLowerCase();
        if (d.isEmpty() || d == "unknown")
            return "unknown";
        if (d == "local" || d.contains("host") || d.contains("encore"))
            return "host";
        return "phone";
    }

    juce::String determineDeviceType(const juce::String& devicePlatform,
                                     const juce::String& deviceId)
    {
        const auto platform = devicePlatform.trim().toLowerCase();
        if (platform.contains("ios") || platform.contains("iphone") || platform.contains("ipad")
            || platform.contains("apple")) return "ios";
        if (platform.contains("android")) return "android";
        if (platform.contains("desktop") || platform.contains("mac") || platform.contains("windows")
            || platform.contains("host")) return "host";

        const auto d = deviceId.trim().toLowerCase();
        if (d.isEmpty() || d == "unknown") return "unknown";
        if (d == "local" || d.contains("host") || d.contains("encore")) return "host";
        if (d.contains("iphone") || d.contains("ios") || d.contains("ipad")) return "ios";
        if (d.contains("android")) return "android";
        if (d.contains("tablet")) return "tablet";
        if (d.contains("mobile") || d.contains("phone")) return "mobile";
        return "mobile";
    }

    juce::String formatDeviceTypeName(const juce::String& key)
    {
        if (key == "host") return "Host/Desktop";
        if (key == "ios") return "iOS";
        if (key == "android") return "Android";
        if (key == "mobile") return "Mobile Device";
        if (key == "tablet") return "Tablet";
        return "Unknown";
    }

    struct AuditRow
    {
        juce::String songName;
        juce::String artist;
        juce::String singerId;
        juce::String singerName;
        juce::String source;
        juce::String deviceId;
        juce::String devicePlatform;
        juce::int64 date = 0;
    };

    struct SessionRow
    {
        juce::String sessionDate;
        juce::int64 startTime = 0;
        juce::int64 endTime = 0;
        int totalSongsPlayed = 0;
        int totalMembers = 0;
    };

    struct MemberRow
    {
        juce::String id;
        juce::String singerName;
        juce::String profileId;
        juce::int64 firstDate = 0;
    };

    juce::String dateKeyFromMs(juce::int64 ms);

    juce::int64 reportingNightTimestamp(juce::int64 timestamp)
    {
        const auto localTime = juce::Time(timestamp);
        return localTime.getHours() < 5
                 ? timestamp - 24LL * 60LL * 60LL * 1000LL
                 : timestamp;
    }

    juce::String singerKey(const AuditRow& audit)
    {
        if (audit.singerId.isNotEmpty())
            return audit.singerId;

        const auto name = audit.singerName.trim().toLowerCase();
        return name.isNotEmpty() ? "name:" + name : juce::String();
    }

    std::vector<SessionRow> buildSessionsFromAudits(const std::vector<AuditRow>& audits)
    {
        struct Agg
        {
            juce::int64 first = std::numeric_limits<juce::int64>::max();
            juce::int64 last = 0;
            std::set<juce::String> members;
            int songs = 0;
        };

        std::map<juce::String, Agg> byNight;
        for (auto& a : audits)
        {
            if (a.date <= 0)
                continue;

            // A reporting night runs until 5 AM, so activity after midnight
            // remains attached to the previous evening (e.g. 8 PM-1 AM).
            auto key = dateKeyFromMs(reportingNightTimestamp(a.date));

            auto& agg = byNight[key];
            agg.first = juce::jmin(agg.first, a.date);
            agg.last = juce::jmax(agg.last, a.date);
            const auto memberKey = singerKey(a);
            if (memberKey.isNotEmpty())
                agg.members.insert(memberKey);
            agg.songs += 1;
        }

        std::vector<SessionRow> out;
        out.reserve(byNight.size());
        for (auto& kv : byNight)
        {
            SessionRow r;
            r.sessionDate = kv.first;
            r.startTime = kv.second.first == std::numeric_limits<juce::int64>::max() ? 0 : kv.second.first;
            r.endTime = kv.second.last;
            r.totalSongsPlayed = kv.second.songs;
            r.totalMembers = (int) kv.second.members.size();
            out.push_back(std::move(r));
        }

        return out;
    }

    std::vector<AuditRow> readAudits(const juce::String& venueId, const AS::TimeRange& range)
    {
        std::vector<AuditRow> out;
        auto liveDocs = FC::getInstance().listCollection("venues/" + venueId + "/audit", 5000);
        auto archivedDocs = FC::getInstance().listCollection("archives/" + venueId + "/audit", 5000);
        out.reserve((size_t) liveDocs.size() + (size_t) archivedDocs.size());
        std::set<juce::String> seenDocumentIds;

        auto appendDocuments = [&](const juce::Array<juce::var>& docs)
        {
            for (auto& d : docs)
            {
                const auto documentId = d.getProperty("name", "").toString()
                                           .fromLastOccurrenceOf("/", false, false);
                if (documentId.isNotEmpty() && ! seenDocumentIds.insert(documentId).second)
                    continue;

                auto f = d.getProperty("fields", juce::var());
                AuditRow r;
                r.songName = fieldStr(f, "songName");
                r.artist = fieldStr(f, "artist");
                r.singerId = firstFieldStr(f, { "singerId", "profileId", "userId", "memberId" });
                r.singerName = firstFieldStr(f, { "singerName", "memberName", "name" });
                r.source = fieldStr(f, "source");
                r.deviceId = fieldStr(f, "deviceId");
                r.devicePlatform = fieldStr(f, "devicePlatform");
                r.date = fieldInt(f, "date");
                if (r.date <= 0)
                    r.date = fieldInt(f, "dateTime");
                if (inRange(reportingNightTimestamp(r.date), range))
                    out.push_back(std::move(r));
            }
        };

        appendDocuments(archivedDocs);
        appendDocuments(liveDocs);

        std::sort(out.begin(), out.end(), [](const AuditRow& a, const AuditRow& b) { return a.date > b.date; });
        return out;
    }

    std::vector<SessionRow> readSessions(const juce::String& venueId, const AS::TimeRange& range)
    {
        std::vector<SessionRow> out;
        auto docs = FC::getInstance().listCollection("archives/" + venueId + "/sessions", 2000);
        out.reserve((size_t) docs.size());

        for (auto& d : docs)
        {
            auto f = d.getProperty("fields", juce::var());
            SessionRow r;
            r.sessionDate = fieldStr(f, "sessionDate");
            r.startTime = fieldInt(f, "startTime");
            r.endTime = fieldInt(f, "endTime");
            r.totalSongsPlayed = (int) fieldInt(f, "totalSongsPlayed");
            r.totalMembers = (int) fieldInt(f, "totalMembers");
            if (inRange(r.startTime, range))
                out.push_back(std::move(r));
        }

        std::sort(out.begin(), out.end(), [](const SessionRow& a, const SessionRow& b) { return a.startTime > b.startTime; });
        return out;
    }

    std::vector<MemberRow> readMembers(const juce::String& venueId, const AS::TimeRange& range)
    {
        std::map<juce::String, MemberRow> unique;

        auto archived = FC::getInstance().listCollection("archives/" + venueId + "/members", 5000);
        for (auto& d : archived)
        {
            auto f = d.getProperty("fields", juce::var());
            MemberRow r;
            r.id = d.getProperty("name", "").toString().fromLastOccurrenceOf("/", false, false);
            r.singerName = fieldStr(f, "singerName");
            r.profileId = fieldStr(f, "profileId");
            r.firstDate = fieldInt(f, "firstDate");
            if (r.firstDate <= range.end.toMilliseconds())
                unique[r.id] = r;
        }

        auto current = FC::getInstance().listCollection("venues/" + venueId + "/membersAudit", 2000);
        for (auto& d : current)
        {
            auto f = d.getProperty("fields", juce::var());
            MemberRow r;
            r.id = d.getProperty("name", "").toString().fromLastOccurrenceOf("/", false, false);
            r.singerName = fieldStr(f, "singerName");
            r.profileId = fieldStr(f, "profileId");
            r.firstDate = fieldInt(f, "firstDate");
            unique[r.id] = r;
        }

        std::vector<MemberRow> out;
        out.reserve(unique.size());
        for (auto& kv : unique)
            out.push_back(std::move(kv.second));
        return out;
    }

    int weekNumber(const juce::Time& t)
    {
        auto y = t.getYear();
        juce::Time jan1(1, 1, y, 0, 0);
        auto pastDays = (int) ((t.toMilliseconds() - jan1.toMilliseconds()) / (1000LL * 60LL * 60LL * 24LL));
        return (int) std::ceil((double) (pastDays + jan1.getDayOfWeek() + 1) / 7.0);
    }

    juce::String dateKeyFromMs(juce::int64 ms)
    {
        auto t = juce::Time(ms);
        return juce::String(t.getYear()) + "-"
             + juce::String(t.getMonth() + 1).paddedLeft('0', 2) + "-"
             + juce::String(t.getDayOfMonth()).paddedLeft('0', 2);
    }
}

AnalyticsService& AnalyticsService::getInstance()
{
    static AnalyticsService instance;
    return instance;
}

std::map<juce::String, AnalyticsService::TimeRange> AnalyticsService::getTimeRangePresets() const
{
    std::map<juce::String, TimeRange> out;

    const auto now = juce::Time::getCurrentTime();
    juce::Time today(now.getDayOfMonth(), now.getMonth() + 1, now.getYear(), 0, 0);

    auto endOfDay = [](const juce::Time& dayStart)
    {
        return juce::Time(dayStart.toMilliseconds() + (24LL * 60LL * 60LL * 1000LL) - 1);
    };

    auto startOfWeek = [](juce::Time t)
    {
        const int dow = t.getDayOfWeek();
        const int toSunday = dow % 7;
        return juce::Time(t.toMilliseconds() - (juce::int64) toSunday * 24LL * 60LL * 60LL * 1000LL);
    };

    auto endOfWeek = [&](juce::Time t)
    {
        auto s = startOfWeek(t);
        return juce::Time(s.toMilliseconds() + (7LL * 24LL * 60LL * 60LL * 1000LL) - 1);
    };

    out["today"] = { today, endOfDay(today), "Today" };
    out["yesterday"] = {
        juce::Time(today.toMilliseconds() - 24LL * 60LL * 60LL * 1000LL),
        juce::Time(today.toMilliseconds() - 1),
        "Yesterday"
    };
    out["thisWeek"] = { startOfWeek(now), now, "This Week" };
    auto lastWeekProbe = juce::Time(now.toMilliseconds() - 7LL * 24LL * 60LL * 60LL * 1000LL);
    out["lastWeek"] = { startOfWeek(lastWeekProbe), endOfWeek(lastWeekProbe), "Last Week" };

    juce::Time thisMonthStart(1, now.getMonth() + 1, now.getYear(), 0, 0);
    out["thisMonth"] = { thisMonthStart, now, "This Month" };

    int lastMonth = now.getMonth();
    int lastMonthYear = now.getYear();
    if (lastMonth < 1)
    {
        lastMonth = 12;
        --lastMonthYear;
    }
    juce::Time lastMonthStart(1, lastMonth, lastMonthYear, 0, 0);
    juce::Time thisMonthStartForEnd(1, now.getMonth() + 1, now.getYear(), 0, 0);
    out["lastMonth"] = { lastMonthStart, juce::Time(thisMonthStartForEnd.toMilliseconds() - 1), "Last Month" };

    juce::Time yearStart(1, 1, now.getYear(), 0, 0);
    out["thisYear"] = { yearStart, now, "This Year" };

    juce::Time allTimeStart(1, 1, 2020, 0, 0);
    out["allTime"] = { allTimeStart, now, "All Time" };

    return out;
}

void AnalyticsService::loadAnalytics(const juce::String& venueId,
                                     const TimeRange& range,
                                     LoadCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone)
            juce::MessageManager::callAsync([onDone]() { onDone(false, {}, "No venue selected"); });
        return;
    }

    juce::Thread::launch([venueId, range, onDone = std::move(onDone)]()
    {
        Snapshot snap;

        try
        {
            auto audits = readAudits(venueId, range);
            auto sessions = readSessions(venueId, range);
            // Audits retain each singer and timestamp, so they can correctly
            // merge after-midnight activity into the preceding karaoke night.
            if (! audits.empty())
                sessions = buildSessionsFromAudits(audits);
            auto members = readMembers(venueId, range);

            std::unordered_map<juce::String, juce::String> memberNamesByProfile;
            memberNamesByProfile.reserve(members.size());
            for (auto& m : members)
                memberNamesByProfile[m.profileId] = m.singerName;

            // Top members
            struct MemberAgg { int songs = 0; juce::int64 last = 0; juce::String name; };
            std::unordered_map<juce::String, MemberAgg> memberAgg;
            for (auto& a : audits)
            {
                const auto nameKey = a.singerName.trim().toLowerCase();
                auto key = a.singerId.isNotEmpty() ? a.singerId
                         : nameKey.isNotEmpty() ? "name:" + nameKey
                                                : juce::String("Unknown");
                auto& agg = memberAgg[key];
                agg.songs += 1;
                agg.last = juce::jmax(agg.last, a.date);
                auto it = memberNamesByProfile.find(key);
                if (a.singerName.isNotEmpty())
                    agg.name = a.singerName;
                else if (it != memberNamesByProfile.end() && it->second.isNotEmpty())
                    agg.name = it->second;
                else
                    agg.name = "Unknown (" + key + ")";
            }
            for (auto& kv : memberAgg)
            {
                TopMember tm;
                tm.memberName = kv.second.name;
                tm.totalSongs = kv.second.songs;
                tm.lastSung = juce::Time(kv.second.last);
                snap.topMembers.push_back(std::move(tm));
            }
            std::sort(snap.topMembers.begin(), snap.topMembers.end(), [](const TopMember& a, const TopMember& b) { return a.totalSongs > b.totalSongs; });

            // Top songs
            struct SongAgg { int plays = 0; juce::int64 last = 0; std::set<juce::String> singers; juce::String song; juce::String artist; };
            std::unordered_map<juce::String, SongAgg> songAgg;
            for (auto& a : audits)
            {
                auto key = a.songName + " - " + a.artist;
                auto& agg = songAgg[key];
                agg.song = a.songName;
                agg.artist = a.artist;
                agg.plays += 1;
                agg.last = juce::jmax(agg.last, a.date);
                auto it = memberNamesByProfile.find(a.singerId);
                agg.singers.insert((it != memberNamesByProfile.end() && it->second.isNotEmpty())
                    ? it->second
                    : ("Unknown (" + a.singerId + ")"));
            }
            for (auto& kv : songAgg)
            {
                TopSong ts;
                ts.songName = kv.second.song;
                ts.artistName = kv.second.artist;
                ts.timesPlayed = kv.second.plays;
                ts.lastPlayed = juce::Time(kv.second.last);
                for (auto& s : kv.second.singers)
                    ts.playedByMembers.push_back(s);
                snap.topSongs.push_back(std::move(ts));
            }
            std::sort(snap.topSongs.begin(), snap.topSongs.end(), [](const TopSong& a, const TopSong& b) { return a.timesPlayed > b.timesPlayed; });

            // Daily stats
            for (auto& s : sessions)
            {
                DailyStats d;
                d.date = s.sessionDate;
                d.totalMembers = s.totalMembers;
                d.totalSongs = s.totalSongsPlayed;
                d.newMembers = 0;
                d.sessionDuration = (int) std::round((double) juce::jmax<juce::int64>(0, s.endTime - s.startTime) / 60000.0);
                snap.dailyStats.push_back(std::move(d));
            }
            std::sort(snap.dailyStats.begin(), snap.dailyStats.end(), [](const DailyStats& a, const DailyStats& b) { return a.date < b.date; });

            // Weekly stats from daily
            std::map<juce::String, std::vector<DailyStats>> weekly;
            for (auto& d : snap.dailyStats)
            {
                auto t = juce::Time::fromISO8601(d.date + "T00:00:00Z");
                auto wk = juce::String(t.getYear()) + "-" + juce::String(weekNumber(t)).paddedLeft('0', 2);
                weekly[wk].push_back(d);
            }
            for (auto& kv : weekly)
            {
                WeeklyStats w;
                w.week = kv.first;
                int totalMembers = 0;
                int totalSongs = 0;
                DailyStats topDay;
                for (auto& d : kv.second)
                {
                    totalMembers += d.totalMembers;
                    totalSongs += d.totalSongs;
                    if (d.totalMembers >= topDay.totalMembers)
                        topDay = d;
                }
                w.totalMembers = totalMembers;
                w.totalSongs = totalSongs;
                w.avgMembersPerNight = (int) std::round((double) totalMembers / (double) juce::jmax(1, (int) kv.second.size()));
                w.avgSongsPerNight = (int) std::round((double) totalSongs / (double) juce::jmax(1, (int) kv.second.size()));
                w.topNight = topDay.date;
                snap.weeklyStats.push_back(std::move(w));
            }

            // Performance + source/device breakdown
            const int totalAuditSongs = (int) audits.size();
            std::set<juce::String> uniqueMembers;
            for (auto& a : audits)
                if (a.singerId.isNotEmpty()) uniqueMembers.insert(a.singerId);
            snap.performanceMetrics.averageSongsPerMember = uniqueMembers.empty()
                ? 0.0 : (double) totalAuditSongs / (double) uniqueMembers.size();

            juce::int64 totalSessionMs = 0;
            for (auto& s : sessions)
                totalSessionMs += juce::jmax<juce::int64>(0, s.endTime - s.startTime);
            snap.performanceMetrics.averageSessionDuration = sessions.empty()
                ? 0 : (int) std::round((double) totalSessionMs / (double) sessions.size() / 60000.0);

            std::map<int, int> hourCounts;
            std::map<juce::String, int> sourceCounts;
            std::map<juce::String, int> deviceCounts;
            for (auto& a : audits)
            {
                auto hour = juce::Time(a.date).getHours();
                hourCounts[hour] += 1;

                auto src = determineSource(a.source, a.deviceId);
                sourceCounts[src] += 1;

                auto dev = determineDeviceType(a.devicePlatform, a.deviceId);
                deviceCounts[dev] += 1;
            }

            for (auto& kv : hourCounts)
                snap.performanceMetrics.peakHours.push_back({ kv.first, kv.second });
            std::sort(snap.performanceMetrics.peakHours.begin(), snap.performanceMetrics.peakHours.end(),
                [](const PeakHour& a, const PeakHour& b) { return a.count > b.count; });
            if ((int) snap.performanceMetrics.peakHours.size() > 5)
                snap.performanceMetrics.peakHours.resize(5);

            auto total = juce::jmax(1, totalAuditSongs);
            for (auto& kv : sourceCounts)
            {
                Bucket b;
                b.name = kv.first.substring(0, 1).toUpperCase() + kv.first.substring(1);
                b.count = kv.second;
                b.percentage = (int) std::round((double) kv.second * 100.0 / (double) total);
                snap.sourceBreakdown.push_back(b);
                snap.performanceMetrics.sourceBreakdown.push_back(b);
            }
            std::sort(snap.sourceBreakdown.begin(), snap.sourceBreakdown.end(), [](const Bucket& a, const Bucket& b) { return a.count > b.count; });

            for (auto& kv : deviceCounts)
            {
                Bucket b;
                b.name = formatDeviceTypeName(kv.first);
                b.count = kv.second;
                b.percentage = (int) std::round((double) kv.second * 100.0 / (double) total);
                snap.deviceBreakdown.push_back(std::move(b));
            }
            std::sort(snap.deviceBreakdown.begin(), snap.deviceBreakdown.end(), [](const Bucket& a, const Bucket& b) { return a.count > b.count; });

            if (onDone)
            {
                juce::MessageManager::callAsync([onDone, snap = std::move(snap)]() mutable
                {
                    onDone(true, std::move(snap), {});
                });
            }
        }
        catch (const std::exception& ex)
        {
            if (onDone)
                juce::MessageManager::callAsync([onDone, msg = juce::String(ex.what())]() { onDone(false, {}, msg); });
        }
    });
}
