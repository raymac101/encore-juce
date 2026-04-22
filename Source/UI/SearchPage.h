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
    int          itemsPerPage = 200;

    //==============================================================================
    // Colours
    juce::Colour bgColour          { 0xff16213e };
    juce::Colour textColour        { 0xffe4e4e4 };
    juce::Colour accentColour      { 0xff30daff };
    juce::Colour darkColour        { 0xff222428 };
    juce::Colour hoverBgColour     { 0xff292929 };
    juce::Colour headerLineColour  { 0xff585757 };
    juce::Colour artistTextColour  { 0xffa3a6a8 };

    //==============================================================================
    // Layout constants
    static constexpr int titleBarHeight     = 64;
    static constexpr int searchBarHeight    = 40;
    static constexpr int letterBarHeight    = 32;
    static constexpr int filterBarHeight    = 36;
    static constexpr int columnHeaderHeight = 28;
    static constexpr int resultRowHeight    = 48;

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
