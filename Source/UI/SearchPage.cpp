/*
  ==============================================================================

    SearchPage.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Implementation of the advanced song search page.

  ==============================================================================
*/

#include "SearchPage.h"
#include "../Services/UserPreferences.h"
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

    int totalW = bounds.getWidth();
    // Defaults if no fractions pointer set
    static const std::vector<float> defaults {0.06f, 0.23f, 0.23f, 0.16f, 0.08f, 0.16f, 0.08f};
    const auto& f = (columnFractions && columnFractions->size() == 7) ? *columnFractions : defaults;
    int artW     = (int)(totalW * f[0]);
    int songW    = (int)(totalW * f[1]);
    int artistW  = (int)(totalW * f[2]);
    int versionW = (int)(totalW * f[3]);
    int yearW    = (int)(totalW * f[4]);
    int genreW   = (int)(totalW * f[5]);
    // edit column fills the remainder

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
    // Edit column starts where genre column ends.
    static const std::vector<float> defaults {0.06f, 0.23f, 0.23f, 0.16f, 0.08f, 0.16f, 0.08f};
    const auto& f = (columnFractions && columnFractions->size() == 7) ? *columnFractions : defaults;
    int editStart = (int)(getWidth() * (f[0] + f[1] + f[2] + f[3] + f[4] + f[5]));
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
    searchBox->setTextToShowWhenEmpty(lm.getText("search.placeholder"), juce::Colour(0xff7d8594));
    searchBox->setColour(juce::TextEditor::backgroundColourId,     juce::Colour(0xff0d1527));
    searchBox->setColour(juce::TextEditor::textColourId,           textColour);
    searchBox->setColour(juce::TextEditor::outlineColourId,        accentColour.withAlpha(0.35f));
    searchBox->setColour(juce::TextEditor::focusedOutlineColourId, accentColour);
    searchBox->setFont(juce::Font(juce::FontOptions().withHeight(18.f)));
    searchBox->setBorder(juce::BorderSize<int>(6, 10, 6, 10));
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

    // Load persisted column fractions (if any) and wire up the header bar.
    loadColumnFractions();
    columnHeaderBar.fractions = &columnFractions_;
    columnHeaderBar.cols[0] = colSongBtn.get();
    columnHeaderBar.cols[1] = colArtistBtn.get();
    columnHeaderBar.cols[2] = colVersionBtn.get();
    columnHeaderBar.cols[3] = colYearBtn.get();
    columnHeaderBar.cols[4] = colGenreBtn.get();
    columnHeaderBar.onDragLive = [this]() {
        for (auto* row : resultRows) row->repaint();
    };
    columnHeaderBar.onDragCommitted = [this]() {
        saveColumnFractions();
    };
    columnHeaderBar.addAndMakeVisible(*colSongBtn);
    columnHeaderBar.addAndMakeVisible(*colArtistBtn);
    columnHeaderBar.addAndMakeVisible(*colVersionBtn);
    columnHeaderBar.addAndMakeVisible(*colYearBtn);
    columnHeaderBar.addAndMakeVisible(*colGenreBtn);
    addAndMakeVisible(columnHeaderBar);

    // Scroll-to-top button
    scrollTopButton = std::make_unique<juce::TextButton>(lm.getText("search.scroll_top"));
    scrollTopButton->setColour(juce::TextButton::buttonColourId, juce::Colours::black.withAlpha(0.7f));
    scrollTopButton->setColour(juce::TextButton::textColourOnId, accentColour);
    scrollTopButton->setColour(juce::TextButton::textColourOffId, accentColour);
    scrollTopButton->onClick = [this]() { listViewport.setViewPosition(0, 0); };
    scrollTopButton->setVisible(false);
    scrollTopButton->setAlwaysOnTop(true);  // must receive clicks above the results list
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
    // Draw rounded translucent "cards" for the controls area and the results
    // area — same visual language as SettingsPage so the app feels consistent.
    for (const auto& r : cardRects_)
    {
        auto rf = r.toFloat();

        // Soft drop shadow
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.35f), 10, {0, 2});
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle(rf, (float)kCardRadius);
        shadow.drawForPath(g, shadowPath);

        // Card fill
        g.setColour(cardFillColour.withAlpha(0.82f));
        g.fillRoundedRectangle(rf, (float)kCardRadius);

        // Border
        g.setColour(cardBorderColour.withAlpha(0.8f));
        g.drawRoundedRectangle(rf.reduced(0.5f), (float)kCardRadius, 1.f);

        // Left accent stripe
        auto stripe = rf.withWidth(3.f).reduced(0.f, 1.f);
        g.setColour(accentSoftColour);
        g.fillRoundedRectangle(stripe, 2.f);
    }

    // Page-title accent gradient underline
    if (titleLabel && titleLabel->getWidth() > 0)
    {
        auto tb = titleLabel->getBounds();
        int uy = tb.getBottom() - 2;
        juce::ColourGradient grad(accentSoftColour, (float)tb.getX(), (float)uy,
                                  accentSoftColour.withAlpha(0.f),
                                  (float)tb.getX() + 220.f, (float)uy, false);
        g.setGradientFill(grad);
        g.fillRect(tb.getX(), uy, 220, 2);
    }
}

