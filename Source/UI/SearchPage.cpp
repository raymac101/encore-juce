/*
  ==============================================================================

    SearchPage.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Implementation of the advanced song search page.

  ==============================================================================
*/

#include "SearchPage.h"
#include <algorithm>
#include <cctype>

//==============================================================================
//  SongResultRow
//==============================================================================
SearchPage::SongResultRow::SongResultRow() {}

void SearchPage::SongResultRow::setImageUrl(const juce::String& url)
{
    if (url.isEmpty()) return;

    // Single call: returns image if cached, queues callback if not.
    juce::Component::SafePointer<SongResultRow> safeThis(this);
    juce::Image img = ArtworkCache::getInstance().getOrFetch(url, [safeThis, url]()
    {
        if (safeThis == nullptr) return;
        juce::Image fetched = ArtworkCache::getInstance().getOrFetch(url);
        if (fetched.isValid())
        {
            safeThis->cachedImage = fetched;
            safeThis->repaint();
        }
    });

    if (img.isValid())
        cachedImage = img;
}

void SearchPage::SongResultRow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Hover highlight
    if (hovering)
        g.fillAll(juce::Colour(0xff292929));

    // Column widths: 6% art | 23% song | 23% artist | 16% version | 8% year | 16% genre | 8% edit
    int totalW = bounds.getWidth();
    int artW     = (int)(totalW * 0.06f);
    int songW    = (int)(totalW * 0.23f);
    int artistW  = (int)(totalW * 0.23f);
    int versionW = (int)(totalW * 0.16f);
    int yearW    = (int)(totalW * 0.08f);
    int genreW   = (int)(totalW * 0.16f);
    // remaining = edit column

    int x = 0;

    // Artwork placeholder
    {
        auto artRect = juce::Rectangle<int>(x + 4, 4, artW - 8, bounds.getHeight() - 8);
        if (cachedImage.isValid())
        {
            g.drawImage(cachedImage, artRect.toFloat(),
                        juce::RectanglePlacement::centred |
                        juce::RectanglePlacement::fillDestination);
        }
        else
        {
            g.setColour(juce::Colour(0xff6c6c6c));
            g.fillRect(artRect);
            // Initial letter placeholder
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.setFont(juce::Font(juce::FontOptions().withHeight(14.f)));
            juce::String initial = juce::String(song.songName).substring(0, 1).toUpperCase();
            g.drawText(initial, artRect, juce::Justification::centred);
        }
    }
    x += artW;

    // Song name
    g.setColour(juce::Colour(0xffe4e4e4));
    g.setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
    g.drawText(juce::String(song.songName), x, 0, songW, bounds.getHeight(),
               juce::Justification::centredLeft, true);
    x += songW;

    // Artist
    g.setColour(juce::Colour(0xffa3a6a8));
    g.drawText(juce::String(song.artistName), x, 0, artistW, bounds.getHeight(),
               juce::Justification::centredLeft, true);
    x += artistW;

    // Version
    {
        juce::String verStr;
        for (size_t i = 0; i < song.version.size(); ++i)
        {
            if (i > 0) verStr += ", ";
            verStr += juce::String(song.version[i]);
        }
        g.setColour(juce::Colour(0xffe4e4e4));
        g.drawText(verStr, x, 0, versionW, bounds.getHeight(),
                   juce::Justification::centredLeft, true);
    }
    x += versionW;

    // Year
    g.drawText(juce::String(song.releaseDate), x, 0, yearW, bounds.getHeight(),
               juce::Justification::centredLeft, true);
    x += yearW;

    // Genre
    {
        juce::String genreStr;
        for (size_t i = 0; i < song.genres.size(); ++i)
        {
            if (i > 0) genreStr += ", ";
            genreStr += juce::String(song.genres[i]);
        }
        g.drawText(genreStr, x, 0, genreW, bounds.getHeight(),
                   juce::Justification::centredLeft, true);
    }
    x += genreW;

    // Edit column – pencil icon area
    auto editRect = juce::Rectangle<int>(x, 0, bounds.getWidth() - x, bounds.getHeight());
    g.setColour(juce::Colour(0xffe4e4e4));
    g.setFont(juce::Font(juce::FontOptions().withHeight(16.f)));
    g.drawText(LocalizationManager::getInstance().getText("search.row_edit"),
               editRect, juce::Justification::centred);

    // Bottom separator
    g.setColour(juce::Colour(0xff585757).withAlpha(0.3f));
    g.drawHorizontalLine(bounds.getBottom() - 1, 0.f, (float)totalW);
}

