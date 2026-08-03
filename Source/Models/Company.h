/*
  ==============================================================================

    Company.h

    Company data model - a karaoke company that owns one or more venues,
    each with its own host(s). Mirrors the ad-hoc shape already read/written
    by CompanyAdminPage (companies/{companyId}), extended with the address
    fields the onboarding wizard captures.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
struct Company
{
    std::string id;                 // Firestore document ID (auto-generated)
    std::string name;
    std::string status = "active";  // "active" | "suspended"
    std::string ownerUserId;
    std::string address;
    std::string city;
    std::string country;
    std::string logoUrl;

    //==============================================================================
    juce::String toJson() const;
    static Company fromJson(const juce::String& json);
    static Company fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
};
