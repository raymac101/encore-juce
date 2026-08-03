/*
  ==============================================================================

    AnalyticsService.h

    Migration of Angular analytics.service.ts for the JUCE desktop app.

    Reads analytics data from the same Firestore collections used by Angular:
      archives/{venueId}/audit
      archives/{venueId}/sessions
      archives/{venueId}/members
      venues/{venueId}/membersAudit

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>

class AnalyticsService
{
public:
    struct TimeRange
    {
        juce::Time start;
        juce::Time end;
        juce::String label;
    };

    struct TopMember
    {
        juce::String memberName;
        int totalSongs = 0;
        juce::Time lastSung;
    };

    struct TopSong
    {
        juce::String songName;
        juce::String artistName;
        int timesPlayed = 0;
        juce::Time lastPlayed;
        std::vector<juce::String> playedByMembers;
    };

    struct DailyStats
    {
        juce::String date; // YYYY-MM-DD
        int totalMembers = 0;
        int totalSongs = 0;
        int newMembers = 0;
        int sessionDuration = 0; // minutes
    };

    struct WeeklyStats
    {
        juce::String week; // YYYY-WW
        int totalMembers = 0;
        int totalSongs = 0;
        int avgMembersPerNight = 0;
        int avgSongsPerNight = 0;
        juce::String topNight;
    };

    struct Bucket
    {
        juce::String name;
        int count = 0;
        int percentage = 0;
    };

    struct PeakHour
    {
        int hour = 0;
        int count = 0;
    };

    struct PerformanceMetrics
    {
        double averageSongsPerMember = 0.0;
        int averageSessionDuration = 0;
        std::vector<PeakHour> peakHours;
        std::vector<Bucket> sourceBreakdown;
    };

    struct Snapshot
    {
        std::vector<TopMember> topMembers;
        std::vector<TopSong> topSongs;
        std::vector<DailyStats> dailyStats;
        std::vector<WeeklyStats> weeklyStats;
        PerformanceMetrics performanceMetrics;
        std::vector<Bucket> sourceBreakdown;
        std::vector<Bucket> deviceBreakdown;
    };

    using LoadCallback = std::function<void(bool ok, Snapshot data, juce::String error)>;

    static AnalyticsService& getInstance();

    std::map<juce::String, TimeRange> getTimeRangePresets() const;

    void loadAnalytics(const juce::String& venueId,
                       const TimeRange& range,
                       LoadCallback onDone);

private:
    AnalyticsService() = default;

    JUCE_DECLARE_NON_COPYABLE(AnalyticsService)
};