void SearchPage::SongResultRow::mouseUp(const juce::MouseEvent& e)
{
    // Check if click was in the edit column (last 8%)
    int editStart = (int)(getWidth() * 0.92f);
    if (e.x >= editStart)
    {
        if (onEditClicked) onEditClicked(index);
    }
    else
    {
        if (onClicked) onClicked(index);
    }
}

//==============================================================================
//  SearchPage
//==============================================================================
SearchPage::SearchPage()
{
    auto& lm = LocalizationManager::getInstance();

    // Title
    titleLabel = std::make_unique<juce::Label>("title", searchName);
    titleLabel->setColour(juce::Label::textColourId, textColour);
    titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(32.f)).boldened());
    addAndMakeVisible(*titleLabel);

    // Count
    countLabel = std::make_unique<juce::Label>("count", "0 songs in this list");
    countLabel->setColour(juce::Label::textColourId, textColour.withAlpha(0.6f));
    countLabel->setFont(juce::Font(juce::FontOptions().withHeight(14.f)));
    addAndMakeVisible(*countLabel);

    // Search box
    searchBox = std::make_unique<juce::TextEditor>("search");
    searchBox->setMultiLine(false);
    searchBox->setTextToShowWhenEmpty(lm.getText("search.placeholder"), juce::Colours::grey);
    searchBox->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3a3a3a));
    searchBox->setColour(juce::TextEditor::textColourId, textColour);
    searchBox->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    searchBox->setFont(juce::Font(juce::FontOptions().withHeight(18.f)));
    searchBox->addListener(this);
    addAndMakeVisible(*searchBox);

    // Clear button
    clearButton = std::make_unique<juce::TextButton>(lm.getText("search.clear"));
    clearButton->setColour(juce::TextButton::buttonColourId, accentColour);
    clearButton->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    clearButton->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    clearButton->onClick = [this]() { clearSearch(); };
    addAndMakeVisible(*clearButton);

    // Filter buttons
    auto makeFilterBtn = [&](const juce::String& text) {
        auto btn = std::make_unique<juce::TextButton>(text);
        btn->setColour(juce::TextButton::buttonColourId, darkColour);
        btn->setColour(juce::TextButton::textColourOnId, textColour);
        btn->setColour(juce::TextButton::textColourOffId, textColour);
        addAndMakeVisible(*btn);
        return btn;
    };

    filterAllBtn    = makeFilterBtn(lm.getText("search.filter_all"));
    filterSongBtn   = makeFilterBtn(lm.getText("search.filter_song"));
    filterArtistBtn = makeFilterBtn(lm.getText("search.filter_artist"));
    filterYearBtn   = makeFilterBtn(lm.getText("search.filter_year"));
    filterGenreBtn  = makeFilterBtn(lm.getText("search.filter_genre"));

    filterAllBtn->onClick    = [this]() { setFilterMode(FilterMode::All);    };
    filterSongBtn->onClick   = [this]() { setFilterMode(FilterMode::Song);   };
    filterArtistBtn->onClick = [this]() { setFilterMode(FilterMode::Artist); };
    filterYearBtn->onClick   = [this]() { setFilterMode(FilterMode::Year);   };
    filterGenreBtn->onClick  = [this]() { setFilterMode(FilterMode::Genre);  };
    updateFilterButtonColours();

    // A-Z letter buttons
    {
        juce::String letters = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        for (int i = 0; i < letters.length(); ++i)
        {
            auto ch = letters.substring(i, i + 1);
            auto* btn = new juce::TextButton(ch);
            btn->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            btn->setColour(juce::TextButton::textColourOnId, textColour);
            btn->setColour(juce::TextButton::textColourOffId, textColour.withAlpha(0.6f));
            btn->onClick = [this, ch]() { applyLetterFilter(ch); };
            addAndMakeVisible(btn);
            letterButtons.add(btn);
        }
    }

    // Column header buttons
    auto makeColBtn = [&](const juce::String& text) {
        auto btn = std::make_unique<juce::TextButton>(text);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btn->setColour(juce::TextButton::textColourOnId, juce::Colour(0xfff7f8fa));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff7f8fa));
        addAndMakeVisible(*btn);
        return btn;
    };

    colSongBtn    = makeColBtn(lm.getText("search.col_song"));
    colArtistBtn  = makeColBtn(lm.getText("search.col_artist"));
    colVersionBtn = makeColBtn(lm.getText("search.col_version"));
    colYearBtn    = makeColBtn(lm.getText("search.col_year"));
    colGenreBtn   = makeColBtn(lm.getText("search.col_genre"));

    colSongBtn->onClick    = [this]() { sortByColumn(SortColumn::Song);    };
    colArtistBtn->onClick  = [this]() { sortByColumn(SortColumn::Artist);  };
    colVersionBtn->onClick = [this]() { sortByColumn(SortColumn::Version); };
    colYearBtn->onClick    = [this]() { sortByColumn(SortColumn::Year);    };
    colGenreBtn->onClick   = [this]() { sortByColumn(SortColumn::Genre);   };

    // Scroll-to-top button
    scrollTopButton = std::make_unique<juce::TextButton>(lm.getText("search.scroll_top"));
    scrollTopButton->setColour(juce::TextButton::buttonColourId, juce::Colours::black.withAlpha(0.7f));
    scrollTopButton->setColour(juce::TextButton::textColourOnId, accentColour);
    scrollTopButton->setColour(juce::TextButton::textColourOffId, accentColour);
    scrollTopButton->onClick = [this]() { listViewport.setViewPosition(0, 0); };
    scrollTopButton->setVisible(false);
    addAndMakeVisible(*scrollTopButton);

    // Results viewport
    listViewport.setViewedComponent(&listContent, false);
    listViewport.setScrollBarsShown(true, false);
    listViewport.onScrolled = [this]() {
        bool scrolled = listViewport.getViewPositionY() > 100;
        scrollTopButton->setVisible(scrolled);

        // Append next batch when within 300px of the loaded content bottom
        int viewBottom = listViewport.getViewPositionY() + listViewport.getMaximumVisibleHeight();
        int contentH   = loadedCount * resultRowHeight;
        if (viewBottom > contentH - 300)
            loadMoreRows();
    };
    addAndMakeVisible(listViewport);

    // Songs are populated via setSongs() once the songbook is loaded.
    // No demo data — the search page starts empty until the library is ready.
}

