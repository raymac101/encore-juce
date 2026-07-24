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
            if (key == juce::KeyPress::escapeKey && owner_.isTrulyFullScreen())
            {
                owner_.setTrueFullScreen (false);
                return true;
            }
            return false;
        }

    private:
        LyricDisplayWindow& owner_;
    };

    // Prefer the first non-primary ("secondary") display if one is connected,
    // otherwise fall back to the primary display. Returns nullptr only if
    // there are no connected displays at all.
    const juce::Displays::Display* getSecondaryOrPrimaryDisplay()
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* primary  = displays.getPrimaryDisplay();

        for (const auto& d : displays.displays)
            if (&d != primary)
                return &d;

        return primary;
    }
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

void LyricDisplayWindow::setVenueContext (const juce::String& venueId,
                                          const juce::String& venueName)
{
    if (display_ != nullptr)
        display_->setVenueContext (venueId, venueName);
}

void LyricDisplayWindow::setQueuePreview (const std::vector<LyricDisplayComponent::QueuePreviewEntry>& entries)
{
    if (display_ != nullptr)
        display_->setQueuePreview (entries);
}

void LyricDisplayWindow::setLowerThirdNextUpSinger (const juce::String& singerName)
{
    if (display_ != nullptr)
        display_->setLowerThirdNextUpSinger (singerName);
}

void LyricDisplayWindow::setNowSingingInfo (const juce::String& singerName,
                                            const juce::String& songName,
                                            const juce::String& artistName,
                                            const juce::String& avatarPath)
{
    if (display_ != nullptr)
        display_->setNowSingingInfo (singerName, songName, artistName, avatarPath);
}

void LyricDisplayWindow::setForceIdleScreen (bool shouldForce)
{
    if (display_ != nullptr)
        display_->setForceIdleScreen (shouldForce);
}

void LyricDisplayWindow::addEmoji (const Emoji& request)
{
    if (display_ != nullptr)
        display_->addEmoji (request);
}

bool LyricDisplayWindow::loadVideo (const juce::File& videoFile, bool autoPlay)
{
    return display_ != nullptr && display_->loadVideo (videoFile, autoPlay);
}

void LyricDisplayWindow::stopVideo()
{
    if (display_ != nullptr)
        display_->stopVideo();
}

void LyricDisplayWindow::playVideo()
{
    if (display_ != nullptr)
        display_->playVideo();
}

void LyricDisplayWindow::pauseVideo()
{
    if (display_ != nullptr)
        display_->pauseVideo();
}

void LyricDisplayWindow::seekVideo (double positionSeconds)
{
    if (display_ != nullptr)
        display_->seekVideo (positionSeconds);
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
        if (savedFullScreen) setTrueFullScreen (true);
       #endif
        return;
    }

    // 2) Otherwise, default to full-screen on the secondary display (or the
    //    primary one, if that's all that's connected).
    if (const auto* target = getSecondaryOrPrimaryDisplay())
    {
        setBounds (target->userArea);
       #if ! (JUCE_IOS || JUCE_ANDROID)
        setTrueFullScreen (true);
       #endif
    }
    else
    {
        centreWithSize (960, 540);
    }
}

void LyricDisplayWindow::toggleFullScreen()
{
    setTrueFullScreen (! trueFullScreen_);
}

void LyricDisplayWindow::setTrueFullScreen (bool shouldBeFullScreen)
{
    // juce::Component::setFullScreen() (SW_SHOWMAXIMIZED under the hood)
    // always snaps to the monitor's *work area*, which excludes the
    // taskbar, regardless of border style -- confirmed by hand-testing, not
    // just reading the JUCE source. The only way to actually cover the
    // taskbar is to go borderless AND explicitly set bounds to the
    // monitor's *total* area ourselves, bypassing
    // setFullScreen()/isFullScreen() entirely. That's also what lets this
    // pin the window to a specific display (always the secondary one if
    // present) on both entry AND exit, rather than trusting wherever JUCE's
    // own pre-fullscreen-bounds memory points -- which knows nothing about
    // "always prefer the secondary display" and can drift the window back
    // to the primary display on exit.
    //
    // setUsingNativeTitleBar(false) alone is NOT enough to get a bare
    // window: it only swaps the OS-drawn title bar for JUCE's OWN
    // custom-drawn one (DocumentWindow::getTitleBarHeight(), default 26px)
    // -- which still renders a title-bar strip with its own
    // minimize/maximize/close controls (yellow/green/red circles), just
    // not in the native Windows style. setTitleBarHeight(0) collapses that
    // strip to nothing so fullscreen is actually bare.
    if (shouldBeFullScreen == trueFullScreen_)
        return;

    const auto* display = getSecondaryOrPrimaryDisplay();

    if (shouldBeFullScreen)
    {
        preFullScreenBounds_ = getBounds();
        setUsingNativeTitleBar (false);
        setTitleBarHeight (0);

        if (display != nullptr)
            setBounds (display->totalArea);
    }
    else
    {
        setUsingNativeTitleBar (UserPreferences::getInstance().getShowTitleBar());
        setTitleBarHeight (26); // JUCE's own DocumentWindow default

        auto restored = preFullScreenBounds_;
        if (display != nullptr
            && (restored.isEmpty() || ! display->totalArea.contains (restored.getCentre())))
            restored = display->userArea.reduced (80);

        setBounds (restored.isEmpty() ? juce::Rectangle<int> (0, 0, 960, 540) : restored);
    }

    trueFullScreen_ = shouldBeFullScreen;
    UserPreferences::getInstance().setLyricWindowFullScreen (trueFullScreen_);
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
    // trueFullScreen_ (not isFullScreen()) -- setTrueFullScreen() never
    // calls the real juce::Component::setFullScreen().
    if (trueFullScreen_ || isMinimised())
    {
        UserPreferences::getInstance().setLyricWindowFullScreen (trueFullScreen_);
        return;
    }

    UserPreferences::getInstance().setLyricWindowBounds (getBounds());
    UserPreferences::getInstance().setLyricWindowFullScreen (false);
}
