/*
  ==============================================================================

    QueueBar.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Implementation of the right-side singer queue bar.

  ==============================================================================
*/

#include "QueueBar.h"

//==============================================================================
// Helper – microphone emoji repeat
static juce::String getMicIcons(int count)
{
    juce::String s;
    // Use a simple Unicode music note since emoji rendering is font-dependent
    for (int i = 0; i < count; ++i)
        s += juce::String::fromUTF8("\xF0\x9F\x8E\xA4");  // 🎤
    return s;
}

//==============================================================================
//  NowPlayingCard
//==============================================================================
QueueBar::NowPlayingCard::NowPlayingCard() {}

void QueueBar::NowPlayingCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(4);

    // Primary colour background (like Angular .card-top)
    g.setColour(juce::Colour(0xff30daff));
    g.fillRoundedRectangle(bounds.toFloat(), 6.f);

    if (!hasSinger)
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawText("--", bounds, juce::Justification::centred);
        return;
    }

    int avatarSize = bounds.getHeight() - 8;
    auto avatarRect = bounds.removeFromLeft(avatarSize + 8).reduced(4);

    // Avatar placeholder circle
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.fillEllipse(avatarRect.toFloat());
    g.setColour(juce::Colours::black);
    g.drawText(juce::String(singer.name.substr(0, 1)),
               avatarRect, juce::Justification::centred);

    // If hovering, draw play/pause overlay on avatar
    if (hovering)
    {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(avatarRect.toFloat());
        g.setColour(juce::Colours::white);
        if (isPlaying)
        {
            // Pause icon (two vertical bars)
            int bw = 4, gap = 3;
            int cx = avatarRect.getCentreX();
            int cy = avatarRect.getCentreY();
            int bh = avatarSize / 3;
            g.fillRect(cx - gap - bw, cy - bh / 2, bw, bh);
            g.fillRect(cx + gap, cy - bh / 2, bw, bh);
        }
        else
        {
            // Play triangle
            juce::Path tri;
            float cx = (float)avatarRect.getCentreX();
            float cy = (float)avatarRect.getCentreY();
            float sz = (float)avatarSize * 0.25f;
            tri.addTriangle(cx - sz * 0.5f, cy - sz,
                            cx - sz * 0.5f, cy + sz,
                            cx + sz,        cy);
            g.fillPath(tri);
        }
    }

    // Name row (top half of remaining area)
    auto textArea = bounds.reduced(4, 0);
    auto nameArea = textArea.removeFromTop(textArea.getHeight() / 2);
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(14.f).boldened());
    juce::String displayName = juce::String(singer.name);
    if (singer.songsPerformed > 0)
        displayName += " " + getMicIcons(singer.songsPerformed);
    g.drawText(displayName, nameArea, juce::Justification::centredLeft, true);

    // Song info (bottom half)
    if (!singer.songs.empty())
    {
        auto& song = singer.songs[0];
        g.setFont(juce::Font(12.f).boldened());
        g.setColour(juce::Colour(0xff262626));
        g.drawText(juce::String(song.songArtist), textArea.removeFromTop(textArea.getHeight() / 2),
                   juce::Justification::centredLeft, true);
        g.setFont(juce::Font(11.f));
        g.drawText(juce::String(song.songName), textArea,
                   juce::Justification::centredLeft, true);
    }
}

void QueueBar::NowPlayingCard::mouseUp(const juce::MouseEvent&)
{
    if (!hasSinger) return;
    if (isPlaying)
    {
        if (onPauseClicked) onPauseClicked();
    }
    else
    {
        if (onPlayClicked) onPlayClicked();
    }
}

//==============================================================================
//  SingerRow
//==============================================================================
QueueBar::SingerRow::SingerRow() {}