//==============================================================================
void SearchPage::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

//==============================================================================
void SearchPage::resized()
{
    auto bounds = getLocalBounds().reduced(20, 10);

    // Title bar (title + count side by side)
    {
        auto titleArea = bounds.removeFromTop(titleBarHeight);
        titleLabel->setBounds(titleArea.removeFromTop(titleArea.getHeight() * 2 / 3));
        countLabel->setBounds(titleArea);
    }

    // Search bar
    {
        auto searchArea = bounds.removeFromTop(searchBarHeight).reduced(0, 2);
        clearButton->setBounds(searchArea.removeFromRight(80).reduced(4, 0));
        searchBox->setBounds(searchArea);
    }

    // Letter bar (A-Z)
    {
        auto letterArea = bounds.removeFromTop(letterBarHeight);
        int btnW = letterArea.getWidth() / letterButtons.size();
        for (int i = 0; i < letterButtons.size(); ++i)
            letterButtons[i]->setBounds(letterArea.removeFromLeft(btnW));
    }

    // Filter bar
    {
        auto filterArea = bounds.removeFromTop(filterBarHeight).reduced(0, 2);
        int bw = 80;
        filterAllBtn->setBounds(filterArea.removeFromLeft(bw).reduced(2, 0));
        filterSongBtn->setBounds(filterArea.removeFromLeft(bw).reduced(2, 0));
        filterArtistBtn->setBounds(filterArea.removeFromLeft(bw).reduced(2, 0));
        filterYearBtn->setBounds(filterArea.removeFromLeft(bw).reduced(2, 0));
        filterGenreBtn->setBounds(filterArea.removeFromLeft(bw).reduced(2, 0));
    }

    // Column headers
    {
        auto colArea = bounds.removeFromTop(columnHeaderHeight);
        int totalW = colArea.getWidth();
        int artW     = (int)(totalW * 0.06f);
        int songW    = (int)(totalW * 0.23f);
        int artistW  = (int)(totalW * 0.23f);
        int versionW = (int)(totalW * 0.16f);
        int yearW    = (int)(totalW * 0.08f);
        int genreW   = (int)(totalW * 0.16f);

        colArea.removeFromLeft(artW); // artwork column has no header
        colSongBtn->setBounds(colArea.removeFromLeft(songW));
        colArtistBtn->setBounds(colArea.removeFromLeft(artistW));
        colVersionBtn->setBounds(colArea.removeFromLeft(versionW));
        colYearBtn->setBounds(colArea.removeFromLeft(yearW));
        colGenreBtn->setBounds(colArea.removeFromLeft(genreW));
    }

    // Results list fills remaining space
    listViewport.setBounds(bounds);

    int totalH = (int)resultRows.size() * resultRowHeight;
    listContent.setSize(bounds.getWidth(), juce::jmax(totalH, bounds.getHeight()));

    for (int i = 0; i < resultRows.size(); ++i)
        resultRows[i]->setBounds(0, i * resultRowHeight, listContent.getWidth(), resultRowHeight);

    // Scroll-to-top button (floating bottom-right)
    scrollTopButton->setBounds(bounds.getRight() - 50, bounds.getBottom() - 50, 40, 40);
}

