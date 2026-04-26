/*
  ==============================================================================

    LoginFlowController.h

    Drives the post-authentication scenario tree (matches the Angular
    LoginFlowService). Decision tree from the design doc:

        PC has venueId?
            YES ─ Is user associated with that venue?
                    YES → Load venue (Scenario 6 / 4)
                    NO  → Request access for that venue (Scenario 5 / 3)
            NO  ─ Does user have any associations?
                    1   → Auto-load it (Scenario 4 / 7)
                    >1  → Pick venue
                    0   → Await invitation (Scenarios 1 & 2: admins may create)

    All Firebase / Firestore traffic happens off the message thread; the
    callbacks below are dispatched on the message thread for safe UI use.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/Host.h"
#include "../Models/UserVenueAssociation.h"
#include "../Services/FirestoreClient.h"

class LoginFlowController
{
public:
    enum class Outcome
    {
        VenueLoaded,        // Single resolved venue. `venueId` is set.
        PickVenue,          // associations.size() > 1, ask the user to pick.
        AwaitInvitation,    // No associations. Show invitations list / block.
        RequestAccess       // Stored venueId is not in user's associations.
    };

    struct Result
    {
        Outcome                              outcome = Outcome::AwaitInvitation;
        juce::String                         venueId;        // For VenueLoaded / RequestAccess
        juce::String                         configuredVenueId; // Stored on this PC (for "Configured on this PC" badge)
        std::vector<UserVenueAssociation>    associations;   // For PickVenue
        std::vector<VenueInvitation>         invitations;    // For AwaitInvitation
        bool                                 canCreateVenue = false; // admin/enterprise
        Host                                 host;
    };

    using ResultCallback = std::function<void(Result)>;
    using ErrorCallback  = std::function<void(juce::String)>;

    /** Run the scenario tree on a background thread. The user must already
        be signed in via FirestoreClient. Callbacks fire on the message thread. */
    static void runPostAuthFlow(ResultCallback onResult,
                                ErrorCallback onError);

    /** Persist the user's chosen venue, update last-access, and remember it
        in user-preferences.json. Runs on a background thread. */
    static void selectVenue(const juce::String& venueId,
                            std::function<void()> onDone);

    /** Send a venue join request (Scenario 5). Runs on a background thread. */
    static void requestVenueAccess(const juce::String& venueId,
                                   const juce::String& venueName,
                                   const juce::String& message,
                                   std::function<void(bool ok, juce::String error)> onDone);

private:
    LoginFlowController() = delete;
};
