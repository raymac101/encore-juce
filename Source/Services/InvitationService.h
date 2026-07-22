/*
  ==============================================================================

    InvitationService.h

    The accept side of the venue-invitation flow — this did not exist
    anywhere in the codebase before the onboarding wizard (send-side only:
    SettingsPage::onInviteUser writes `venueInvitations`). Used both by
    LoginFlowController's existing "AwaitInvitation" screen (query only) and
    by the onboarding wizard's "you were invited" detection + accept step.

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

    /** Accept an invitation: creates `user-venue-lookup/{uid}_{venueId}`
        (role copied from the invitation) then patches the invitation doc
        (`isAccepted: true, acceptedDate`). */
    void acceptInvitation(const VenueInvitation& invitation,
                          const juce::String& uid,
                          WriteCallback onDone = nullptr);

private:
    InvitationService() = default;
    JUCE_DECLARE_NON_COPYABLE(InvitationService)
};
