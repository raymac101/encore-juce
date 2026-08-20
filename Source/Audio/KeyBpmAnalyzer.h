/*
  ==============================================================================

    KeyBpmAnalyzer.h

    Local, offline BPM + musical-key detection from an audio file -- no
    network call, no third-party API, no quota. Exists because Spotify
    deprecated its Audio Features endpoint (Nov 2024) for apps without
    Extended Quota Mode, so tempo/keySignature can no longer be sourced from
    Spotify for any new song; these two fields are purely informational
    (shown in TopBar's "Music Info" and recorded on the Playing/Audit
    record -- see MainComponent.cpp), never used to drive pitch-shifting or
    any other audio processing, so a standard-but-not-lab-grade estimate is
    an acceptable trade for "automatic and free forever."

    Algorithms (both textbook Music Information Retrieval techniques):
      - Tempo: spectral-flux onset-strength envelope, autocorrelated over
        the lag range for 50-200 BPM.
      - Key: a chromagram (FFT bins folded into 12 pitch classes) averaged
        over the analysed window, correlated against all 12 rotations of
        the Krumhansl-Kessler major/minor key profiles.

    Both run on a downmixed-to-mono, resampled-to-22.05kHz copy of roughly
    the first 90 seconds of the file (skipping a short lead-in), which is
    enough signal for a stable estimate without decoding/analysing an
    entire multi-minute track.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/CdgSong.h"

class KeyBpmAnalyzer
{
public:
    struct Result
    {
        bool         ok = false;
        int          bpm = 0;
        juce::String keySignature;   // Same letter/format convention as ApiService::getKeySignature, e.g. "F#", "Eb minor".
        juce::String errorMessage;
    };

    /** Synchronous -- decodes and analyses the file. Call from a background
        thread; this can take a couple of seconds for a typical song. */
    static Result analyze (const juce::File& audioFile);

    /** Resolves the actual decodable audio file for one version of a
        CdgSong, mirroring MainComponent::loadAndPlaySong's resolution
        (sibling audio file for .cdg/.xml, extraction for .zip, direct file
        for .mp4/.m4a/etc). If a temporary file was extracted (from a .zip),
        it is returned via outTempFileToDelete so the caller can clean it up
        after analysis; otherwise outTempFileToDelete is left invalid. */
    static juce::File resolvePlayableAudioFile (const CdgSong& song, int versionIndex,
                                                juce::File& outTempFileToDelete);

private:
    KeyBpmAnalyzer() = delete;
};
