/*
  ==============================================================================

    VenueItem.h
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Venue data model - migrated from TypeScript VenueItem interface

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
/**
    Venue data structure that mirrors the Firebase Firestore venue document.
    This maintains compatibility with the existing Angular/Electron version.
*/
struct VenueItem
{
    // ── Core identity ────────────────────────────────────────────────────────
    std::string id;                         // Firestore document ID
    std::string name;                       // Venue Name
    std::string address;                    // Street address
    std::string city;                       // City
    std::string country;                    // Country
    std::string adminEmail;                 // Admin email
    std::string password;                   // Password for venue access
    std::string registrationKey;            // License / registration key

    // ── Venue codes ─────────────────────────────────────────────────────────
    std::string code;                       // Primary code for Tagg app
    std::string codePlus;                   // Emergency / backup code

    // ── Queue policy ────────────────────────────────────────────────────────
    // NOTE: stored as strings in Firestore ("3", "5", etc.)
    int numSongs    = 5;                    // Max songs per singer  (maps to "numSongs")
    int numSingers  = 3;                    // Singers visible in queue (maps to "numSingers")
    int numStrikes  = 3;                    // Skips allowed per singer (maps to "numStrikes")
    bool repeatSongs   = false;             // Allow repeat songs
    bool autoapprove   = true;              // Auto-approve singers

    // ── Display / UI ────────────────────────────────────────────────────────
    // NOTE: background is stored as string in Firestore ("6")
    int  background = 0;                    // Lyrics screensaver index (0–6)
    std::string encoreBackground;           // Background image path (encoreBackground)
    std::string encodeBackgroundSize;       // Background tile size  (encodeBackgroundSize — note spelling)
    bool showOnlineSongs        = true;     // Show songs from Tagg / RightTracks
    bool showOnlineSongsEncore  = false;    // Show online songs in Encore
    bool showMemoryStats        = false;    // Show memory debug stats

    // ── Media / assets ──────────────────────────────────────────────────────
    std::string logoUrl;                    // Venue logo image URL
    std::string songBookUrl;                // Hosted songbook JSON URL
    std::string staticMapImageUrl;          // Static map preview image URL
    std::string venueImageUrl;              // Picture of outside the venue

    // ── Timestamps ──────────────────────────────────────────────────────────
    int64_t updateSongs = 0;                // Epoch ms — triggers songbook refresh
    int64_t dateTime    = 0;               // Epoch ms — last code change
    int64_t lat         = -1;              // Latitude  (-1 = not set)
    int64_t lng         = -1;              // Longitude (-1 = not set)

    // ── Version / metadata ──────────────────────────────────────────────────
    std::string taggVersion;                // Tagg app version string
    std::string version;                    // Encore app version string

    // ── Localization ────────────────────────────────────────────────────────
    std::string language = "en_US";         // Primary language
    std::string timezone = "UTC";           // Venue timezone
    
    //==============================================================================
    /** 
     * Convert venue data to JSON string for Firebase communication 
     */
    juce::String toJson() const;
    
    /** 
     * Create VenueItem from JSON string received from Firebase 
     */
    static VenueItem fromJson(const juce::String& json);
    
    /** 
     * Validate venue data integrity 
     */
    bool isValid() const;
    
    /** 
     * Get localized venue name (if translations available) 
     */
    juce::String getLocalizedName() const;
};