/*
  ==============================================================================

    HomePage.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Home page implementation – horizontally-scrollable song card rows.

  ==============================================================================
*/

#include "HomePage.h"

//==============================================================================
//  SongCard
//==============================================================================
SongCard::SongCard()
{
    // A random-ish placeholder colour will be set when setTrack is called
    placeholderColour = juce::Colour(0xff6c6c6c);
}

void SongCard::setTrack(const juce::String& artist, const juce::String& song,
                         const juce::String& imageUrl, bool isExplicit)
{
    artistText   = artist;
    songText     = song;
    imageUrlText = imageUrl;
    explicitFlag = isExplicit;

    // Generate a deterministic placeholder colour from the song name
    auto hash = (uint32_t)song.hashCode();
    placeholderColour = juce::Colour::fromHSV((float)(hash % 360) / 360.f, 0.35f, 0.45f, 1.f);

    repaint();
}

void SongCard::setArtwork(const juce::Image& img)
{
    artwork = img;
    hasArtwork = img.isValid();
    repaint();
}

void SongCard::loadArtwork()
{
    if (imageUrlText.isEmpty()) return;

    // Single call: returns image if cached, queues callback if not.
    juce::Component::SafePointer<SongCard> safeThis(this);
    juce::String urlCopy = imageUrlText;
    juce::Image img = ArtworkCache::getInstance().getOrFetch(urlCopy, [safeThis, urlCopy]()
    {
        if (safeThis == nullptr) return;
        juce::Image fetched = ArtworkCache::getInstance().getOrFetch(urlCopy);
        if (fetched.isValid())
            safeThis->setArtwork(fetched);
    });

    if (img.isValid())
        setArtwork(img);
}

void SongCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    int artSize = bounds.getWidth();

    // Artwork area (square, top portion)
    auto artRect = bounds.removeFromTop(artSize);

    if (hasArtwork)
    {
        g.drawImage(artwork, artRect.toFloat(),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }
    else
    {
        // Placeholder square
        g.setColour(placeholderColour);
        g.fillRect(artRect);

        // Draw a music-note icon
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.setFont(juce::Font(juce::FontOptions().withHeight(40.f)));
        g.drawText(juce::String::charToString(0x266B), artRect, juce::Justification::centred);
    }

    // Hover scale effect – subtle highlight border
    if (hovering)
    {
        g.setColour(juce::Colour(0xff30daff).withAlpha(0.6f));
        g.drawRect(getLocalBounds(), 2);
    }

    // Artist (below artwork)
    auto textArea = bounds.reduced(2, 2);
    g.setColour(juce::Colour(0xffe4e4e4));
    g.setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
    g.drawText(artistText, textArea.removeFromTop(16),
               juce::Justification::centredLeft, true);

    // Song title
    g.setColour(juce::Colour(0xffa3a6a8));
    g.setFont(juce::Font(juce::FontOptions().withHeight(12.f)).boldened());
    g.drawText(songText, textArea.removeFromTop(15),
               juce::Justification::centredLeft, true);

    // Explicit badge
    if (explicitFlag)
    {
        g.setColour(juce::Colours::red);
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
        g.drawText("[EXPLICIT]", textArea.removeFromTop(12),
                   juce::Justification::centredLeft, true);
    }
}

void SongCard::mouseUp(const juce::MouseEvent& e)
{
    // Suppress the click if the user dragged — the parent Viewport's
    // ScrollOnDragMode::all interpreted this as a swipe gesture, so the card
    // shouldn't open the Song Selection dialog under the released finger/cursor.
    if (e.mouseWasDraggedSinceMouseDown())
        return;

    if (onClick) onClick();
}

//==============================================================================
//  SongRow
//==============================================================================
SongRow::SongRow()
{
    viewport.setViewedComponent(&strip, false);
    // Hide scrollbars — touch users swipe, mouse users get the wheel-to-horizontal
    // hook below.  A thin scrollbar still appears on macOS only while scrolling.
    viewport.setScrollBarsShown(false, false);

    // Drag-to-scroll for touchscreens (and trackpads / mice that hold and drag).
    // ScrollOnDragMode::all means *any* drag inside the viewport scrolls; child
    // components only receive the click if the user releases without moving.
    viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);

    addAndMakeVisible(viewport);
}

void SongRow::setTitle(const juce::String& title)
{
    sectionTitle = title;
    repaint();
}