void QueueBar::SingerRow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Card background
    g.setColour(juce::Colour(0xff262626));
    g.fillRect(bounds);

    // Round border: blue for first, red for last
    if (isFirst)
    {
        g.setColour(juce::Colour(0xff30daff));
        g.drawRect(bounds, 3);
    }
    else if (isLast)
    {
        g.setColour(juce::Colour(0xffdb3d40));
        g.drawRect(bounds, 3);
    }

    // Bottom separator
    g.setColour(juce::Colour(0xff474747));
    g.drawHorizontalLine(bounds.getBottom() - 1, (float)bounds.getX(), (float)bounds.getRight());

    int avatarSize = 40;
    auto avatarRect = bounds.removeFromLeft(avatarSize + 12).reduced(6).withSizeKeepingCentre(avatarSize, avatarSize);

    // Avatar circle
    g.setColour(juce::Colours::grey);
    g.fillEllipse(avatarRect.toFloat());
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.f));
    g.drawText(juce::String(singer.name.substr(0, 1)),
               avatarRect, juce::Justification::centred);

    // Play overlay on hover
    if (hovering)
    {
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillEllipse(avatarRect.toFloat());
        g.setColour(juce::Colours::white);
        juce::Path tri;
        float cx = (float)avatarRect.getCentreX();
        float cy = (float)avatarRect.getCentreY();
        float sz = 8.f;
        tri.addTriangle(cx - sz * 0.5f, cy - sz,
                        cx - sz * 0.5f, cy + sz,
                        cx + sz,        cy);
        g.fillPath(tri);
    }

    // Strikes (if no songs)
    if (singer.songs.empty() && singer.strikes > 0)
    {
        g.setColour(juce::Colour(0xffdb3d40));
        g.setFont(juce::Font(12.f));
        juce::String strikeStr;
        for (int s = 0; s < singer.strikes; ++s)
            strikeStr += "X ";
        g.drawText(strikeStr.trimEnd(), bounds.reduced(4),
                   juce::Justification::centredLeft, true);
        return;
    }

    // Name area – top half
    auto nameArea = bounds.removeFromTop(bounds.getHeight() / 2).reduced(4, 0);
    g.setColour(juce::Colour(0xffe4e4e4));
    g.setFont(juce::Font(13.f).boldened());
    juce::String displayName = juce::String(singer.name);
    if (singer.songsPerformed > 0)
        displayName += " " + getMicIcons(singer.songsPerformed);
    g.drawText(displayName, nameArea, juce::Justification::centredLeft, true);

    // Song chips (bottom half, up to 5)
    auto songArea = bounds.reduced(4, 0);
    int chipCount = juce::jmin((int)singer.songs.size(), 5);
    if (chipCount > 0)
    {
        int chipWidth = juce::jmin(songArea.getWidth() / chipCount, 120);
        g.setFont(juce::Font(10.f));
        for (int i = 0; i < chipCount; ++i)
        {
            auto chipRect = songArea.removeFromLeft(chipWidth).reduced(1);
            // First song highlighted
            if (i == 0)
            {
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                g.fillRoundedRectangle(chipRect.toFloat(), 3.f);
            }
            g.setColour(juce::Colour(0xffe4e4e4).withAlpha(0.7f));
            auto topHalf = chipRect.removeFromTop(chipRect.getHeight() / 2);
            g.setFont(juce::Font(10.f).boldened());
            g.drawText(juce::String(singer.songs[(size_t)i].songArtist),
                       topHalf, juce::Justification::centredLeft, true);
            g.setFont(juce::Font(9.f));
            g.drawText(juce::String(singer.songs[(size_t)i].songName),
                       chipRect, juce::Justification::centredLeft, true);
        }
    }
}

void QueueBar::SingerRow::mouseUp(const juce::MouseEvent& e)
{
    // Click on the left 52px = play; elsewhere = song chip detection
    if (e.x < 52)
    {
        if (onPlayClicked) onPlayClicked(index);
    }
    else if (!singer.songs.empty())
    {
        // Determine which song chip was clicked based on x position
        int songAreaStart = 52 + 4; // avatar width + padding
        int songAreaWidth = getWidth() - songAreaStart - 8;
        int chipCount = juce::jmin((int)singer.songs.size(), 5);
        if (chipCount > 0)
        {
            int chipWidth = juce::jmin(songAreaWidth / chipCount, 120);
            int relX = e.x - songAreaStart;
            int chipIdx = relX / chipWidth;
            if (chipIdx >= 0 && chipIdx < chipCount)
            {
                if (onSongChipClicked) onSongChipClicked(index, chipIdx);
            }
        }
    }
}

