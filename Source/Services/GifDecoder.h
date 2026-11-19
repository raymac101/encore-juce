/*
  ==============================================================================

    GifDecoder.h

    Minimal animated-GIF frame decoder. juce::ImageFileFormat (and its
    underlying GIFLoader) only ever decodes the FIRST frame of a GIF --
    confirmed against JUCE's own source -- so any real multi-frame playback
    needs this instead. Supports what real-world sprite/decoration GIFs
    actually use: global/local colour tables, single-transparent-index
    alpha, interlacing, and disposal methods 0-3 (method 3, "restore to
    previous", is rare but handled correctly rather than approximated).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

class GifDecoder
{
public:
    struct Frame
    {
        juce::Image image;   // ARGB, full logical-screen size, pre-composited
        int delayMs = 100;   // GIF's own per-frame delay (clamped to a sane minimum)
    };

    // Parses every frame into full-canvas, pre-composited images -- each one
    // already reflects every earlier frame's disposal -- so the caller can
    // just draw frames[i] directly with no compositing logic of its own.
    // Returns false (leaving outFrames empty) on any parse failure; a
    // decorative animation that fails to load should never crash or blank
    // the screen, just silently not show.
    static bool decode (const juce::File& file, std::vector<Frame>& outFrames);
};
