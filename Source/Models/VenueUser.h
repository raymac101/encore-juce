/*
  ==============================================================================

    VenueUser.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    VenueUser, VenueUserInvite, UserVenueAssociation, UserVenueLookup,
    VenueCodeSchedule models

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>
#include "AccessRights.h"

//==============================================================================
enum class VenueUserStatus
{
    Pending,
    Active,
    Inactive,
    Removed
};

namespace VenueUserStatusUtil
{
    inline std::string toString(VenueUserStatus s)
    {
        switch (s)
        {
            case VenueUserStatus::Pending:  return "pending";
            case VenueUserStatus::Active:   return "active";
            case VenueUserStatus::Inactive: return "inactive";
            case VenueUserStatus::Removed:  return "removed";
        }
        return "pending";
    }

    inline VenueUserStatus fromString(const std::string& s)
    {
        if (s == "active")   return VenueUserStatus::Active;
        if (s == "inactive") return VenueUserStatus::Inactive;
        if (s == "removed")  return VenueUserStatus::Removed;
        return VenueUserStatus::Pending;
    }
}

//==============================================================================
struct VenueUser
{
    std::string id;
    std::string userId;
    std::string email;
    std::string displayName;
    UserRole role = UserRole::Basic;
    VenueUserStatus status = VenueUserStatus::Pending;
    std::string invitedBy;
    int64_t invitedDate = 0;
    int64_t joinedDate = 0;
    int64_t lastActive = 0;
    bool isOwner = false;

    //==============================================================================
    juce::String toJson() const;
    static VenueUser fromJson(const juce::String& json);
    static VenueUser fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
};

//==============================================================================
struct VenueUserInvite
{
    std::string email;
    UserRole role = UserRole::Basic;
    std::string message;
    bool autoActivate = false;
    std::string userId;

    juce::String toJson() const;
    static VenueUserInvite fromJsonObject(juce::DynamicObject* obj);
};

//==============================================================================
struct UserVenueAssociation
{
    std::string id;
    std::string userId;
    std::string userEmail;
    std::string venueId;
    std::string venueName;
    UserRole role = UserRole::Basic;
    bool isActive = true;
    std::string invitedBy;
    int64_t invitedDate = 0;
    int64_t acceptedDate = 0;
    int64_t lastAccessDate = 0;
    std::string notes;

    juce::String toJson() const;
    static UserVenueAssociation fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
};

//==============================================================================
struct VenueInvitation
{
    std::string id;
    std::string venueId;
    std::string venueName;
    std::string invitedUserEmail;
    std::string invitedByEmail;
    std::string invitedByName;
    UserRole role = UserRole::Basic;
    int64_t invitationDate = 0;
    int64_t expirationDate = 0;
    bool isAccepted = false;
    bool isExpired = false;
    int64_t acceptedDate = 0;
    std::string notes;

    juce::String toJson() const;
    static VenueInvitation fromJsonObject(juce::DynamicObject* obj);
};

//==============================================================================
struct UserVenueLookup
{
    std::string userId;
    std::string venueId;
    std::string status;                     // "active", "inactive", "pending"
    std::string role;
    int64_t joinedDate = 0;
    int64_t lastActive = 0;
    std::string venueName;
    std::string venueCity;

    juce::String toJson() const;
    static UserVenueLookup fromJsonObject(juce::DynamicObject* obj);
};

//==============================================================================
struct DaySchedule
{
    bool enabled = false;
    std::string time;
};

struct VenueCodeSchedule
{
    bool enabled = false;
    DaySchedule sunday, monday, tuesday, wednesday, thursday, friday, saturday;
    int64_t lastCodeChange = 0;
    std::string manualCode;

    juce::String toJson() const;
    static VenueCodeSchedule fromJsonObject(juce::DynamicObject* obj);
};

struct VenueCodeSettings
{
    VenueCodeSchedule schedule;
    std::string currentCode;
    std::string previousCode;

    juce::String toJson() const;
    static VenueCodeSettings fromJsonObject(juce::DynamicObject* obj);
};