//==============================================================================
//  QueueBar
//==============================================================================
QueueBar::QueueBar()
{
    // Now-playing card
    nowPlayingCard = std::make_unique<NowPlayingCard>();
    nowPlayingCard->onPlayClicked  = [this]() { if (onPlayCurrent)  onPlayCurrent();  };
    nowPlayingCard->onPauseClicked = [this]() { if (onPauseCurrent) onPauseCurrent(); };
    addAndMakeVisible(*nowPlayingCard);

    // Viewport for singer list
    listViewport.setViewedComponent(&listContent, false);
    listViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(listViewport);

    // Venue header labels
    venueNameLabel = std::make_unique<juce::Label>("venueName", "");
    venueNameLabel->setColour(juce::Label::textColourId, textColour);
    venueNameLabel->setFont(juce::Font(14.f).boldened());
    addAndMakeVisible(*venueNameLabel);

    venueCodeLabel = std::make_unique<juce::Label>("venueCode", "");
    venueCodeLabel->setColour(juce::Label::textColourId, accentColour);
    venueCodeLabel->setFont(juce::Font(12.f));
    addAndMakeVisible(*venueCodeLabel);

    nowSingingLabel = std::make_unique<juce::Label>("nowSinging", "Now Singing:");
    nowSingingLabel->setColour(juce::Label::textColourId, textColour);
    nowSingingLabel->setFont(juce::Font(12.f));
    addAndMakeVisible(*nowSingingLabel);

    // Bottom status bar
    singerCountLabel = std::make_unique<juce::Label>("singerCount", "# singers: 0");
    singerCountLabel->setColour(juce::Label::textColourId, textColour);
    singerCountLabel->setFont(juce::Font(11.f));
    addAndMakeVisible(*singerCountLabel);

    songCountLabel = std::make_unique<juce::Label>("songCount", "# songs: 0");
    songCountLabel->setColour(juce::Label::textColourId, textColour);
    songCountLabel->setFont(juce::Font(11.f));
    addAndMakeVisible(*songCountLabel);

    totalTimeLabel = std::make_unique<juce::Label>("totalTime", "Time: --");
    totalTimeLabel->setColour(juce::Label::textColourId, textColour);
    totalTimeLabel->setFont(juce::Font(11.f));
    addAndMakeVisible(*totalTimeLabel);

    clearQueueButton = std::make_unique<juce::TextButton>("Clear Queue");
    clearQueueButton->setColour(juce::TextButton::buttonColourId, accentColour);
    clearQueueButton->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    clearQueueButton->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    clearQueueButton->onClick = [this]() { if (onClearQueue) onClearQueue(); };
    addAndMakeVisible(*clearQueueButton);

    queueToggle = std::make_unique<juce::ToggleButton>("Queue");
    queueToggle->setToggleState(true, juce::dontSendNotification);
    queueToggle->setColour(juce::ToggleButton::textColourId, textColour);
    queueToggle->setColour(juce::ToggleButton::tickColourId, accentColour);
    queueToggle->onClick = [this]()
    {
        if (onQueueToggled) onQueueToggled(queueToggle->getToggleState());
    };
    addAndMakeVisible(*queueToggle);

    autoPlayToggle = std::make_unique<juce::ToggleButton>("Auto Play");
    autoPlayToggle->setColour(juce::ToggleButton::textColourId, textColour);
    autoPlayToggle->setColour(juce::ToggleButton::tickColourId, accentColour);
    autoPlayToggle->onClick = [this]()
    {
        if (onAutoPlayToggled) onAutoPlayToggled(autoPlayToggle->getToggleState());
    };
    addAndMakeVisible(*autoPlayToggle);

    delaySlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                  juce::Slider::TextBoxLeft);
    delaySlider->setRange(0, 60, 1);
    delaySlider->setValue(0, juce::dontSendNotification);
    delaySlider->setColour(juce::Slider::thumbColourId, accentColour);
    delaySlider->setColour(juce::Slider::trackColourId, juce::Colour(0xff474747));
    delaySlider->setColour(juce::Slider::textBoxTextColourId, textColour);
    delaySlider->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 36, 20);
    delaySlider->onValueChange = [this]()
    {
        delaySec = (int)delaySlider->getValue();
        if (onDelayChanged) onDelayChanged(delaySec);
    };
    addAndMakeVisible(*delaySlider);

    delayLabel = std::make_unique<juce::Label>("delay", "Delay (sec):");
    delayLabel->setColour(juce::Label::textColourId, textColour);
    delayLabel->setFont(juce::Font(11.f));
    addAndMakeVisible(*delayLabel);
}

//==============================================================================
void QueueBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Full background
    g.setColour(bgColour);
    g.fillRect(bounds);

    // Venue header background
    g.setColour(headerBgColour);
    g.fillRect(bounds.removeFromTop(venueHeaderHeight));

    // Resize handle visual (left 5px)
    auto handleArea = getLocalBounds().removeFromLeft(resizeHandleWidth);
    if (draggingResize)
    {
        g.setColour(accentColour.withAlpha(0.6f));
        g.fillRect(handleArea);
    }
}

