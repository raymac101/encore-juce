/*
  ==============================================================================

    HomePage.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Home page implementation – horizontally-scrollable song card rows.

  ==============================================================================
*/

#include "HomePage.h"
#include "DesignTokens.h"

//==============================================================================
//  Small stroke-icon button shared by the home icon and the search-submit
//  icon -- same lightweight pattern NavBar/QueueBar already use for their
//  own hand-drawn icons, kept file-local since nothing else needs it.
//==============================================================================
namespace
{
    class IconButton : public juce::Button
    {
    public:
        IconButton(const juce::String& name, juce::Path path)
            : juce::Button(name), iconPath(std::move(path)) {}

        void paintButton(juce::Graphics& g, bool highlighted, bool down) override
        {
            auto bounds = getLocalBounds().toFloat();

            g.setColour(DesignTokens::colour(DesignTokens::kSurface2));
            g.fillRoundedRectangle(bounds.reduced(0.5f), 8.0f);

            auto iconColour = DesignTokens::colour((highlighted || down) ? DesignTokens::kAccent
                                                                          : DesignTokens::kText);
            g.setColour(iconColour.withAlpha(down ? 0.75f : 1.0f));

            auto iconBounds = bounds.reduced(bounds.getWidth() * 0.26f);
            juce::Path scaled(iconPath);
            scaled.scaleToFit(iconBounds.getX(), iconBounds.getY(),
                              iconBounds.getWidth(), iconBounds.getHeight(), true);
            g.strokePath(scaled, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

    private:
        juce::Path iconPath;
    };

    juce::Path makeHomeIconPath()
    {
        juce::Path p;
        p.startNewSubPath(2.0f, 11.0f);
        p.lineTo(12.0f, 3.0f);
        p.lineTo(22.0f, 11.0f);
        p.lineTo(19.0f, 11.0f);
        p.lineTo(19.0f, 21.0f);
        p.lineTo(5.0f, 21.0f);
        p.lineTo(5.0f, 11.0f);
        p.closeSubPath();
        return p;
    }

    juce::Path makeSearchIconPath()
    {
        juce::Path p;
        p.addEllipse(2.0f, 2.0f, 13.0f, 13.0f);
        p.startNewSubPath(13.5f, 13.5f);
        p.lineTo(21.0f, 21.0f);
        return p;
    }

    // JUCE draws the "text when empty" placeholder at the editor's indents
    // only, ignoring its border -- so with the search box's padding it sat
    // against the left edge and lower than typed text. This draws it inside
    // the same border-adjusted area the real text uses.
    class SearchTextEditor : public juce::TextEditor
    {
    public:
        using juce::TextEditor::TextEditor;

        void paintOverChildren(juce::Graphics& g) override
        {
            if (getTotalNumChars() == 0 && ! hasKeyboardFocus(false))
            {
                auto area = getBorder().subtractedFrom(getLocalBounds())
                                       .withTrimmedLeft(getLeftIndent());
                g.setColour(DesignTokens::colour(DesignTokens::kTextDim));
                g.setFont(getFont());
                g.drawText(placeholder, area, getJustificationType(), true);
            }

            getLookAndFeel().drawTextEditorOutline(g, getWidth(), getHeight(), *this);
        }

        void focusGained(FocusChangeType cause) override
        {
            juce::TextEditor::focusGained(cause);
            if (onFocusGained) onFocusGained();
        }

        void focusLost(FocusChangeType cause) override
        {
            juce::TextEditor::focusLost(cause);
            if (onBlur) onBlur();
        }

        juce::String placeholder;
        std::function<void()> onFocusGained;
        std::function<void()> onBlur;
    };

    // Stacked cards -- "browse everything" (same idea as Spotify's browse icon).
    juce::Path makeBrowseIconPath()
    {
        juce::Path p;
        p.addRoundedRectangle(3.0f, 9.0f, 18.0f, 12.0f, 2.0f);
        p.startNewSubPath(5.0f, 6.0f);
        p.lineTo(19.0f, 6.0f);
        p.startNewSubPath(7.5f, 3.0f);
        p.lineTo(16.5f, 3.0f);
        return p;
    }

    juce::File recentSearchesSettingsFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("EncoreKaraoke");
        dir.createDirectory();
        return dir.getChildFile("settings.xml");
    }

