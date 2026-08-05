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
#include "Services/PluginHostService.h"
#include "Services/UpdateService.h"
#include "BuildInfo.h"

//==============================================================================
class EncoreApplication : public juce::JUCEApplication,
                          public juce::MenuBarModel
{
public:
    //==============================================================================
    EncoreApplication() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ENCORE_VERSION_WITH_BUILD; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    //==============================================================================
    void initialise (const juce::String& commandLine) override
    {
        // Plugin-scan child-process mode: if this invocation was launched
        // by PluginHostService::scanForPlugins() with --scan-plugin=/
        // --scan-output= arguments, do ONLY that (metadata scan of a single
        // candidate plugin file, which is the operation that can crash for
        // a malformed plugin — isolated here in a disposable child process
        // instead of the main running app), then quit immediately. This
        // check must stay the absolute first thing in this function, before
        // any login/venue/network bootstrap below, or every scanned
        // candidate plugin would launch the full app UI first.
        if (PluginHostService::handleScanCommandLineIfPresent())
        {
            quit();
            return;
        }

        juce::ignoreUnused (commandLine);

        // Launch-time-only update check (see UpdateService.h). Fire-and-
        // forget: runs on a background thread, never blocks or delays
        // showLoginWindow() below, and every failure path resolves silently
        // to "no update" -- this is the only place a check ever happens, by
        // design, so a live show can never be interrupted by one later.
        // If the download finishes while the login/venue flow is still on
        // screen, the banner simply appears once MainComponent is built
        // (see its constructor); this callback covers the case where it
        // finishes after that.
        UpdateService::getInstance().checkForUpdates ([this] (juce::String version, juce::String releaseNotesUrl)
        {
            juce::ignoreUnused (releaseNotesUrl);
            if (mainWindow != nullptr)
                if (auto* content = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
                    content->showUpdateAvailableBanner (version);
        });

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
    /** "Sign Out" from the TopBar user-menu dropdown -- tears down the main
        window and shows the login flow again, WITHOUT quitting the process
        (unlike shutdown(), which only ever runs as part of app exit). By the
        time this runs, MainComponent has already stopped its own watchers
        and cleared session-scoped service state (see its dropdown handler in
        setupUI()); this just handles the window swap, mirroring shutdown()'s
        menu-bar cleanup exactly. */
    void signOutAndReturnToLogin()
    {
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (nullptr);
       #else
        if (mainWindow != nullptr)
            if (auto* content = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
                content->installMenuBarModel (nullptr);
       #endif

        mainWindow = nullptr;
        showLoginWindow();
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
        cmdExit                = 0x3000,
        cmdFullscreen          = 0x3001,
        cmdResetScreenPosition = 0x3002,
        cmdShowTitleBar        = 0x3003,
        cmdCheckForUpdates     = 0x3004,

        // Dynamic language items use IDs starting at this base.
        cmdLanguageBase        = 0x3100,
    };

    juce::StringArray getMenuBarNames() override
    {
        auto& lm = LocalizationManager::getInstance();
        return { lm.getText ("menu.file"), lm.getText ("menu.window"), lm.getText ("menu.local") };
    }

    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex,
                                     const juce::String& /*menuName*/) override
    {
        auto& lm = LocalizationManager::getInstance();
        juce::PopupMenu menu;

        if (topLevelMenuIndex == 0)
        {
            menu.addItem (cmdCheckForUpdates, lm.getText ("menu.file.check_for_updates"));
            menu.addSeparator();
            menu.addItem (cmdExit, lm.getText ("menu.file.exit"));
        }
        else if (topLevelMenuIndex == 1)
        {
            const bool isFull        = mainWindow != nullptr && mainWindow->isTrulyFullScreen();
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
        else if (topLevelMenuIndex == 2)
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
            case cmdExit:                 systemRequestedQuit();  break;
            case cmdFullscreen:          toggleMainFullscreen();  break;
            case cmdResetScreenPosition: resetScreenPositions();  break;
            case cmdShowTitleBar:        toggleTitleBars();       break;
            case cmdCheckForUpdates:     checkForUpdatesManually(); break;
            default: break;
        }
    }

    //==============================================================================
    /** "File > Check for Updates" -- an explicit, user-initiated check, as
        opposed to the silent launch-time-only one in initialise() above.
        Unlike that one, this always reports a result: an "update available"
        dialog with a button that downloads + verifies the installer before
        handing off to UpdateService::restartAndInstall(), or a plain
        "you're up to date" dialog. Dialogs are shown unattached to any
        window (nullptr associated component) since the download step in
        between can take a while for a large installer, and this must stay
        correct even if the main window is closed/torn down (sign-out, etc.)
        during that gap. */
    void checkForUpdatesManually()
    {
        UpdateService::getInstance().checkForUpdatesManual ([] (bool hasUpdate, juce::String version, juce::String releaseNotesUrl)
        {
            juce::ignoreUnused (releaseNotesUrl);
            auto& lm = LocalizationManager::getInstance();

            if (! hasUpdate)
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::InfoIcon,
                    lm.getText ("update.dialog_uptodate_title"),
                    lm.getText ("update.dialog_uptodate_body"),
                    lm.getText ("update.btn_close"));
                return;
            }

            juce::AlertWindow::showOkCancelBox (
                juce::AlertWindow::InfoIcon,
                lm.getText ("update.dialog_available_title"),
                lm.getText ("update.dialog_available_body") + " " + version,
                lm.getText ("update.btn_update_now"),
                lm.getText ("update.btn_later"),
                nullptr,
                juce::ModalCallbackFunction::create ([] (int result)
                {
                    if (result != 1)
                        return;

                    UpdateService::getInstance().downloadUpdateNow ([] (bool success)
                    {
                        if (success)
                        {
                            UpdateService::getInstance().restartAndInstall();
                            return;
                        }

                        auto& lm2 = LocalizationManager::getInstance();
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::AlertWindow::WarningIcon,
                            lm2.getText ("update.dialog_failed_title"),
                            lm2.getText ("update.dialog_failed_body"));
                    });
                }));
        });
    }

    //==============================================================================
    /** Toggle the main window between its normal windowed state and a true,
        taskbar-covering fullscreen. Public because it's also invoked from
        BottomBar's "expand main screen" button via
        MainComponent::onToggleMainFullscreenRequested (MainComponent has no
        visibility into this class, so that callback dynamic_casts
        JUCEApplication::getInstance() back to EncoreApplication and calls
        this directly -- same hand-off pattern as signOutAndReturnToLogin()).
        All the actual chrome/bounds/menu-bar-visibility work lives in
        MainWindow::setTrueFullScreen() -- see its comment for why this
        doesn't just use juce::Component::setFullScreen(). */
    void toggleMainFullscreen()
    {
        if (mainWindow != nullptr)
            mainWindow->setTrueFullScreen (! mainWindow->isTrulyFullScreen());

        menuItemsChanged();
    }

