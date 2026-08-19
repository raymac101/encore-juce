/*
  ==============================================================================

    ChartsPage.cpp

  ==============================================================================
*/

#include "ChartsPage.h"
#include "MenuTheme.h"
#include "../Services/VenueService.h"
#include "../Services/GlobalProgressService.h"
#include "../Localization/LocalizationManager.h"
#include <numeric>

namespace
{
    const juce::Colour kBorder(0x7A4CC9FF);
    const juce::Colour kText(0xffe7ebff);
    const juce::Colour kMuted(0xff99a8d6);
    const juce::Colour kCardBg(0xfff9fbff);
    const juce::Colour kCardBorder(0x26000000);
    const juce::Colour kCardTitle(0xff213560);

    juce::Colour palette(int idx)
    {
        static const juce::Colour c[] = {
            juce::Colour(0xff36a2eb), juce::Colour(0xffff6384), juce::Colour(0xffffce56),
            juce::Colour(0xff4bc0c0), juce::Colour(0xff9966ff), juce::Colour(0xffff9f40),
            juce::Colour(0xff9ad0f5), juce::Colour(0xff7bc96f), juce::Colour(0xfff67019),
            juce::Colour(0xffd45087)
        };
        return c[idx % (int) std::size(c)];
    }
}

class ChartsPage::BarChart : public juce::Component
{
public:
    BarChart()
    {
        setOpaque(true);
    }

    void setNoDataText(const juce::String& text)
    {
        noDataText_ = text;
        repaint();
    }

    void setData(const juce::String& title, std::vector<juce::String> labels,
                 std::vector<double> values, bool horizontal = false, bool line = false)
    {
        title_ = title;
        labels_ = std::move(labels);
        values_ = std::move(values);
        horizontal_ = horizontal;
        line_ = line;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill(juce::ColourGradient(kCardBg, r.getX(), r.getY(), juce::Colour(0xffedf3ff), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(kCardBorder);
        g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.0f);

        g.setColour(kCardTitle);
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)).boldened());
        g.drawFittedText(title_, getLocalBounds().removeFromTop(24), juce::Justification::centred, 1);

        auto area = getLocalBounds().reduced(10);
        area.removeFromTop(22);
        if (labels_.empty() || values_.empty())
        {
            g.setColour(juce::Colour(0xff7987ad));
            g.drawFittedText(noDataText_, area, juce::Justification::centred, 1);
            return;
        }

        const double maxV = juce::jmax(1.0, *std::max_element(values_.begin(), values_.end()));
        const int n = juce::jmin((int) labels_.size(), (int) values_.size());

        if (line_)
        {
            auto plot = area.reduced(38, 8).toFloat();
            plot.removeFromBottom(24.0f);

            const int maxMembers = juce::jmax(1, (int) std::ceil(maxV));
            const int tickStep = juce::jmax(1, (int) std::ceil((double) maxMembers / 4.0));
            g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));

            for (int tick = 0; tick <= maxMembers; tick += tickStep)
            {
                const float y = plot.getBottom() - (float) tick / (float) maxMembers * plot.getHeight();
                g.setColour(juce::Colour(0x1833456f));
                g.drawHorizontalLine((int) std::round(y), plot.getX(), plot.getRight());
                g.setColour(juce::Colour(0xff52658d));
                g.drawText(juce::String(tick), (int) plot.getX() - 34, (int) y - 7, 28, 14,
                           juce::Justification::centredRight);
            }

            g.setColour(juce::Colour(0xff52658d));
            g.drawLine(plot.getX(), plot.getY(), plot.getX(), plot.getBottom(), 1.0f);
            g.drawLine(plot.getX(), plot.getBottom(), plot.getRight(), plot.getBottom(), 1.0f);

            juce::Path path;
            std::vector<juce::Point<float>> points;
            points.reserve((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const float x = n == 1 ? plot.getCentreX()
                                       : plot.getX() + (float) i / (float) (n - 1) * plot.getWidth();
                const float y = plot.getBottom() - (float) (values_[(size_t) i] / maxV) * plot.getHeight();
                points.emplace_back(x, y);
                if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
            }

            g.setColour(palette(0));
            g.strokePath(path, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            for (int i = 0; i < n; ++i)
            {
                const auto point = points[(size_t) i];
                g.fillEllipse(point.x - 4.0f, point.y - 4.0f, 8.0f, 8.0f);

                g.setColour(juce::Colour(0xff33456f));
                g.drawText(juce::String((int) values_[(size_t) i]),
                           (int) point.x - 16, (int) point.y - 20, 32, 14,
                           juce::Justification::centred);

                const int labelWidth = juce::jmax(54, (int) plot.getWidth() / juce::jmax(1, n));
                g.drawFittedText(labels_[(size_t) i], (int) point.x - labelWidth / 2,
                                 (int) plot.getBottom() + 5, labelWidth, 18,
                                 juce::Justification::centred, 1);
                g.setColour(palette(0));
            }
        }
        else if (horizontal_)
        {
            const int rowH = juce::jmax(16, area.getHeight() / juce::jmax(1, n));
            for (int i = 0; i < n; ++i)
            {
                auto row = area.removeFromTop(rowH).reduced(0, 2);
                auto labelArea = row.removeFromLeft(juce::jmin(130, row.getWidth() / 2));
                g.setColour(juce::Colour(0xff33456f));
                g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
                g.drawFittedText(labels_[(size_t) i], labelArea, juce::Justification::centredLeft, 1);

                auto pct = values_[(size_t) i] / maxV;
                auto barW = (int) std::round((double) row.getWidth() * pct);
                g.setColour(palette(i).withAlpha(0.85f));
                g.fillRoundedRectangle(row.removeFromLeft(barW).toFloat(), 4.0f);
            }
        }
        else
        {
            const int gap = 6;
            const int w = juce::jmax(10, (area.getWidth() - gap * juce::jmax(0, n - 1)) / juce::jmax(1, n));
            int x = area.getX();
            for (int i = 0; i < n; ++i)
            {
                auto pct = values_[(size_t) i] / maxV;
                auto h = (int) std::round((double) area.getHeight() * pct);
                juce::Rectangle<int> bar(x, area.getBottom() - h, w, h);
                g.setColour(palette(i).withAlpha(0.85f));
                g.fillRoundedRectangle(bar.toFloat(), 4.0f);
                x += w + gap;
            }
        }
    }