//==============================================================================
void SearchPage::resized()
{
    auto bounds = getLocalBounds().reduced(20, 12);
    cardRects_.clear();

    // Title bar (title + count side by side) — no card, title has gradient underline.
    {
        auto titleArea = bounds.removeFromTop(titleBarHeight);
        titleLabel->setBounds(titleArea.removeFromTop(titleArea.getHeight() * 2 / 3));
        countLabel->setBounds(titleArea);
    }

    bounds.removeFromTop(4);

    // ── Controls card (search box + letter bar + filter bar + column headers) ──
    const int controlsH = searchBarHeight
                        + letterBarHeight
                        + filterBarHeight
                        + columnHeaderHeight
                        + kCardPad * 2 + 12; // extra inner gaps between sub-rows
    juce::Rectangle<int> controlsCard(bounds.getX(), bounds.getY(),
                                      bounds.getWidth(), controlsH);
    cardRects_.push_back(controlsCard);

    auto controlsInner = controlsCard.reduced(kCardPad);
    // shift inner content right by a few px so it clears the left accent stripe
    controlsInner.setX(controlsInner.getX() + 4);
    controlsInner.setWidth(controlsInner.getWidth() - 4);

    // Search bar
    {
        auto searchArea = controlsInner.removeFromTop(searchBarHeight).reduced(0, 2);
        clearButton->setBounds(searchArea.removeFromRight(80).reduced(4, 0));
        searchBox->setBounds(searchArea);
    }
    controlsInner.removeFromTop(4);

    // Letter bar (A-Z)
    {
        auto letterArea = controlsInner.removeFromTop(letterBarHeight);
        int btnW = letterArea.getWidth() / letterButtons.size();
        for (int i = 0; i < letterButtons.size(); ++i)
            letterButtons[i]->setBounds(letterArea.removeFromLeft(btnW));
    }

    // Filter bar
    {
        auto filterArea = controlsInner.removeFromTop(filterBarHeight).reduced(0, 2);
        int bw = 84;
        filterAllBtn->setBounds(filterArea.removeFromLeft(bw).reduced(2, 0));
        filterSongBtn->setBounds(filterArea.removeFromLeft(bw).reduced(2, 0));
        filterArtistBtn->setBounds(filterArea.removeFromLeft(bw).reduced(2, 0));
        filterYearBtn->setBounds(filterArea.removeFromLeft(bw).reduced(2, 0));
        filterGenreBtn->setBounds(filterArea.removeFromLeft(bw).reduced(2, 0));
    }
    controlsInner.removeFromTop(2);

    // Column headers (resizable)
    {
        auto colArea = controlsInner.removeFromTop(columnHeaderHeight);
        columnHeaderBar.setBounds(colArea);
    }

    bounds.removeFromTop(controlsH + kCardGap);

    // ── Results list card (fills remaining space) ──
    cardRects_.push_back(bounds);

    auto listArea = bounds.reduced(kCardPad);
    listArea.setX(listArea.getX() + 4);
    listArea.setWidth(listArea.getWidth() - 4);

    listViewport.setBounds(listArea);

    int totalH = (int)resultRows.size() * resultRowHeight;
    listContent.setSize(listArea.getWidth(), juce::jmax(totalH, listArea.getHeight()));

    for (int i = 0; i < resultRows.size(); ++i)
        resultRows[i]->setBounds(0, i * resultRowHeight, listContent.getWidth(), resultRowHeight);

    // Scroll-to-top button (floating bottom-right, inside list card)
    scrollTopButton->setBounds(listArea.getRight() - 50, listArea.getBottom() - 50, 40, 40);
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
        row->columnFractions = &columnFractions_;
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
        row->columnFractions = &columnFractions_;
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
                       active ? accentColour : darkColour);
        btn->setColour(juce::TextButton::textColourOffId,
                       active ? juce::Colours::black : textColour);
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

//==============================================================================
// Column fractions – persist to UserPreferences
//==============================================================================
void SearchPage::loadColumnFractions()
{
    auto loaded = UserPreferences::getInstance().getSearchColumnFractions();
    if (loaded.size() != 7) return;

    float sum = 0.f;
    for (float v : loaded)
    {
        if (v < 0.02f || v > 0.9f) return;  // sanity check
        sum += v;
    }
    if (sum < 0.95f || sum > 1.05f) return;
    columnFractions_ = std::move(loaded);
}

void SearchPage::saveColumnFractions() const
{
    UserPreferences::getInstance().setSearchColumnFractions(columnFractions_);
}

//==============================================================================
// ColumnHeaderBar impl
//==============================================================================
SearchPage::ColumnHeaderBar::ColumnHeaderBar()
{
    dividerOverlay_.owner = this;
    addAndMakeVisible(dividerOverlay_);
}

