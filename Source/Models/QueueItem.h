/*
  ==============================================================================

    QueueItem.h
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Queue item data model - migrated from TypeScript QueueItem interface

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
/**
    Queue item structure representing a single song in the queue.
    Maintains compatibility with existing Firebase queue structure.
*/
struct QueueItem
{
    std::string id;                     // Unique identifier
    std::string deviceId;               // Device that submitted the request
    std::string profileId;              // Firestore profileId (mobile users)
    std::string foxId;                  // Internal fox id
    std::string singerName;             // Name of the singer
    std::string singerAvatar;           // Avatar/image URL for singer
    std::string songId;                 // Unique song identifier
    std::string songName;               // Display name of the song
    std::string songArtist;             // Artist name
    std::string songVersion;            // Version (e.g., "Male", "Female", "Duet")
    int duration = 0;                   // Song duration in seconds
    int order = 0;                      // Position in overall queue
    int songOrder = 0;                  // Position within singer's songs
    float pitch = 1.0f;                 // Pitch adjustment (1.0 = normal)
    std::string status = "queued";      // Status: "queued", "playing", "completed", "new", "approved", "approvedpending", "rejected"
    std::string action;                 // Optional action verb on /requested ("delete", "next", "now", ...)
    std::string reason;                 // Rejection reason (when status == "rejected")
    std::string time;                   // Estimated time to play (formatted)
    bool alerts = false;                // Whether song has special alerts
    int64_t dateAdded = 0;              // Timestamp when added to queue
    
    // Audio properties
    int key = 0;                        // Key change in semitones (-12 to +12)
    float volume = 1.0f;                // Volume adjustment (0.0 to 1.0)
    
    // Display properties  
    juce::Colour backgroundColour = juce::Colours::transparentBlack;
    juce::Colour textColour = juce::Colours::white;
    
    //==============================================================================
    /** 
     * Convert queue item to JSON for Firebase storage 
     */
    juce::String toJson() const;
    
    /** 
     * Create QueueItem from Firebase JSON data 
     */
    static QueueItem fromJson(const juce::String& json);
    
    /** 
     * Validate queue item data 
     */
    bool isValid() const;
    
    /** 
     * Get formatted duration string (e.g., "3:45") 
     */
    juce::String getFormattedDuration() const;
    
    /** 
     * Get localized status text 
     */
    juce::String getLocalizedStatus() const;
    
    /** 
     * Calculate estimated wait time based on position 
     */
    juce::String getEstimatedWaitTime(int averageSongLength = 210) const;
};