private:
    juce::String title_;
    juce::String noDataText_ = "No data";
    std::vector<juce::String> labels_;
    std::vector<double> values_;
    bool horizontal_ = false;
    bool line_ = false;
};

class ChartsPage::PieChart : public juce::Component
{
public:
    PieChart()
    {
        setOpaque(true);
    }

    void setNoDataText(const juce::String& text)
    {
        noDataText_ = text;
        repaint();
    }

    void setData(const juce::String& title,
                 std::vector<juce::String> labels,
                 std::vector<double> values)
    {
        title_ = title;
        labels_ = std::move(labels);
        values_ = std::move(values);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill(juce::ColourGradient(kCardBg, r.getX(), r.getY(), juce::Colour(0xffedf3ff), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(kCardBorder);
        g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.0f);

        g.setColour(kCardTitle);
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)).boldened());
        g.drawFittedText(title_, getLocalBounds().removeFromTop(24), juce::Justification::centred, 1);

        auto area = getLocalBounds().reduced(10);
        area.removeFromTop(22);
        if (labels_.empty() || values_.empty())
        {
            g.setColour(juce::Colour(0xff7987ad));
            g.drawFittedText(noDataText_, area, juce::Justification::centred, 1);
            return;
        }

        auto pieArea = area.removeFromLeft(juce::jmin(area.getWidth() / 2, area.getHeight()));
        pieArea = pieArea.withSizeKeepingCentre(juce::jmin(pieArea.getWidth(), pieArea.getHeight()),
                                                juce::jmin(pieArea.getWidth(), pieArea.getHeight()));

        const double total = std::accumulate(values_.begin(), values_.end(), 0.0);
        if (total <= 0.0)
            return;

        float start = -juce::MathConstants<float>::halfPi;
        for (size_t i = 0; i < values_.size(); ++i)
        {
            const float angle = juce::MathConstants<float>::twoPi * (float) (values_[i] / total);
            juce::Path p;
            p.addPieSegment(pieArea.toFloat(), start, start + angle, 0.55f);
            g.setColour(palette((int) i).withAlpha(0.9f));
            g.fillPath(p);
            start += angle;
        }

        g.setColour(juce::Colour(0xff2f446f));
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        int y = area.getY();
        for (size_t i = 0; i < labels_.size(); ++i)
        {
            auto row = juce::Rectangle<int>(area.getX(), y, area.getWidth(), 16);
            g.setColour(palette((int) i));
            g.fillRoundedRectangle(row.removeFromLeft(10).toFloat(), 2.0f);
            row.removeFromLeft(6);
            g.setColour(juce::Colour(0xff33456f));
            g.drawFittedText(labels_[i], row, juce::Justification::centredLeft, 1);
            y += 18;
            if (y > area.getBottom() - 14)
                break;
        }
    }

private:
    juce::String title_;
    juce::String noDataText_ = "No data";
    std::vector<juce::String> labels_;
    std::vector<double> values_;
};

class ChartsPage::SortableTable : public juce::Component,
                                  private juce::ListBoxModel
{
public:
    enum class Kind { Members, Songs };

    SortableTable(Kind kind, juce::String title)
        : kind_(kind), title_(std::move(title)), list_("charts-sortable-table", this)
    {
        setOpaque(true);
        addAndMakeVisible(list_);
        list_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1f3564));
        list_.setColour(juce::ListBox::outlineColourId, kBorder);
        list_.setRowHeight(22);

        for (auto* button : { &firstHeader_, &secondHeader_, &thirdHeader_, &fourthHeader_ })
        {
            addAndMakeVisible(*button);
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f376b));
            button->setColour(juce::TextButton::textColourOffId, kText);
            button->onClick = [this, button]()
            {
                const int column = button == &firstHeader_ ? 0
                                 : button == &secondHeader_ ? 1
                                 : button == &thirdHeader_ ? 2 : 3;
                if (sortColumn_ == column)
                    ascending_ = ! ascending_;
                else
                {
                    sortColumn_ = column;
                    ascending_ = true;
                }
                resort();
            };
        }

        firstHeader_.setButtonText(kind_ == Kind::Members ? "Member" : "Song");
        secondHeader_.setButtonText(kind_ == Kind::Members ? "Songs" : "Artist");
        thirdHeader_.setButtonText(kind_ == Kind::Members ? "Last Sung" : "Plays");
        fourthHeader_.setButtonText(kind_ == Kind::Members ? "" : "Last Played");
        fourthHeader_.setEnabled(kind_ != Kind::Members);

        addAndMakeVisible(limitBox_);
        limitBox_.addItem("5", 1);
        limitBox_.addItem("10", 2);
        limitBox_.addItem("20", 3);
        limitBox_.addItem("50", 4);
        limitBox_.addItem("100", 5);
        limitBox_.setSelectedId(2, juce::dontSendNotification);
        limitBox_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1630));
        limitBox_.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        limitBox_.setColour(juce::ComboBox::outlineColourId, kBorder);
        limitBox_.onChange = [this]()
        {
            const int selectedId = limitBox_.getSelectedId();
            limit_ = selectedId == 1 ? 5
                   : selectedId == 2 ? 10
                   : selectedId == 3 ? 20
                   : selectedId == 4 ? 50 : 100;
            list_.updateContent();
              list_.setVerticalPosition(0.0);
            list_.repaint();
        };
    }

    void setMembers(std::vector<AnalyticsService::TopMember> rows)
    {
        members_ = std::move(rows);
        songs_.clear();
        resetSort();
    }

    void setSongs(std::vector<AnalyticsService::TopSong> rows)
    {
        songs_ = std::move(rows);
        members_.clear();
        resetSort();
    }

    void setTitle(const juce::String& title)
    {
        title_ = title;
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6);
        area.removeFromTop(22);
        auto top = area.removeFromTop(26);
        limitBox_.setBounds(top.removeFromRight(64));
        top.removeFromRight(6);
        top.removeFromLeft(38);
        firstHeader_.setBounds(top.removeFromLeft(kind_ == Kind::Members ? 170 : 210));
        secondHeader_.setBounds(top.removeFromLeft(kind_ == Kind::Members ? 70 : 150));
        thirdHeader_.setBounds(top.removeFromLeft(kind_ == Kind::Members ? 110 : 80));
        fourthHeader_.setBounds(top);
        area.removeFromTop(4);
        list_.setBounds(area);
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff1f3564));
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(kCardBorder);
        g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.0f);
        g.setColour(kText);
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)).boldened());
        g.drawFittedText(title_, getLocalBounds().removeFromTop(22), juce::Justification::centred, 1);
    }

