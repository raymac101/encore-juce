/*
  ==============================================================================

    SearchPage.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Implementation of the advanced song search page.

  ==============================================================================
*/

#include "SearchPage.h"
#include "MenuTheme.h"
#include "../Services/UserPreferences.h"
#include <algorithm>
#include <cctype>

//==============================================================================
//  SongResultRow
//==============================================================================
SearchPage::SongResultRow::SongResultRow() {}

void SearchPage::SongResultRow::setSong(const CdgSong& s)
{
    song = s;
    cachedImage = {};
    repaint();

    if (! song.imageUrl.empty())
        setImageUrl(juce::String::fromUTF8(song.imageUrl.data(), (int)song.imageUrl.size()));
}

void SearchPage::SongResultRow::setImageUrl(const juce::String& url)
{
    if (url.isEmpty()) return;

    // Single call: returns image if cached, queues callback if not.
    juce::Component::SafePointer<SongResultRow> safeThis(this);
    juce::Image img = ArtworkCache::getInstance().getOrFetch(url, [safeThis, url]()
    {
        if (safeThis == nullptr) return;
        // Rows are reused across searches -- ignore a late fetch for the
        // song this row used to show.
        if (safeThis->song.imageUrl != url.toStdString()) return;
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
        g.fillAll(juce::Colours::white.withAlpha(0.08f));

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

int SearchPage::SongResultRow::getEditColumnStartX() const
{
    // Edit column starts where genre column ends.
    static const std::vector<float> defaults {0.06f, 0.23f, 0.23f, 0.16f, 0.08f, 0.16f, 0.08f};
    const auto& f = (columnFractions && columnFractions->size() == 7) ? *columnFractions : defaults;
    return (int)(getWidth() * (f[0] + f[1] + f[2] + f[3] + f[4] + f[5]));
}

void SearchPage::SongResultRow::mouseDrag(const juce::MouseEvent& e)
{
    // Don't start a drag from the edit-pencil column -- that's a click zone.
    if (e.getMouseDownPosition().x >= getEditColumnStartX())
        return;

    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor(this))
    {
        if (! dnd->isDragAndDropActive())
        {
            // The whole song is serialized to JSON as the drag description
            // (CdgSong already has toJson()/fromJson()) so the drop target
            // (QueueBar::ListContent) doesn't need to know about SearchPage
            // or reach back into its result list -- see
            // QueueBar::ListContent::itemDropped().
            auto img = createComponentSnapshot(getLocalBounds(), true);
            dnd->startDragging(song.toJson(), this, juce::ScaledImage(img),
                               /*allowDraggingToOtherWindows*/ false);
        }
    }
}

void SearchPage::SongResultRow::mouseUp(const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown())
        return;

    if (e.x >= getEditColumnStartX())
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
    setOpaque(true);

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
    // HomePage now owns the one visible search box above this embedded
    // page (see setQueryText()); this one still exists and still drives
    // applySearch() via the listener above, it's just never shown/laid out.
    searchBox->setVisible(false);

    // Clear button
    clearButton = std::make_unique<juce::TextButton>(lm.getText("search.clear"));
    clearButton->setColour(juce::TextButton::buttonColourId, accentColour);
    clearButton->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    clearButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    clearButton->onClick = [this]() { clearSearch(); };
    addAndMakeVisible(*clearButton);
    clearButton->setVisible(false);

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

    // Independent toggle, not part of the FilterMode group above.
    filterHasMetaBtn = makeFilterBtn(lm.getText("search.filter_has_meta"));
    filterHasMetaBtn->setClickingTogglesState(true);
    filterHasMetaBtn->onClick = [this]() { toggleMetadataFilter(); };

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
    MenuTheme::drawPageBackground(g, getLocalBounds());

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

    // ── Controls card (letter bar + filter bar + column headers) ──
    // The search box + clear button used to be the first slice of this
    // card; SearchPage is now embedded under HomePage's own search bar
    // (see setQueryText()), so that slice -- and the two components
    // themselves (hidden in the constructor) -- are no longer laid out here.
    const int controlsH = letterBarHeight
                        + filterBarHeight
                        + columnHeaderHeight
                        + kCardPad * 2 + 8; // extra inner gaps between sub-rows
    juce::Rectangle<int> controlsCard(bounds.getX(), bounds.getY(),
                                      bounds.getWidth(), controlsH);
    cardRects_.push_back(controlsCard);

    auto controlsInner = controlsCard.reduced(kCardPad);
    // shift inner content right by a few px so it clears the left accent stripe
    controlsInner.setX(controlsInner.getX() + 4);
    controlsInner.setWidth(controlsInner.getWidth() - 4);

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
        // Independent toggle -- right-justified, separated from the
        // mutually-exclusive All/Song/Artist/Year/Genre group on the left.
        filterHasMetaBtn->setBounds(filterArea.removeFromRight(150).reduced(2, 0));

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
    // applyLetterFilter() clears the search box, and TextEditor delivers
    // that change asynchronously -- don't let it wipe the letter filter.
    if (searchBox->isEmpty() && currentLetter.isNotEmpty())
        return;

    // Debounce: fast typing coalesces into a single search.
    startTimer(kSearchDebounceMs);
}

void SearchPage::timerCallback()
{
    stopTimer();
    applySearch();
}

//==============================================================================
std::string SearchPage::normalise(const juce::String& text)
{
    // Strip non-alphanumeric, lowercase. Returned as UTF-8 so matching is a
    // plain byte-wise std::string::find (valid for UTF-8 substrings).
    std::string result;
    result.reserve(text.getNumBytesAsUTF8());
    for (auto p = text.getCharPointer(); ! p.isEmpty();)
    {
        auto ch = p.getAndAdvance();
        if (juce::CharacterFunctions::isLetterOrDigit(ch) || ch == ' ')
        {
            char buf[8] = {};
            juce::CharPointer_UTF8 out(buf);
            out.write(juce::CharacterFunctions::toLowerCase(ch));
            result += buf;
        }
    }
    return result;
}

void SearchPage::buildSearchIndex()
{
    auto str = [](const std::string& s) { return juce::String::fromUTF8(s.data(), (int)s.size()); };
    auto lower = [&](const std::string& s) { return str(s).toLowerCase().toStdString(); };
    auto firstUpper = [&](const std::string& s) {
        return juce::CharacterFunctions::toUpperCase(str(s).trimStart()[0]);
    };

    searchIndex_.clear();
    searchIndex_.reserve(allSongs.size());

    for (const auto& s : allSongs)
    {
        SearchKey k;
        k.song   = normalise(str(s.songName));
        k.artist = normalise(str(s.artistName));
        k.year   = normalise(str(s.releaseDate));
        for (const auto& g : s.genres)  k.genre   += normalise(str(g)) + " ";
        for (const auto& v : s.version) k.version += normalise(str(v)) + " ";

        k.sortSong    = lower(s.songName);
        k.sortArtist  = lower(s.artistName);
        k.sortYear    = lower(s.releaseDate);
        k.sortVersion = s.version.empty() ? std::string() : lower(s.version[0]);
        k.sortGenre   = s.genres.empty()  ? std::string() : lower(s.genres[0]);

        k.firstSong   = firstUpper(s.songName);
        k.firstArtist = firstUpper(s.artistName);
        k.hasMeta     = s.hasMetadata();

        searchIndex_.push_back(std::move(k));
    }
}

bool SearchPage::matchesPartialSearch(const std::vector<std::string>& words,
                                       const SearchKey& k,
                                       FilterMode mode)
{
    auto has = [](const std::string& field, const std::string& word) {
        return field.find(word) != std::string::npos;
    };

    // Every word must match at least one relevant field
    for (const auto& word : words)
    {
        bool found = false;

        switch (mode)
        {
            case FilterMode::Song:   found = has(k.song, word);   break;
            case FilterMode::Artist: found = has(k.artist, word); break;
            case FilterMode::Year:   found = has(k.year, word);   break;
            case FilterMode::Genre:  found = has(k.genre, word);  break;
            case FilterMode::All:
            default:
                found = has(k.song, word)
                     || has(k.artist, word)
                     || has(k.version, word)
                     || has(k.year, word)
                     || has(k.genre, word);
                break;
        }

        if (!found) return false;
    }
    return true;
}

//==============================================================================
void SearchPage::applySearch()
{
    stopTimer();

    const std::string query = normalise(searchBox->getText());

    // Split search into words
    std::vector<std::string> words;
    {
        size_t pos = 0;
        while (pos < query.size())
        {
            size_t end = query.find(' ', pos);
            if (end == std::string::npos) end = query.size();
            if (end > pos) words.push_back(query.substr(pos, end - pos));
            pos = end + 1;
        }
    }

    // If the new query extends the previous one, every match must already be
    // in the previous result set -- just narrow it (order, incl. any column
    // sort, is preserved). Otherwise scan the whole index.
    const bool narrow = lastQueryValid_ && currentLetter.isEmpty()
                     && query.compare(0, lastQuery_.size(), lastQuery_) == 0;
    currentLetter = "";

    std::vector<int> result;
    auto consider = [&](int i) {
        const auto& k = searchIndex_[(size_t)i];
        if ((! onlyWithMetadata_ || k.hasMeta) && matchesPartialSearch(words, k, filterMode))
            result.push_back(i);
    };

    if (narrow)
    {
        result.reserve(filtered_.size());
        for (int i : filtered_) consider(i);
    }
    else
    {
        result.reserve(searchIndex_.size());
        for (int i = 0; i < (int)searchIndex_.size(); ++i) consider(i);
    }

    filtered_ = std::move(result);
    if (! narrow)
        sortFiltered();

    lastQuery_ = query;
    lastQueryValid_ = true;

    rebuildDisplayList();
}

void SearchPage::applyLetterFilter(const juce::String& letter)
{
    lastQueryValid_ = false;
    stopTimer();
    filtered_.clear();

    if (currentLetter == letter)
    {
        // Toggle off – show all (still honoring the metadata toggle)
        currentLetter = "";
        for (int i = 0; i < (int)searchIndex_.size(); ++i)
            if (! onlyWithMetadata_ || searchIndex_[(size_t)i].hasMeta)
                filtered_.push_back(i);
    }
    else
    {
        currentLetter = letter;
        searchBox->clear();

        const juce::juce_wchar letterCh = letter[0];
        for (int i = 0; i < (int)searchIndex_.size(); ++i)
        {
            const auto& k = searchIndex_[(size_t)i];
            if (onlyWithMetadata_ && ! k.hasMeta)
                continue;

            const auto firstChar = (filterMode == FilterMode::Artist) ? k.firstArtist : k.firstSong;

            if (letter == "#")
            {
                // Numbers and symbols
                if (!juce::CharacterFunctions::isLetter(firstChar))
                    filtered_.push_back(i);
            }
            else if (firstChar == letterCh)
            {
                filtered_.push_back(i);
            }
        }
    }
    sortFiltered();

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
    lastQueryValid_ = false;

    for (auto* btn : letterButtons)
        btn->setColour(juce::TextButton::textColourOffId, textColour.withAlpha(0.6f));

    updateFilterButtonColours();
    updateColumnHeaderText();
    applySearch();
    searchBox->grabKeyboardFocus();
}

void SearchPage::setFilterMode(FilterMode mode)
{
    filterMode = mode;
    lastQueryValid_ = false;
    updateFilterButtonColours();
    applySearch();
}

void SearchPage::toggleMetadataFilter()
{
    onlyWithMetadata_ = filterHasMetaBtn->getToggleState();
    lastQueryValid_ = false;
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

    sortFiltered();
    updateColumnHeaderText();
    rebuildDisplayList();
}

void SearchPage::sortFiltered()
{
    if (sortColumn == SortColumn::None) return;

    auto keyFor = [this](int i) -> const std::string& {
        const auto& k = searchIndex_[(size_t)i];
        switch (sortColumn)
        {
            case SortColumn::Artist:  return k.sortArtist;
            case SortColumn::Version: return k.sortVersion;
            case SortColumn::Year:    return k.sortYear;
            case SortColumn::Genre:   return k.sortGenre;
            case SortColumn::Song:
            default:                  return k.sortSong;
        }
    };

    const bool asc = (sortDir == SortDir::Asc);
    std::stable_sort(filtered_.begin(), filtered_.end(), [&](int a, int b) {
        return asc ? (keyFor(a) < keyFor(b)) : (keyFor(a) > keyFor(b));
    });
}

//==============================================================================
void SearchPage::rebuildDisplayList()
{
    const int count = juce::jmin((int)filtered_.size(), kInitialBatch);
    loadedCount = count;

    rebuildResultRows();
    updateCountLabel();
    listViewport.setViewPosition(0, 0);
    resized();
}

SearchPage::SongResultRow* SearchPage::createRow(int index)
{
    auto* row = new SongResultRow();
    row->index = index;
    row->columnFractions = &columnFractions_;
    row->onClicked = [this](int idx) {
        if (idx >= 0 && idx < resultRows.size())
            if (onSongClicked) onSongClicked(resultRows[idx]->song);
    };
    row->onEditClicked = [this](int idx) {
        if (idx >= 0 && idx < resultRows.size())
            if (onSongEditClicked) onSongEditClicked(resultRows[idx]->song);
    };
    listContent.addAndMakeVisible(row);
    resultRows.add(row);
    return row;
}

void SearchPage::rebuildResultRows()
{
    // Reuse existing row components; only create / delete the difference.
    while (resultRows.size() > loadedCount)
    {
        listContent.removeChildComponent(resultRows.getLast());
        resultRows.removeLast();
    }

    for (int i = 0; i < loadedCount; ++i)
    {
        auto* row = (i < resultRows.size()) ? resultRows[i] : createRow(i);
        row->setSong(allSongs[(size_t)filtered_[(size_t)i]]);
    }
}

void SearchPage::loadMoreRows()
{
    if (loadedCount >= (int)filtered_.size()) return;

    int next = juce::jmin(loadedCount + kScrollBatch, (int)filtered_.size());

    for (int i = loadedCount; i < next; ++i)
        createRow(i)->setSong(allSongs[(size_t)filtered_[(size_t)i]]);

    loadedCount = next;
    resized();
}

void SearchPage::updateCountLabel()
{
    juce::String text = juce::String((int)filtered_.size()) + " " + LocalizationManager::getInstance().getText("search.count_suffix");
    countLabel->setText(text, juce::dontSendNotification);
}

//==============================================================================
void SearchPage::updateFilterButtonColours()
{
    auto setActive = [&](juce::TextButton* btn, bool active) {
        btn->setColour(juce::TextButton::buttonColourId,
                       active ? accentColour : darkColour);
        btn->setColour(juce::TextButton::textColourOffId,
                       textColour);
    };
    setActive(filterAllBtn.get(),    filterMode == FilterMode::All);
    setActive(filterSongBtn.get(),   filterMode == FilterMode::Song);
    setActive(filterArtistBtn.get(), filterMode == FilterMode::Artist);
    setActive(filterYearBtn.get(),   filterMode == FilterMode::Year);
    setActive(filterGenreBtn.get(),  filterMode == FilterMode::Genre);
    setActive(filterHasMetaBtn.get(), onlyWithMetadata_);
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
    buildSearchIndex();

    // Re-run whatever the user currently has on screen against the new data.
    lastQueryValid_ = false;
    if (currentLetter.isNotEmpty())
    {
        const auto letter = currentLetter;
        currentLetter = "";
        applyLetterFilter(letter);
    }
    else
    {
        applySearch();
    }
}

void SearchPage::setSearchName(const juce::String& name)
{
    searchName = name;
    titleLabel->setText(name, juce::dontSendNotification);
}

void SearchPage::setQueryText(const juce::String& text)
{
    // Goes through the exact same textEditorTextChanged() -> debounce ->
    // applySearch() pipeline a keystroke would, just driven by whoever
    // owns the visible search box now (HomePage) instead of this hidden one.
    // TextEditor::setText's second argument is a plain bool (fire the
    // listener notification), not a NotificationType.
    searchBox->setText(text, true);
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
    filterHasMetaBtn->setButtonText(lm.getText("search.filter_has_meta"));
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
