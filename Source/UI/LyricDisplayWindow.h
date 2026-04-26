/*
  ==============================================================================

    LyricDisplayWindow.h

    Secondary-display window that shows the CDG lyrics for singers. Created
    at application startup and positioned on the secondary monitor when one
    is available.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "LyricDisplayComponent.h"

class AudioEngine;

class LyricDisplayWindow  : public juce::DocumentWindow,
                            public juce::ChangeListener
{
public:
    LyricDisplayWindow (AudioEngine* audioEngine);
    ~LyricDisplayWindow() override;

    /** Access the inner display component for loading CDG files, updating
        the next-singer banner, etc. */
    LyricDisplayComponent* getDisplay() noexcept { return display_; }

    /** Convenience passthrough to the component. */
    void loadCDG (const juce::File& cdgFile);

    /** Loads an MP4/M4V/MOV video for full-screen playback on the lyric
        display. Audio is provided by the video itself — the AudioEngine is
        not used in this mode. Returns true on success. */
    bool loadVideo (const juce::File& videoFile);

    /** Stops any video playback and reverts to the CDG/idle view. */
    void stopVideo();

    /** True while a video is loaded on this window. */
    bool isVideoActive() const;

    /** Current play position / total duration of the loaded video, seconds. */
    double getVideoPosition() const;
    double getVideoDuration() const;

    void closeButtonPressed() override;

    /** Place the window on the first non-primary display if one exists,
        otherwise centre it on the primary display with a sensible size. */
    void moveToSecondaryDisplay();

    /** Enter/exit full-screen mode on the current display. */
    void toggleFullScreen();

    //==============================================================================
    // Persist bounds on move/resize (debounced).
    void resized() override;
    void moved() override;

    /** Refreshes the window title when the UI language changes. */
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    void scheduleBoundsSave();
    void writeBoundsNow();

    class BoundsSaveTimer;
    std::unique_ptr<BoundsSaveTimer> boundsSaveTimer_;

    LyricDisplayComponent* display_ = nullptr;   // owned by DocumentWindow
    std::unique_ptr<juce::KeyListener> fullScreenKeyListener_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LyricDisplayWindow)
};
