/*
  ==============================================================================

    NavBar.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Left navigation bar implementation

  ==============================================================================
*/

#include "NavBar.h"

//==============================================================================
// Simple SVG-style path helpers for menu icons
//==============================================================================
namespace NavIcons
{
    static juce::Path makeHome()
    {
        juce::Path p;
        p.addTriangle(2, 10, 10, 2, 18, 10);      // roof
        p.addRectangle(4, 10, 12, 9);              // house body
        p.addRectangle(7, 13, 6, 6);               // door
        return p;
    }

    static juce::Path makeSearch()
    {
        juce::Path p;
        p.addEllipse(3, 3, 11, 11);                // circle
        p.addLineSegment({ 12, 12, 18, 18 }, 2.0f); // handle
        return p;
    }

    static juce::Path makeLibrary()
    {
        juce::Path p;
        p.addRectangle(2, 2, 5, 16);               // book 1
        p.addRectangle(8, 4, 5, 14);               // book 2
        p.addRectangle(14, 2, 5, 16);              // book 3
        return p;
    }

    static juce::Path makeCharts()
    {
        juce::Path p;
        p.addRectangle(2, 10, 4, 8);               // bar 1
        p.addRectangle(8, 6, 4, 12);               // bar 2
        p.addRectangle(14, 2, 4, 16);              // bar 3
        return p;
    }

    static juce::Path makeMixer()
    {
        juce::Path p;
        p.addRectangle(3, 2, 2, 16);               // slider track 1
        p.addRectangle(9, 2, 2, 16);               // slider track 2
        p.addRectangle(15, 2, 2, 16);              // slider track 3
        p.addRectangle(1, 6, 6, 3);                // thumb 1
        p.addRectangle(7, 11, 6, 3);               // thumb 2
        p.addRectangle(13, 4, 6, 3);               // thumb 3
        return p;
    }

    static juce::Path makeSettings()
    {
        juce::Path p;
        p.addEllipse(6, 6, 8, 8);                  // center circle
        // Gear teeth (simplified)
        for (int i = 0; i < 8; ++i)
        {
            float angle = static_cast<float>(i) * juce::MathConstants<float>::twoPi / 8.0f;
            float cx = 10 + std::cos(angle) * 9;
            float cy = 10 + std::sin(angle) * 9;
            p.addEllipse(cx - 1.5f, cy - 1.5f, 3, 3);
        }
        return p;
    }

    static juce::Path makeTesting()
    {
        juce::Path p;
        // Flask shape
        p.startNewSubPath(7, 2);
        p.lineTo(13, 2);
        p.lineTo(13, 7);
        p.lineTo(17, 16);
        p.lineTo(3, 16);
        p.lineTo(7, 7);
        p.closeSubPath();
        return p;
    }

    static juce::Path makeAds()
    {
        juce::Path p;
        p.addRectangle(2, 3, 16, 12);              // screen
        p.addTriangle(7, 6, 7, 12, 14, 9);         // play button
        return p;
    }
}

//==============================================================================
// NavButton
//==============================================================================
NavBar::NavButton::NavButton(const juce::String& text, const juce::Path& icon)
    : label(text), iconPath(icon)
{
    setRepaintsOnMouseActivity(true);
}

void NavBar::NavButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const juce::Colour bg         = juce::Colour(0xff262626);
    const juce::Colour bgHover    = juce::Colour(0xff161616);
    const juce::Colour textNorm   = juce::Colour(0xffe0e0e0).withAlpha(0.4f);
    const juce::Colour textActive = juce::Colour(0xffffffff);

    // Background
    if (isActive || hovering)
        g.setColour(bgHover);
    else
        g.setColour(bg);
    g.fillRect(bounds);

    // Active indicator bar on left edge
    if (isActive)
    {
        g.setColour(juce::Colour(0xff30daff));
        g.fillRect(0, 0, 3, getHeight());
    }

    // Icon
    float iconSize = 18.0f;
    float iconX = 14.0f;
    float iconY = (bounds.getHeight() - iconSize) / 2.0f;
    juce::Path scaledIcon(iconPath);
    scaledIcon.scaleToFit(iconX, iconY, iconSize, iconSize, true);

    g.setColour(isActive ? textActive : (hovering ? textActive : textNorm));
    g.fillPath(scaledIcon);

    // Label text (only if wide enough)
    if (getWidth() > 55)
    {
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
        g.drawText(label, juce::Rectangle<float>(iconX + iconSize + 10, 0,
                   bounds.getWidth() - iconX - iconSize - 20, bounds.getHeight()),
                   juce::Justification::centredLeft, true);
    }
}

