/*
  ==============================================================================

    UserVenueAssociation.h

    Mirror of the Angular UserVenueAssociation model. Stored in Firestore at
    /userVenueAssociations/{id}.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "AccessRights.h"

struct UserVenueAssociation
{
    juce::String id;             // Firestore doc id (assigned on creation)
    juce::String userId;         // Firebase Auth UID
    juce::String userEmail;
    juce::String venueId;
    juce::String venueName;      // Cached for display
    UserRole     role = UserRole::Host;
    bool         isActive = true;
    juce::String invitedBy;      // Email of inviter
    juce::Time   invitedDate;
    juce::Time   acceptedDate;
    juce::Time   lastAccessDate;
    juce::String notes;
};

struct VenueInvitation
{
    juce::String id;
    juce::String venueId;
    juce::String venueName;
    juce::String invitedUserEmail;
    juce::String invitedByEmail;
    juce::String invitedByName;
    UserRole     role = UserRole::Host;
    juce::Time   invitationDate;
    juce::Time   expirationDate;
    bool         isAccepted = false;
    bool         isExpired  = false;
    juce::Time   acceptedDate;
    juce::String notes;
};

struct VenueJoinRequest
{
    juce::String id;
    juce::String venueId;
    juce::String venueName;
    juce::String requestedByUserId;
    juce::String requestedByEmail;
    juce::String requestedByName;
    juce::String message;
    juce::String status;          // "pending" | "approved" | "denied" | "expired"
    juce::Time   requestDate;
    juce::Time   expirationDate;
    juce::String resolvedByEmail;
    juce::Time   resolvedDate;
};
