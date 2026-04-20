/*
  ==============================================================================

    VenueUser.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    VenueUser and related model implementations

  ==============================================================================
*/

#include "VenueUser.h"

//==============================================================================
// VenueUser
//==============================================================================
juce::String VenueUser::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id", juce::String(id));
    obj->setProperty("userId", juce::String(userId));
    obj->setProperty("email", juce::String(email));
    obj->setProperty("displayName", juce::String(displayName));
    obj->setProperty("role", juce::String(AccessRightsUtil::userRoleToString(role)));
    obj->setProperty("status", juce::String(VenueUserStatusUtil::toString(status)));
    obj->setProperty("invitedBy", juce::String(invitedBy));
    obj->setProperty("invitedDate", (juce::int64)invitedDate);
    obj->setProperty("joinedDate", (juce::int64)joinedDate);
    obj->setProperty("lastActive", (juce::int64)lastActive);
    obj->setProperty("isOwner", isOwner);
    return juce::JSON::toString(juce::var(obj.get()));
}

VenueUser VenueUser::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

VenueUser VenueUser::fromJsonObject(juce::DynamicObject* obj)
{
    VenueUser v;
    if (obj == nullptr) return v;
    v.id          = obj->getProperty("id").toString().toStdString();
    v.userId      = obj->getProperty("userId").toString().toStdString();
    v.email       = obj->getProperty("email").toString().toStdString();
    v.displayName = obj->getProperty("displayName").toString().toStdString();
    v.role        = AccessRightsUtil::stringToUserRole(obj->getProperty("role").toString().toStdString());
    v.status      = VenueUserStatusUtil::fromString(obj->getProperty("status").toString().toStdString());
    v.invitedBy   = obj->getProperty("invitedBy").toString().toStdString();
    v.invitedDate = (int64_t)obj->getProperty("invitedDate");
    v.joinedDate  = (int64_t)obj->getProperty("joinedDate");
    v.lastActive  = (int64_t)obj->getProperty("lastActive");
    v.isOwner     = (bool)obj->getProperty("isOwner");
    return v;
}

bool VenueUser::isValid() const
{
    return !id.empty() && !email.empty();
}

//==============================================================================
// VenueUserInvite
//==============================================================================
juce::String VenueUserInvite::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("email", juce::String(email));
    obj->setProperty("role", juce::String(AccessRightsUtil::userRoleToString(role)));
    obj->setProperty("message", juce::String(message));
    obj->setProperty("autoActivate", autoActivate);
    obj->setProperty("userId", juce::String(userId));
    return juce::JSON::toString(juce::var(obj.get()));
}

VenueUserInvite VenueUserInvite::fromJsonObject(juce::DynamicObject* obj)
{
    VenueUserInvite i;
    if (obj == nullptr) return i;
    i.email        = obj->getProperty("email").toString().toStdString();
    i.role         = AccessRightsUtil::stringToUserRole(obj->getProperty("role").toString().toStdString());
    i.message      = obj->getProperty("message").toString().toStdString();
    i.autoActivate = (bool)obj->getProperty("autoActivate");
    i.userId       = obj->getProperty("userId").toString().toStdString();
    return i;
}

//==============================================================================
// UserVenueAssociation
//==============================================================================
juce::String UserVenueAssociation::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id", juce::String(id));
    obj->setProperty("userId", juce::String(userId));
    obj->setProperty("userEmail", juce::String(userEmail));
    obj->setProperty("venueId", juce::String(venueId));
    obj->setProperty("venueName", juce::String(venueName));
    obj->setProperty("role", juce::String(AccessRightsUtil::userRoleToString(role)));
    obj->setProperty("isActive", isActive);
    obj->setProperty("invitedBy", juce::String(invitedBy));
    obj->setProperty("invitedDate", (juce::int64)invitedDate);
    obj->setProperty("acceptedDate", (juce::int64)acceptedDate);
    obj->setProperty("lastAccessDate", (juce::int64)lastAccessDate);
    obj->setProperty("notes", juce::String(notes));
    return juce::JSON::toString(juce::var(obj.get()));
}

