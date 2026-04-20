/*
  ==============================================================================

    NavBar.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Left navigation bar with resizable width, menu items in top half,
    and genre/playlist list in bottom half.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/AccessRights.h"
#include "../Localization/LocalizationManager.h"
#include <vector>
#include <functional>

//==============================================================================
/**
    Identifiers for each page/screen the NavBar can navigate to.
*/
enum class NavPage
{
    Home = 0,
    Search,
    Library,
    Charts,
    Mixer,
    Settings,
    Testing,
    Ads,
    Playlist,
    VenueManagement
};

//==============================================================================
/**
    Left-hand navigation bar.
    - Top section: icon + label menu items, visibility governed by UserRole/AccessRights
    - Bottom section: scrollable genre/playlist list (visible when Playlist access is granted)
    - Right edge: drag handle for resizing width
*/
class NavBar : public juce::Component
{
public:
    NavBar();
    ~NavBar() override = default;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Mouse handling for resize drag handle
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    //==============================================================================
    /** Set which page is currently active (highlights the item). */
    void setActivePage(NavPage page);
    NavPage getActivePage() const { return activePage; }

    /** Update menu visibility based on a user role. */
    void setUserRole(UserRole role);

    /** Populate the genre/playlist list in the bottom half. */
    void setGenreList(const juce::StringArray& genres);

    /** Re-read all translatable strings from LocalizationManager. */
    void updateAllText();

    /** Current bar width (persisted across resize). */
    int getBarWidth() const { return barWidth; }
    void setBarWidth(int w);

    //==============================================================================
    // Callbacks
    std::function<void(NavPage)>        onPageSelected;
    std::function<void(const juce::String&)> onGenreSelected;
    std::function<void(int)>            onWidthChanged;  // Fired while dragging

private:
    //==============================================================================
    /** Internal description of a single menu item. */
    struct MenuItem
    {
        NavPage     page;
        juce::String label;
        juce::String iconPathData;   // SVG-style path data for the icon
        AccessRight requiredRight;
        bool        visible = true;
    };

    void buildMenuItems();
    void layoutMenuItems();
    bool isOverResizeHandle(const juce::Point<int>& pos) const;

    //==============================================================================
    // Menu item buttons
    std::vector<MenuItem> menuItems;

    class NavButton : public juce::Component
    {
    public:
        NavButton(const juce::String& text, const juce::Path& icon);
        void paint(juce::Graphics& g) override;
        void mouseUp(const juce::MouseEvent& e) override;
        void mouseEnter(const juce::MouseEvent&) override { hovering = true;  repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { hovering = false; repaint(); }

        bool isActive = false;
        bool hovering = false;
        std::function<void()> onClick;

    private:
        juce::String label;
        juce::Path   iconPath;
    };

    juce::OwnedArray<NavButton> buttons;

    //==============================================================================
    // Genre / Playlist list (bottom half)
    class GenreListModel : public juce::ListBoxModel
    {
    public:
        int getNumRows() override { return items.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;

        juce::StringArray items;
        int selectedRow = -1;
        std::function<void(const juce::String&)> onItemClicked;
    };

    std::unique_ptr<juce::Label>   genreHeader;
    std::unique_ptr<juce::ListBox> genreListBox;
    GenreListModel                 genreModel;
    bool                           genreSectionVisible = false;

    //==============================================================================
    // State
    NavPage   activePage = NavPage::Home;
    UserRole  currentRole = UserRole::Host;
    int       barWidth = 200;
    int       minWidth = 60;
    int       maxWidth = 400;
    int       resizeHandleWidth = 5;
    bool      draggingResize = false;
    int       dragStartX = 0;
    int       dragStartWidth = 0;

    //==============================================================================
    // Colours (matching the Angular dark theme)
    juce::Colour bgColour            { 0xff262626 };
    juce::Colour bgHoverColour       { 0xff161616 };
    juce::Colour textColour          { 0xffe0e0e0 };
    juce::Colour activeTextColour    { 0xffffffff };
    juce::Colour separatorColour     { 0xff424242 };
    juce::Colour accentColour        { 0xff30daff };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NavBar)
};