private:
    int getNumRows() override
    {
        return juce::jmin(limit_, kind_ == Kind::Members ? (int) members_.size() : (int) songs_.size());
    }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (selected)
            g.fillAll(juce::Colour(0xff254a83));
        else if ((row & 1) != 0)
            g.fillAll(juce::Colour(0xff19325f));

        g.setColour(kText);
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        auto draw = [&g, height](const juce::String& text, int x, int w)
        {
            g.drawFittedText(text, juce::Rectangle<int>(x + 5, 0, w - 8, height),
                             juce::Justification::centredLeft, 1);
        };

        draw(juce::String(row + 1), 0, 38);
        if (kind_ == Kind::Members)
        {
            const auto& item = members_[(size_t) row];
            draw(item.memberName, 38, 170);
            draw(juce::String(item.totalSongs), 208, 70);
            draw(item.lastSung.formatted("%Y-%m-%d %H:%M"), 278, width - 278);
        }
        else
        {
            const auto& item = songs_[(size_t) row];
            draw(item.songName, 38, 210);
            draw(item.artistName, 248, 150);
            draw(juce::String(item.timesPlayed), 398, 80);
            draw(item.lastPlayed.formatted("%Y-%m-%d %H:%M"), 478, width - 478);
        }
    }

    void resetSort()
    {
        sortColumn_ = 2;
        ascending_ = false;
        resort();
    }

    void resort()
    {
        if (kind_ == Kind::Members)
        {
            std::sort(members_.begin(), members_.end(), [this](const auto& a, const auto& b)
            {
                int result = sortColumn_ == 0 ? a.memberName.compareIgnoreCase(b.memberName)
                            : sortColumn_ == 1 ? a.totalSongs - b.totalSongs
                            : a.lastSung < b.lastSung ? -1 : a.lastSung > b.lastSung ? 1 : 0;
                return ascending_ ? result < 0 : result > 0;
            });
        }
        else
        {
            std::sort(songs_.begin(), songs_.end(), [this](const auto& a, const auto& b)
            {
                int result = sortColumn_ == 0 ? a.songName.compareIgnoreCase(b.songName)
                            : sortColumn_ == 1 ? a.artistName.compareIgnoreCase(b.artistName)
                            : sortColumn_ == 2 ? a.timesPlayed - b.timesPlayed
                            : a.lastPlayed < b.lastPlayed ? -1 : a.lastPlayed > b.lastPlayed ? 1 : 0;
                return ascending_ ? result < 0 : result > 0;
            });
        }
        list_.updateContent();
        list_.repaint();
    }

    Kind kind_;
    juce::String title_;
    juce::ListBox list_;
    juce::TextButton firstHeader_, secondHeader_, thirdHeader_, fourthHeader_;
    juce::ComboBox limitBox_;
    std::vector<AnalyticsService::TopMember> members_;
    std::vector<AnalyticsService::TopSong> songs_;
    int limit_ = 10;
    int sortColumn_ = 2;
    bool ascending_ = false;
};

ChartsPage::~ChartsPage() = default;

juce::String ChartsPage::tr(const juce::String& key, const juce::String& fallback) const
{
    auto txt = LocalizationManager::getInstance().getText(key);
    if (txt.isEmpty() || txt.startsWith("["))
        return fallback;
    return txt;
}

