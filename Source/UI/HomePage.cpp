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

void HomePage::setNewSongs(const std::vector<Playlist>& playlists)
{
    newSongsRow->setPlaylists(playlists);
}

void HomePage::setPopularSongs(const std::vector<Playlist>& playlists)
{
    popularRow->setPlaylists(playlists);
}

void HomePage::setRecommendedSongs(const std::vector<Playlist>& playlists)
{
    recommendedRow->setPlaylists(playlists);
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
    if (songs.empty()) return;

    constexpr int maxCards = 20;

    // Helper: build a Playlist from a CdgSong
    auto toPlaylist = [](const CdgSong& s) {
        Playlist p;
        p.songName   = s.songName;
        p.artistName = s.artistName;
        p.imageUrl   = s.imageUrl;
        return p;
    };

    // Restrict every home-page row to songs that have album artwork — the
    // cards look broken without an image, and the venue's full library is
    // already available on the Search page if the user wants more.
    std::vector<const CdgSong*> withArt;
    withArt.reserve(songs.size());
    for (auto& s : songs)
        if (! s.imageUrl.empty())
            withArt.push_back(&s);

    if (withArt.empty()) return;

    // Helper: take up to maxCards songs from `pool`, write them into the
    // SongRow + backing list, and refresh the row.
    auto fillRow = [&](SongRow& row,
                       const std::vector<const CdgSong*>& pool,
                       std::vector<CdgSong>& outBacking)
    {
        std::vector<Playlist> cards;
        outBacking.clear();
        cards.reserve((size_t) maxCards);
        outBacking.reserve((size_t) maxCards);

        for (auto* sp : pool)
        {
            cards.push_back(toPlaylist(*sp));
            outBacking.push_back(*sp);
            if ((int) cards.size() >= maxCards) break;
        }
        if (! cards.empty())
            row.setPlaylists(cards);
    };

    // --- Popular: highest total rating, drawn from the art-only pool ---
    {
        std::vector<const CdgSong*> sorted = withArt;
        std::stable_sort(sorted.begin(), sorted.end(),
            [](const CdgSong* a, const CdgSong* b) {
                double rA = 0.0, rB = 0.0;
                for (auto r : a->rating) rA += r;
                for (auto r : b->rating) rB += r;
                return rA > rB;
            });
        fillRow(*popularRow, sorted, popularSongsSongs_);
    }

    // --- New songs: most recent release date (empty dates sort last) ---
    {
        std::vector<const CdgSong*> dated = withArt;
        std::stable_sort(dated.begin(), dated.end(),
            [](const CdgSong* a, const CdgSong* b) {
                if (a->releaseDate.empty() && b->releaseDate.empty()) return false;
                if (a->releaseDate.empty()) return false;
                if (b->releaseDate.empty()) return true;
                return a->releaseDate > b->releaseDate;
            });
        fillRow(*newSongsRow, dated, newSongsSongs_);
    }

    // --- Recommended: random shuffle (fresh seed each app launch) ---
    {
        std::vector<const CdgSong*> rec = withArt;
        juce::Random rng((int) juce::Time::getMillisecondCounter());
        for (int i = (int) rec.size() - 1; i > 0; --i)
        {
            int j = rng.nextInt(i + 1);
            std::swap(rec[(size_t) i], rec[(size_t) j]);
        }
        fillRow(*recommendedRow, rec, recommendedSongsSongs_);
    }

    // --- Recently Played: separate random shuffle until real play history
    //     is wired up.  Different seed so it doesn't mirror Recommended. ---
    {
        std::vector<const CdgSong*> recent = withArt;
        juce::Random rng((int) juce::Time::getMillisecondCounter() ^ 0x5a5a5a5a);
        for (int i = (int) recent.size() - 1; i > 0; --i)
        {
            int j = rng.nextInt(i + 1);
            std::swap(recent[(size_t) i], recent[(size_t) j]);
        }
        fillRow(*recentRow, recent, recentSongsSongs_);
    }
}
