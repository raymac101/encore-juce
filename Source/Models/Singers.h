/*
  ==============================================================================

    Singers.h
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Singer data model - migrated from TypeScript Singers interface

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>
#include "QueueItem.h"

//==============================================================================
/**
    Singer structure representing a participant in the karaoke queue.
    Contains their personal info and collection of queued songs.
*/
struct Singers
{
    std::string id;                     // Unique singer identifier  
    std::string name;                   // Singer's display name
    std::string avatar;                 // Avatar/image URL
    std::string deviceId;               // Device associated with singer
    int order = 0;                      // Position in singer rotation
    int rotationOrder = 0;              // Round-robin rotation position
    int strikes = 0;                    // Number of strikes (for removal)
    int songsPerformed = 0;             // Total songs completed
    bool currentlyUp = false;           // Is this singer currently performing?
    std::vector<QueueItem> songs;       // List of queued songs for this singer
    int64_t lastSeen = 0;               // Last activity timestamp
    
    // Display properties
    juce::Colour nameColour = juce::Colours::white;
    bool isHighlighted = false;
    
    // Personal preferences
    std::string preferredLanguage = "en_US";
    std::string timezone = "UTC";
    
    //==============================================================================
    /** 
     * Convert singer data to JSON for Firebase storage 
     */
    juce::String toJson() const;
    
    /** 
     * Create Singers object from Firebase JSON data 
     */
    static Singers fromJson(const juce::String& json);
    
    /** 
     * Validate singer data 
     */
    bool isValid() const;
    
    /** 
     * Add a song to this singer's queue 
     */
    void addSong(const QueueItem& song);
    
    /** 
     * Remove a song by ID 
     */
    bool removeSong(const std::string& songId);
    
    /** 
     * Get next song to be performed 
     */
    QueueItem* getNextSong();
    
    /** 
     * Get total duration of all queued songs 
     */
    int getTotalQueuedDuration() const;
    
    /** 
     * Get formatted queue summary (e.g., "3 songs, 12 mins") 
     */
    juce::String getQueueSummary() const;
    
    /** 
     * Check if singer should be removed due to strikes 
     */
    bool shouldBeRemoved(int maxStrikes = 3) const;
    
    /** 
     * Mark singer as having performed a song 
     */
    void markSongPerformed();
    
    /** 
     * Update last seen timestamp 
     */
    void updateLastSeen();
};