ChartsPage::ChartsPage()
    : contentHolder_ (std::make_unique<ContentHolder>())
{
    setOpaque(true);
    addAndMakeVisible(viewport_);
    viewport_.setViewedComponent(contentHolder_.get(), false);
    viewport_.setScrollBarsShown(true, false);

    contentHolder_->addAndMakeVisible(titleLabel_);
    titleLabel_.setText(tr("charts.title", "Analytics Dashboard"), juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, kText);
    titleLabel_.setFont(juce::Font(juce::FontOptions().withHeight(24.0f)).boldened());

    contentHolder_->addAndMakeVisible(venueBox_);
    venueBox_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1630));
    venueBox_.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    venueBox_.setColour(juce::ComboBox::outlineColourId, kBorder);
    venueBox_.onChange = [this]()
    {
        const int idx = venueBox_.getSelectedItemIndex();
        if (idx >= 0 && idx < (int) venueIds_.size())
            refreshAnalytics();
    };

    contentHolder_->addAndMakeVisible(timeRangeBox_);
    rebuildTimeRangeOptions();
    timeRangeBox_.onChange = [this]()
    {
        const int idx = juce::jlimit(0, juce::jmax(0, (int) timeRangeKeys_.size() - 1), timeRangeBox_.getSelectedItemIndex());
        const bool custom = (! timeRangeKeys_.empty() && timeRangeKeys_[(size_t) idx] == "custom");
        customStartDate_.setVisible(custom);
        customEndDate_.setVisible(custom);
        customStartLabel_.setVisible(custom);
        customEndLabel_.setVisible(custom);
        refreshAnalytics();
    };

    contentHolder_->addAndMakeVisible(customStartLabel_);
    contentHolder_->addAndMakeVisible(customEndLabel_);
    contentHolder_->addAndMakeVisible(customStartDate_);
    contentHolder_->addAndMakeVisible(customEndDate_);
    customStartLabel_.setText(tr("charts.custom_start", "Start"), juce::dontSendNotification);
    customEndLabel_.setText(tr("charts.custom_end", "End"), juce::dontSendNotification);
    customStartDate_.setText("2026-01-01", juce::dontSendNotification);
    customEndDate_.setText(juce::Time::getCurrentTime().formatted("%Y-%m-%d"), juce::dontSendNotification);
    customStartDate_.onTextChange = [this]()
    {
        const int idx = juce::jlimit(0, juce::jmax(0, (int) timeRangeKeys_.size() - 1), timeRangeBox_.getSelectedItemIndex());
        if (! timeRangeKeys_.empty() && timeRangeKeys_[(size_t) idx] == "custom")
            refreshAnalytics();
    };
    customEndDate_.onTextChange = [this]()
    {
        const int idx = juce::jlimit(0, juce::jmax(0, (int) timeRangeKeys_.size() - 1), timeRangeBox_.getSelectedItemIndex());
        if (! timeRangeKeys_.empty() && timeRangeKeys_[(size_t) idx] == "custom")
            refreshAnalytics();
    };

    contentHolder_->addAndMakeVisible(refreshButton_);
    contentHolder_->addAndMakeVisible(exportButton_);
    refreshButton_.onClick = [this]() { refreshAnalytics(); };
    exportButton_.onClick = [this]() { exportData(); };
    refreshButton_.setButtonText(tr("charts.refresh", "Refresh"));
    exportButton_.setButtonText(tr("charts.export", "Export JSON"));

    refreshButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2f7bff));
    refreshButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    exportButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f8f63));
    exportButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    customStartDate_.setInputRestrictions(10, "0123456789-");
    customEndDate_.setInputRestrictions(10, "0123456789-");
    customStartDate_.setJustification(juce::Justification::centred);
    customEndDate_.setJustification(juce::Justification::centred);
    customStartDate_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d1630));
    customEndDate_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d1630));
    customStartDate_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    customEndDate_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    customStartDate_.setColour(juce::TextEditor::outlineColourId, kBorder);
    customEndDate_.setColour(juce::TextEditor::outlineColourId, kBorder);

    timeRangeBox_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1630));
    timeRangeBox_.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    timeRangeBox_.setColour(juce::ComboBox::outlineColourId, kBorder);

    contentHolder_->addAndMakeVisible(loadingLabel_);
    loadingLabel_.setColour(juce::Label::textColourId, kMuted);

    auto makeMetric = [this](juce::Label& lbl)
    {
        contentHolder_->addAndMakeVisible(lbl);
        lbl.setJustificationType(juce::Justification::centred);
        lbl.setColour(juce::Label::textColourId, juce::Colours::white);
        lbl.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1f3564));
        lbl.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)).boldened());
        lbl.setOpaque(true);
    };
    makeMetric(metricSongsPerMember_);
    makeMetric(metricSessionDuration_);
    makeMetric(metricActiveMembers_);
    makeMetric(metricSessionCount_);

    membersPerNightChart_ = std::make_unique<BarChart>();
    topMembersChart_ = std::make_unique<BarChart>();
    topSongsChart_ = std::make_unique<PieChart>();
    peakHoursLineChart_ = std::make_unique<BarChart>();
    sourceBreakdownChart_ = std::make_unique<PieChart>();
    deviceBreakdownChart_ = std::make_unique<PieChart>();

    contentHolder_->addAndMakeVisible(*membersPerNightChart_);
    contentHolder_->addAndMakeVisible(*topMembersChart_);
    contentHolder_->addAndMakeVisible(*topSongsChart_);
    contentHolder_->addAndMakeVisible(*peakHoursLineChart_);
    contentHolder_->addAndMakeVisible(*sourceBreakdownChart_);
    contentHolder_->addAndMakeVisible(*deviceBreakdownChart_);

    auto prepGroup = [this](juce::GroupComponent& g, const juce::String& title)
    {
        contentHolder_->addAndMakeVisible(g);
        g.setText(title);
        g.setColour(juce::GroupComponent::textColourId, kText);
        g.setColour(juce::GroupComponent::outlineColourId, kBorder);
    };
    prepGroup(dailyStatsGroup_, tr("charts.daily_stats", "Daily Statistics"));
    prepGroup(weeklyStatsGroup_, tr("charts.weekly_summary", "Weekly Summary"));
    prepGroup(insightsGroup_, tr("charts.insights", "Insights"));

    auto prepText = [this](juce::TextEditor& t)
    {
        contentHolder_->addAndMakeVisible(t);
        t.setMultiLine(true);
        t.setReadOnly(true);
        t.setScrollbarsShown(true);
        t.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff13264d));
        t.setColour(juce::TextEditor::textColourId, kText);
        t.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        t.setOpaque(true);
    };
    prepText(dailyStatsText_);
    prepText(weeklyStatsText_);
    prepText(insightsText_);

    const auto noData = tr("charts.no_data", "No data");
    membersPerNightChart_->setNoDataText(noData);
    topMembersChart_->setNoDataText(noData);
    topSongsChart_->setNoDataText(noData);
    peakHoursLineChart_->setNoDataText(noData);
    sourceBreakdownChart_->setNoDataText(noData);
    deviceBreakdownChart_->setNoDataText(noData);

    topMembersTable_ = std::make_unique<SortableTable>(SortableTable::Kind::Members,
                                                        tr("charts.top_members", "Top Members"));
    topSongsTable_ = std::make_unique<SortableTable>(SortableTable::Kind::Songs,
                                                      tr("charts.top_songs", "Top Songs"));
    contentHolder_->addAndMakeVisible(*topMembersTable_);
    contentHolder_->addAndMakeVisible(*topSongsTable_);

    customStartLabel_.setVisible(false);
    customEndLabel_.setVisible(false);
    customStartDate_.setVisible(false);
    customEndDate_.setVisible(false);

    // Decorative header panel, drawn against contentHolder_'s own bounds
    // (not ChartsPage's) so it scrolls along with the rest of the content.
    contentHolder_->onPaint = [this](juce::Graphics& g)
    {
        MenuTheme::drawHeaderPanel(g, contentHolder_->getLocalBounds().reduced(8).removeFromTop(74));
    };

    refreshVenueOptions();
}

