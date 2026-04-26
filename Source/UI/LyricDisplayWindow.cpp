/*
  ==============================================================================

    LyricDisplayWindow.cpp

  ==============================================================================
*/

#include "LyricDisplayWindow.h"
#include "../Services/UserPreferences.h"
#include "../Localization/LocalizationManager.h"

//==============================================================================
class LyricDisplayWindow::BoundsSaveTimer  : public juce::Timer
{
public:
    explicit BoundsSaveTimer (LyricDisplayWindow& o) : owner_ (o) {}
    void timerCallback() override
    {
        stopTimer();
        owner_.writeBoundsNow();
    }
private:
    LyricDisplayWindow& owner_;
};

namespace
{
    // Lightweight key listener so pressing F11 (or Cmd/Ctrl+F) toggles full-
    // screen on the lyric window regardless of which child component has focus.
    class FullScreenKeyListener  : public juce::KeyListener
    {
    public:
        explicit FullScreenKeyListener (LyricDisplayWindow& o) : owner_ (o) {}

        bool keyPressed (const juce::KeyPress& key, juce::Component*) override
        {
            if (key == juce::KeyPress::F11Key
             || (key.getKeyCode() == 'F'
                 && (key.getModifiers().isCommandDown()
                  || key.getModifiers().isCtrlDown())))
            {
                owner_.toggleFullScreen();
                return true;
            }
            if (key == juce::KeyPress::escapeKey && owner_.isFullScreen())
            {
                owner_.setFullScreen (false);
                return true;
            }
            return false;
        }

    private:
        LyricDisplayWindow& owner_;
    };
}

//==============================================================================
LyricDisplayWindow::LyricDisplayWindow (AudioEngine* audioEngine)
    : juce::DocumentWindow (LocalizationManager::getInstance().getText ("lyric_window.title"),
                            juce::Colour (0xff393939),
                            juce::DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (UserPreferences::getInstance().getShowTitleBar());
    setResizable (true, true);

    auto* comp = new LyricDisplayComponent();
    comp->setAudioEngine (audioEngine);
    display_ = comp;

    // setContentOwned transfers ownership to the DocumentWindow.
    setContentOwned (comp, false);

    // Own the key listener so it outlives the window's event plumbing and
    // is destroyed together with the window.
    fullScreenKeyListener_ = std::make_unique<FullScreenKeyListener> (*this);
    addKeyListener (fullScreenKeyListener_.get());

    // Refresh the title whenever the user changes language.
    LocalizationManager::getInstance().addChangeListener (this);

    moveToSecondaryDisplay();
    setVisible (true);
}

LyricDisplayWindow::~LyricDisplayWindow()
{
    LocalizationManager::getInstance().removeChangeListener (this);

    if (fullScreenKeyListener_ != nullptr)
        removeKeyListener (fullScreenKeyListener_.get());
}

void LyricDisplayWindow::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &LocalizationManager::getInstance())
        setName (LocalizationManager::getInstance().getText ("lyric_window.title"));
}

void LyricDisplayWindow::loadCDG (const juce::File& cdgFile)
{
    if (display_ != nullptr)
        display_->loadCDG (cdgFile);
}

bool LyricDisplayWindow::loadVideo (const juce::File& videoFile)
{
    return display_ != nullptr && display_->loadVideo (videoFile);
}

void LyricDisplayWindow::stopVideo()
{
    if (display_ != nullptr)
        display_->stopVideo();
}

bool LyricDisplayWindow::isVideoActive() const
{
    return display_ != nullptr && display_->isVideoActive();
}

double LyricDisplayWindow::getVideoPosition() const
{
    return display_ != nullptr ? display_->getVideoPosition() : 0.0;
}

double LyricDisplayWindow::getVideoDuration() const
{
    return display_ != nullptr ? display_->getVideoDuration() : 0.0;
}

void LyricDisplayWindow::closeButtonPressed()
{
    // Just hide the window — it can be reopened from the main window. We
    // don't delete because the main app owns this object.
    setVisible (false);
}

//==============================================================================
void LyricDisplayWindow::moveToSecondaryDisplay()
{
    auto& prefs = UserPreferences::getInstance();
    const auto saved = prefs.getLyricWindowBounds();
    const bool savedFullScreen = prefs.getLyricWindowFullScreen();

    // 1) If we have a saved position+size, honour it (but clamp so we don't
    //    end up off-screen when a monitor was disconnected since last run).
    if (saved.getWidth() > 0 && saved.getHeight() > 0
        && (saved.getX() > 0 || saved.getY() > 0))
    {
        auto displayArea = juce::Desktop::getInstance().getDisplays()
                               .getTotalBounds (true);
        auto target = saved;
        if (! displayArea.intersects (target))
            target.setPosition (displayArea.getCentreX() - target.getWidth()  / 2,
                                displayArea.getCentreY() - target.getHeight() / 2);
        setBounds (target);

       #if ! (JUCE_IOS || JUCE_ANDROID)
        if (savedFullScreen) setFullScreen (true);
       #endif
        return;
    }

    // 2) Otherwise, default to full-screen on the first non-primary display.
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* primary  = displays.getPrimaryDisplay();

    const juce::Displays::Display* target = nullptr;
    for (const auto& d : displays.displays)
    {
        if (&d != primary)
        {
            target = &d;
            break;
        }
    }

    if (target != nullptr)
    {
        setBounds (target->userArea);
       #if ! (JUCE_IOS || JUCE_ANDROID)
        setFullScreen (true);
       #endif
    }
    else
    {
        centreWithSize (960, 540);
    }
}

void LyricDisplayWindow::toggleFullScreen()
{
    setFullScreen (! isFullScreen());
    UserPreferences::getInstance().setLyricWindowFullScreen (isFullScreen());
    scheduleBoundsSave();
}

//==============================================================================
void LyricDisplayWindow::resized()
{
    juce::DocumentWindow::resized();
    scheduleBoundsSave();
}

void LyricDisplayWindow::moved()
{
    juce::DocumentWindow::moved();
    scheduleBoundsSave();
}

void LyricDisplayWindow::scheduleBoundsSave()
{
    if (boundsSaveTimer_ == nullptr)
        boundsSaveTimer_ = std::make_unique<BoundsSaveTimer> (*this);
    boundsSaveTimer_->startTimer (400);
}

void LyricDisplayWindow::writeBoundsNow()
{
    // Don't overwrite the remembered windowed bounds while full-screen or
    // minimised — the OS-reported rect would clobber the user's choice.
    if (isFullScreen() || isMinimised())
    {
        UserPreferences::getInstance().setLyricWindowFullScreen (isFullScreen());
        return;
    }

    UserPreferences::getInstance().setLyricWindowBounds (getBounds());
    UserPreferences::getInstance().setLyricWindowFullScreen (false);
}
