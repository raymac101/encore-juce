/*
  ==============================================================================

    Track.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Track data model - migrated from TypeScript Track class

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>

//==============================================================================
/**
    Track structure representing a karaoke song in the online catalog.
    Maps to the Firestore 'tracks' collection documents.
*/
struct Track
{
    // Core identification
    std::string id;                         // Firestore generated ID
    std::string name;                       // Track name
    std::string artists;                    // Artist(s), comma-delimited
    std::string album;                      // Album name

    // External URLs
    std::string appleUrl;
    std::string spotifyUrl;
    std::string youtubeUrl;
    std::string kwiUrl;
    std::string href;                       // Web API endpoint link
    std::string previewUrl;                 // 30-second preview URL

    // Media URLs
    std::string imageUrl;                   // Album cover image URL
    std::string musicUrl;                   // Playable karaoke music URL
    std::string lyricUrl;                   // Lyric sync data URL

    // Music metadata
    std::string keySignature;               // Key signature for this version
    std::string releaseDate;                // Release date
    std::string type;                       // "karaoke" or "singalong"
    std::string gender;                     // Original vocal gender: "male", "female", "duet", "unknown"
    std::string musicLabel;                 // Cover music owner: "Stingray", "Sunfly", etc.
    std::vector<std::string> genres;        // Music genres
    std::vector<std::string> availableMarkets; // ISO 3166-1 alpha-2 country codes

    // Numeric metadata
    int discNumber = 1;
    int durationMS = 0;                     // Track length in milliseconds
    int trackNumber = 0;
    double tempo = 0.0;                     // Tempo in BPM
    int popularity = 0;                     // 0-100
    int quality = 3;                        // 1=best .. 5=worst
    int key = -1;                           // Pitch class: 0=C, 1=C#, ... 11=B. -1=unknown
    int mode = 0;                           // 0=minor, 1=major
    int timeSignature = 4;                  // 3-7

    // Boolean flags
    bool isExplicit = false;                // Explicit lyrics
    bool isPlayable = true;                 // Playable in market

    // External IDs
    std::string isrc;
    std::string iswc;

    // Rights / publishing
    std::string performanceRO;              // Performance royalty collector (e.g. Socan)
    std::string publisherMechanical;        // Mechanical publishers
    std::string publisherSync;              // Sync publishers
    std::string restrictions;               // Market restrictions

    // Audio features (0.0 - 1.0 unless noted)
    double acousticness = 0.0;
    double danceability = 0.0;
    double energy = 0.0;
    double instrumentalness = 0.0;
    double liveness = 0.0;
    double loudness = 0.0;                  // dB, typically -60 to 0
    double speechiness = 0.0;
    double valence = 0.0;

    // Karaoke-specific
    double difficulty = -1.0;               // 0.0=easy, 0.5=med, 1.0=hard, -1=unset

    //==============================================================================
    juce::String toJson() const;
    static Track fromJson(const juce::String& json);
    static Track fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
    juce::String getFormattedDuration() const;
};
