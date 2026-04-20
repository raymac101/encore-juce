/*
  ==============================================================================

    MainArea.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

  ==============================================================================
*/

#include "MainArea.h"

//==============================================================================
MainArea::MainArea()
{
    auto& lm = LocalizationManager::getInstance();

    // Create real Home page
    {
        auto hp = std::make_unique<HomePage>();
        homePage = hp.get();
        addChildComponent(hp.get());
        pages[static_cast<int>(NavPage::Home)] = std::move(hp);
    }

    // Create real Search page
    {
        auto sp = std::make_unique<SearchPage>();
        searchPage = sp.get();
        addChildComponent(sp.get());
        pages[static_cast<int>(NavPage::Search)] = std::move(sp);
    }

    // Create placeholder pages for the other NavPages
    addPage(NavPage::Library,         lm.getText("page.library"));
    addPage(NavPage::Charts,          lm.getText("page.charts"));
    addPage(NavPage::Mixer,           lm.getText("page.mixer"));
    addPage(NavPage::Settings,        lm.getText("page.settings"));
    addPage(NavPage::Testing,         lm.getText("page.testing"));
    addPage(NavPage::Ads,             lm.getText("page.ads"));
    addPage(NavPage::Playlist,        lm.getText("page.playlist"));
    addPage(NavPage::VenueManagement, lm.getText("page.venue_management"));

    // Show Home by default
    setCurrentPage(NavPage::Home);
}

//==============================================================================
void MainArea::addPage(NavPage page, const juce::String& label)
{
    auto comp = std::make_unique<PlaceholderPage>(label);
    addChildComponent(comp.get());
    pages[static_cast<int>(page)] = std::move(comp);
}

//==============================================================================
void MainArea::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff16213e));
}

//==============================================================================
void MainArea::resized()
{
    auto bounds = getLocalBounds();
    for (auto& [key, comp] : pages)
        comp->setBounds(bounds);
}

//==============================================================================
void MainArea::setCurrentPage(NavPage page)
{
    currentPage = page;

    for (auto& [key, comp] : pages)
        comp->setVisible(key == static_cast<int>(page));
}

//==============================================================================
void MainArea::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();

    static const struct { NavPage page; const char* key; } pageKeys[] = {
        { NavPage::Home,            "page.home" },
        { NavPage::Search,          "page.search" },
        { NavPage::Library,         "page.library" },
        { NavPage::Charts,          "page.charts" },
        { NavPage::Mixer,           "page.mixer" },
        { NavPage::Settings,        "page.settings" },
        { NavPage::Testing,         "page.testing" },
        { NavPage::Ads,             "page.ads" },
        { NavPage::Playlist,        "page.playlist" },
        { NavPage::VenueManagement, "page.venue_management" },
    };

    for (auto& pk : pageKeys)
    {
        auto it = pages.find(static_cast<int>(pk.page));
        if (it != pages.end())
        {
            if (auto* placeholder = dynamic_cast<PlaceholderPage*>(it->second.get()))
                placeholder->setPageName(lm.getText(pk.key));
        }
    }

    // Update concrete pages
    if (homePage) homePage->updateAllText();
    if (searchPage) searchPage->updateAllText();
}
