/*
  ==============================================================================

    LibVlcVideoView.h

    A muted, looping video surface backed by libvlc.

    juce::VideoComponent on Windows uses DirectShow, which cannot decode the
    MP4/H.264 ad clips that play fine through AVFoundation on macOS. Two
    Windows-only workarounds were tried and discarded before this one:
    WebView2 (a whole Chromium process tree just to loop a silent <video> tag)
    and Windows Media Foundation's IMFMediaEngine (much lighter, but leaks
    20-30MB of decoder resources on every clip switch -- confirmed to scale
    with switch count, not switch frequency, so it balloons unbounded over a
    real multi-hour show). libvlc decodes the same H.264/MP4 with its own
    bundled decoders and paints into a native child window, and -- unlike
    IMFMediaEngine -- has a genuinely supported, documented way to swap the
    playing source on a live player without leaking (see LibVlcVideoView.cpp).

    There is no transport control -- it is fire-and-forget looping playback,
    which is all the idle-screen ad rotation needs. The owning component decides
    when to switch clips (play()) or clear the slot (stop()).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class LibVlcVideoView : public juce::Component
{
public:
    LibVlcVideoView();
    ~LibVlcVideoView() override;

    /** True when the libvlc backend actually came up on this machine. If
        false, every other method is a no-op and the caller should fall back
        (e.g. leave the static "Ads will appear here" placeholder). */
    bool isAvailable() const noexcept;

    /** Starts looping playback of a local media file, muted, letterboxed to
        fit. Safe to call repeatedly; switching clips just re-sources the same
        player. */
    void play (const juce::File& mediaFile);

    /** Blanks the surface and stops playback. */
    void stop();

    /** The clip currently requested via play(), or an invalid File. */
    juce::File getCurrentFile() const    { return current_; }

    void resized() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    juce::File current_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibVlcVideoView)
};
