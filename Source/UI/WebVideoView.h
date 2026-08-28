/*
  ==============================================================================

    WebVideoView.h

    A muted, looping video surface backed by the Edge WebView2 (Chromium)
    engine on Windows.

    juce::VideoComponent on Windows uses DirectShow, which cannot decode the
    MP4/H.264 ad clips that play fine through AVFoundation on macOS. The lyric
    screen's idle-ad slot uses this instead on Windows: it hosts a tiny local
    HTML page with a <video autoplay muted loop> element pointed at the
    already-downloaded ad file (see AdMediaCache).

    There is no transport control -- it is fire-and-forget looping playback,
    which is all the idle-screen ad rotation needs. The owning component decides
    when to switch clips (play()) or clear the slot (stop()).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class WebVideoView : public juce::Component
{
public:
    WebVideoView();
    ~WebVideoView() override;

    /** True when the WebView2 backend actually came up on this machine. If
        false, every other method is a no-op and the caller should fall back
        (e.g. leave the static "Ads will appear here" placeholder). */
    bool isAvailable() const noexcept    { return browser_ != nullptr; }

    /** Starts looping playback of a local media file, muted, letterboxed to
        fit. Safe to call repeatedly; switching clips just renavigates. */
    void play (const juce::File& mediaFile);

    /** Blanks the surface and releases the decoder. */
    void stop();

    /** The clip currently requested via play(), or an invalid File. */
    juce::File getCurrentFile() const    { return current_; }

    void resized() override;

private:
    std::unique_ptr<juce::WebBrowserComponent> browser_;
    juce::File current_;
    int navCounter_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebVideoView)
};