void SearchPage::ColumnHeaderBar::resized()
{
    layoutColumns();
    // Overlay sits above the column buttons and only hit-tests near dividers.
    dividerOverlay_.setBounds(getLocalBounds());
    dividerOverlay_.toFront(false);
}

void SearchPage::ColumnHeaderBar::layoutColumns()
{
    if (fractions == nullptr || fractions->size() != 7) return;
    const int w = getWidth();
    const int h = getHeight();
    const auto& f = *fractions;

    int x = (int)(w * f[0]);  // art column has no header button
    for (int i = 0; i < 5; ++i)
    {
        int colW = (int)(w * f[i + 1]);
        if (cols[i]) cols[i]->setBounds(x, 0, colW, h);
        x += colW;
    }
}

void SearchPage::ColumnHeaderBar::paint(juce::Graphics& g)
{
    if (fractions == nullptr || fractions->size() != 7) return;
    const int w = getWidth();
    const auto& f = *fractions;

    // Vertical divider lines between columns (4 internal dividers)
    float xf = f[0] + f[1];
    g.setColour(juce::Colour(0xff585757).withAlpha(0.5f));
    for (int i = 0; i < 4; ++i)
    {
        int px = (int)(w * xf);
        g.drawVerticalLine(px, 4.f, (float)(getHeight() - 4));
        xf += f[i + 2];
    }
}

int SearchPage::ColumnHeaderBar::findDivider(int localX) const
{
    if (fractions == nullptr || fractions->size() != 7) return -1;
    const int w = getWidth();
    const auto& f = *fractions;
    constexpr int kGrab = 6;

    float xf = f[0] + f[1];
    for (int i = 0; i < 4; ++i)
    {
        int px = (int)(w * xf);
        if (std::abs(localX - px) <= kGrab) return i + 1; // leftColIdx in the 1..5 range
        xf += f[i + 2];
    }
    return -1;
}

void SearchPage::ColumnHeaderBar::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(findDivider(e.x) >= 0
                   ? juce::MouseCursor::LeftRightResizeCursor
                   : juce::MouseCursor::NormalCursor);
}

void SearchPage::ColumnHeaderBar::mouseDown(const juce::MouseEvent& e)
{
    draggingDivider_ = findDivider(e.x);
    if (draggingDivider_ >= 0)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
}

void SearchPage::ColumnHeaderBar::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingDivider_ < 0 || fractions == nullptr || fractions->size() != 7) return;
    const int w = getWidth();
    if (w <= 0) return;

    auto& f = *fractions;
    const int leftIdx  = draggingDivider_;        // 1..4
    const int rightIdx = draggingDivider_ + 1;    // 2..5

    // Current boundary pixel (before this col's right edge).
    float xfLeft = 0.f;
    for (int i = 0; i <= leftIdx; ++i) xfLeft += f[i];
    int curPx = (int)(w * xfLeft);

    int newPx = juce::jlimit(0, w, e.x);
    float dFrac = (float)(newPx - curPx) / (float)w;

    const float minFrac = 0.05f;
    float pair = f[leftIdx] + f[rightIdx];
    float newLeft  = juce::jlimit(minFrac, pair - minFrac, f[leftIdx]  + dFrac);
    float newRight = pair - newLeft;

    f[leftIdx]  = newLeft;
    f[rightIdx] = newRight;

    layoutColumns();
    repaint();
    if (onDragLive) onDragLive();
}

void SearchPage::ColumnHeaderBar::mouseUp(const juce::MouseEvent&)
{
    const bool wasDragging = (draggingDivider_ >= 0);
    draggingDivider_ = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    if (wasDragging && onDragCommitted) onDragCommitted();
}

//==============================================================================
// DividerOverlay – transparent layer over the column buttons that only grabs
// mouse events within a few pixels of a divider, forwarding them to the owner.
//==============================================================================
bool SearchPage::ColumnHeaderBar::DividerOverlay::hitTest(int x, int /*y*/)
{
    return owner != nullptr && owner->findDivider(x) >= 0;
}

void SearchPage::ColumnHeaderBar::DividerOverlay::mouseMove(const juce::MouseEvent& e)
{
    if (owner) owner->mouseMove(e.getEventRelativeTo(owner));
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
}

void SearchPage::ColumnHeaderBar::DividerOverlay::mouseDown(const juce::MouseEvent& e)
{
    if (owner) owner->mouseDown(e.getEventRelativeTo(owner));
}

void SearchPage::ColumnHeaderBar::DividerOverlay::mouseDrag(const juce::MouseEvent& e)
{
    if (owner) owner->mouseDrag(e.getEventRelativeTo(owner));
}

void SearchPage::ColumnHeaderBar::DividerOverlay::mouseUp(const juce::MouseEvent& e)
{
    if (owner) owner->mouseUp(e.getEventRelativeTo(owner));
}