void ChartsPage::paint(juce::Graphics& g)
{
    MenuTheme::drawPageBackground(g, getLocalBounds());
}

void ChartsPage::resized()
{
    viewport_.setBounds(getLocalBounds());

    const int startingWidth  = juce::jmax(900, viewport_.getWidth() - viewport_.getScrollBarThickness());
    const int startingHeight = juce::jmax(contentHolder_->getHeight(), 900);
    contentHolder_->setSize(startingWidth, startingHeight);
    layoutContent();

    // Grow to fit the insights panel at the bottom -- lets the viewport's
    // scrollbar reach it on any window size. layoutContent() only depends
    // on width, so a second pass at the corrected height reproduces the
    // same positions.
    const int neededHeight = insightsGroup_.getBottom() + 12;
    if (neededHeight != contentHolder_->getHeight())
    {
        contentHolder_->setSize(startingWidth, neededHeight);
        layoutContent();
    }
}

void ChartsPage::layoutContent()
{
    auto area = contentHolder_->getLocalBounds().reduced(12);

    auto header = area.removeFromTop(70).reduced(12, 8);
    titleLabel_.setBounds(header.removeFromLeft(230));
    header.removeFromLeft(8);
    venueBox_.setBounds(header.removeFromLeft(180));

    auto controls = header;
    timeRangeBox_.setBounds(controls.removeFromLeft(170));
    controls.removeFromLeft(8);
    customStartLabel_.setBounds(controls.removeFromLeft(36));
    customStartDate_.setBounds(controls.removeFromLeft(108));
    controls.removeFromLeft(6);
    customEndLabel_.setBounds(controls.removeFromLeft(28));
    customEndDate_.setBounds(controls.removeFromLeft(108));
    controls.removeFromLeft(8);
    refreshButton_.setBounds(controls.removeFromLeft(90));
    controls.removeFromLeft(6);
    exportButton_.setBounds(controls.removeFromLeft(110));
    controls.removeFromLeft(8);
    loadingLabel_.setBounds(controls);

    auto metrics = area.removeFromTop(58);
    const int gap = 8;
    const int w = (metrics.getWidth() - gap * 3) / 4;
    metricSongsPerMember_.setBounds(metrics.removeFromLeft(w).reduced(2));
    metrics.removeFromLeft(gap);
    metricSessionDuration_.setBounds(metrics.removeFromLeft(w).reduced(2));
    metrics.removeFromLeft(gap);
    metricActiveMembers_.setBounds(metrics.removeFromLeft(w).reduced(2));
    metrics.removeFromLeft(gap);
    metricSessionCount_.setBounds(metrics.removeFromLeft(w).reduced(2));

    area.removeFromTop(6);

    auto topTables = area.removeFromTop(280);
    const int colGap = 10;
    auto topMembersArea = topTables.removeFromLeft((topTables.getWidth() - colGap) / 2);
    topTables.removeFromLeft(colGap);
    topMembersTable_->setBounds(topMembersArea);
    topSongsTable_->setBounds(topTables);

    area.removeFromTop(8);

    auto chartsGrid = area.removeFromTop(420);
    auto left = chartsGrid.removeFromLeft((chartsGrid.getWidth() - colGap) / 2);
    chartsGrid.removeFromLeft(colGap);
    auto right = chartsGrid;

    membersPerNightChart_->setBounds(left.removeFromTop(136));
    left.removeFromTop(6);
    topMembersChart_->setBounds(left.removeFromTop(136));
    left.removeFromTop(6);
    topSongsChart_->setBounds(left.removeFromTop(136));

    peakHoursLineChart_->setBounds(right.removeFromTop(136));
    right.removeFromTop(6);
    sourceBreakdownChart_->setBounds(right.removeFromTop(136));
    right.removeFromTop(6);
    deviceBreakdownChart_->setBounds(right.removeFromTop(136));

    area.removeFromTop(8);

    auto tablesBottom = area.removeFromTop(220);
    auto bleft = tablesBottom.removeFromLeft((tablesBottom.getWidth() - colGap) / 2);
    tablesBottom.removeFromLeft(colGap);
    auto bright = tablesBottom;

    dailyStatsGroup_.setBounds(bleft);
    dailyStatsText_.setBounds(bleft.reduced(8, 22));

    weeklyStatsGroup_.setBounds(bright);
    weeklyStatsText_.setBounds(bright.reduced(8, 22));

    area.removeFromTop(8);
    // Fixed height, not "whatever's left" -- the content now grows to fit
    // its own needs rather than being squeezed into a given window size, so
    // there's no natural "remainder" to fill.
    auto insightsArea = area.removeFromTop(200);
    insightsGroup_.setBounds(insightsArea);
    insightsText_.setBounds(insightsArea.reduced(8, 22));
}

