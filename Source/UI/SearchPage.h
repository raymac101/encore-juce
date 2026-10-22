/*
  ==============================================================================

    SearchPage.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Advanced song search page.
    - Live text search (partial/multi-word: "don st bel" finds "Don't Stop Believin'")
    - A-Z letter quick-filter bar
    - Filter mode: All / Song / Artist / Year / Genre
    - Sortable columns: Song, Artist, Version, Year, Genre (asc/desc toggle)
    - Virtual-scrolled result list with artwork, song, artist, version, year, genre
    - Scroll-to-top button when scrolled down
    - Localized labels

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/CdgSong.h"
#include "../Localization/LocalizationManager.h"
#include "../Services/ImageCache.h"
#include <vector>
#include <functional>

//==============================================================================
class SearchPage : public juce::Component,
                   public juce::TextEditor::Listener,
                   private juce::Timer
{
public:
    SearchPage();
    ~SearchPage() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // Data
    void setSongs(const std::vector<CdgSong>& allSongs);
    void setSearchName(const juce::String& name);

    // Programmatically injects a query and runs it through the normal
    // search pipeline (same debounce/applySearch() path a keystroke would
    // take) -- used by HomePage, which owns the one visible search box now
    // that SearchPage is embedded underneath it instead of showing its own.
    void setQueryText(const juce::String& text);

    // Localization
    void updateAllText();

    //==============================================================================
    // Callbacks
    std::function<void(const CdgSong& song)>  onSongClicked;
    std::function<void(const CdgSong& song)>  onSongEditClicked;

    //==============================================================================
    // A single row in the results list. Public (not private, despite being an
    // implementation-detail row component) so QueueBar's drop target can
    // dynamic_cast<SongResultRow*>(sourceComponent) to distinguish a
    // dragged-in song from a SingerRow reorder drag -- see
    // QueueBar::ListContent::isInterestedInDragSource().
    class SongResultRow : public juce::Component
    {
    public:
        SongResultRow();
        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent&) override { hovering = true;  repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { hovering = false; repaint(); }
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

        CdgSong song;
        int     index = 0;
        bool    hovering = false;

        // Pointer to the owning SearchPage's column fractions (indices 0-6:
        // art, song, artist, version, year, genre, edit). nullptr falls back
        // to built-in defaults.
        const std::vector<float>* columnFractions = nullptr;

        std::function<void(int)> onClicked;
        std::function<void(int)> onEditClicked;

        // Rebinds this row to a different song (rows are reused across
        // searches instead of being destroyed and recreated per keystroke).
        void setSong(const CdgSong& s);

        // Triggers an async download of the song's imageUrl and repaints when ready.
        void setImageUrl(const juce::String& url);

    private:
        int getEditColumnStartX() const;
        juce::Image cachedImage;
    };

private:
    //==============================================================================
    // Filter mode enum
    enum class FilterMode { All, Song, Artist, Year, Genre };

    // Sort column enum
    enum class SortColumn { None, Song, Artist, Version, Year, Genre };
    enum class SortDir    { Asc, Desc };

    //==============================================================================
    // UI sub-components
    std::unique_ptr<juce::TextEditor>     searchBox;
    std::unique_ptr<juce::Label>          titleLabel;
    std::unique_ptr<juce::Label>          countLabel;
    std::unique_ptr<juce::TextButton>     clearButton;
    std::unique_ptr<juce::TextButton>     scrollTopButton;

    // Filter buttons
    std::unique_ptr<juce::TextButton> filterAllBtn;
    std::unique_ptr<juce::TextButton> filterSongBtn;
    std::unique_ptr<juce::TextButton> filterArtistBtn;
    std::unique_ptr<juce::TextButton> filterYearBtn;
    std::unique_ptr<juce::TextButton> filterGenreBtn;

    // Independent on/off toggle (not part of the mutually-exclusive
    // FilterMode group above) -- only show songs with complete metadata.
    std::unique_ptr<juce::TextButton> filterHasMetaBtn;

    // A-Z letter buttons
    juce::OwnedArray<juce::TextButton> letterButtons;
    juce::String currentLetter;

    // Flat table-header cell: small left-aligned label (lined up with the
    // row text under it), accent + arrow when it's the sorted column.
    class HeaderCell : public juce::Button
    {
    public:
        explicit HeaderCell(const juce::String& name) : juce::Button(name) {}
        int sortState = 0;   // 0 unsorted, 1 ascending, -1 descending
        void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
    };

    // Column header buttons
    std::unique_ptr<HeaderCell> colSongBtn;
    std::unique_ptr<HeaderCell> colArtistBtn;
    std::unique_ptr<HeaderCell> colVersionBtn;
    std::unique_ptr<HeaderCell> colYearBtn;
    std::unique_ptr<HeaderCell> colGenreBtn;

    // Header bar that owns the column buttons and supports drag-to-resize.
    class ColumnHeaderBar : public juce::Component
    {
    public:
        ColumnHeaderBar();

        std::vector<float>* fractions = nullptr;     // owned by SearchPage, size 7
        std::function<void()> onDragLive;            // fired during drag (relayout only)
        std::function<void()> onDragCommitted;       // fired on mouseUp (persist)
        juce::Button* cols[5] = { nullptr };         // song/artist/version/year/genre

        void layoutColumns();
        void resized() override;
        void paint(juce::Graphics& g) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp  (const juce::MouseEvent& e) override;

        int findDivider(int localX) const;  // returns leftColIdx (1..4) or -1

    private:
        // Transparent overlay sitting above the column buttons. Only hit-tests
        // true within a few pixels of a divider so clicks on column labels still
        // reach the buttons, while drags on dividers reach the header bar.
        class DividerOverlay : public juce::Component
        {
        public:
            ColumnHeaderBar* owner = nullptr;
            DividerOverlay() { setInterceptsMouseClicks(true, false); }
            bool hitTest(int x, int y) override;
            void mouseMove(const juce::MouseEvent& e) override;
            void mouseDown(const juce::MouseEvent& e) override;
            void mouseDrag(const juce::MouseEvent& e) override;
            void mouseUp  (const juce::MouseEvent& e) override;
        };

        DividerOverlay dividerOverlay_;
        int draggingDivider_ = -1;
    };
    ColumnHeaderBar columnHeaderBar;

    // Results list — custom viewport to detect scroll
    class ScrollViewport : public juce::Viewport
    {
    public:
        std::function<void()> onScrolled;
        void visibleAreaChanged(const juce::Rectangle<int>&) override
        {
            if (onScrolled) onScrolled();
        }
    };

    ScrollViewport                          listViewport;
    juce::Component                     listContent;
    juce::OwnedArray<SongResultRow>     resultRows;

    //==============================================================================
    // State
    std::vector<CdgSong> allSongs;

    // In-memory search index, built once in setSongs() and parallel to
    // allSongs. Holds pre-normalised (lowercase, alnum + space, UTF-8)
    // haystacks and pre-lowered sort keys so a keystroke never has to touch
    // the full CdgSong records or re-normalise any field.
    struct SearchKey
    {
        std::string song, artist, year, genre, version;          // normalised for matching
        std::string sortSong, sortArtist, sortYear,
                    sortVersion, sortGenre;                      // lowercased for sorting
        juce::juce_wchar firstSong = 0, firstArtist = 0;         // uppercased, for A-Z bar
        bool hasMeta = false;
    };
    std::vector<SearchKey> searchIndex_;

    // Indices into allSongs / searchIndex_ for the current result set.
    std::vector<int> filtered_;

    // Last applied normalised query. When the next query extends it (the
    // usual typing case) we only re-filter the previous results.
    std::string lastQuery_;
    bool        lastQueryValid_ = false;
    static constexpr int kSearchDebounceMs = 90;

    juce::String searchName = "Search";
    FilterMode   filterMode = FilterMode::All;
    bool         onlyWithMetadata_ = false;
    SortColumn   sortColumn = SortColumn::None;
    SortDir      sortDir    = SortDir::Asc;
    int          loadedCount = 0;
    static constexpr int kInitialBatch = 100;
    static constexpr int kScrollBatch  = 50;

    // Column fractions (sum ~1.0): art, song, artist, version, year, genre, edit.
    // Loaded from / saved to UserPreferences when the user drags a divider.
    std::vector<float> columnFractions_ {0.06f, 0.23f, 0.23f, 0.16f, 0.08f, 0.16f, 0.08f};
    void loadColumnFractions();
    void saveColumnFractions() const;

    //==============================================================================
    // Colours
    juce::Colour bgColour          { 0xff16213e };
    juce::Colour textColour        { 0xffe4e4e4 };
    juce::Colour accentColour      { 0xff5a8fd8 };
    juce::Colour accentSoftColour  { 0xff8cb6f0 };
    juce::Colour darkColour        { 0x1affffff };
    juce::Colour hoverBgColour     { 0x14ffffff };
    juce::Colour headerLineColour  { 0x55b8caee };
    juce::Colour artistTextColour  { 0xffa3a6a8 };
    juce::Colour cardFillColour    { 0xff1a2030 };
    juce::Colour cardBorderColour  { 0xff2d3a5a };

    //==============================================================================
    // Layout constants
    static constexpr int titleBarHeight     = 64;
    static constexpr int searchBarHeight    = 44;
    static constexpr int letterBarHeight    = 32;
    static constexpr int filterBarHeight    = 36;
    static constexpr int columnHeaderHeight = 30;
    // Left inset of each text column, shared by result rows and header
    // cells so labels line up with the text below them.
    static constexpr int kColumnTextPad     = 8;
    static constexpr int resultRowHeight    = 48;
    static constexpr int kCardPad           = 12;
    static constexpr int kCardRadius        = 10;
    static constexpr int kCardGap           = 12;

    // Card rectangles computed in resized() and drawn in paint()
    std::vector<juce::Rectangle<int>> cardRects_;

    //==============================================================================
    // Internal methods
    void textEditorTextChanged(juce::TextEditor&) override;
    void timerCallback() override;

    void buildSearchIndex();
    void sortFiltered();
    SongResultRow* createRow(int index);
    void applySearch();
    void applyLetterFilter(const juce::String& letter);
    void clearSearch();
    void setFilterMode(FilterMode mode);
    void toggleMetadataFilter();
    void sortByColumn(SortColumn col);
    void rebuildDisplayList();
    void rebuildResultRows();
    void loadMoreRows();
    void updateCountLabel();

    // Partial multi-word search ("don st bel" matches "Don't Stop Believin'")
    static bool matchesPartialSearch(const std::vector<std::string>& words,
                                      const SearchKey& key,
                                      FilterMode mode);
    static std::string normalise(const juce::String& text);

    void updateFilterButtonColours();
    void updateColumnHeaderText();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SearchPage)
};
