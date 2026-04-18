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
    std::string id;                     // Venue ID assigned by Firestore  
    std::string name;                   // Venue Name
    std::string address;                // Venue Address
    std::string city;                   // Venue City
    std::string country;                // Venue Country
    std::string adminEmail;             // Admin Email for venue management
    std::string password;               // Password for venue access
    std::string code;                   // Venue Code that users enter into Tagg app
    std::string codePlus;               // Backup Code if primary code fails
    int64_t updateSongs = 0;            // Flag to tell Encore to update songbook
    int64_t dateTime = 0;               // Last Date/Time the Venue Code was changed
    std::string songBookUrl;            // URL to the Venue's Songbook
    std::string logoUrl;                // URL to the Venue's Logo
    int background = 0;                 // Selected Background Screensaver
    bool showOnlineSongs = true;        // Flag to show/hide songs from RightTracks
    int numSongs = 5;                   // Maximum songs per singer
    int numStrikes = 3;                 // Number of strikes before removal
    bool repeatSongs = false;           // Allow repeat songs policy
    bool autoapprove = true;            // Auto-approve song requests
    
    // Localization fields
    std::string language = "en_US";     // Primary language for venue
    std::string timezone = "UTC";       // Venue timezone
    
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