void ChartsPage::updateAllText()
{
    titleLabel_.setText(tr("charts.title", "Analytics Dashboard"), juce::dontSendNotification);
    customStartLabel_.setText(tr("charts.custom_start", "Start"), juce::dontSendNotification);
    customEndLabel_.setText(tr("charts.custom_end", "End"), juce::dontSendNotification);
    refreshButton_.setButtonText(tr("charts.refresh", "Refresh"));
    exportButton_.setButtonText(tr("charts.export", "Export JSON"));

    topMembersTable_->setTitle(tr("charts.top_members", "Top Members"));
    topSongsTable_->setTitle(tr("charts.top_songs", "Top Songs"));
    dailyStatsGroup_.setText(tr("charts.daily_stats", "Daily Statistics"));
    weeklyStatsGroup_.setText(tr("charts.weekly_summary", "Weekly Summary"));
    insightsGroup_.setText(tr("charts.insights", "Insights"));

    rebuildTimeRangeOptions();
}

void ChartsPage::rebuildTimeRangeOptions()
{
    presets_ = AnalyticsService::getInstance().getTimeRangePresets();

    const int prevId = timeRangeBox_.getSelectedId();

    struct Item { const char* key; const char* labelKey; const char* fallback; };
    const Item items[] = {
        { "today", "charts.range.today", "Today" },
        { "yesterday", "charts.range.yesterday", "Yesterday" },
        { "thisWeek", "charts.range.this_week", "This Week" },
        { "lastWeek", "charts.range.last_week", "Last Week" },
        { "thisMonth", "charts.range.this_month", "This Month" },
        { "lastMonth", "charts.range.last_month", "Last Month" },
        { "thisYear", "charts.range.this_year", "This Year" },
        { "allTime", "charts.range.all_time", "All Time" },
        { "custom", "charts.range.custom", "Custom Range" },
    };

    timeRangeBox_.clear();
    timeRangeKeys_.clear();
    int id = 1;
    for (auto& i : items)
    {
        timeRangeBox_.addItem(tr(i.labelKey, i.fallback), id++);
        timeRangeKeys_.push_back(i.key);
    }

    const int defaultId = (prevId > 0 && prevId <= (int) timeRangeKeys_.size()) ? prevId : 3;
    timeRangeBox_.setSelectedId(defaultId, juce::dontSendNotification); // thisWeek
}

void ChartsPage::refreshVenueOptions()
{
    const auto currentId = VenueService::getInstance().getCurrentVenueId();
    venueBox_.clear();
    venueIds_.clear();

    venueBox_.addItem(tr("charts.venue.loading", "Loading venues..."), 1);
    venueBox_.setSelectedId(1, juce::dontSendNotification);
    venueBox_.setEnabled(false);

    juce::Component::SafePointer<ChartsPage> safe(this);
    VenueService::getInstance().getVenues(
        [safe, currentId](bool ok, std::vector<VenueItem> venues, juce::String error)
        {
            if (safe == nullptr)
                return;

            safe->venueBox_.clear();
            safe->venueIds_.clear();

            if (! ok || venues.empty())
            {
                safe->venueBox_.addItem(safe->tr("charts.no_venues", "No accessible venues"), 1);
                safe->venueBox_.setSelectedId(1, juce::dontSendNotification);
                safe->venueBox_.setEnabled(false);
                safe->loadingLabel_.setText(error.isNotEmpty() ? error
                                             : safe->tr("charts.no_active_venue", "No active venue"),
                                             juce::dontSendNotification);
                return;
            }

            int selectedId = 1;
            int itemId = 1;
            for (const auto& venue : venues)
            {
                const auto id = juce::String(venue.id);
                if (id.isEmpty())
                    continue;

                safe->venueIds_.push_back(id);
                const auto label = juce::String(venue.name).trim()
                                 + (juce::String(venue.city).trim().isNotEmpty()
                                    ? " - " + juce::String(venue.city).trim() : juce::String());
                safe->venueBox_.addItem(label.isNotEmpty() ? label : id, itemId);
                if (id == currentId)
                    selectedId = itemId;
                ++itemId;
            }

            safe->venueBox_.setSelectedId(selectedId, juce::dontSendNotification);
            safe->venueBox_.setEnabled(! safe->venueIds_.empty());
            safe->refreshAnalytics();
        });
}

AnalyticsService::TimeRange ChartsPage::currentRange() const
{
    const int idx = juce::jlimit(0, juce::jmax(0, (int) timeRangeKeys_.size() - 1), timeRangeBox_.getSelectedItemIndex());
    const auto selectedKey = timeRangeKeys_.empty() ? juce::String("thisWeek") : timeRangeKeys_[(size_t) idx];

    if (selectedKey == "custom")
    {
        auto s = juce::Time::fromISO8601(customStartDate_.getText().trim() + "T00:00:00Z");
        auto e = juce::Time::fromISO8601(customEndDate_.getText().trim() + "T23:59:59Z");
        if (s.toMilliseconds() <= 0 || e.toMilliseconds() <= 0 || s > e)
            return presets_.at("thisWeek");
        return { s, e, tr("charts.range.custom", "Custom Range") };
    }

    auto it = presets_.find(selectedKey);
    if (it != presets_.end())
        return it->second;

    return presets_.at("thisWeek");
}