    juce::Path makeClearIconPath()
    {
        juce::Path p;
        p.startNewSubPath(5.0f, 5.0f);
        p.lineTo(19.0f, 19.0f);
        p.startNewSubPath(19.0f, 5.0f);
        p.lineTo(5.0f, 19.0f);
        return p;
    }
}

//==============================================================================
//  SongCard
//==============================================================================
SongCard::SongCard()
{
    // One neutral placeholder tone for every card -- previously a
    // per-song hashed HSV hue, which read as muddy/inconsistent saturation
    // from card to card with no real information encoded in the colour.
    placeholderColour = DesignTokens::colour(DesignTokens::kSurface2);
}

void SongCard::setTrack(const juce::String& artist, const juce::String& song,
                         const juce::String& imageUrl, bool isExplicit)
{
    artistText   = artist;
    songText     = song;
    imageUrlText = imageUrl;
    explicitFlag = isExplicit;

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
        g.setColour(DesignTokens::colour(DesignTokens::kAccent).withAlpha(0.6f));
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

    // Top search row -- replaces the standalone Search NavBar destination.
    // Typing a query and hitting Enter (or clicking the magnifying glass)
    // swaps the rows above for the embedded SearchPage below; the home
    // icon brings the rows back. See showBrowseView()/showSearchResults().
    homeIconButton_ = std::make_unique<IconButton>("homeIcon", makeHomeIconPath());
    homeIconButton_->onClick = [this]() { showBrowseView(); };
    addAndMakeVisible(*homeIconButton_);

    auto searchEditor = std::make_unique<SearchTextEditor>("homeSearchBox");
    searchEditor->placeholder = lm.getText("search.placeholder");
    searchEditor->onFocusGained = [this]() { updateRecentPanel(); };
    searchEditor->onBlur = [this]() { hideRecentPanel(); };
    searchBox_ = std::move(searchEditor);
    searchBox_->setMultiLine(false);
    searchBox_->setColour(juce::TextEditor::backgroundColourId, DesignTokens::colour(DesignTokens::kSurface2));
    searchBox_->setColour(juce::TextEditor::textColourId, DesignTokens::colour(DesignTokens::kText));
    searchBox_->setColour(juce::TextEditor::outlineColourId, DesignTokens::colour(DesignTokens::kBorder));
    searchBox_->setColour(juce::TextEditor::focusedOutlineColourId, DesignTokens::colour(DesignTokens::kAccent));
    // Vertically centred, larger text; the right border leaves room for the
    // clear (X) | browse buttons that sit inside the box (see resized()).
    // JUCE centres the full line height including descent, which reads as
    // sitting low; the extra bottom border nudges the text up to look
    // optically centred.
    searchBox_->setFont(juce::Font(juce::FontOptions().withHeight(20.0f)));
    searchBox_->setJustification(juce::Justification::centredLeft);
    searchBox_->setBorder(juce::BorderSize<int>(0, 14, 6, 90));
    // Live filtering: every keystroke swaps to the Search view and forwards
    // the current text straight into the embedded SearchPage (which runs it
    // through its own debounced applySearch() -- see setQueryText()), so
    // results update continuously instead of waiting for Enter. Clearing
    // the box back to empty falls back to the browse view automatically
    // (see showSearchResults()'s empty-query check).
    searchBox_->onTextChange = [this]()
    {
        clearSearchButton_->setVisible(searchBox_->getText().isNotEmpty());
        showSearchResults();
        updateRecentPanel();
    };
    searchBox_->onReturnKey = [this]()
    {
        recordRecentSearch(searchBox_->getText());
        hideRecentPanel();
        showSearchResults();
    };
    searchBox_->onEscapeKey = [this]() { hideRecentPanel(); };
    addAndMakeVisible(*searchBox_);

    // Clears the box but stays in the Search view (an empty query lists the
    // whole library); only shown while there's text to clear.
    clearSearchButton_ = std::make_unique<IconButton>("searchClear", makeClearIconPath());
    clearSearchButton_->onClick = [this]()
    {
        searchBox_->setText({}, true);
        searchBox_->grabKeyboardFocus();
    };
    addChildComponent(*clearSearchButton_);

    // Right of the X: the whole library, no query typed.
    browseAllButton_ = std::make_unique<IconButton>("searchBrowseAll", makeBrowseIconPath());
    browseAllButton_->setTooltip(lm.getText("search.browse_all"));
    browseAllButton_->onClick = [this]()
    {
        searchBox_->setText({}, false);
        clearSearchButton_->setVisible(false);
        hideRecentPanel();
        showSearchResults();
    };
    addAndMakeVisible(*browseAllButton_);

    searchSubmitButton_ = std::make_unique<IconButton>("searchSubmit", makeSearchIconPath());
    searchSubmitButton_->onClick = [this]() { showSearchResults(); };
    addAndMakeVisible(*searchSubmitButton_);

    embeddedSearch_ = std::make_unique<SearchPage>();
    embeddedSearch_->onSongClicked = [this](const CdgSong& song)
    {
        recordRecentSearch(searchBox_->getText());
        if (onSongClicked) onSongClicked(song);
    };
    embeddedSearch_->onSongEditClicked = [this](const CdgSong& song)
    {
        if (onSongEditClicked) onSongEditClicked(song);
    };
    addChildComponent(*embeddedSearch_);   // hidden until showSearchResults()

    loadRecentSearches();
    recentPanel_.onPick = [this](const juce::String& query)
    {
        recordRecentSearch(query);
        hideRecentPanel();
        searchBox_->setText(query, true);
        searchBox_->moveCaretToEnd();
    };
    recentPanel_.onClear = [this]()
    {
        recentSearches_.clear();
        saveRecentSearches();
        hideRecentPanel();
    };
    addChildComponent(recentPanel_);   // shown by updateRecentPanel()
}

//==============================================================================
void HomePage::loadRecentSearches()
{
    juce::PropertiesFile prefs(recentSearchesSettingsFile(), juce::PropertiesFile::Options());
    recentSearches_.addLines(prefs.getValue("recentSearches"));
    recentSearches_.removeEmptyStrings();
}

void HomePage::saveRecentSearches() const
{
    juce::PropertiesFile prefs(recentSearchesSettingsFile(), juce::PropertiesFile::Options());
    prefs.setValue("recentSearches", recentSearches_.joinIntoString("\n"));
    prefs.save();
}

void HomePage::recordRecentSearch(const juce::String& query)
{
    const auto q = query.trim();
    if (q.isEmpty())
        return;

    recentSearches_.removeString(q, true);   // de-dupe, case-insensitive
    recentSearches_.insert(0, q);
    while (recentSearches_.size() > kMaxRecentSearches)
        recentSearches_.remove(recentSearches_.size() - 1);
    saveRecentSearches();
}

void HomePage::updateRecentPanel()
{
    if (! searchBox_->hasKeyboardFocus(false))
    {
        hideRecentPanel();
        return;
    }

    // Empty box: the latest searches. While typing: the ones that match,
    // minus an exact match of what's already typed.
    const auto typed = searchBox_->getText().trim();
    juce::StringArray matches;
    for (const auto& s : recentSearches_)
    {
        if (matches.size() >= kMaxRecentShown)
            break;
        if (typed.isEmpty()
            || (s.containsIgnoreCase(typed) && ! s.equalsIgnoreCase(typed)))
            matches.add(s);
    }

    if (matches.isEmpty())
    {
        hideRecentPanel();
        return;
    }

    recentPanel_.setItems(matches);
    const auto box = searchBox_->getBounds();
    recentPanel_.setBounds(box.getX(), box.getBottom() + 4,
                           juce::jmin(box.getWidth(), 640),
                           recentPanel_.getPreferredHeight());
    recentPanel_.setVisible(true);
    recentPanel_.toFront(false);
}

void HomePage::hideRecentPanel()
{
    recentPanel_.setVisible(false);
}

//==============================================================================
RecentSearchesPanel::RecentSearchesPanel()
{
    // Clicking here must not steal focus from the search box -- its focus
    // loss is what hides this panel.
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    clearButton_.setWantsKeyboardFocus(false);
    clearButton_.setMouseClickGrabsKeyboardFocus(false);
    clearButton_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    clearButton_.setColour(juce::TextButton::textColourOffId, DesignTokens::colour(DesignTokens::kText));
    clearButton_.onClick = [this]() { if (onClear) onClear(); };
    addAndMakeVisible(clearButton_);
    updateAllText();
}

void RecentSearchesPanel::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    title_ = lm.getText("search.recent_title");
    clearButton_.setButtonText(lm.getText("search.recent_clear"));
    repaint();
}

