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
                   public juce::TextEditor::Listener
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

    // Localization
    void updateAllText();

    //==============================================================================
    // Callbacks
    std::function<void(const CdgSong& song)>  onSongClicked;
    std::function<void(const CdgSong& song)>  onSongEditClicked;

private:
    //==============================================================================
    // Filter mode enum
    enum class FilterMode { All, Song, Artist, Year, Genre };

    // Sort column enum
    enum class SortColumn { None, Song, Artist, Version, Year, Genre };
    enum class SortDir    { Asc, Desc };

    //==============================================================================
    // A single row in the results list
    class SongResultRow : public juce::Component
    {
    public:
        SongResultRow();
        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent&) override { hovering = true;  repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { hovering = false; repaint(); }
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

        // Triggers an async download of the song's imageUrl and repaints when ready.
        void setImageUrl(const juce::String& url);

    private:
        juce::Image cachedImage;
    };

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

    // A-Z letter buttons
    juce::OwnedArray<juce::TextButton> letterButtons;
    juce::String currentLetter;

    // Column header buttons
    std::unique_ptr<juce::TextButton> colSongBtn;
    std::unique_ptr<juce::TextButton> colArtistBtn;
    std::unique_ptr<juce::TextButton> colVersionBtn;
    std::unique_ptr<juce::TextButton> colYearBtn;
    std::unique_ptr<juce::TextButton> colGenreBtn;

    // Header bar that owns the column buttons and supports drag-to-resize.
    class ColumnHeaderBar : public juce::Component
    {
    public:
        ColumnHeaderBar();

        std::vector<float>* fractions = nullptr;     // owned by SearchPage, size 7
        std::function<void()> onDragLive;            // fired during drag (relayout only)
        std::function<void()> onDragCommitted;       // fired on mouseUp (persist)
        juce::TextButton* cols[5] = { nullptr };     // song/artist/version/year/genre

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
    std::vector<CdgSong> filteredSongs;
    std::vector<CdgSong> displaySongs;

    juce::String searchName = "Search";
    FilterMode   filterMode = FilterMode::All;
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
    juce::Colour accentColour      { 0xff30daff };
    juce::Colour accentSoftColour  { 0xff70e6ff };
    juce::Colour darkColour        { 0xff222428 };
    juce::Colour hoverBgColour     { 0xff292929 };
    juce::Colour headerLineColour  { 0xff585757 };
    juce::Colour artistTextColour  { 0xffa3a6a8 };
    juce::Colour cardFillColour    { 0xff1a2030 };
    juce::Colour cardBorderColour  { 0xff2d3a5a };

    //==============================================================================
    // Layout constants
    static constexpr int titleBarHeight     = 64;
    static constexpr int searchBarHeight    = 44;
    static constexpr int letterBarHeight    = 32;
    static constexpr int filterBarHeight    = 36;
    static constexpr int columnHeaderHeight = 28;
    static constexpr int resultRowHeight    = 48;
    static constexpr int kCardPad           = 12;
    static constexpr int kCardRadius        = 10;
    static constexpr int kCardGap           = 12;

    // Card rectangles computed in resized() and drawn in paint()
    std::vector<juce::Rectangle<int>> cardRects_;

    //==============================================================================
    // Internal methods
    void textEditorTextChanged(juce::TextEditor&) override;

    void applySearch();
    void applyLetterFilter(const juce::String& letter);
    void clearSearch();
    void setFilterMode(FilterMode mode);
    void sortByColumn(SortColumn col);
    void rebuildDisplayList();
    void rebuildResultRows();
    void loadMoreRows();
    void updateCountLabel();

    // Partial multi-word search ("don st bel" matches "Don't Stop Believin'")
    static bool matchesPartialSearch(const juce::String& searchText,
                                      const CdgSong& song,
                                      FilterMode mode);
    static juce::String normalise(const juce::String& text);

    void updateFilterButtonColours();
    void updateColumnHeaderText();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SearchPage)
};