void ChartsPage::refreshAnalytics()
{
    const int idx = juce::jlimit(0, juce::jmax(0, (int) timeRangeKeys_.size() - 1), timeRangeBox_.getSelectedItemIndex());
    const auto selectedKey = timeRangeKeys_.empty() ? juce::String("thisWeek") : timeRangeKeys_[(size_t) idx];
    if (selectedKey == "custom")
    {
        auto s = juce::Time::fromISO8601(customStartDate_.getText().trim() + "T00:00:00Z");
        auto e = juce::Time::fromISO8601(customEndDate_.getText().trim() + "T23:59:59Z");
        if (s.toMilliseconds() <= 0 || e.toMilliseconds() <= 0 || s > e)
        {
            loadingLabel_.setText(tr("charts.invalid_custom_range", "Invalid custom date range (use YYYY-MM-DD)"), juce::dontSendNotification);
            return;
        }
    }

    const int venueIndex = venueBox_.getSelectedItemIndex();
    const auto venueId = (venueIndex >= 0 && venueIndex < (int) venueIds_.size())
                       ? venueIds_[(size_t) venueIndex]
                       : VenueService::getInstance().getCurrentVenueId();
    if (venueId.isEmpty())
    {
        loadingLabel_.setText(tr("charts.no_active_venue", "No active venue"), juce::dontSendNotification);
        return;
    }

    loading_ = true;
    loadingLabel_.setText(tr("charts.loading", "Loading analytics..."), juce::dontSendNotification);
    refreshButton_.setEnabled(false);

    const int taskId = GlobalProgressService::getInstance().beginTask(tr("charts.loading", "Loading analytics..."));
    juce::Component::SafePointer<ChartsPage> safe(this);
    AnalyticsService::getInstance().loadAnalytics(venueId, currentRange(),
        [safe, taskId](bool ok, AnalyticsService::Snapshot data, juce::String error)
        {
            GlobalProgressService::getInstance().endTask(taskId);

            if (safe == nullptr)
                return;

            safe->loading_ = false;
            safe->refreshButton_.setEnabled(true);

            if (! ok)
            {
                safe->loadingLabel_.setText(safe->tr("charts.load_failed", "Load failed") + ": " + error, juce::dontSendNotification);
                return;
            }

            safe->loadingLabel_.setText({}, juce::dontSendNotification);
            safe->applySnapshot(std::move(data));
        });
}

void ChartsPage::applySnapshot(AnalyticsService::Snapshot data)
{
    snapshot_ = std::move(data);

    topMembersTable_->setMembers(snapshot_.topMembers);
    topSongsTable_->setSongs(snapshot_.topSongs);

    metricSongsPerMember_.setText(tr("charts.metric.avg_songs_member", "Avg Songs/Member") + "\n"
                                  + juce::String(snapshot_.performanceMetrics.averageSongsPerMember, 1), juce::dontSendNotification);
    metricSessionDuration_.setText(tr("charts.metric.avg_session", "Avg Session") + "\n"
                                   + juce::String(snapshot_.performanceMetrics.averageSessionDuration)
                                   + tr("charts.minutes_suffix", "m"), juce::dontSendNotification);
    metricActiveMembers_.setText(tr("charts.metric.active_members", "Active Members") + "\n"
                                 + juce::String((int) snapshot_.topMembers.size()), juce::dontSendNotification);
    metricSessionCount_.setText(tr("charts.metric.sessions", "Sessions") + "\n"
                                + juce::String((int) snapshot_.dailyStats.size()), juce::dontSendNotification);

    {
        std::vector<juce::String> labels;
        std::vector<double> values;
        for (auto& d : snapshot_.dailyStats)
        {
            labels.push_back(d.date);
            values.push_back((double) d.totalMembers);
        }
        membersPerNightChart_->setData(tr("charts.chart.members_per_night", "Singers per Karaoke Night"),
                           std::move(labels), std::move(values), false, true);
    }

    {
        std::vector<juce::String> labels;
        std::vector<double> values;
        for (auto& m : snapshot_.topMembers)
        {
            labels.push_back(m.memberName);
            values.push_back((double) m.totalSongs);
        }
        topMembersChart_->setData(tr("charts.chart.top_members", "Top Members by Songs Sang"), std::move(labels), std::move(values), true);
    }

    {
        std::vector<juce::String> labels;
        std::vector<double> values;
        for (auto& s : snapshot_.topSongs)
        {
            labels.push_back(s.songName + " - " + s.artistName + " (" + juce::String(s.timesPlayed) + ")");
            values.push_back((double) s.timesPlayed);
        }
        topSongsChart_->setData(tr("charts.chart.top_songs", "Most Popular Songs"), std::move(labels), std::move(values));
    }

    {
        // Peak hours line chart for 8pm-2am (hours 20-23, 0-2)
        std::vector<juce::String> labels;
        std::vector<double> values;
        std::vector<int> peakHourRange = { 20, 21, 22, 23, 0, 1, 2 };
        
        for (int hour : peakHourRange)
        {
            // Find matching hour in peakHours data
            double count = 0.0;
            for (auto& p : snapshot_.performanceMetrics.peakHours)
            {
                if (p.hour == hour)
                {
                    count = (double) p.count;
                    break;
                }
            }
            
            // Format hour display
            juce::String hourLabel;
            if (hour < 12)
                hourLabel = hour == 0 ? "12 AM" : juce::String(hour) + " AM";
            else
                hourLabel = hour == 12 ? "12 PM" : juce::String(hour - 12) + " PM";
            
            labels.push_back(hourLabel);
            values.push_back(count);
        }
        peakHoursLineChart_->setData(tr("charts.chart.peak_hours_line", "Peak Hours (8PM-2AM)"), 
                                     std::move(labels), std::move(values), false, true);
    }

    {
        std::vector<juce::String> labels;
        std::vector<double> values;
        for (auto& b : snapshot_.sourceBreakdown)
        {
            labels.push_back(b.name + " (" + juce::String(b.percentage) + "%)");
            values.push_back((double) b.count);
        }
        sourceBreakdownChart_->setData(tr("charts.chart.source_breakdown", "Song Addition Source"), std::move(labels), std::move(values));
    }

    {
        std::vector<juce::String> labels;
        std::vector<double> values;
        for (auto& b : snapshot_.deviceBreakdown)
        {
            labels.push_back(b.name + " (" + juce::String(b.percentage) + "%)");
            values.push_back((double) b.count);
        }
        deviceBreakdownChart_->setData(tr("charts.chart.device_breakdown", "Device Types Used"), std::move(labels), std::move(values));
    }

    rebuildReportingText();
}