//==============================================================================
void SearchPage::textEditorTextChanged(juce::TextEditor&)
{
    applySearch();
}

//==============================================================================
juce::String SearchPage::normalise(const juce::String& text)
{
    // Strip non-alphanumeric, lowercase
    juce::String result;
    for (int i = 0; i < text.length(); ++i)
    {
        auto ch = text[i];
        if (juce::CharacterFunctions::isLetterOrDigit(ch) || ch == ' ')
            result += juce::CharacterFunctions::toLowerCase(ch);
    }
    return result;
}

bool SearchPage::matchesPartialSearch(const juce::String& searchText,
                                       const CdgSong& song,
                                       FilterMode mode)
{
    if (searchText.isEmpty()) return true;

    // Split search into words
    juce::StringArray words;
    words.addTokens(normalise(searchText), " ", "");
    words.removeEmptyStrings();

    // Build field strings based on filter mode
    juce::String songNorm    = normalise(juce::String(song.songName));
    juce::String artistNorm  = normalise(juce::String(song.artistName));
    juce::String yearNorm    = normalise(juce::String(song.releaseDate));

    juce::String genreNorm;
    for (auto& g : song.genres)
        genreNorm += normalise(juce::String(g)) + " ";

    juce::String versionNorm;
    for (auto& v : song.version)
        versionNorm += normalise(juce::String(v)) + " ";

    // Every word must match at least one relevant field
    for (auto& word : words)
    {
        bool found = false;

        switch (mode)
        {
            case FilterMode::Song:
                found = songNorm.contains(word);
                break;
            case FilterMode::Artist:
                found = artistNorm.contains(word);
                break;
            case FilterMode::Year:
                found = yearNorm.contains(word);
                break;
            case FilterMode::Genre:
                found = genreNorm.contains(word);
                break;
            case FilterMode::All:
            default:
                found = songNorm.contains(word)
                     || artistNorm.contains(word)
                     || versionNorm.contains(word)
                     || yearNorm.contains(word)
                     || genreNorm.contains(word);
                break;
        }

        if (!found) return false;
    }
    return true;
}

//==============================================================================
void SearchPage::applySearch()
{
    juce::String query = searchBox->getText();
    currentLetter = "";

    filteredSongs.clear();
    for (auto& s : allSongs)
    {
        if (matchesPartialSearch(query, s, filterMode))
            filteredSongs.push_back(s);
    }

    rebuildDisplayList();
}

