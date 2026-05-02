/*
  ==============================================================================

    MainArea.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

  ==============================================================================
*/

#include "MainArea.h"
#include "SongSelectionDialog.h"

//==============================================================================
void MainArea::loadBackgroundTile()
{
    auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    juce::File tileFile = appDir.getChildFile("assets/images/backgrounds/background1.png");
    if (tileFile.existsAsFile())
    {
        backgroundTile_ = juce::ImageFileFormat::loadFrom(tileFile);
        if (backgroundTile_.isValid())
            repaint();
    }
}

//==============================================================================
MainArea::MainArea()
{
    auto& lm = LocalizationManager::getInstance();

    // Create real Home page
    {
        auto hp = std::make_unique<HomePage>();
        homePage = hp.get();
        addChildComponent(hp.get());
        pages[static_cast<int>(NavPage::Home)] = std::move(hp);
    }
    // Create real Search page
    {
        auto sp = std::make_unique<SearchPage>();
        searchPage = sp.get();
        addChildComponent(sp.get());
        pages[static_cast<int>(NavPage::Search)] = std::move(sp);
    }

    // Show the Song Selection dialog whenever a song is chosen from Search or Home.
    // Only require a song name + artist — we don't gate on `id` here because
    // some scanned files have valid title/artist but blank IDs, and silently
    // dropping the click was confusing for the user.
    auto openSongDialog = [this](const CdgSong& song) {
        if (song.songName.empty() && song.artistName.empty()) return;
        SongSelectionDialog::launch(this, song,
            [this](const SongSelectionResult& r) {
                if (onSongSelectionResult) onSongSelectionResult(r);
            });
    };
    searchPage->onSongClicked = openSongDialog;

    // Edit column → Song Edit dialog → forward the result up to MainComponent
    // for persistence.  MainComponent supplies the initial playlist
    // membership and (later) a Spotify-style metadata fetcher.
    searchPage->onSongEditClicked = [this](const CdgSong& song) {
        SongEditDialog::InitialPlaylists pls;
        if (onSongEditPlaylistQuery)
            onSongEditPlaylistQuery(song, pls);

        SongEditDialog::launch(this, song, pls, onSongEditFetchMetadata,
            [this](const SongEditResult& r) {
                if (! r.isCancel() && onSongEditResult)
                    onSongEditResult(r);
            });
    };

    // Create real Library page
    {
        auto lp = std::make_unique<LibraryPage>();
        libraryPage = lp.get();

        // When the songbook changes, push the updated songs to Search and Home pages
        libraryPage->onSongbookChanged = [this](const std::vector<CdgSong>& songs) {
            if (searchPage) searchPage->setSongs(songs);
            if (homePage)   homePage->setSongsFromLibrary(songs);
        };

        // Seed Search and Home pages with whatever is already on disk
        if (searchPage)
            searchPage->setSongs(libraryPage->getSongs());
        if (homePage)
        {
            homePage->setSongsFromLibrary(libraryPage->getSongs());
            homePage->onSongClicked = openSongDialog;
        }

        addChildComponent(lp.get());
        pages[static_cast<int>(NavPage::Library)] = std::move(lp);
    }

    addPage(NavPage::Charts,          lm.getText("page.charts"));
    addPage(NavPage::Mixer,           lm.getText("page.mixer"));

    // Create real Settings page
    {
        auto sp = std::make_unique<SettingsPage>();
        settingsPage = sp.get();

        settingsPage->onSettingsChanged = [this](const VenueItem& updated) {
            if (onVenueSettingsChanged)
                onVenueSettingsChanged(updated);
        };

        addChildComponent(sp.get());
        pages[static_cast<int>(NavPage::Settings)] = std::move(sp);
    }
    addPage(NavPage::Testing,         lm.getText("page.testing"));
    addPage(NavPage::Ads,             lm.getText("page.ads"));
    addPage(NavPage::Playlist,        lm.getText("page.playlist"));
    addPage(NavPage::VenueManagement, lm.getText("page.venue_management"));

    // Show Home by default
    setCurrentPage(NavPage::Home);

    loadBackgroundTile();
}

//==============================================================================
void MainArea::addPage(NavPage page, const juce::String& label)
{
    auto comp = std::make_unique<PlaceholderPage>(label);
    addChildComponent(comp.get());
    pages[static_cast<int>(page)] = std::move(comp);
}

//==============================================================================
void MainArea::paint(juce::Graphics& g)
{
    if (backgroundTile_.isValid())
    {
        float aspect = (float)backgroundTile_.getHeight() / (float)backgroundTile_.getWidth();
        int tw = tileSize_;
        int th = juce::roundToInt(tw * aspect);
        if (th < 1) th = tw;
        for (int y = 0; y < getHeight(); y += th)
            for (int x = 0; x < getWidth(); x += tw)
                g.drawImage(backgroundTile_, x, y, tw, th,
                            0, 0, backgroundTile_.getWidth(), backgroundTile_.getHeight());
    }
    else
    {
        g.fillAll(juce::Colour(0xff16213e));
    }
}

//==============================================================================
void MainArea::resized()
{
    auto bounds = getLocalBounds();
    for (auto& [key, comp] : pages)
        comp->setBounds(bounds);
}

//==============================================================================
void MainArea::setCurrentPage(NavPage page)
{
    currentPage = page;

    for (auto& [key, comp] : pages)
        comp->setVisible(key == static_cast<int>(page));
}

const std::vector<CdgSong>& MainArea::getLibrarySongs() const
{
    static const std::vector<CdgSong> empty;
    return libraryPage != nullptr ? libraryPage->getSongs() : empty;
}

//==============================================================================
void MainArea::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();

    static const struct { NavPage page; const char* key; } pageKeys[] = {
        { NavPage::Home,            "page.home" },
        { NavPage::Search,          "page.search" },
        { NavPage::Library,         "page.library" },
        { NavPage::Charts,          "page.charts" },
        { NavPage::Mixer,           "page.mixer" },
        { NavPage::Settings,        "page.settings" },
        { NavPage::Testing,         "page.testing" },
        { NavPage::Ads,             "page.ads" },
        { NavPage::Playlist,        "page.playlist" },
        { NavPage::VenueManagement, "page.venue_management" },
    };

    for (auto& pk : pageKeys)
    {
        auto it = pages.find(static_cast<int>(pk.page));
        if (it != pages.end())
        {
            if (auto* placeholder = dynamic_cast<PlaceholderPage*>(it->second.get()))
                placeholder->setPageName(lm.getText(pk.key));
        }
    }

    // Update concrete pages
    if (homePage)     homePage->updateAllText();
    if (searchPage)   searchPage->updateAllText();
    if (libraryPage)  libraryPage->updateAllText();
    if (settingsPage) settingsPage->updateAllText();
}
