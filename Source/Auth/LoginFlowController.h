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
        AwaitInvitation,    // No associations (pending invitations, if any, were already auto-claimed above -- see runPostAuthFlow). Offers self-serve venue setup.
        RequestAccess,      // Stored venueId is not in user's associations.
        VenueLicenseInvalid // Resolved venue exists but its license is invalid/expired.
    };

    struct Result
    {
        Outcome                              outcome = Outcome::AwaitInvitation;
        juce::String                         venueId;        // For VenueLoaded / RequestAccess / VenueLicenseInvalid
        juce::String                         venueName;      // For VenueLicenseInvalid's message
        juce::String                         licenseMessage; // For VenueLicenseInvalid
        juce::String                         configuredVenueId; // Stored on this PC (for "Configured on this PC" badge)
        std::vector<UserVenueAssociation>    associations;   // For PickVenue
        bool                                 canCreateVenue = false; // admin/enterprise (gates the legacy privileged-admin path)
        bool                                 offerSelfServeSetup = false; // Zero associations — offer the onboarding wizard regardless of role. Pending invitations (if any) are auto-claimed before this is even checked, see runPostAuthFlow.
        bool                                 hasCompanyContext = false;
        juce::String                         companyId;
        juce::String                         companyRole;
        Host                                 host;
    };

    using ResultCallback = std::function<void(Result)>;
    using ErrorCallback  = std::function<void(juce::String)>;

    /** Run the scenario tree on a background thread. The user must already
        be signed in via FirestoreClient. Callbacks fire on the message thread. */
    static void runPostAuthFlow(ResultCallback onResult,
                                ErrorCallback onError);

    /** Check the chosen venue's license, then (if valid) persist it, update
        last-access, and remember it in user-preferences.json. Runs on a
        background thread. `ok` is false (with `licenseMessage` set) if the
        venue's license is invalid/expired — in that case nothing is
        persisted. */
    static void selectVenue(const juce::String& venueId,
                            std::function<void(bool ok, juce::String licenseMessage)> onDone);

    /** Send a venue join request (Scenario 5). Runs on a background thread. */
    static void requestVenueAccess(const juce::String& venueId,
                                   const juce::String& venueName,
                                   const juce::String& message,
                                   std::function<void(bool ok, juce::String error)> onDone);

private:
    LoginFlowController() = delete;
};
