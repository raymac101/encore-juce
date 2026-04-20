/*
  ==============================================================================

    AccessRights.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Access rights and user role enums - migrated from TypeScript

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>
#include <map>

//==============================================================================
enum class AccessRight
{
    Home,
    Search,
    Library,
    Charts,
    Mixer,
    Settings,
    Profile,
    Testing,
    Ads,
    Playlist,
    NewVenue,
    ChangeVenue,
    RefreshQueue,
    SetAccess,
    VenueManagement,
    UserManagement,
    SystemAdmin
};

//==============================================================================
enum class UserRole
{
    Host,
    Basic,
    Admin,
    Tester,
    EnterpriseAdmin
};

//==============================================================================
/** Utility functions for AccessRights / UserRole conversion and lookup */
namespace AccessRightsUtil
{
    inline std::string userRoleToString(UserRole role)
    {
        switch (role)
        {
            case UserRole::Host:            return "Host";
            case UserRole::Basic:           return "Basic";
            case UserRole::Admin:           return "Admin";
            case UserRole::Tester:          return "Tester";
            case UserRole::EnterpriseAdmin: return "EnterpriseAdmin";
        }
        return "Basic";
    }

    inline UserRole stringToUserRole(const std::string& s)
    {
        if (s == "Host")            return UserRole::Host;
        if (s == "Admin")           return UserRole::Admin;
        if (s == "Tester")          return UserRole::Tester;
        if (s == "EnterpriseAdmin") return UserRole::EnterpriseAdmin;
        return UserRole::Basic;
    }

    inline std::string accessRightToString(AccessRight r)
    {
        switch (r)
        {
            case AccessRight::Home:             return "Home";
            case AccessRight::Search:           return "Search";
            case AccessRight::Library:          return "Library";
            case AccessRight::Charts:           return "Charts";
            case AccessRight::Mixer:            return "Mixer";
            case AccessRight::Settings:         return "Settings";
            case AccessRight::Profile:          return "Profile";
            case AccessRight::Testing:          return "Testing";
            case AccessRight::Ads:              return "Ads";
            case AccessRight::Playlist:         return "Playlist";
            case AccessRight::NewVenue:         return "NewVenue";
            case AccessRight::ChangeVenue:      return "ChangeVenue";
            case AccessRight::RefreshQueue:     return "RefreshQueue";
            case AccessRight::SetAccess:        return "SetAccess";
            case AccessRight::VenueManagement:  return "VenueManagement";
            case AccessRight::UserManagement:   return "UserManagement";
            case AccessRight::SystemAdmin:      return "SystemAdmin";
        }
        return "Home";
    }

    /** Get the access rights for a given role */
    inline std::vector<AccessRight> getRightsForRole(UserRole role)
    {
        switch (role)
        {
            case UserRole::Basic:
                return { AccessRight::Home, AccessRight::Search, AccessRight::Charts,
                         AccessRight::Settings, AccessRight::Playlist };

            case UserRole::Host:
                return { AccessRight::Home, AccessRight::Search, AccessRight::Library,
                         AccessRight::Charts, AccessRight::Settings, AccessRight::Profile,
                         AccessRight::Playlist };

            case UserRole::Admin:
                return { AccessRight::Home, AccessRight::Search, AccessRight::Library,
                         AccessRight::Charts, AccessRight::Settings, AccessRight::Profile,
                         AccessRight::Ads, AccessRight::Playlist, AccessRight::ChangeVenue,
                         AccessRight::RefreshQueue, AccessRight::NewVenue, AccessRight::SetAccess };

            case UserRole::Tester:
            case UserRole::EnterpriseAdmin:
                return { AccessRight::Home, AccessRight::Search, AccessRight::Library,
                         AccessRight::Charts, AccessRight::Mixer, AccessRight::Settings,
                         AccessRight::Profile, AccessRight::Testing, AccessRight::Ads,
                         AccessRight::Playlist, AccessRight::ChangeVenue, AccessRight::RefreshQueue,
                         AccessRight::NewVenue, AccessRight::SetAccess, AccessRight::VenueManagement,
                         AccessRight::UserManagement, AccessRight::SystemAdmin };
        }
        return {};
    }

    /** Check if a role has a specific access right */
    inline bool hasAccess(UserRole role, AccessRight right)
    {
        auto rights = getRightsForRole(role);
        for (auto& r : rights)
            if (r == right) return true;
        return false;
    }
}
