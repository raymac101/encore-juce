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
#include <vector>

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

    struct VenueMember
    {
        juce::String userId;
        juce::String email;
        juce::String stageName;
        juce::String role;
        juce::String status;
    };
    using VenueMembersCallback = std::function<void(bool ok, std::vector<VenueMember> members, juce::String error)>;

    /** Lists everyone assigned to `venueId` via `user-venue-lookup` (the
        per-venue staffing model -- distinct from company-wide
        companies/{id}/members). Enriches each result with email/stageName
        from `hosts/{userId}` on a best-effort basis (small N per venue).
        Requires the Firestore rules' isCompanyAdminOfVenue() grant (or
        platform admin) to actually return anything for a non-legacy-venue-
        admin caller. */
    void getVenueMembers(const juce::String& venueId, VenueMembersCallback onDone);

    using RemoveVenueMemberCallback = std::function<void(bool ok, juce::String error)>;

    /** Deletes `user-venue-lookup/{userId}_{venueId}`. Requires the same
        rules grant as getVenueMembers(). */
    void removeVenueMember(const juce::String& venueId, const juce::String& userId, RemoveVenueMemberCallback onDone);

private:
    CompanyService() = default;
    JUCE_DECLARE_NON_COPYABLE(CompanyService)
};