void RecentSearchesPanel::setItems(const juce::StringArray& items)
{
    items_ = items;
    hoverRow_ = -1;
    resized();
    repaint();
}

int RecentSearchesPanel::getPreferredHeight() const
{
    return kHeaderHeight + items_.size() * kRowHeight + kFooterHeight;
}

int RecentSearchesPanel::rowAt(juce::Point<int> p) const
{
    const int row = (p.y - kHeaderHeight) / kRowHeight;
    return (p.y >= kHeaderHeight && row < items_.size()) ? row : -1;
}

void RecentSearchesPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(DesignTokens::colour(DesignTokens::kSurface));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(DesignTokens::colour(DesignTokens::kBorder));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    g.setColour(DesignTokens::colour(DesignTokens::kText));
    g.setFont(juce::Font(juce::FontOptions().withHeight(DesignTokens::kFontHeading)).boldened());
    g.drawText(title_, getLocalBounds().removeFromTop(kHeaderHeight).withTrimmedLeft(16),
               juce::Justification::centredLeft);

    const auto searchIcon = makeSearchIconPath();
    for (int i = 0; i < items_.size(); ++i)
    {
        auto row = juce::Rectangle<int>(0, kHeaderHeight + i * kRowHeight, getWidth(), kRowHeight);
        if (i == hoverRow_)
        {
            g.setColour(DesignTokens::colour(DesignTokens::kAccentSoft));
            g.fillRect(row.reduced(4, 2));
        }

        auto iconArea = row.removeFromLeft(48).toFloat().reduced(14.0f);
        juce::Path icon(searchIcon);
        icon.scaleToFit(iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true);
        g.setColour(DesignTokens::colour(DesignTokens::kTextDim));
        g.strokePath(icon, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        g.setColour(DesignTokens::colour(DesignTokens::kText));
        g.setFont(juce::Font(juce::FontOptions().withHeight(17.0f)));
        g.drawText(items_[i], row.withTrimmedRight(12), juce::Justification::centredLeft, true);
    }
}

