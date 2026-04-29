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
// Resolve a Singers/QueueItem.avatar string (typically something like
// "assets/icons/1064391.png" from the Angular client) to a concrete file in
// the JUCE app bundle's assets folder. Tries `assets/icon/<basename>` first
// (this repo uses singular "icon"), then the literal relative path.
static juce::Image loadAvatarFromAssets(const juce::String& avatarPath)
{
    if (avatarPath.isEmpty())
        return {};

    auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                      .getParentDirectory();

    // 1) Try assets/icon/<basename>
    auto baseName = avatarPath.fromLastOccurrenceOf("/", false, false);
    if (baseName.isEmpty())
        baseName = avatarPath;
    auto candidate1 = appDir.getChildFile("assets/icon/" + baseName);
    if (candidate1.existsAsFile())
        return juce::ImageFileFormat::loadFrom(candidate1);

    // 2) Try the literal relative path (in case it already begins with assets/)
    auto candidate2 = appDir.getChildFile(avatarPath);
    if (candidate2.existsAsFile())
        return juce::ImageFileFormat::loadFrom(candidate2);

    // 3) Try assets/<avatarPath> if the path was given without the "assets/"
    auto candidate3 = appDir.getChildFile("assets/" + avatarPath);
    if (candidate3.existsAsFile())
        return juce::ImageFileFormat::loadFrom(candidate3);

    return {};
}

//==============================================================================
//  NowPlayingCard
//==============================================================================
QueueBar::NowPlayingCard::NowPlayingCard() {}