//==============================================================================
void QueueBar::resized()
{
    auto bounds = getLocalBounds();

    // Reserve left resize handle
    bounds.removeFromLeft(resizeHandleWidth);

    //--- Venue header ---
    auto headerArea = bounds.removeFromTop(venueHeaderHeight);
    auto headerLeft = headerArea.reduced(8, 4);
    venueNameLabel->setBounds(headerLeft.removeFromTop(headerLeft.getHeight() / 2));
    venueCodeLabel->setBounds(headerLeft);

    //--- Now Singing ---
    auto npArea = bounds.removeFromTop(nowPlayingHeight);
    nowSingingLabel->setBounds(npArea.removeFromTop(18).reduced(8, 0));
    nowPlayingCard->setBounds(npArea.reduced(4));

    //--- Status bar at bottom ---
    auto statusArea = bounds.removeFromBottom(statusBarHeight);
    {
        auto row1 = statusArea.removeFromTop(22).reduced(8, 0);
        int third = row1.getWidth() / 3;
        singerCountLabel->setBounds(row1.removeFromLeft(third));
        songCountLabel->setBounds(row1.removeFromLeft(third));
        totalTimeLabel->setBounds(row1);

        auto row2 = statusArea.removeFromTop(28).reduced(8, 2);
        queueToggle->setBounds(row2.removeFromLeft(row2.getWidth() / 2));
        autoPlayToggle->setBounds(row2);

        auto row3 = statusArea.removeFromTop(28).reduced(8, 2);
        delayLabel->setBounds(row3.removeFromLeft(80));
        delaySlider->setBounds(row3);

        auto row4 = statusArea.reduced(8, 2);
        clearQueueButton->setBounds(row4.withHeight(juce::jmin(row4.getHeight(), 28)));
    }

    //--- Singer list fills the middle ---
    auto listArea = bounds;
    listViewport.setBounds(listArea);

    int totalHeight = (int)singerRows.size() * singerRowHeight;
    listContent.setSize(listArea.getWidth(), juce::jmax(totalHeight, listArea.getHeight()));

    for (int i = 0; i < singerRows.size(); ++i)
    {
        singerRows[i]->setBounds(0, i * singerRowHeight,
                                  listContent.getWidth(), singerRowHeight);
    }
}

//==============================================================================
// Resize handle
//==============================================================================
bool QueueBar::isOverResizeHandle(const juce::Point<int>& pos) const
{
    return pos.getX() < resizeHandleWidth;
}

void QueueBar::mouseDown(const juce::MouseEvent& e)
{
    if (isOverResizeHandle(e.getPosition()))
    {
        draggingResize = true;
        dragStartX = e.getScreenX();
        dragStartWidth = barWidth;
        repaint();
    }
}

void QueueBar::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingResize)
    {
        // Dragging left edge LEFT increases width, RIGHT decreases width
        int delta = dragStartX - e.getScreenX();
        int newWidth = juce::jlimit(minWidth, maxWidth, dragStartWidth + delta);
        if (newWidth != barWidth)
        {
            barWidth = newWidth;
            if (onWidthChanged) onWidthChanged(barWidth);
        }
    }
}

void QueueBar::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(isOverResizeHandle(e.getPosition())
                       ? juce::MouseCursor::LeftRightResizeCursor
                       : juce::MouseCursor::NormalCursor);
}

//==============================================================================
// Data setters
//==============================================================================
void QueueBar::setVenueInfo(const juce::String& name, const juce::String& code)
{
    venueNameLabel->setText(name, juce::dontSendNotification);
    venueCodeLabel->setText(code, juce::dontSendNotification);
}

void QueueBar::setNowPlaying(const Singers& singer)
{
    hasCurrentSinger = true;
    currentSinger = singer;
    nowPlayingCard->hasSinger = true;
    nowPlayingCard->singer = singer;
    nowPlayingCard->repaint();
}

void QueueBar::clearNowPlaying()
{
    hasCurrentSinger = false;
    nowPlayingCard->hasSinger = false;
    nowPlayingCard->repaint();
}

void QueueBar::setSingers(const std::vector<Singers>& newSingers)
{
    singers = newSingers;
    rebuildSingerRows();
    updateStatusLabels();
    resized();
}