void RecentSearchesPanel::resized()
{
    auto footer = getLocalBounds().removeFromBottom(kFooterHeight).reduced(16, 10);
    clearButton_.setBounds(footer.removeFromLeft(220));
}

void RecentSearchesPanel::mouseMove(const juce::MouseEvent& e)
{
    const int row = rowAt(e.getPosition());
    if (row != hoverRow_)
    {
        hoverRow_ = row;
        repaint();
    }
    setMouseCursor(row >= 0 ? juce::MouseCursor::PointingHandCursor
                            : juce::MouseCursor::NormalCursor);
}

void RecentSearchesPanel::mouseExit(const juce::MouseEvent&)
{
    hoverRow_ = -1;
    repaint();
}

void RecentSearchesPanel::mouseUp(const juce::MouseEvent& e)
{
    const int row = rowAt(e.getPosition());
    if (row >= 0 && onPick)
        onPick(items_[row]);
}

//==============================================================================
void HomePage::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void HomePage::paintOverChildren(juce::Graphics& g)
{
    // Divider between the X and browse buttons inside the search box.
    const auto browse = browseAllButton_->getBounds();
    const float x = (float) browse.getX() - 5.0f;
    g.setColour(DesignTokens::colour(DesignTokens::kBorder));
    g.drawLine(x, (float) browse.getY() + 4.0f, x, (float) browse.getBottom() - 4.0f, 1.0f);
}

//==============================================================================
void HomePage::resized()
{
    auto bounds = getLocalBounds().reduced(20, 10);

    constexpr int kSearchRowHeight = 40;
    constexpr int kGap = 10;
    auto searchRow = bounds.removeFromTop(kSearchRowHeight);
    homeIconButton_->setBounds(searchRow.removeFromLeft(kSearchRowHeight));
    searchRow.removeFromLeft(kGap);
    searchSubmitButton_->setBounds(searchRow.removeFromLeft(kSearchRowHeight));
    searchRow.removeFromLeft(kGap);
    searchBox_->setBounds(searchRow);
    // X | browse, inset so the box's own outline still shows around them;
    // the divider between them is drawn in paintOverChildren().
    browseAllButton_->setBounds(searchRow.removeFromRight(kSearchRowHeight).reduced(4));
    searchRow.removeFromRight(6);
    clearSearchButton_->setBounds(searchRow.removeFromRight(kSearchRowHeight).reduced(4));

    if (recentPanel_.isVisible())
        updateRecentPanel();

    bounds.removeFromTop(kGap);

    pageViewport.setBounds(bounds);
    embeddedSearch_->setBounds(bounds);

    layoutRows();
}

void HomePage::showBrowseView()
{
    showingSearch_ = false;
    searchBox_->clear();
    clearSearchButton_->setVisible(false);
    hideRecentPanel();
    pageViewport.setVisible(true);
    embeddedSearch_->setVisible(false);
}

void HomePage::showSearchResults()
{
    // Always switches to the Search view, even with an empty box -- clicking
    // the magnifying glass (or just starting to type) should take you there
    // regardless; an empty query is SearchPage's own concern (it shows
    // everything, same as clearing its own search box always did). The
    // home icon (showBrowseView()) is the only way back to the browse rows.
    embeddedSearch_->setQueryText(searchBox_->getText().trim());
    showingSearch_ = true;
    pageViewport.setVisible(false);
    embeddedSearch_->setVisible(true);
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
    embeddedSearch_->updateAllText();
    recentPanel_.updateAllText();
    browseAllButton_->setTooltip(lm.getText("search.browse_all"));
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
    embeddedSearch_->setSongs(songs);
}