private:
    //==============================================================================
    void resetScreenPositions()
    {
        // Move both windows to (0,0) with a reasonable default size. This is
        // the "panic button" for when a user gets a window off-screen after
        // disconnecting a monitor.
        if (mainWindow != nullptr)
        {
            mainWindow->setTrueFullScreen (false);
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
                    lw->setTrueFullScreen (false);
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

        /** True while this window is in "true" (taskbar-covering) fullscreen,
            as toggled by setTrueFullScreen() below. Deliberately NOT the same
            thing as juce::Component::isFullScreen() -- see setTrueFullScreen's
            comment for why. */
        bool isTrulyFullScreen() const noexcept { return trueFullScreen_; }

        /** Enter/exit true fullscreen: covers the entire monitor including
            the Windows taskbar, and hides the embedded application menu bar.

            juce::Component::setFullScreen() (SW_SHOWMAXIMIZED under the
            hood) always snaps to the monitor's *work area*, which excludes
            the taskbar, regardless of whether the window is borderless --
            confirmed by hand-testing, not just reading the JUCE source. The
            only way to actually cover the taskbar is to go borderless AND
            explicitly set the window's bounds to the monitor's *total* area
            ourselves, bypassing setFullScreen()/isFullScreen() entirely.
            That's why this tracks its own trueFullScreen_ flag and
            preFullScreenBounds_ rather than asking JUCE.

            setUsingNativeTitleBar(false) alone is NOT enough to get a bare
            window: it only swaps the OS-drawn title bar for JUCE's OWN
            custom-drawn one (DocumentWindow::getTitleBarHeight(), default
            26px) -- which still renders a title-bar strip with its own
            minimize/maximize/close controls (yellow/green/red circles),
            just not in the native Windows style. setTitleBarHeight(0)
            collapses that strip to nothing so fullscreen is actually bare.

            macOS is a genuinely different path, not just this window's copy
            of the above -- confirmed by hand-testing on LyricDisplayWindow
            (same underlying problem there): a borderless window resized to
            the display's full bounds does NOT actually get composited over
            the area the menu bar occupies, even with the menu bar hidden
            via NSApplicationPresentationOptions -- that layer sits above
            ordinary windows regardless of frame. Real native fullscreen
            ([NSWindow toggleFullScreen:], the same call the window's own
            green button makes) does cover it correctly. JUCE's kiosk-mode
            implementation only takes that native-fullscreen branch when the
            peer still has a native title bar, so macOS deliberately keeps
            the native title bar ON and lets kiosk mode drive real
            fullscreen instead of going borderless like Windows/Linux do. */
        void setTrueFullScreen (bool shouldBeFullScreen)
        {
            if (shouldBeFullScreen == trueFullScreen_)
                return;

           #if JUCE_MAC
            if (shouldBeFullScreen)
            {
                preFullScreenBounds_ = getBounds();
                setUsingNativeTitleBar (true);
                juce::Desktop::getInstance().setKioskModeComponent (this, false);
            }
            else
            {
                juce::Desktop::getInstance().setKioskModeComponent (nullptr, false);
                setUsingNativeTitleBar (UserPreferences::getInstance().getShowTitleBar());
                setBounds (preFullScreenBounds_.isEmpty()
                               ? juce::Rectangle<int> (0, 0, 1280, 800)
                               : preFullScreenBounds_);
            }
           #else
            if (shouldBeFullScreen)
            {
                preFullScreenBounds_ = getBounds();
                setUsingNativeTitleBar (false);
                setTitleBarHeight (0);

                if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (preFullScreenBounds_))
                    setBounds (display->totalArea);
            }
            else
            {
                setUsingNativeTitleBar (UserPreferences::getInstance().getShowTitleBar());
                setTitleBarHeight (26); // JUCE's own DocumentWindow default
                setBounds (preFullScreenBounds_.isEmpty()
                               ? juce::Rectangle<int> (0, 0, 1280, 800)
                               : preFullScreenBounds_);
            }
           #endif

            trueFullScreen_ = shouldBeFullScreen;

            if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
            {
                content->setMenuBarVisible (! trueFullScreen_);
                content->setMainScreenFullscreenIcon (trueFullScreen_);
            }
        }

        void attachMainContent (const juce::String& venueId,
                                bool requestInitialScan,
                                juce::MenuBarModel* menuModel)
        {
            const auto previousBounds = getBounds();

            setContentOwned (new MainComponent(), true);
            setBounds (previousBounds);

           #if ! JUCE_MAC
            if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
                content->installMenuBarModel (menuModel);
           #else
            juce::ignoreUnused (menuModel);
           #endif

            if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
            {
                content->setVenueId (venueId, requestInitialScan);
                content->onSignOutRequested = []
                {
                    if (auto* app = dynamic_cast<EncoreApplication*> (juce::JUCEApplication::getInstance()))
                        app->signOutAndReturnToLogin();
                };
                content->onToggleMainFullscreenRequested = []
                {
                    if (auto* app = dynamic_cast<EncoreApplication*> (juce::JUCEApplication::getInstance()))
                        app->toggleMainFullscreen();
                };

                // The fresh content's menu bar starts visible (installMenuBarModel
                // above) -- re-hide it if this window is currently in true
                // fullscreen (this rebuild doesn't change trueFullScreen_ itself,
                // previousBounds already preserved the correct monitor-covering
                // bounds above).
                content->setMenuBarVisible (! trueFullScreen_);
                content->setMainScreenFullscreenIcon (trueFullScreen_);
            }
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

            setContentOwned (new MainComponent(), true);

            // Re-apply the active venue so the queue/lyric UI doesn't go blank
            // after the rebuild.
            if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
            {
                content->setVenueId (VenueService::getInstance().getCurrentVenueId());
                content->onSignOutRequested = []
                {
                    if (auto* app = dynamic_cast<EncoreApplication*> (juce::JUCEApplication::getInstance()))
                        app->signOutAndReturnToLogin();
                };
                content->onToggleMainFullscreenRequested = []
                {
                    if (auto* app = dynamic_cast<EncoreApplication*> (juce::JUCEApplication::getInstance()))
                        app->toggleMainFullscreen();
                };
            }

            setBounds (previousBounds);

           #if ! JUCE_MAC
            // Re-attach the menu bar model to the fresh content component, then
            // re-hide it again if this window is currently in true fullscreen
            // (previousBounds above already preserved the correct
            // monitor-covering bounds; trueFullScreen_ itself is untouched by
            // this rebuild).
            if (auto* app = dynamic_cast<EncoreApplication*> (juce::JUCEApplication::getInstance()))
            {
                if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
                {
                    content->installMenuBarModel (app);
                    content->setMenuBarVisible (! trueFullScreen_);
                    content->setMainScreenFullscreenIcon (trueFullScreen_);
                }
            }
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
            // trueFullScreen_ (not isFullScreen()) -- setTrueFullScreen() never
            // calls the real juce::Component::setFullScreen(), so isFullScreen()
            // would always read false here and let monitor-covering bounds get
            // saved as if they were the normal windowed size.
            if (trueFullScreen_ || isMinimised()) return;
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

        bool trueFullScreen_ = false;
        juce::Rectangle<int> preFullScreenBounds_;

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