void SearchPage::applyLetterFilter(const juce::String& letter)
{
    if (currentLetter == letter)
    {
        // Toggle off – show all
        currentLetter = "";
        filteredSongs = allSongs;
    }
    else
    {
        currentLetter = letter;
        filteredSongs.clear();
        searchBox->clear();

        for (auto& s : allSongs)
        {
            juce::String firstChar;
            if (filterMode == FilterMode::Artist)
                firstChar = juce::String(s.artistName).trimStart().substring(0, 1).toUpperCase();
            else
                firstChar = juce::String(s.songName).trimStart().substring(0, 1).toUpperCase();

            if (letter == "#")
            {
                // Numbers and symbols
                if (!juce::CharacterFunctions::isLetter(firstChar[0]))
                    filteredSongs.push_back(s);
            }
            else
            {
                if (firstChar == letter)
                    filteredSongs.push_back(s);
            }
        }
    }

    // Highlight the active letter button
    for (auto* btn : letterButtons)
    {
        bool active = (btn->getButtonText() == currentLetter);
        btn->setColour(juce::TextButton::textColourOffId,
                       active ? accentColour : textColour.withAlpha(0.6f));
    }

    rebuildDisplayList();
}

void SearchPage::clearSearch()
{
    searchBox->clear();
    currentLetter = "";
    filterMode = FilterMode::All;
    sortColumn = SortColumn::None;
    filteredSongs = allSongs;

    for (auto* btn : letterButtons)
        btn->setColour(juce::TextButton::textColourOffId, textColour.withAlpha(0.6f));

    updateFilterButtonColours();
    rebuildDisplayList();
    searchBox->grabKeyboardFocus();
}

void SearchPage::setFilterMode(FilterMode mode)
{
    filterMode = mode;
    updateFilterButtonColours();
    applySearch();
}

void SearchPage::sortByColumn(SortColumn col)
{
    if (sortColumn == col)
        sortDir = (sortDir == SortDir::Asc) ? SortDir::Desc : SortDir::Asc;
    else
    {
        sortColumn = col;
        sortDir = SortDir::Asc;
    }

    auto cmp = [&](const CdgSong& a, const CdgSong& b) -> bool {
        juce::String va, vb;
        switch (sortColumn)
        {
            case SortColumn::Song:
                va = juce::String(a.songName).toLowerCase();
                vb = juce::String(b.songName).toLowerCase();
                break;
            case SortColumn::Artist:
                va = juce::String(a.artistName).toLowerCase();
                vb = juce::String(b.artistName).toLowerCase();
                break;
            case SortColumn::Version:
                va = a.version.empty() ? "" : juce::String(a.version[0]).toLowerCase();
                vb = b.version.empty() ? "" : juce::String(b.version[0]).toLowerCase();
                break;
            case SortColumn::Year:
                va = juce::String(a.releaseDate).toLowerCase();
                vb = juce::String(b.releaseDate).toLowerCase();
                break;
            case SortColumn::Genre:
                va = a.genres.empty() ? "" : juce::String(a.genres[0]).toLowerCase();
                vb = b.genres.empty() ? "" : juce::String(b.genres[0]).toLowerCase();
                break;
            default:
                return false;
        }
        return (sortDir == SortDir::Asc) ? (va < vb) : (va > vb);
    };

    std::sort(filteredSongs.begin(), filteredSongs.end(), cmp);
    updateColumnHeaderText();
    rebuildDisplayList();
}

//==============================================================================
void SearchPage::rebuildDisplayList()
{
    int count = juce::jmin((int)filteredSongs.size(), kInitialBatch);
    displaySongs.assign(filteredSongs.begin(), filteredSongs.begin() + count);
    loadedCount = count;

    rebuildResultRows();
    updateCountLabel();
    resized();
}

void SearchPage::rebuildResultRows()
{
    for (auto* row : resultRows)
        listContent.removeChildComponent(row);
    resultRows.clear();

    for (int i = 0; i < (int)displaySongs.size(); ++i)
    {
        auto* row = new SongResultRow();
        row->song  = displaySongs[(size_t)i];
        row->index = i;
        row->onClicked = [this](int idx) {
            if (idx >= 0 && idx < (int)displaySongs.size())
                if (onSongClicked) onSongClicked(displaySongs[(size_t)idx]);
        };
        row->onEditClicked = [this](int idx) {
            if (idx >= 0 && idx < (int)displaySongs.size())
                if (onSongEditClicked) onSongEditClicked(displaySongs[(size_t)idx]);
        };
        listContent.addAndMakeVisible(row);
        resultRows.add(row);

        // Trigger async image load if the song has artwork
        if (! displaySongs[(size_t)i].imageUrl.empty())
            row->setImageUrl(juce::String(displaySongs[(size_t)i].imageUrl));
    }
}

