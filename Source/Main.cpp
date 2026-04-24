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
#include "Localization/LocalizationManager.h"
#include "Services/UserPreferences.h"

//==============================================================================
class EncoreApplication : public juce::JUCEApplication
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
        // Initialize localization system
        LocalizationManager::getInstance().detectSystemLanguage();
        
        // Create the main application window
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override
    {
        // Clear the main window
        mainWindow = nullptr;
        
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
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                        .findColour (juce::ResizableWindow::backgroundColourId),
                            DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);

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
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (EncoreApplication)