void QueueBar::addSinger(const Singers& singer)
{
    singers.push_back(singer);
    rebuildSingerRows();
    updateStatusLabels();
    resized();
}

void QueueBar::removeSinger(int index)
{
    if (index >= 0 && index < (int)singers.size())
    {
        singers.erase(singers.begin() + index);
        rebuildSingerRows();
        updateStatusLabels();
        resized();
    }
}

void QueueBar::moveSinger(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= (int)singers.size()) return;
    if (toIndex < 0 || toIndex >= (int)singers.size()) return;
    if (fromIndex == toIndex) return;

    auto singer = singers[(size_t)fromIndex];
    singers.erase(singers.begin() + fromIndex);
    singers.insert(singers.begin() + toIndex, singer);

    // Reindex order
    for (int i = 0; i < (int)singers.size(); ++i)
        singers[(size_t)i].order = i;

    if (onReorder) onReorder(fromIndex, toIndex);
    rebuildSingerRows();
    resized();
}

void QueueBar::setPlaying(bool playing)
{
    isPlayingState = playing;
    nowPlayingCard->isPlaying = playing;
    nowPlayingCard->repaint();
}

void QueueBar::setQueueRunning(bool running)
{
    queueRunning = running;
    queueToggle->setToggleState(running, juce::dontSendNotification);
}

void QueueBar::setAutoPlay(bool enabled)
{
    autoPlayEnabled = enabled;
    autoPlayToggle->setToggleState(enabled, juce::dontSendNotification);
}

void QueueBar::setDelaySec(int seconds)
{
    delaySec = seconds;
    delaySlider->setValue(seconds, juce::dontSendNotification);
}

void QueueBar::setBarWidth(int w)
{
    barWidth = juce::jlimit(minWidth, maxWidth, w);
}

//==============================================================================
void QueueBar::rebuildSingerRows()
{
    // Clear existing rows
    for (auto* row : singerRows)
        listContent.removeChildComponent(row);
    singerRows.clear();

    // Determine first/last in round
    int firstRotation = INT_MAX;
    int lastRotation  = INT_MIN;
    for (auto& s : singers)
    {
        if (s.rotationOrder < firstRotation) firstRotation = s.rotationOrder;
        if (s.rotationOrder > lastRotation)  lastRotation  = s.rotationOrder;
    }

    for (int i = 0; i < (int)singers.size(); ++i)
    {
        auto* row = new SingerRow();
        row->singer  = singers[(size_t)i];
        row->index   = i;
        row->isFirst = (singers[(size_t)i].rotationOrder == firstRotation);
        row->isLast  = (singers[(size_t)i].rotationOrder == lastRotation) && singers.size() > 1;

        row->onPlayClicked = [this](int idx)
        {
            if (onPlaySinger) onPlaySinger(idx);
        };
        row->onSongChipClicked = [this](int singerIdx, int songIdx)
        {
            if (onSongClicked) onSongClicked(singerIdx, songIdx);
        };

        listContent.addAndMakeVisible(row);
        singerRows.add(row);
    }
}

//==============================================================================
void QueueBar::updateStatusLabels()
{
    int numSingers = (int)singers.size();
    int numSongs = 0;
    int totalSec = 0;

    for (auto& s : singers)
    {
        numSongs += (int)s.songs.size();
        numSongs += 0; // placeholder for duration accumulation
        for (auto& song : s.songs)
            totalSec += song.duration;
    }

    singerCountLabel->setText("# singers: " + juce::String(numSingers),
                              juce::dontSendNotification);
    songCountLabel->setText("# songs: " + juce::String(numSongs),
                            juce::dontSendNotification);

    int hrs = totalSec / 3600;
    int mins = (totalSec % 3600) / 60;
    juce::String timeStr;
    if (hrs > 0)
        timeStr = juce::String(hrs) + "h " + juce::String(mins) + "m";
    else
        timeStr = juce::String(mins) + " min";
    totalTimeLabel->setText("Time: " + timeStr, juce::dontSendNotification);
}

//==============================================================================
void QueueBar::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    nowSingingLabel->setText(lm.getText("queue.now_singing"), juce::dontSendNotification);
    clearQueueButton->setButtonText(lm.getText("queue.clear_queue"));
    queueToggle->setButtonText(lm.getText("queue.queue_label"));
    autoPlayToggle->setButtonText(lm.getText("queue.auto_play"));
    delayLabel->setText(lm.getText("queue.delay_label"), juce::dontSendNotification);
    updateStatusLabels();
}
