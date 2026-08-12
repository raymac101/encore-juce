/*
  ==============================================================================

    NavBar.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Left navigation bar with resizable width and menu items in top half.

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
    CompanyAdmin,
    CustomerAdmin,

    // Not a sidebar item -- reachable only via the TopBar user-menu dropdown
    // ("Edit Profile"), matching the old Angular app where profile editing
    // was a menu-triggered component swap, not a persistent nav entry. See
    // MainArea::pages / MainComponent's TopBar dropdown wiring.
    Profile
};

//==============================================================================
/**
    Left-hand navigation bar.
    - Top section: icon + label menu items, visibility governed by UserRole/AccessRights
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
    void mouseUp(const juce::MouseEvent& e) override;

    //==============================================================================
    /** Set which page is currently active (highlights the item). */
    void setActivePage(NavPage page);
    NavPage getActivePage() const { return activePage; }

    /** Update menu visibility based on a user role. Note: this role can be
        venue-scoped (see MainComponent::applyNavRoleForActiveVenue, which
        resolves a per-venue role from user-venue-lookup and calls this) --
        it is NOT necessarily the same as the account's global hosts.role.
        enterpriseAdminOnly items are deliberately NOT gated here; see
        setGlobalHostRole(). */
    void setUserRole(UserRole role);

    /** Gates enterpriseAdminOnly nav items (e.g. Customer Admin), based on
        the account's global hosts.role -- independent of setUserRole()'s
        venue-scoped role, so a venue association can never mask a genuine
        platform-level EnterpriseAdmin designation. Call whenever the
        signed-in host's own role is (re)loaded, regardless of venue
        context. */
    void setGlobalHostRole(UserRole role);

    /** Enable or disable company-mode pages. */
    void setCompanyContext(bool enabled, const juce::String& role = {});

    /** Re-read all translatable strings from LocalizationManager. */
    void updateAllText();

    /** Current bar width (persisted across resize). */
    int getBarWidth() const { return barWidth; }
    void setBarWidth(int w);

    //==============================================================================
    // Callbacks
    std::function<void(NavPage)>        onPageSelected;
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
        bool        companyOnly = false;

        // Unlike companyOnly (which also shows for UserRole::Tester, since
        // Tester and EnterpriseAdmin share the same AccessRights rights-set
        // -- see AccessRightsUtil::getRightsForRole), this bypasses the
        // rights table entirely and is visible for EnterpriseAdmin ONLY.
        // Used for the Customer Admin page, which can delete accounts and
        // reset passwords.
        bool        enterpriseAdminOnly = false;
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
        void setLabel(const juce::String& newLabel) { label = newLabel; repaint(); }

        bool isActive = false;
        bool hovering = false;
        std::function<void()> onClick;

    private:
        juce::String label;
        juce::Path   iconPath;
    };

    juce::OwnedArray<NavButton> buttons;

    //==============================================================================
    // State
    NavPage   activePage = NavPage::Home;
    UserRole  currentRole = UserRole::Host;
    UserRole  globalHostRole_ = UserRole::Host;
    bool      companyModeEnabled = false;
    bool      companyDashboardVisible = false;
    juce::String companyRole;
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