void SongRow::setTracks(const std::vector<Track>& tracks)
{
    cards.clear();
    for (int i = 0; i < (int)tracks.size(); ++i)
    {
        auto* card = new SongCard();
        card->setTrack(juce::String(tracks[(size_t)i].artists),
                       juce::String(tracks[(size_t)i].name),
                       juce::String(tracks[(size_t)i].imageUrl),
                       tracks[(size_t)i].isExplicit);

        int idx = i;
        card->onClick = [this, idx]() { if (onSongClicked) onSongClicked(idx); };

        card->loadArtwork();
        cards.add(card);
    }
    rebuildStrip();
}

void SongRow::setPlaylists(const std::vector<Playlist>& playlists)
{
    cards.clear();
    for (int i = 0; i < (int)playlists.size(); ++i)
    {
        auto* card = new SongCard();
        card->setTrack(juce::String(playlists[(size_t)i].artistName),
                       juce::String(playlists[(size_t)i].songName),
                       juce::String(playlists[(size_t)i].imageUrl));

        int idx = i;
        card->onClick = [this, idx]() { if (onSongClicked) onSongClicked(idx); };

        card->loadArtwork();
        cards.add(card);
    }
    rebuildStrip();
}

void SongRow::clear()
{
    for (auto* card : cards)
        strip.removeChildComponent(card);
    cards.clear();
    rebuildStrip();
}

void SongRow::paint(juce::Graphics& g)
{
    // Section title
    g.setColour(juce::Colour(0xffe4e4e4));
    g.setFont(juce::Font(juce::FontOptions().withHeight(20.f)).boldened());
    g.drawText(sectionTitle, 0, 0, getWidth(), titleHeight,
               juce::Justification::centredLeft);
}

void SongRow::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(titleHeight);
    viewport.setBounds(bounds);

    rebuildStrip();
}

void SongRow::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    // Convert vertical scroll into horizontal scroll for the viewport
    if (std::abs(wheel.deltaY) > std::abs(wheel.deltaX))
    {
        auto& vp = viewport;
        auto pos = vp.getViewPosition();
        pos.setX(pos.getX() - (int)(wheel.deltaY * 200.f));
        vp.setViewPosition(pos);
    }
}

void SongRow::rebuildStrip()
{
    int cardH = viewport.getHeight();
    int totalW = (int)cards.size() * (cardWidth + cardSpacing);
    strip.setSize(juce::jmax(totalW, viewport.getWidth()), cardH);

    for (int i = 0; i < cards.size(); ++i)
    {
        auto* card = cards[i];
        card->setBounds(i * (cardWidth + cardSpacing), 0, cardWidth, cardH);
        strip.addAndMakeVisible(card);
    }
}

//==============================================================================
//  HomePage
//==============================================================================
HomePage::HomePage()
{
    auto& lm = LocalizationManager::getInstance();

    recentRow      = std::make_unique<SongRow>();
    newSongsRow    = std::make_unique<SongRow>();
    popularRow     = std::make_unique<SongRow>();
    recommendedRow = std::make_unique<SongRow>();

    recentRow->setTitle(lm.getText("home.recently_played"));
    newSongsRow->setTitle(lm.getText("home.new_songs"));
    popularRow->setTitle(lm.getText("home.popular_songs"));
    recommendedRow->setTitle(lm.getText("home.recommended_songs"));

    // Wire click callbacks — every row resolves to a real CdgSong via its
    // backing list populated by setSongsFromLibrary().
    recentRow->onSongClicked = [this](int idx)
    {
        if (onSongClicked && idx >= 0 && idx < (int)recentSongsSongs_.size())
            onSongClicked(recentSongsSongs_[(size_t)idx]);
    };
    newSongsRow->onSongClicked = [this](int idx)
    {
        if (onSongClicked && idx >= 0 && idx < (int)newSongsSongs_.size())
            onSongClicked(newSongsSongs_[(size_t)idx]);
    };
    popularRow->onSongClicked = [this](int idx)
    {
        if (onSongClicked && idx >= 0 && idx < (int)popularSongsSongs_.size())
            onSongClicked(popularSongsSongs_[(size_t)idx]);
    };
    recommendedRow->onSongClicked = [this](int idx)
    {
        if (onSongClicked && idx >= 0 && idx < (int)recommendedSongsSongs_.size())
            onSongClicked(recommendedSongsSongs_[(size_t)idx]);
    };

    // Add rows to scroll content
    pageContent.addAndMakeVisible(recentRow.get());
    pageContent.addAndMakeVisible(newSongsRow.get());
    pageContent.addAndMakeVisible(popularRow.get());
    pageContent.addAndMakeVisible(recommendedRow.get());

    pageViewport.setViewedComponent(&pageContent, false);
    pageViewport.setScrollBarsShown(true, false);
    // Vertical swipe gesture for touchscreens.  Inner SongRows have their own
    // ScrollOnDragMode set so horizontal drags stay inside them and only
    // vertical drags propagate up to scroll the page.
    pageViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    addAndMakeVisible(pageViewport);

    // All four rows are populated by setSongsFromLibrary() once the
    // LibraryPage finishes scanning the venue's songbook.
}