UserVenueAssociation UserVenueAssociation::fromJsonObject(juce::DynamicObject* obj)
{
    UserVenueAssociation a;
    if (obj == nullptr) return a;
    a.id             = obj->getProperty("id").toString().toStdString();
    a.userId         = obj->getProperty("userId").toString().toStdString();
    a.userEmail      = obj->getProperty("userEmail").toString().toStdString();
    a.venueId        = obj->getProperty("venueId").toString().toStdString();
    a.venueName      = obj->getProperty("venueName").toString().toStdString();
    a.role           = AccessRightsUtil::stringToUserRole(obj->getProperty("role").toString().toStdString());
    a.isActive       = (bool)obj->getProperty("isActive");
    a.invitedBy      = obj->getProperty("invitedBy").toString().toStdString();
    a.invitedDate    = (int64_t)obj->getProperty("invitedDate");
    a.acceptedDate   = (int64_t)obj->getProperty("acceptedDate");
    a.lastAccessDate = (int64_t)obj->getProperty("lastAccessDate");
    a.notes          = obj->getProperty("notes").toString().toStdString();
    return a;
}

bool UserVenueAssociation::isValid() const
{
    return !userId.empty() && !venueId.empty();
}

//==============================================================================
// VenueInvitation
//==============================================================================
juce::String VenueInvitation::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id", juce::String(id));
    obj->setProperty("venueId", juce::String(venueId));
    obj->setProperty("venueName", juce::String(venueName));
    obj->setProperty("invitedUserEmail", juce::String(invitedUserEmail));
    obj->setProperty("invitedByEmail", juce::String(invitedByEmail));
    obj->setProperty("invitedByName", juce::String(invitedByName));
    obj->setProperty("role", juce::String(AccessRightsUtil::userRoleToString(role)));
    obj->setProperty("invitationDate", (juce::int64)invitationDate);
    obj->setProperty("expirationDate", (juce::int64)expirationDate);
    obj->setProperty("isAccepted", isAccepted);
    obj->setProperty("isExpired", isExpired);
    obj->setProperty("acceptedDate", (juce::int64)acceptedDate);
    obj->setProperty("notes", juce::String(notes));
    return juce::JSON::toString(juce::var(obj.get()));
}

VenueInvitation VenueInvitation::fromJsonObject(juce::DynamicObject* obj)
{
    VenueInvitation v;
    if (obj == nullptr) return v;
    v.id               = obj->getProperty("id").toString().toStdString();
    v.venueId          = obj->getProperty("venueId").toString().toStdString();
    v.venueName        = obj->getProperty("venueName").toString().toStdString();
    v.invitedUserEmail = obj->getProperty("invitedUserEmail").toString().toStdString();
    v.invitedByEmail   = obj->getProperty("invitedByEmail").toString().toStdString();
    v.invitedByName    = obj->getProperty("invitedByName").toString().toStdString();
    v.role             = AccessRightsUtil::stringToUserRole(obj->getProperty("role").toString().toStdString());
    v.invitationDate   = (int64_t)obj->getProperty("invitationDate");
    v.expirationDate   = (int64_t)obj->getProperty("expirationDate");
    v.isAccepted       = (bool)obj->getProperty("isAccepted");
    v.isExpired        = (bool)obj->getProperty("isExpired");
    v.acceptedDate     = (int64_t)obj->getProperty("acceptedDate");
    v.notes            = obj->getProperty("notes").toString().toStdString();
    return v;
}

//==============================================================================
// UserVenueLookup
//==============================================================================
juce::String UserVenueLookup::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("userId", juce::String(userId));
    obj->setProperty("venueId", juce::String(venueId));
    obj->setProperty("status", juce::String(status));
    obj->setProperty("role", juce::String(role));
    obj->setProperty("joinedDate", (juce::int64)joinedDate);
    obj->setProperty("lastActive", (juce::int64)lastActive);
    obj->setProperty("venueName", juce::String(venueName));
    obj->setProperty("venueCity", juce::String(venueCity));
    return juce::JSON::toString(juce::var(obj.get()));
}

