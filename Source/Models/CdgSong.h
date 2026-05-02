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
    std::vector<std::string> version;       // e.g. "Male", "Female", "Duet"
    std::vector<std::string> code;          // Disc/track codes
    std::vector<double> rating;             // Quality ratings per version (0.0–5.0)

    //==============================================================================
    juce::String toJson() const;
    static CdgSong fromJson(const juce::String& json);
    static CdgSong fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
    juce::String getFormattedDuration() const;
};