void NavBar::NavButton::mouseUp(const juce::MouseEvent& e)
{
    if (contains(e.getPosition()) && onClick)
        onClick();
}

//==============================================================================
// GenreListModel
//==============================================================================
void NavBar::GenreListModel::paintListBoxItem(int row, juce::Graphics& g,
                                               int w, int h, bool /*selected*/)
{
    if (row < 0 || row >= items.size()) return;

    bool isSelected = (row == selectedRow);
    if (isSelected)
        g.fillAll(juce::Colour(0xff161616));

    g.setColour(juce::Colour(0xffe0e0e0).withAlpha(isSelected ? 1.0f : 0.4f));
    g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    g.drawText(items[row], 16, 0, w - 20, h, juce::Justification::centredLeft, true);
}

void NavBar::GenreListModel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < items.size())
    {
        selectedRow = row;
        if (onItemClicked)
            onItemClicked(items[row]);
    }
}

//==============================================================================
// NavBar
//==============================================================================
NavBar::NavBar()
{
    buildMenuItems();

    // Create buttons from menu items
    for (auto& item : menuItems)
    {
        juce::Path iconPath;
        switch (item.page)
        {
            case NavPage::Home:            iconPath = NavIcons::makeHome();     break;
            case NavPage::Search:          iconPath = NavIcons::makeSearch();   break;
            case NavPage::Library:         iconPath = NavIcons::makeLibrary();  break;
            case NavPage::Charts:          iconPath = NavIcons::makeCharts();   break;
            case NavPage::Mixer:           iconPath = NavIcons::makeMixer();    break;
            case NavPage::Settings:        iconPath = NavIcons::makeSettings(); break;
            case NavPage::Testing:         iconPath = NavIcons::makeTesting();  break;
            case NavPage::Ads:             iconPath = NavIcons::makeAds();      break;
            case NavPage::Playlist:        break;
            case NavPage::VenueManagement: break;
        }

        auto* btn = buttons.add(new NavButton(item.label, iconPath));
        btn->onClick = [this, page = item.page]()
        {
            setActivePage(page);
            if (onPageSelected)
                onPageSelected(page);
        };
        addAndMakeVisible(btn);
    }

    // Set Home as active by default
    if (!buttons.isEmpty())
        buttons[0]->isActive = true;

    // Genre header
    genreHeader = std::make_unique<juce::Label>("genreHeader", LocalizationManager::getInstance().getText("nav.genres"));
    genreHeader->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)).boldened());
    genreHeader->setColour(juce::Label::textColourId, textColour);
    addChildComponent(genreHeader.get());

    // Genre list
    genreListBox = std::make_unique<juce::ListBox>("genres", &genreModel);
    genreListBox->setRowHeight(28);
    genreListBox->setColour(juce::ListBox::backgroundColourId, bgColour);
    genreListBox->setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    addChildComponent(genreListBox.get());

    genreModel.onItemClicked = [this](const juce::String& name)
    {
        if (onGenreSelected)
            onGenreSelected(name);
    };

    // Apply default role visibility
    setUserRole(currentRole);
}

//==============================================================================
void NavBar::buildMenuItems()
{
    auto& lm = LocalizationManager::getInstance();
    menuItems = {
        { NavPage::Home,     lm.getText("nav.home"),     {}, AccessRight::Home     },
        { NavPage::Search,   lm.getText("nav.search"),   {}, AccessRight::Search   },
        { NavPage::Library,  lm.getText("nav.library"),  {}, AccessRight::Library  },
        { NavPage::Charts,   lm.getText("nav.charts"),   {}, AccessRight::Charts   },
        { NavPage::Mixer,    lm.getText("nav.mixer"),    {}, AccessRight::Mixer    },
        { NavPage::Settings, lm.getText("nav.settings"), {}, AccessRight::Settings },
        { NavPage::Testing,  lm.getText("nav.testing"),  {}, AccessRight::Testing  },
        { NavPage::Ads,      lm.getText("nav.ads"),      {}, AccessRight::Ads      },
    };
}

