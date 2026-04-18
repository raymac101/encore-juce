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
            // Set initial window size (will be made responsive)
            setResizable (true, true);
            centreWithSize (1200, 800);
           #endif

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            // This is called when the user tries to close this window
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

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