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
    class ShellLoadingComponent : public juce::Component,
                                  private juce::Timer
    {
    public:
        ShellLoadingComponent()
        {
            startTimerHz(30);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff16213e));

            auto area = getLocalBounds();
            auto centre = area.getCentre().toFloat();

            for (int i = 0; i < 12; ++i)
            {
                const float a = phase_ + juce::MathConstants<float>::twoPi * (float) i / 12.0f;
                const float inner = 12.0f;
                const float outer = 22.0f;
                const float alpha = 0.2f + 0.8f * ((float) i / 12.0f);
                g.setColour(juce::Colour(0xff30daff).withAlpha(alpha));
                juce::Point<float> p1(centre.x + std::cos(a) * inner,
                                      centre.y - 20.0f + std::sin(a) * inner);
                juce::Point<float> p2(centre.x + std::cos(a) * outer,
                                      centre.y - 20.0f + std::sin(a) * outer);
                g.drawLine({ p1, p2 }, 3.0f);
            }

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions().withHeight(22.0f)).boldened());
            g.drawText("Encore Karaoke", area.withTrimmedTop(area.getCentreY()), juce::Justification::centredTop);

            g.setColour(juce::Colour(0xffc8d4e8));
            g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
            g.drawText("Loading application...", area.withTrimmedTop(area.getCentreY() + 32), juce::Justification::centredTop);
        }

    private:
        void timerCallback() override
        {
            phase_ += 0.14f;
            if (phase_ > juce::MathConstants<float>::twoPi)
                phase_ -= juce::MathConstants<float>::twoPi;
            repaint();
        }

        float phase_ = 0.0f;
    };

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
            setContentOwned (new ShellLoadingComponent(), true);

            // Rebuild the content whenever the language changes so that every
            // label, button, tab etc. picks up the new translations.
            LocalizationManager::getInstance().addChangeListener (this);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            // Restore saved window bounds, or use a sensible default (1920x1080 centred).
            setResizable (true, true);
            
            // Get the primary display bounds to check against screen size
            auto displayArea = juce::Desktop::getInstance().getDisplays().getTotalBounds(true);
            auto saved = UserPreferences::getInstance().getWindowBounds();
            
            // Determine the target window size
            int targetWidth = 1920;
            int targetHeight = 1080;
            
            if (saved.getWidth() > 0 && saved.getHeight() > 0)
            {
                targetWidth = saved.getWidth();
                targetHeight = saved.getHeight();
            }
            
            // If window is larger than screen, resize it to fit with some padding
            if (targetWidth > displayArea.getWidth() || targetHeight > displayArea.getHeight())
            {
                const int padding = 40; // Leave some padding from screen edges
                targetWidth = juce::jmin(targetWidth, displayArea.getWidth() - padding);
                targetHeight = juce::jmin(targetHeight, displayArea.getHeight() - padding);
            }
            
            // Now position the window
            if (saved.getWidth() > 0 && saved.getHeight() > 0 && (saved.getX() > 0 || saved.getY() > 0))
            {
                // Try to restore the saved position
                auto target = saved;
                target.setWidth(targetWidth);
                target.setHeight(targetHeight);
                if (! displayArea.intersects(target))
                    target.setPosition(displayArea.getCentreX() - targetWidth / 2,
                                       displayArea.getCentreY() - targetHeight / 2);
                setBounds(target);
            }
            else
            {
                // Centre the window with the target size
                centreWithSize(targetWidth, targetHeight);
            }
           #endif

            setVisible (true);
        }

        void attachMainContent (const juce::String& venueId,
                                bool requestInitialScan,
                                juce::MenuBarModel* menuModel)
        {
            const auto previousBounds = getBounds();
            const bool wasFullScreen  = isFullScreen();

            setContentOwned (new MainComponent(), true);
            setBounds (previousBounds);
            if (wasFullScreen)
                setFullScreen (true);

           #if ! JUCE_MAC
            if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
                content->installMenuBarModel (menuModel);
           #else
            juce::ignoreUnused (menuModel);
           #endif

            if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
                content->setVenueId (venueId, requestInitialScan);
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
        loginWindow_.reset (new LoginWindow ([this](juce::String venueId, bool requestInitialScan)
        {
            // Create the main shell immediately, then close the login window.
            juce::MessageManager::callAsync ([this, venueId, requestInitialScan]
            {
                createMainWindow (venueId, requestInitialScan);
                juce::Timer::callAfterDelay(1, [this] { loginWindow_ = nullptr; });
            });
        }));
    }

    void createMainWindow (const juce::String& venueId, bool requestInitialScan = false)
    {
        if (mainWindow != nullptr)
            return;

        const auto startMs = juce::Time::getMillisecondCounterHiRes();

        mainWindow.reset (new MainWindow (getApplicationName()));
        DBG("[Startup] MainWindow ctor: " + juce::String(juce::Time::getMillisecondCounterHiRes() - startMs, 1) + " ms");

        // Let the main window paint first, then start venue/network loading.
        juce::Component::SafePointer<MainWindow> safeWindow (mainWindow.get());
        juce::Timer::callAfterDelay(1, [safeWindow, venueId, requestInitialScan, this]
        {
            if (safeWindow == nullptr)
                return;

            const auto venueStartMs = juce::Time::getMillisecondCounterHiRes();
            safeWindow->attachMainContent (venueId, requestInitialScan, this);
            DBG("[Startup] attachMainContent finished in "
                + juce::String(juce::Time::getMillisecondCounterHiRes() - venueStartMs, 1)
                + " ms");

           #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu (this);
           #endif
        });
    }

    std::unique_ptr<LoginWindow> loginWindow_;
    std::unique_ptr<MainWindow>  mainWindow;

    // Language codes for the dynamic Local menu, index matches (menuID - cmdLanguageBase).
    juce::StringArray languageCodes_;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (EncoreApplication)