//==============================================================================
void HomePage::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

//==============================================================================
void HomePage::resized()
{
    pageViewport.setBounds(getLocalBounds().reduced(20, 10));
    layoutRows();
}

void HomePage::layoutRows()
{
    int w = pageViewport.getWidth();
    int y = 10;
    int rh = SongRow::rowHeight;

    recentRow->setBounds(0, y, w, rh);
    y += rh + 10;

    newSongsRow->setBounds(0, y, w, rh);
    y += rh + 10;

    popularRow->setBounds(0, y, w, rh);
    y += rh + 10;

    recommendedRow->setBounds(0, y, w, rh);
    y += rh + 10;

    pageContent.setSize(w, y);
}

//==============================================================================
void HomePage::setRecentlyPlayed(const std::vector<Track>& tracks)
{
    recentRow->setTracks(tracks);
}

// Populate a SongRow + its backing click-resolution list from a Playlist
// vector.  Each Playlist entry is matched against `library` by id; when a
// match is found the full CdgSong is stored so Play Now / Add to Queue have
// access to the local file paths.  Otherwise a minimal stub is built from
// the Playlist data — clicks still open the Song Selection dialog but with
// only the metadata Firestore knows about.
static void populateRowFromPlaylist(SongRow& row,
                                    std::vector<CdgSong>& backing,
                                    const std::vector<Playlist>& playlists,
                                    const std::vector<CdgSong>& library)
{
    backing.clear();
    backing.reserve(playlists.size());

    for (const auto& p : playlists)
    {
        const CdgSong* match = nullptr;
        if (! p.id.empty())
        {
            for (const auto& s : library)
                if (s.id == p.id) { match = &s; break; }
        }
        if (match != nullptr)
        {
            CdgSong song = *match;
            if (! p.imageUrl.empty()) song.imageUrl = p.imageUrl;
            backing.push_back(std::move(song));
        }
        else
        {
            CdgSong stub;
            stub.id         = p.id;
            stub.songName   = p.songName;
            stub.artistName = p.artistName;
            stub.imageUrl   = p.imageUrl;
            backing.push_back(std::move(stub));
        }
    }

    row.setPlaylists(playlists);
}

void HomePage::setRecentlyPlayedFromHistory(const std::vector<Playlist>& items)
{
    populateRowFromPlaylist(*recentRow, recentSongsSongs_, items, libraryRef_);
}

void HomePage::setNewSongs(const std::vector<Playlist>& playlists)
{
    populateRowFromPlaylist(*newSongsRow, newSongsSongs_, playlists, libraryRef_);
}

void HomePage::setPopularSongs(const std::vector<Playlist>& playlists)
{
    populateRowFromPlaylist(*popularRow, popularSongsSongs_, playlists, libraryRef_);
}

void HomePage::setRecommendedSongs(const std::vector<Playlist>& playlists)
{
    populateRowFromPlaylist(*recommendedRow, recommendedSongsSongs_, playlists, libraryRef_);
}

//==============================================================================
void HomePage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    recentRow->setTitle(lm.getText("home.recently_played"));
    newSongsRow->setTitle(lm.getText("home.new_songs"));
    popularRow->setTitle(lm.getText("home.popular_songs"));
    recommendedRow->setTitle(lm.getText("home.recommended_songs"));
}

//==============================================================================
void HomePage::setSongsFromLibrary(const std::vector<CdgSong>& songs)
{
    // Cached so populateRowFromPlaylist() (setNewSongs/setPopularSongs/
    // setRecommendedSongs/setRecentlyPlayedFromHistory) can resolve a
    // Firestore Playlist entry back to its full local CdgSong by id.
    // New/Popular/Recommended/Recently Played are all driven by the
    // venue's real Firestore data now (see MainComponent) -- none of them
    // are derived from the local library directly, so a fresh/first scan
    // never shows up as "new" on its own.
    libraryRef_ = songs;
}
