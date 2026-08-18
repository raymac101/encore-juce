/*
  ==============================================================================

    ChartsPage.h

    JUCE migration of Angular Charts component.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Services/AnalyticsService.h"

class ChartsPage : public juce::Component
{
public:
    ChartsPage();
  ~ChartsPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateAllText();

private:
    class BarChart;
    class PieChart;
  class SortableTable;

    // Plain Component with a paint callback -- lets the decorative header
    // panel scroll along with the rest of the content (see LibraryPage for
    // the same pattern and rationale).
    class ContentHolder : public juce::Component
    {
    public:
        std::function<void(juce::Graphics&)> onPaint;
        void paint(juce::Graphics& g) override { if (onPaint) onPaint(g); }
    };

    std::unique_ptr<ContentHolder> contentHolder_;
    juce::Viewport viewport_;
    void layoutContent();

  juce::String tr(const juce::String& key, const juce::String& fallback) const;
    void rebuildTimeRangeOptions();
    void refreshVenueOptions();
    AnalyticsService::TimeRange currentRange() const;
    void refreshAnalytics();
    void applySnapshot(AnalyticsService::Snapshot data);
    void rebuildReportingText();
    void exportData();

    juce::Label titleLabel_;
    juce::ComboBox venueBox_;
    juce::ComboBox timeRangeBox_;
    juce::Label customStartLabel_;
    juce::Label customEndLabel_;
    juce::TextEditor customStartDate_;
    juce::TextEditor customEndDate_;
    juce::TextButton refreshButton_ { "Refresh" };
    juce::TextButton exportButton_ { "Export JSON" };
    juce::Label loadingLabel_;

    juce::Label metricSongsPerMember_;
    juce::Label metricSessionDuration_;
    juce::Label metricActiveMembers_;
    juce::Label metricSessionCount_;

    std::unique_ptr<BarChart> membersPerNightChart_;
    std::unique_ptr<BarChart> topMembersChart_;
    std::unique_ptr<PieChart> topSongsChart_;
    std::unique_ptr<BarChart> peakHoursChart_;
    std::unique_ptr<PieChart> sourceBreakdownChart_;
    std::unique_ptr<PieChart> deviceBreakdownChart_;

    std::unique_ptr<SortableTable> topMembersTable_;
    std::unique_ptr<SortableTable> topSongsTable_;
    juce::GroupComponent dailyStatsGroup_;
    juce::GroupComponent weeklyStatsGroup_;
    juce::GroupComponent insightsGroup_;

    juce::TextEditor dailyStatsText_;
    juce::TextEditor weeklyStatsText_;
    juce::TextEditor insightsText_;

    bool loading_ = false;
    AnalyticsService::Snapshot snapshot_;
    std::map<juce::String, AnalyticsService::TimeRange> presets_;
    std::vector<juce::String> timeRangeKeys_;
    std::vector<juce::String> venueIds_;
    std::shared_ptr<juce::FileChooser> exportChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChartsPage)
};
