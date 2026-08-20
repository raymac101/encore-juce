/*
  ==============================================================================

    CdgSong.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    CDG Song model - local karaoke file representation

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>

//==============================================================================
/**
    Represents a local CDG karaoke file (or group of related files).
    A single song may have multiple file versions (CDG+MP3, ZIP, etc.).
*/
struct CdgSong
{
    std::string id;
    std::string imageUrl;
    int durationMS = 0;
    std::string keySignature;
    double tempo = 0.0;
    std::string songName;
    std::string artistName;
    std::vector<std::string> fullPath;      // Full paths to each file version
    std::vector<std::string> fileName;      // File names only
    std::vector<std::string> filePath;      // Directory paths
    std::vector<std::string> fileType;      // File extensions / types
    int64_t fileDate  = 0;                  // File modification date (epoch ms)
    int64_t fileSize  = 0;                  // File size in bytes
    int64_t addedAt   = 0;                  // Epoch ms when first added to the local library (0 = unknown)
    std::string releaseDate;
    std::vector<std::string> genres;
    std::vector<std::string> version;       // Manufacturer/label for this release, e.g. "Sunfly",
                                             // "Zoom Karaoke" -- at import, files for the same
                                             // song+artist from different manufacturers are grouped
                                             // into one CdgSong record, one entry per manufacturer
                                             // here (parallel to fullPath[]/code[]/etc. by index).
    std::vector<std::string> code;          // Disc/track codes
    std::vector<double> rating;             // Quality ratings per version (0.0–5.0)

    //==============================================================================
    juce::String toJson() const;
    static CdgSong fromJson(const juce::String& json);
    static CdgSong fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
    juce::String getFormattedDuration() const;

    /** True once the enrichment fields Spotify can actually still supply
        (art, duration, release date, genres) have all been filled in --
        either by LibraryScanner's local metadata pass or a remote lookup.
        keySignature/tempo are NOT required: Spotify deprecated the Audio
        Features endpoint in Nov 2024 for apps without Extended Quota Mode
        (this app was never granted it), so a live lookup can never supply
        them anymore -- they're still saved whenever present (e.g. from
        older bootstrap-imported data), just no longer block completeness.
        Mirrors (inverted) the "needs remote metadata" check LibraryPage uses
        to decide which songs to queue for a "Get Meta Data" fetch, and
        backs the Search page's "Only songs with metadata" toggle. */
    bool hasMetadata() const;

    /** Indices into version[]/code[]/rating[] etc., ordered by rating
        descending (highest-rated manufacturer's version first). Versions
        with no recorded rating sort as 0.0, i.e. last. Ties keep their
        original relative order. Shared by every "change version" picker
        (EditSingerModal, Now Playing) so they always agree on order. */
    std::vector<int> getVersionIndicesByRating() const;

    /** Human-readable label for versionIndex, e.g. "SF123 - Sunfly (4.5)".
        Falls back to whichever of code[]/version[] is present, then to
        "Version N" if neither is. Omits the "(rating)" suffix when that
        version has no rating recorded. Returns {} for an out-of-range index. */
    juce::String getVersionLabel (int versionIndex) const;
};