void QueueBar::NowPlayingCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(4);

    // Nothing to draw until someone is actually up to sing — leave the
    // space blank so the bar reads as "no current singer" rather than an
    // empty blue card.
    if (!hasSinger)
        return;

    // Primary colour background (like Angular .card-top)
    g.setColour(juce::Colour(0xff30daff));
    g.fillRoundedRectangle(bounds.toFloat(), 6.f);

    int avatarSize = bounds.getHeight() - 8;
    auto avatarRect = bounds.removeFromLeft(avatarSize + 8).reduced(4);

    // Avatar — clip to a circle and draw the loaded image, falling back to
    // a translucent circle with the singer's first initial.
    juce::Path circle;
    circle.addEllipse(avatarRect.toFloat());
    g.saveState();
    g.reduceClipRegion(circle);
    if (avatarImage.isValid())
    {
        g.drawImage(avatarImage, avatarRect.toFloat(),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRect(avatarRect);
        g.setColour(juce::Colours::black);
        g.drawText(juce::String(singer.name.substr(0, 1)),
                   avatarRect, juce::Justification::centred);
    }
    g.restoreState();

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

juce::Rectangle<int> QueueBar::SingerRow::getAvatarRect() const
{
    auto bounds = getLocalBounds();
    const int avatarSize = 40;
    return bounds.removeFromLeft(avatarSize + 12).reduced(6)
                 .withSizeKeepingCentre(avatarSize, avatarSize);
}

bool QueueBar::SingerRow::isOverAvatar(juce::Point<int> p) const
{
    auto a = getAvatarRect().toFloat();
    auto cx = a.getCentreX();
    auto cy = a.getCentreY();
    auto r  = juce::jmin(a.getWidth(), a.getHeight()) * 0.5f;
    auto dx = (float) p.x - cx;
    auto dy = (float) p.y - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

int QueueBar::SingerRow::songChipIndexAt(juce::Point<int> p) const
{
    if (singer.songs.empty())
        return -1;

    // Match the layout used in paint(): chips fill the bottom half of the
    // area to the right of the avatar.
    const int songAreaStart = 52 + 4;        // avatar width + padding
    const int songAreaWidth = getWidth() - songAreaStart - 8;
    if (p.x < songAreaStart || songAreaWidth <= 0)
        return -1;

    // Vertical: only the bottom half of the row counts as "over a song"
    if (p.y < getHeight() / 2)
        return -1;

    const int chipCount = juce::jmin((int) singer.songs.size(), 5);
    if (chipCount <= 0) return -1;
    const int chipWidth = juce::jmin(songAreaWidth / chipCount, 120);
    if (chipWidth <= 0) return -1;
    const int relX = p.x - songAreaStart;
    const int idx  = relX / chipWidth;
    if (idx < 0 || idx >= chipCount) return -1;
    if (relX > chipCount * chipWidth) return -1;
    return idx;
}

void QueueBar::SingerRow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Card background
    g.setColour(juce::Colour(0xff262626));
    g.fillRoundedRectangle(bounds.toFloat(), 8.f);

    // Round border: green for host, red for the singer at the tail of the
    // current round. The "first in round" highlight is intentionally not
    // drawn — the host already represents the round leader.
    if (isHost)
    {
        g.setColour(juce::Colour(0xff10b981));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.5f), 8.f, 3.f);
    }
    else if (isLast)
    {
        g.setColour(juce::Colour(0xffdb3d40));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.5f), 8.f, 3.f);
    }

    // Bottom separator
    g.setColour(juce::Colour(0xff474747));
    g.drawHorizontalLine(bounds.getBottom() - 1, (float)bounds.getX(), (float)bounds.getRight());

    int avatarSize = 40;
    auto avatarRect = bounds.removeFromLeft(avatarSize + 12).reduced(6).withSizeKeepingCentre(avatarSize, avatarSize);

    // Avatar — circle clip, real image if we loaded one, else first-initial.
    juce::Path circle;
    circle.addEllipse(avatarRect.toFloat());
    g.saveState();
    g.reduceClipRegion(circle);
    if (avatarImage.isValid())
    {
        g.drawImage(avatarImage, avatarRect.toFloat(),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }
    else
    {
        g.setColour(juce::Colours::grey);
        g.fillRect(avatarRect);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.f));
        g.drawText(juce::String(singer.name.substr(0, 1)),
                   avatarRect, juce::Justification::centred);
    }
    g.restoreState();

    // Play overlay only when the cursor is over the avatar specifically
    // (not the rest of the row).
    if (hoverAvatar)
    {
        g.saveState();
        g.reduceClipRegion(circle);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRect(avatarRect);
        g.restoreState();

        g.setColour(juce::Colours::white);
        juce::Path tri;
        const float cx = (float) avatarRect.getCentreX();
        const float cy = (float) avatarRect.getCentreY();
        const float sz = 9.f;
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
        for (int i = 0; i < chipCount; ++i)
        {
            auto chipRect = songArea.removeFromLeft(chipWidth).reduced(1);

            // First song highlighted; hovered song gets a slightly lifted
            // background.
            if (i == hoverSongIdx)
            {
                g.setColour(juce::Colours::white.withAlpha(0.22f));
                g.fillRoundedRectangle(chipRect.toFloat(), 3.f);
            }
            else if (i == 0)
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

void QueueBar::SingerRow::mouseExit(const juce::MouseEvent&)
{
    hovering     = false;
    hoverAvatar  = false;
    hoverSongIdx = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void QueueBar::SingerRow::mouseMove(const juce::MouseEvent& e)
{
    const bool overAvatar = isOverAvatar(e.getPosition());
    const int  songIdx    = overAvatar ? -1 : songChipIndexAt(e.getPosition());

    bool changed = false;
    if (overAvatar != hoverAvatar)   { hoverAvatar  = overAvatar; changed = true; }
    if (songIdx   != hoverSongIdx)   { hoverSongIdx = songIdx;   changed = true; }

    juce::MouseCursor cursor (juce::MouseCursor::NormalCursor);
    if (overAvatar)
        cursor = juce::MouseCursor::PointingHandCursor;
    else if (songIdx >= 0)
        cursor = juce::MouseCursor::PointingHandCursor;
    else if (! isHost)
        cursor = juce::MouseCursor::DraggingHandCursor;
    setMouseCursor(cursor);

    if (changed) repaint();
}

void QueueBar::SingerRow::mouseDown(const juce::MouseEvent& /*e*/)
{
    // Nothing to do — the actual click is handled in mouseUp, and any drag
    // is initiated from mouseDrag through the DragAndDropContainer.
}

void QueueBar::SingerRow::mouseDrag(const juce::MouseEvent& e)
{
    // Host can never be reordered.
    if (isHost) return;

    // Drags only start from the "body" of the row — not from the avatar
    // (which is a play-click target) and not from a song chip (which opens
    // the edit-singer modal).
    const auto start = e.getMouseDownPosition();
    const bool startedOnAvatar = isOverAvatar(start);
    const bool startedOnSong   = ! startedOnAvatar && songChipIndexAt(start) >= 0;
    if (startedOnAvatar || startedOnSong)
        return;

    // Defer to JUCE's drag-and-drop framework. The container will paint a
    // ghost of this row that follows the cursor and route drop events to
    // the matching DragAndDropTarget (ListContent in our case).
    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor(this))
    {
        if (! dnd->isDragAndDropActive())
        {
            // Encode the source row index in the drag description so the
            // target knows which singer to move.
            juce::var description (index);

            // Build a snapshot image of the row to use as the drag ghost.
            // 70% opacity makes it feel "lifted" without obscuring the list.
            auto img = createComponentSnapshot (getLocalBounds(), true);
            dnd->startDragging (description, this, juce::ScaledImage (img),
                                /*allowDraggingToOtherWindows*/ false);
        }
    }
}

void QueueBar::SingerRow::mouseUp(const juce::MouseEvent& e)
{
    // If the user dragged this row (handled by DragAndDropContainer), the
    // matching mouseUp is not a click — don't open the play screen or the
    // edit-singer modal.
    if (e.mouseWasDraggedSinceMouseDown())
        return;

    const auto p = e.getPosition();
    if (isOverAvatar(p))
    {
        if (onPlayClicked) onPlayClicked(index);
        return;
    }

    const int songIdx = songChipIndexAt(p);
    if (songIdx >= 0 && onSongChipClicked)
    {
        onSongChipClicked(index, songIdx);
        return;
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
    nowSingingLabel->setFont(juce::Font(12.f).boldened());
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

    // Now-playing area + 5px separator below it (#474747).
    bounds.removeFromTop(nowPlayingHeight);
    g.setColour(juce::Colour(0xff474747));
    g.fillRect(bounds.removeFromTop(5));

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

    // 5 px separator between now-playing and the queue list.
    bounds.removeFromTop(5);

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
    nowPlayingCard->avatarImage = loadAvatarFromAssets(juce::String(singer.avatar));
    nowPlayingCard->repaint();
}

void QueueBar::clearNowPlaying()
{
    hasCurrentSinger = false;
    nowPlayingCard->hasSinger = false;
    nowPlayingCard->avatarImage = {};
    nowPlayingCard->repaint();
    // The "Now Singing:" label always remains visible above the card,
    // even when there is no active singer.
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
        // Host is pinned to the queue and cannot be removed.
        if (singers[(size_t)index].isHost)
            return;

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

    // Host is pinned to position 0 — refuse to move the host or to displace
    // the host out of the first slot.
    if (singers[(size_t)fromIndex].isHost) return;
    if (! singers.empty() && singers.front().isHost && toIndex == 0) return;

    auto singer = singers[(size_t)fromIndex];
    singers.erase(singers.begin() + fromIndex);
    singers.insert(singers.begin() + toIndex, singer);

    // Reindex `order` for every singer, and `rotationOrder` for the
    // non-host singers (the host stays at -1 / unchanged). The first/last
    // round borders are computed from rotationOrder, so this is what makes
    // the red "last in round" border follow a manual reorder.
    int rot = 0;
    for (int i = 0; i < (int)singers.size(); ++i)
    {
        singers[(size_t)i].order = i;
        if (! singers[(size_t)i].isHost)
            singers[(size_t)i].rotationOrder = rot++;
    }

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

    // Determine first/last in round across NON-HOST singers only. The host
    // is the round leader (green border) and isn't part of the
    // first/last-of-round colouring. If there are no non-host singers in
    // the queue (i.e. nobody has submitted a song yet) then no row gets
    // the red "last in round" border.
    int firstRotation = INT_MAX;
    int lastRotation  = INT_MIN;
    int nonHostCount  = 0;
    for (auto& s : singers)
    {
        if (s.isHost) continue;
        ++nonHostCount;
        if (s.rotationOrder < firstRotation) firstRotation = s.rotationOrder;
        if (s.rotationOrder > lastRotation)  lastRotation  = s.rotationOrder;
    }

    for (int i = 0; i < (int)singers.size(); ++i)
    {
        auto* row = new SingerRow();
        row->singer  = singers[(size_t)i];
        row->index   = i;
        row->isHost  = singers[(size_t)i].isHost;
        row->isFirst = (! row->isHost) && nonHostCount > 0
                     && (singers[(size_t)i].rotationOrder == firstRotation);
        row->isLast  = (! row->isHost) && nonHostCount > 1
                     && (singers[(size_t)i].rotationOrder == lastRotation);
        row->avatarImage = loadAvatarFromAssets(juce::String(singers[(size_t)i].avatar));

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

//==============================================================================
//  ListContent — DragAndDropTarget implementation for singer-row reorder.
//==============================================================================
bool QueueBar::ListContent::isInterestedInDragSource (const SourceDetails& d)
{
    // Only accept drags coming from one of our SingerRow children.
    return dynamic_cast<SingerRow*> (d.sourceComponent.get()) != nullptr;
}

void QueueBar::ListContent::itemDragMove (const SourceDetails& d)
{
    // Compute the Y coordinate of the slot the drop will land in and use
    // it to draw a horizontal "insertion line" indicator across the list.
    const int targetY = d.localPosition.y;
    const int fromIndex = (int) d.description;

    int toIndex = 0;
    for (int i = 0; i < owner.singerRows.size(); ++i)
    {
        if (i == fromIndex) continue;
        auto* r = owner.singerRows[i];
        const int rowCentreY = r->getY() + r->getHeight() / 2;
        if (rowCentreY < targetY)
            ++toIndex;
    }

    const bool hasHost = ! owner.singers.empty() && owner.singers.front().isHost;
    if (hasHost) toIndex = juce::jmax (toIndex, 1);
    toIndex = juce::jlimit (0, (int) owner.singers.size(), toIndex);

    // Convert the slot index into a Y coordinate. Insertion line is drawn
    // at the top of the row currently at `toIndex` (or below the last row
    // if dropping at the end).
    int newY;
    if (toIndex >= owner.singerRows.size())
    {
        auto* last = owner.singerRows.getLast();
        newY = last != nullptr ? last->getBottom() : 0;
    }
    else
    {
        newY = owner.singerRows[toIndex]->getY();
    }

    if (newY != dropIndicatorY)
    {
        dropIndicatorY = newY;
        repaint();
    }
}

void QueueBar::ListContent::itemDragExit (const SourceDetails&)
{
    if (dropIndicatorY != -1)
    {
        dropIndicatorY = -1;
        repaint();
    }
}

void QueueBar::ListContent::itemDropped (const SourceDetails& d)
{
    const int fromIndex = (int) d.description;
    const int targetY   = d.localPosition.y;

    int toIndex = 0;
    for (int i = 0; i < owner.singerRows.size(); ++i)
    {
        if (i == fromIndex) continue;
        auto* r = owner.singerRows[i];
        const int rowCentreY = r->getY() + r->getHeight() / 2;
        if (rowCentreY < targetY)
            ++toIndex;
    }

    const bool hasHost = ! owner.singers.empty() && owner.singers.front().isHost;
    if (hasHost) toIndex = juce::jmax (toIndex, 1);
    toIndex = juce::jlimit (0, (int) owner.singers.size() - 1, toIndex);

    dropIndicatorY = -1;

    if (fromIndex < 0 || fromIndex >= (int) owner.singers.size()) return;
    if (toIndex == fromIndex) { repaint(); return; }

    owner.moveSinger (fromIndex, toIndex);
    if (owner.onReorder) owner.onReorder (fromIndex, toIndex);
}

void QueueBar::ListContent::paintOverChildren (juce::Graphics& g)
{
    if (dropIndicatorY < 0) return;
    g.setColour (juce::Colour (0xff30daff));
    g.fillRect (juce::Rectangle<int> (0, dropIndicatorY - 1, getWidth(), 3));
}
