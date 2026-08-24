/*
  ==============================================================================

    CompanyService.h

    Company creation for the first-run onboarding wizard (account tiers
    "multi-venue owner" and "karaoke company"). Companies are created with a
    Firestore auto-ID (matching VenueService::addVenue's pattern) rather
    than a hand-picked short code, to avoid any risk of an explicit-ID
    createDocument() call silently colliding with an existing company.

    Threading matches every other service in this codebase: network I/O runs
    via juce::Thread::launch, callbacks are dispatched on the message thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/Company.h"

class CompanyService
{
public:
    static CompanyService& getInstance();

    using CreateCallback = std::function<void(bool ok, juce::String companyId, juce::String error)>;
    using WriteCallback  = std::function<void(bool ok, juce::String error)>;

    /** Create `companies/{autoId}`. The generated id is returned via onDone. */
    void createCompany(const Company& company, CreateCallback onDone);

    /** Upsert `companies/{companyId}/members/{userId}` — doc id is the
        member's uid (matches CompanyAdminPage::saveMemberMapping). */
    void addCompanyMember(const juce::String& companyId,
                          const juce::String& userId,
                          const juce::String& role,
                          WriteCallback onDone = nullptr);

    using MembershipCallback = std::function<void(bool found, juce::String companyId, juce::String role)>;

    /** Looks up whether `userId` already belongs to a company, via a
        collection-group query on `members` filtered by userId (company
        membership isn't in custom claims -- the members subcollection doc
        is the only record). Mirrors the synchronous helper
        LoginFlowController uses internally during the post-auth boot
        sequence, exposed here as a proper async service call for UI code
        (e.g. the TopBar "My Company" menu action) that isn't already
        running on a background thread. */
    void findMembershipForUser(const juce::String& userId, MembershipCallback onDone);

private:
    CompanyService() = default;
    JUCE_DECLARE_NON_COPYABLE(CompanyService)
};