UserVenueLookup UserVenueLookup::fromJsonObject(juce::DynamicObject* obj)
{
    UserVenueLookup l;
    if (obj == nullptr) return l;
    l.userId     = obj->getProperty("userId").toString().toStdString();
    l.venueId    = obj->getProperty("venueId").toString().toStdString();
    l.status     = obj->getProperty("status").toString().toStdString();
    l.role       = obj->getProperty("role").toString().toStdString();
    l.joinedDate = (int64_t)obj->getProperty("joinedDate");
    l.lastActive = (int64_t)obj->getProperty("lastActive");
    l.venueName  = obj->getProperty("venueName").toString().toStdString();
    l.venueCity  = obj->getProperty("venueCity").toString().toStdString();
    return l;
}

//==============================================================================
// VenueCodeSchedule
//==============================================================================
static void serializeDay(juce::DynamicObject* parent, const char* name, const DaySchedule& day)
{
    juce::DynamicObject::Ptr d = new juce::DynamicObject();
    d->setProperty("enabled", day.enabled);
    d->setProperty("time", juce::String(day.time));
    parent->setProperty(name, juce::var(d.get()));
}

static DaySchedule deserializeDay(juce::DynamicObject* parent, const char* name)
{
    DaySchedule d;
    auto v = parent->getProperty(name);
    if (auto* obj = v.getDynamicObject())
    {
        d.enabled = (bool)obj->getProperty("enabled");
        d.time    = obj->getProperty("time").toString().toStdString();
    }
    return d;
}

juce::String VenueCodeSchedule::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("enabled", enabled);

    juce::DynamicObject::Ptr days = new juce::DynamicObject();
    serializeDay(days.get(), "sunday", sunday);
    serializeDay(days.get(), "monday", monday);
    serializeDay(days.get(), "tuesday", tuesday);
    serializeDay(days.get(), "wednesday", wednesday);
    serializeDay(days.get(), "thursday", thursday);
    serializeDay(days.get(), "friday", friday);
    serializeDay(days.get(), "saturday", saturday);
    obj->setProperty("days", juce::var(days.get()));

    obj->setProperty("lastCodeChange", (juce::int64)lastCodeChange);
    obj->setProperty("manualCode", juce::String(manualCode));

    return juce::JSON::toString(juce::var(obj.get()));
}

VenueCodeSchedule VenueCodeSchedule::fromJsonObject(juce::DynamicObject* obj)
{
    VenueCodeSchedule s;
    if (obj == nullptr) return s;

    s.enabled = (bool)obj->getProperty("enabled");

    if (auto* days = obj->getProperty("days").getDynamicObject())
    {
        s.sunday    = deserializeDay(days, "sunday");
        s.monday    = deserializeDay(days, "monday");
        s.tuesday   = deserializeDay(days, "tuesday");
        s.wednesday = deserializeDay(days, "wednesday");
        s.thursday  = deserializeDay(days, "thursday");
        s.friday    = deserializeDay(days, "friday");
        s.saturday  = deserializeDay(days, "saturday");
    }

    s.lastCodeChange = (int64_t)obj->getProperty("lastCodeChange");
    s.manualCode     = obj->getProperty("manualCode").toString().toStdString();

    return s;
}

//==============================================================================
// VenueCodeSettings
//==============================================================================
juce::String VenueCodeSettings::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    auto scheduleJson = juce::JSON::parse(schedule.toJson());
    obj->setProperty("schedule", scheduleJson);
    obj->setProperty("currentCode", juce::String(currentCode));
    obj->setProperty("previousCode", juce::String(previousCode));

    return juce::JSON::toString(juce::var(obj.get()));
}

VenueCodeSettings VenueCodeSettings::fromJsonObject(juce::DynamicObject* obj)
{
    VenueCodeSettings s;
    if (obj == nullptr) return s;

    if (auto* schedObj = obj->getProperty("schedule").getDynamicObject())
        s.schedule = VenueCodeSchedule::fromJsonObject(schedObj);

    s.currentCode  = obj->getProperty("currentCode").toString().toStdString();
    s.previousCode = obj->getProperty("previousCode").toString().toStdString();

    return s;
}
