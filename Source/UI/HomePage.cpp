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

void SongCard::mouseUp(const juce::MouseEvent&)
{
    if (onClick) onClick();
}

//==============================================================================
//  SongRow
//==============================================================================
SongRow::SongRow()
{
    viewport.setViewedComponent(&strip, false);
    viewport.setScrollBarsShown(false, true); // horizontal scrollbar
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

    // Wire click callbacks
    recentRow->onSongClicked = [this](int idx)
    {
        if (onSongClicked) onSongClicked("recent", idx);
    };
    newSongsRow->onSongClicked = [this](int idx)
    {
        if (onSongClicked) onSongClicked("new", idx);
    };
    popularRow->onSongClicked = [this](int idx)
    {
        if (onSongClicked) onSongClicked("popular", idx);
    };
    recommendedRow->onSongClicked = [this](int idx)
    {
        if (onSongClicked) onSongClicked("recommended", idx);
    };

    // Add rows to scroll content
    pageContent.addAndMakeVisible(recentRow.get());
    pageContent.addAndMakeVisible(newSongsRow.get());
    pageContent.addAndMakeVisible(popularRow.get());
    pageContent.addAndMakeVisible(recommendedRow.get());

    pageViewport.setViewedComponent(&pageContent, false);
    pageViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(pageViewport);

    // Populate with sample data for demo
    {
        std::vector<Track> sampleRecent;
        auto makeTrack = [](const char* name, const char* artist) {
            Track t;
            t.name = name;
            t.artists = artist;
            return t;
        };
        sampleRecent.push_back(makeTrack("Bohemian Rhapsody", "Queen"));
        sampleRecent.push_back(makeTrack("Don't Stop Believin'", "Journey"));
        sampleRecent.push_back(makeTrack("Sweet Caroline", "Neil Diamond"));
        // Recently played is populated at runtime when actual play history is available.
        // New / Popular / Recommended are populated via setSongsFromLibrary().
    }
}

//==============================================================================
void HomePage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff16213e));
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

    // --- Popular: highest total rating across all versions ---
    {
        std::vector<const CdgSong*> sorted;
        sorted.reserve(songs.size());
        for (auto& s : songs)
            sorted.push_back(&s);

        std::stable_sort(sorted.begin(), sorted.end(), [](const CdgSong* a, const CdgSong* b) {
            double rA = 0.0, rB = 0.0;
            for (auto r : a->rating) rA += r;
            for (auto r : b->rating) rB += r;
            return rA > rB;
        });

        // Only include songs that actually have a rating
        std::vector<Playlist> popular;
        for (auto* sp : sorted)
        {
            double total = 0.0;
            for (auto r : sp->rating) total += r;
            if (total <= 0.0) break;
            popular.push_back(toPlaylist(*sp));
            if ((int)popular.size() >= maxCards) break;
        }
        if (! popular.empty())
            popularRow->setPlaylists(popular);
    }

    // --- New songs: most recent release date (non-empty) ---
    {
        std::vector<const CdgSong*> dated;
        for (auto& s : songs)
            if (! s.releaseDate.empty()) dated.push_back(&s);

        std::stable_sort(dated.begin(), dated.end(), [](const CdgSong* a, const CdgSong* b) {
            return a->releaseDate > b->releaseDate; // lexicographic desc (ISO date strings)
        });

        std::vector<Playlist> newest;
        for (auto* sp : dated)
        {
            newest.push_back(toPlaylist(*sp));
            if ((int)newest.size() >= maxCards) break;
        }
        if (! newest.empty())
            newSongsRow->setPlaylists(newest);
    }

    // --- Recommended: random subset of songs with artwork ---
    {
        std::vector<const CdgSong*> withArt;
        for (auto& s : songs)
            if (! s.imageUrl.empty()) withArt.push_back(&s);

        // Deterministic shuffle using a fixed seed so it changes each scan but is stable within a session
        juce::Random rng(42);
        for (int i = (int)withArt.size() - 1; i > 0; --i)
        {
            int j = rng.nextInt(i + 1);
            std::swap(withArt[(size_t)i], withArt[(size_t)j]);
        }

        std::vector<Playlist> rec;
        for (auto* sp : withArt)
        {
            rec.push_back(toPlaylist(*sp));
            if ((int)rec.size() >= maxCards) break;
        }
        if (! rec.empty())
            recommendedRow->setPlaylists(rec);
    }
}
