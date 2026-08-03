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
    // Stable Round Robin position. Assigned once when a singer first adds a
    // song (appended to the bottom of the RR) and changed only by manual
    // KJ drag-reorder or removal -- never by a song finishing, starting a
    // performance, or being skipped. The host is always order 0.
    int order = 0;
    // Derived/cached queue rank relative to the current rotation anchor
    // (0 = up next). Recomputed by QueueRotation::stampDerivedRanks()
    // whenever the RR or anchor changes; also used to *recover* the anchor
    // identity on load (see QueueRotation::findAnchorId). Not itself a
    // source of truth for RR membership/order -- see `order` above.
    int rotationOrder = 0;
    int strikes = 0;                    // Number of strikes (for removal)
    int songsPerformed = 0;             // Total songs completed
    bool currentlyUp = false;           // Is this singer currently performing?
    std::vector<QueueItem> songs;       // List of queued songs for this singer
    int64_t lastSeen = 0;               // Last activity timestamp
    
    // Display properties
    juce::Colour nameColour = juce::Colours::white;
    bool isHighlighted = false;

    // Host flag — when true this row represents the signed-in host (KJ).
    // The host is always the first entry in the queue, can never be removed,
    // and is rendered with a red border in the QueueBar.
    bool isHost = false;

    // Transient UI flag — set true when a singer is first added to the queue
    // so the SingerRow can render a green "newly added" highlight. Cleared
    // after the first repaint cycle (or after onSongbookChanged fires).
    bool isNewlyAdded = false;
    
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