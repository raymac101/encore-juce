/*
  ==============================================================================

    Main.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Encore Karaoke - Professional Karaoke Application
    Main application entry point for JUCE version

  ==============================================================================
*/

#include <JuceHeader.h>
#include "UI/MainComponent.h"
#include "UI/LyricDisplayWindow.h"
#include "UI/LoginWindow.h"
#include "Localization/LocalizationManager.h"
#include "Services/UserPreferences.h"
#include "Services/VenueService.h"

//==============================================================================
class EncoreApplication : public juce::JUCEApplication,
                          public juce::MenuBarModel
{
public:
    //==============================================================================
    EncoreApplication() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    //==============================================================================
    void initialise (const juce::String& commandLine) override
    {
        // Initialize localization: honour the user's saved choice if any,
        // otherwise fall back to the system language.
        auto savedLang = UserPreferences::getInstance().getLanguage();
        if (savedLang.isNotEmpty())
            LocalizationManager::getInstance().setLanguage (savedLang);
        else
            LocalizationManager::getInstance().detectSystemLanguage();

        // Show the login + venue-selection flow first. The main app window
        // is constructed only after the user is signed in and a venue (or
        // an admin "create venue" decision) has been resolved.
        showLoginWindow();
    }

    void shutdown() override
    {
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (nullptr);
       #else
        if (mainWindow != nullptr)
            if (auto* content = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
                content->installMenuBarModel (nullptr);
       #endif

        // Clear the main window
        mainWindow = nullptr;
        loginWindow_ = nullptr;

        // Note: LocalizationManager will be cleaned up automatically as static instance
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // Handle quit request gracefully
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        // When another instance is launched, bring this one to the front
        if (mainWindow != nullptr)
            mainWindow->toFront (true);
    }

    //==============================================================================
    // MenuBarModel
    //==============================================================================
    enum MenuCommand
    {
        cmdFullscreen          = 0x3001,
        cmdResetScreenPosition = 0x3002,
        cmdShowTitleBar        = 0x3003,

        // Dynamic language items use IDs starting at this base.
        cmdLanguageBase        = 0x3100,
    };

    juce::StringArray getMenuBarNames() override
    {
        auto& lm = LocalizationManager::getInstance();
        return { lm.getText ("menu.window"), lm.getText ("menu.local") };
    }

    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex,
                                     const juce::String& /*menuName*/) override
    {
        auto& lm = LocalizationManager::getInstance();
        juce::PopupMenu menu;

        if (topLevelMenuIndex == 0)
        {
            const bool isFull        = mainWindow != nullptr && mainWindow->isFullScreen();
            const bool showTitleBar  = UserPreferences::getInstance().getShowTitleBar();

            menu.addItem (cmdFullscreen,
                          lm.getText ("menu.window.fullscreen"),
                          /*isActive*/ mainWindow != nullptr,
                          /*isTicked*/ isFull);
            menu.addSeparator();
            menu.addItem (cmdResetScreenPosition,
                          lm.getText ("menu.window.reset_position"));
            menu.addSeparator();
            menu.addItem (cmdShowTitleBar,
                          lm.getText ("menu.window.show_title_bar"),
                          /*isActive*/ true,
                          /*isTicked*/ showTitleBar);
        }
        else if (topLevelMenuIndex == 1)
        {
            // Populate the Local menu with every supported language.
            // Cache the code list so menuItemSelected can look up by offset.
            languageCodes_.clear();
            const auto entries = lm.getAvailableLanguages();                   // "code - Display Name"
            const auto currentCode = lm.getCurrentLanguage();

            for (const auto& entry : entries)
            {
                const auto dashIdx = entry.indexOf (" - ");
                const auto code    = dashIdx > 0 ? entry.substring (0, dashIdx) : entry;
                const auto name    = dashIdx > 0 ? entry.substring (dashIdx + 3) : entry;

                const int id = cmdLanguageBase + languageCodes_.size();
                languageCodes_.add (code);

                menu.addItem (id, name, /*isActive*/ true, /*isTicked*/ code == currentCode);
            }
        }

        return menu;
    }

    void menuItemSelected (int menuItemID, int /*topLevelMenuIndex*/) override
    {
        if (menuItemID >= cmdLanguageBase
            && menuItemID < cmdLanguageBase + languageCodes_.size())
        {
            const auto code = languageCodes_[menuItemID - cmdLanguageBase];
            LocalizationManager::getInstance().setLanguage (code);
            UserPreferences::getInstance().setLanguage (code);
            menuItemsChanged();
            return;
        }

        switch (menuItemID)
        {
            case cmdFullscreen:          toggleMainFullscreen();  break;
            case cmdResetScreenPosition: resetScreenPositions();  break;
            case cmdShowTitleBar:        toggleTitleBars();       break;
            default: break;
        }
    }

private:
    //==============================================================================
    void toggleMainFullscreen()
    {
        if (mainWindow != nullptr)
            mainWindow->setFullScreen (! mainWindow->isFullScreen());

        menuItemsChanged();
    }

    void resetScreenPositions()
    {
        // Move both windows to (0,0) with a reasonable default size. This is
        // the "panic button" for when a user gets a window off-screen after
        // disconnecting a monitor.
        if (mainWindow != nullptr)
        {
            if (mainWindow->isFullScreen())
                mainWindow->setFullScreen (false);
            mainWindow->setBounds (0, 0,
                                   juce::jmax (960,  mainWindow->getWidth()),
                                   juce::jmax (600,  mainWindow->getHeight()));
            UserPreferences::getInstance().setWindowBounds (mainWindow->getBounds());
        }

        if (mainWindow != nullptr)
        {
            if (auto* content = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
            {
                if (auto* lw = content->getLyricWindow())
                {
                    if (lw->isFullScreen()) lw->setFullScreen (false);
                    lw->setBounds (0, 0,
                                   juce::jmax (960, lw->getWidth()),
                                   juce::jmax (540, lw->getHeight()));
                    UserPreferences::getInstance().setLyricWindowBounds (lw->getBounds());
                    UserPreferences::getInstance().setLyricWindowFullScreen (false);
                }
            }
        }
    }

    void toggleTitleBars()
    {
        const bool newValue = ! UserPreferences::getInstance().getShowTitleBar();
        UserPreferences::getInstance().setShowTitleBar (newValue);

        if (mainWindow != nullptr)
            mainWindow->setUsingNativeTitleBar (newValue);

        if (mainWindow != nullptr)
        {
            if (auto* content = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
                if (auto* lw = content->getLyricWindow())
                    lw->setUsingNativeTitleBar (newValue);
        }

        menuItemsChanged();
    }

public:
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow : public juce::DocumentWindow,
                       public juce::ChangeListener
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                        .findColour (juce::ResizableWindow::backgroundColourId),
                            DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (UserPreferences::getInstance().getShowTitleBar());
            setContentOwned (new MainComponent(), true);

            // Rebuild the content whenever the language changes so that every
            // label, button, tab etc. picks up the new translations.
            LocalizationManager::getInstance().addChangeListener (this);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            // Restore saved window bounds, or use a sensible default (1920x1080 centred).
            setResizable (true, true);
            auto saved = UserPreferences::getInstance().getWindowBounds();
            if (saved.getWidth() > 0 && saved.getHeight() > 0)
            {
                if (saved.getX() > 0 || saved.getY() > 0)
                {
                    // Clamp to the current display so we don't end up off-screen
                    // if the user disconnected a monitor since last run.
                    auto displayArea = juce::Desktop::getInstance().getDisplays()
                                           .getTotalBounds(true);
                    auto target = saved;
                    if (! displayArea.intersects(target))
                        target.setPosition(displayArea.getCentreX() - target.getWidth()  / 2,
                                           displayArea.getCentreY() - target.getHeight() / 2);
                    setBounds(target);
                }
                else
                {
                    centreWithSize(saved.getWidth(), saved.getHeight());
                }
            }
            else
            {
                centreWithSize(1920, 1080);
            }
           #endif

            setVisible (true);
        }

        ~MainWindow() override
        {
            LocalizationManager::getInstance().removeChangeListener (this);
        }

        void closeButtonPressed() override
        {
            // This is called when the user tries to close this window
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void resized() override
        {
            juce::DocumentWindow::resized();
            scheduleBoundsSave();
        }

        void moved() override
        {
            juce::DocumentWindow::moved();
            scheduleBoundsSave();
        }

        /** Called when the UI language changes. Rebuild the content component so
            every child reruns its text-setup code against the new translations. */
        void changeListenerCallback (juce::ChangeBroadcaster* source) override
        {
            if (source != &LocalizationManager::getInstance())
                return;

            const auto previousBounds = getBounds();
            const bool wasFullScreen  = isFullScreen();

            setContentOwned (new MainComponent(), true);

            // Re-apply the active venue so the queue/lyric UI doesn't go blank
            // after the rebuild.
            if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
                content->setVenueId (VenueService::getInstance().getCurrentVenueId());

            setBounds (previousBounds);
            if (wasFullScreen)
                setFullScreen (true);

           #if ! JUCE_MAC
            // Re-attach the menu bar model to the fresh content component.
            if (auto* app = dynamic_cast<EncoreApplication*> (juce::JUCEApplication::getInstance()))
                if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
                    content->installMenuBarModel (app);
           #else
            // Ask the system menu bar to repaint with translated strings.
            if (auto* app = dynamic_cast<EncoreApplication*> (juce::JUCEApplication::getInstance()))
                app->menuItemsChanged();
           #endif
        }

    private:
        // Debounce bounds saves so dragging doesn't hammer the disk.
        void scheduleBoundsSave()
        {
            if (boundsSaveTimer_ == nullptr)
            {
                boundsSaveTimer_ = std::make_unique<BoundsSaveTimer>(*this);
            }
            boundsSaveTimer_->startTimer(400);
        }

        void writeBoundsNow()
        {
            if (isFullScreen() || isMinimised()) return;
            UserPreferences::getInstance().setWindowBounds(getBounds());
        }

        class BoundsSaveTimer : public juce::Timer
        {
        public:
            explicit BoundsSaveTimer(MainWindow& o) : owner(o) {}
            void timerCallback() override
            {
                stopTimer();
                owner.writeBoundsNow();
            }
            MainWindow& owner;
        };
        std::unique_ptr<BoundsSaveTimer> boundsSaveTimer_;

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    //==============================================================================
    void showLoginWindow()
    {
        loginWindow_.reset (new LoginWindow ([this](juce::String venueId)
        {
            // Defer so the login window can finish closing on the message thread.
            juce::MessageManager::callAsync ([this, venueId]
            {
                loginWindow_ = nullptr;
                createMainWindow (venueId);
            });
        }));
    }

    void createMainWindow (const juce::String& venueId)
    {
        if (mainWindow != nullptr)
            return;

        mainWindow.reset (new MainWindow (getApplicationName()));

        if (auto* content = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
            content->setVenueId (venueId);

       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (this);
       #else
        if (auto* content = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
            content->installMenuBarModel (this);
       #endif
    }

    std::unique_ptr<LoginWindow> loginWindow_;
    std::unique_ptr<MainWindow>  mainWindow;

    // Language codes for the dynamic Local menu, index matches (menuID - cmdLanguageBase).
    juce::StringArray languageCodes_;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (EncoreApplication)