//==============================================================================
void NavBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(bgColour);
    g.fillRect(bounds);

    // Resize handle highlight on right edge
    auto handleRect = bounds.removeFromRight(resizeHandleWidth);
    if (draggingResize)
        g.setColour(accentColour.withAlpha(0.6f));
    else
        g.setColour(separatorColour.withAlpha(0.3f));
    g.fillRect(handleRect);

    // Separator between menu items and genre section
    if (genreSectionVisible && !buttons.isEmpty())
    {
        int separatorY = 0;
        for (auto* btn : buttons)
            if (btn->isVisible())
                separatorY = btn->getBottom();

        g.setColour(separatorColour);
        g.drawHorizontalLine(separatorY + 4, 8.0f, (float)(getWidth() - resizeHandleWidth - 8));
    }
}

//==============================================================================
void NavBar::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromRight(resizeHandleWidth); // Reserve resize handle

    int itemHeight = getHeight() > 600 ? 42 : 34;  // Responsive item height

    // Layout visible menu buttons
    auto menuArea = bounds;
    for (int i = 0; i < buttons.size(); ++i)
    {
        auto* btn = buttons[i];
        if (i < (int)menuItems.size() && menuItems[(size_t)i].visible)
        {
            btn->setVisible(true);
            btn->setBounds(menuArea.removeFromTop(itemHeight));
        }
        else
        {
            btn->setVisible(false);
        }
    }

    // Genre section fills remaining space
    if (genreSectionVisible)
    {
        menuArea.removeFromTop(10); // spacing

        genreHeader->setVisible(true);
        genreHeader->setBounds(menuArea.removeFromTop(24).withTrimmedLeft(12));

        genreListBox->setVisible(true);
        genreListBox->setBounds(menuArea);
    }
    else
    {
        genreHeader->setVisible(false);
        genreListBox->setVisible(false);
    }
}

//==============================================================================
void NavBar::setActivePage(NavPage page)
{
    activePage = page;

    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->isActive = (i < (int)menuItems.size() && menuItems[(size_t)i].page == page);

    repaint();
}

//==============================================================================
void NavBar::setUserRole(UserRole role)
{
    currentRole = role;
    auto rights = AccessRightsUtil::getRightsForRole(role);

    for (auto& item : menuItems)
    {
        item.visible = false;
        for (auto& r : rights)
        {
            if (r == item.requiredRight)
            {
                item.visible = true;
                break;
            }
        }
    }

    // Playlist/genre section visible if user has Playlist right
    genreSectionVisible = AccessRightsUtil::hasAccess(role, AccessRight::Playlist);

    resized();
    repaint();
}

//==============================================================================
void NavBar::setGenreList(const juce::StringArray& genres)
{
    genreModel.items = genres;
    genreModel.selectedRow = -1;
    if (genreListBox)
        genreListBox->updateContent();
}

//==============================================================================
void NavBar::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();

    // Rebuild menu item labels
    static const juce::String navKeys[] = {
        "nav.home", "nav.search", "nav.library", "nav.charts",
        "nav.mixer", "nav.settings", "nav.testing", "nav.ads"
    };

    for (size_t i = 0; i < menuItems.size() && i < 8; ++i)
        menuItems[i].label = lm.getText(navKeys[i]);

    // Genre header
    if (genreHeader)
        genreHeader->setText(lm.getText("nav.genres"), juce::dontSendNotification);

    repaint();
}

//==============================================================================
void NavBar::setBarWidth(int w)
{
    barWidth = juce::jlimit(minWidth, maxWidth, w);
}

//==============================================================================
// Resize drag handle
//==============================================================================
bool NavBar::isOverResizeHandle(const juce::Point<int>& pos) const
{
    return pos.getX() >= getWidth() - resizeHandleWidth;
}

void NavBar::mouseDown(const juce::MouseEvent& e)
{
    if (isOverResizeHandle(e.getPosition()))
    {
        draggingResize = true;
        dragStartX = e.getScreenX();
        dragStartWidth = barWidth;
        repaint();
    }
}

void NavBar::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingResize)
    {
        int delta = e.getScreenX() - dragStartX;
        int newWidth = juce::jlimit(minWidth, maxWidth, dragStartWidth + delta);

        if (newWidth != barWidth)
        {
            barWidth = newWidth;
            if (onWidthChanged)
                onWidthChanged(barWidth);
        }
    }
}

void NavBar::mouseMove(const juce::MouseEvent& e)
{
    if (isOverResizeHandle(e.getPosition()))
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}
