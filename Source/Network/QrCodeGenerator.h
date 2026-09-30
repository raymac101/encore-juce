/*
  ==============================================================================

    QrCodeGenerator.h

    Minimal, self-contained QR Code encoder used to let a phone scan its way
    to the local lyric-stream URL (see LyricStreamServer) instead of typing
    it in. Supports Byte-mode encoding only, versions 1-4, error correction
    level L -- comfortably enough capacity (up to 78 bytes) for any
    "http://<lan-ip>:<port>/" URL, with no external dependencies and no
    network access required to generate.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace QrCodeGenerator
{
    /** Encodes `text` (ASCII/Latin-1 recommended -- URLs are) as a QR code and
        renders it to a black-on-white juce::Image, `pixelsPerModule` pixels per
        module plus a `quietZoneModules`-module white border on every side (the
        spec requires a quiet zone for reliable scanning).

        Returns an invalid (default-constructed) juce::Image if `text` is too
        long to fit in a Version 4 / Level L symbol (more than 78 bytes) --
        callers should fall back to showing the URL as plain text in that case.
    */
    juce::Image generateQrCodeImage (const juce::String& text,
                                     int pixelsPerModule = 6,
                                     int quietZoneModules = 4);
}