void ChartsPage::rebuildReportingText()
{
    juce::String daily;
        daily << tr("charts.col.date", "Date") << "\t"
            << tr("charts.col.members", "Members") << "\t"
            << tr("charts.col.songs", "Songs") << "\t"
            << tr("charts.col.duration", "Duration") << "\t"
            << tr("charts.metric.avg_songs_member", "Avg Songs/Member") << "\n";
    for (auto& d : snapshot_.dailyStats)
    {
        const double avg = d.totalMembers > 0 ? (double) d.totalSongs / (double) d.totalMembers : 0.0;
        daily << d.date << "\t"
              << juce::String(d.totalMembers) << "\t"
              << juce::String(d.totalSongs) << "\t"
              << juce::String(d.sessionDuration) << tr("charts.minutes_suffix", "m") << "\t"
              << juce::String(avg, 1) << "\n";
    }
    dailyStatsText_.setText(daily, juce::dontSendNotification);

    juce::String weekly;
        weekly << tr("charts.col.week", "Week") << "\t"
            << tr("charts.col.total_members", "Total Members") << "\t"
            << tr("charts.col.total_songs", "Total Songs") << "\t"
            << tr("charts.col.avg_members_night", "Avg Members/Night") << "\t"
            << tr("charts.col.avg_songs_night", "Avg Songs/Night") << "\t"
            << tr("charts.col.best_night", "Best Night") << "\n";
    for (auto& w : snapshot_.weeklyStats)
    {
        weekly << w.week << "\t"
               << juce::String(w.totalMembers) << "\t"
               << juce::String(w.totalSongs) << "\t"
               << juce::String(w.avgMembersPerNight) << "\t"
               << juce::String(w.avgSongsPerNight) << "\t"
               << w.topNight << "\n";
    }
    weeklyStatsText_.setText(weekly, juce::dontSendNotification);

    juce::String insights;
    insights << tr("charts.insight.peak_hours", "Peak Hours") << "\n";
    for (auto& p : snapshot_.performanceMetrics.peakHours)
        insights << "  " << juce::String(p.hour) << ":00 - " << juce::String(p.count) << " songs\n";

    insights << "\n" << tr("charts.insight.source_breakdown", "Source Breakdown") << "\n";
    for (auto& b : snapshot_.sourceBreakdown)
        insights << "  " << b.name << ": " << juce::String(b.count) << " (" << juce::String(b.percentage) << "%)\n";

    insights << "\n" << tr("charts.insight.device_breakdown", "Device Breakdown") << "\n";
    for (auto& b : snapshot_.deviceBreakdown)
        insights << "  " << b.name << ": " << juce::String(b.count) << " (" << juce::String(b.percentage) << "%)\n";

    insightsText_.setText(insights, juce::dontSendNotification);
}

void ChartsPage::exportData()
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("rangeLabel", currentRange().label);

    juce::Array<juce::var> members;
    for (auto& m : snapshot_.topMembers)
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty("memberName", m.memberName);
        o->setProperty("totalSongs", m.totalSongs);
        o->setProperty("lastSung", m.lastSung.toISO8601(true));
        members.add(juce::var(o.get()));
    }
    root->setProperty("topMembers", juce::var(members));

    juce::Array<juce::var> songs;
    for (auto& s : snapshot_.topSongs)
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty("songName", s.songName);
        o->setProperty("artistName", s.artistName);
        o->setProperty("timesPlayed", s.timesPlayed);
        o->setProperty("lastPlayed", s.lastPlayed.toISO8601(true));
        songs.add(juce::var(o.get()));
    }
    root->setProperty("topSongs", juce::var(songs));

    const auto jsonText = juce::JSON::toString(juce::var(root.get()), true);

    auto suggested = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("karaoke-analytics-" + juce::Time::getCurrentTime().formatted("%Y-%m-%d") + ".json");

    exportChooser_ = std::make_shared<juce::FileChooser>(tr("charts.save_dialog_title", "Save analytics JSON"), suggested, "*.json");
    exportChooser_->launchAsync(juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, jsonText](const juce::FileChooser& chooser)
        {
            auto out = chooser.getResult();
            if (out == juce::File())
                return;

            out.replaceWithText(jsonText);
            exportChooser_.reset();
        });
}
