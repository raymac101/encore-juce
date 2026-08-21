/*
  ==============================================================================

    InvitationService.h

    Client for both halves of venue membership (firebase/functions/
    venueMembers.js): SettingsPage's "Invite" (addVenueMember) on the send
    side, and auto-claiming a pending invitation (acceptVenueInvitation) on
    the accept side. Both go through Cloud Functions rather than direct
    Firestore writes because firestore.rules restricts create/delete on
    user-venue-lookup to a platformAdmin custom claim nothing in this
    codebase ever sets -- a client-side write there silently fails (this is
    exactly what made the old in-app "Accept" button a no-op).

    Accept-side usage: LoginFlowController::runPostAuthFlow claims every
    pending invitation for the signed-in email automatically, on every
    login -- no UI, no button -- and the onboarding wizard's "you were
    invited" detection + join step claims the one it found during signup.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/UserVenueAssociation.h"
#include <vector>

class InvitationService
{
public:
    static InvitationService& getInstance();

    using FindCallback  = std::function<void(std::vector<VenueInvitation>)>;
    using WriteCallback = std::function<void(bool ok, juce::String error)>;

    /** `activated` is only meaningful when `ok` is true: true if `email`
        already had an Encore account and is now an active venue member
        immediately; false if no account exists yet and a pending
        invitation was left for them to auto-claim on first sign-in. */
    using AddMemberCallback = std::function<void(bool ok, bool activated, juce::String error)>;

    /** SettingsPage > Invite's actual write path -- adds `email` to
        `venueId` with `role` via the addVenueMember Cloud Function (caller
        must be an active Host/Admin/Tester/EnterpriseAdmin of that venue,
        re-checked server-side). Also enqueues a notification email via the
        Firebase "Trigger Email" extension. Runs on a background thread;
        callback fires on the message thread. */
    void addVenueMember(const juce::String& venueId, const juce::String& email,
                        const juce::String& role, AddMemberCallback onDone = nullptr);

    /** Pending (not accepted, not expired) invitations for `email`. Runs on
        a background thread; callback fires on the message thread. */
    void findPendingInvitations(const juce::String& email, FindCallback onDone);

    /** Synchronous core of findPendingInvitations — performs the Firestore
        query directly on the calling thread (must NOT be called from the
        message thread). Exposed so callers that are already running on
        their own background thread (LoginFlowController::runPostAuthFlow)
        can reuse the exact same query without a redundant nested
        juce::Thread::launch. */
    static std::vector<VenueInvitation> queryPendingInvitationsSync(const juce::String& email);

    /** Claims ONE pending invitation via the acceptVenueInvitation Cloud
        Function -- creates user-venue-lookup/{callerUid}_{venueId} active,
        then deletes the invitation doc. The caller is always whoever's
        currently signed in (FirestoreClient); the invitation's target
        email is re-checked against that account server-side. Runs on a
        background thread; callback fires on the message thread. */
    void acceptInvitation(const VenueInvitation& invitation, WriteCallback onDone = nullptr);

    /** Synchronous core of acceptInvitation() — for callers already
        running on their own background thread. Returns false (with
        `outError` set, if non-null) on failure. */
    static bool acceptInvitationSync(const juce::String& invitationId, juce::String* outError = nullptr);

    /** Claims every pending invitation for `email` in one go — called once
        per sign-in by LoginFlowController::runPostAuthFlow, BEFORE
        querying venue associations, so a host an admin added while they
        were signed out simply finds the venue already in their list on
        their very next login; no separate accept step ever surfaces.
        Synchronous — call from a background thread. Best-effort: a failed
        claim (e.g. a network hiccup) just leaves the invitation pending
        for the next login to retry, so this never throws/reports errors. */
    static void claimAllPendingSync(const juce::String& email);

private:
    InvitationService() = default;
    JUCE_DECLARE_NON_COPYABLE(InvitationService)
};