void SearchPage::loadMoreRows()
{
    if (loadedCount >= (int)filteredSongs.size()) return;

    int next = juce::jmin(loadedCount + kScrollBatch, (int)filteredSongs.size());

    for (int i = loadedCount; i < next; ++i)
    {
        displaySongs.push_back(filteredSongs[(size_t)i]);

        auto* row = new SongResultRow();
        row->song  = filteredSongs[(size_t)i];
        row->index = i;
        row->onClicked = [this](int idx) {
            if (idx >= 0 && idx < (int)displaySongs.size())
                if (onSongClicked) onSongClicked(displaySongs[(size_t)idx]);
        };
        row->onEditClicked = [this](int idx) {
            if (idx >= 0 && idx < (int)displaySongs.size())
                if (onSongEditClicked) onSongEditClicked(displaySongs[(size_t)idx]);
        };
        listContent.addAndMakeVisible(row);
        resultRows.add(row);

        if (! filteredSongs[(size_t)i].imageUrl.empty())
            row->setImageUrl(juce::String(filteredSongs[(size_t)i].imageUrl));
    }

    loadedCount = next;
    resized();
}

void SearchPage::updateCountLabel()
{
    juce::String text = juce::String((int)filteredSongs.size()) + " " + LocalizationManager::getInstance().getText("search.count_suffix");
    countLabel->setText(text, juce::dontSendNotification);
}

//==============================================================================
void SearchPage::updateFilterButtonColours()
{
    auto setActive = [&](juce::TextButton* btn, bool active) {
        btn->setColour(juce::TextButton::buttonColourId,
                       active ? juce::Colours::lightgrey : darkColour);
        btn->setColour(juce::TextButton::textColourOffId,
                       active ? juce::Colour(0xff6c6c6c) : textColour);
    };
    setActive(filterAllBtn.get(),    filterMode == FilterMode::All);
    setActive(filterSongBtn.get(),   filterMode == FilterMode::Song);
    setActive(filterArtistBtn.get(), filterMode == FilterMode::Artist);
    setActive(filterYearBtn.get(),   filterMode == FilterMode::Year);
    setActive(filterGenreBtn.get(),  filterMode == FilterMode::Genre);
}

void SearchPage::updateColumnHeaderText()
{
    auto& lm = LocalizationManager::getInstance();
    auto arrow = [&](SortColumn col) -> juce::String {
        if (sortColumn != col) return "";
        return (sortDir == SortDir::Asc) ? " ^" : " v";
    };
    colSongBtn->setButtonText(lm.getText("search.col_song")    + arrow(SortColumn::Song));
    colArtistBtn->setButtonText(lm.getText("search.col_artist")  + arrow(SortColumn::Artist));
    colVersionBtn->setButtonText(lm.getText("search.col_version") + arrow(SortColumn::Version));
    colYearBtn->setButtonText(lm.getText("search.col_year")    + arrow(SortColumn::Year));
    colGenreBtn->setButtonText(lm.getText("search.col_genre")   + arrow(SortColumn::Genre));
}

//==============================================================================
void SearchPage::setSongs(const std::vector<CdgSong>& songs)
{
    allSongs = songs;
    filteredSongs = songs;
    rebuildDisplayList();
}

void SearchPage::setSearchName(const juce::String& name)
{
    searchName = name;
    titleLabel->setText(name, juce::dontSendNotification);
}

//==============================================================================
void SearchPage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    searchBox->setTextToShowWhenEmpty(lm.getText("search.placeholder"), juce::Colours::grey);
    clearButton->setButtonText(lm.getText("search.clear"));
    filterAllBtn->setButtonText(lm.getText("search.filter_all"));
    filterSongBtn->setButtonText(lm.getText("search.filter_song"));
    filterArtistBtn->setButtonText(lm.getText("search.filter_artist"));
    filterYearBtn->setButtonText(lm.getText("search.filter_year"));
    filterGenreBtn->setButtonText(lm.getText("search.filter_genre"));
    updateCountLabel();
}
