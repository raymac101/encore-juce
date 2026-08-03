/*
  ==============================================================================

    CustomerAdminService.h

    Client for the EnterpriseAdmin-only "Customer Admin" Cloud Functions
    (firebase/functions/adminUsers.js): legacy-user venue/role assignment,
    and customer support search/reset/deactivate/delete.

    Every method here calls a privileged onCall Cloud Function with
    Authorization: Bearer <idToken> attached so the function's own
    requireEnterpriseAdmin() check has a real request.auth to inspect --
    unlike the existing ApiService::enqueueMetadataFetch precedent, which
    sends no Authorization header (fine for its unauthenticated use case,
    wrong for this one). The in-app role gating on the UI side
    (NavBar::MenuItem::enterpriseAdminOnly) is a workflow convenience, not
    the real security boundary -- that's enforced server-side.

    sendPasswordResetEmail() is the one exception: it wraps
    FirestoreClient::sendPasswordResetEmail(), a public Identity Toolkit
    endpoint that isn't a privileged operation at all.

    Threading: every network-calling method here runs on a background
    thread via juce::Thread::launch; callbacks are always dispatched on the
    message thread via juce::MessageManager::callAsync (matches the
    threading model documented in CLAUDE.md and used throughout Services/).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

class CustomerAdminService
{
public:
    static CustomerAdminService& getInstance();

    //==========================================================================
    struct HostSummary
    {
        juce::String userId, email, stageName, fullName, country, city;
        juce::String signUpDate, lastLogin, role, accountStatus;
        int  loginCount = 0;
        bool authOnly   = false;   // true = found only in Firebase Auth, no hosts doc
    };

    struct VenueAssociation
    {
        juce::String id, venueId, venueName, venueCity, role;
    };

    /** One entry in the full venue list (see listVenues()) -- distinct from
        VenueAssociation, which is a specific host's existing membership row. */
    struct VenueSummary
    {
        juce::String id, name, address, city, country, code, codePlus, adminEmail;
        int  numSongs    = 0;
        int  numSingers  = 0;
        int  numStrikes  = 0;
        bool repeatSongs = false;
        bool autoapprove = false;
    };

    struct UserProfile
    {
        bool ok = false;
        HostSummary host;

        bool      hasLegacyProfile = false;
        juce::var legacyProfileRaw;      // raw fields from the read-only `users`/TAGG doc, for display only

        std::vector<VenueAssociation> venues;

        bool         hasAuthRecord = false;
        bool         authDisabled  = false;
        juce::StringArray authProviders;
        juce::String authLastSignInTime, authCreationTime;

        juce::String error;
    };

    using SearchCallback  = std::function<void(bool ok, std::vector<HostSummary> results,
                                               juce::String nextCursor, juce::String error)>;
    using ProfileCallback = std::function<void(UserProfile profile)>;
    using WriteCallback   = std::function<void(bool ok, juce::String error)>;
    using VenuesCallback  = std::function<void(bool ok, std::vector<VenueSummary> venues, juce::String error)>;

    //==========================================================================
    /** Search by exact email or stage-name prefix (case-insensitive either
        way). Falls back to a Firebase-Auth-only lookup server-side if
        nothing matches in `hosts` and the query looks like an email. */
    void searchUsers(const juce::String& query, const juce::String& cursor, SearchCallback onDone);

    /** Backlog of `hosts` docs with no active `user-venue-lookup` row. */
    void listUnassignedHosts(const juce::String& cursor, SearchCallback onDone);

    /** Every venue (uncapped-in-practice, capped at 1000 server-side) --
        for the "assign venue + role" picker. Deliberately not
        VenueService::getVenues(), which only returns venues the calling
        account is itself an active member of (see adminUsers.js). */
    void listVenues(VenuesCallback onDone);

    /** Irreversible: deletes the venue doc, its standard subcollections
        (queue/requested/playing/emojis/playlists), and every
        user-venue-lookup row referencing it. confirmName is re-validated
        server-side against the venue's real name. */
    void deleteVenue(const juce::String& venueId, const juce::String& confirmName, WriteCallback onDone);

    /** Combined profile: hosts fields + active venue associations + the
        read-only legacy `users`/TAGG doc (if any) + Firebase Auth record
        (disabled flag, providers, last sign-in). */
    void getUserProfile(const juce::String& uid, ProfileCallback onDone);

    /** Upserts user-venue-lookup/{uid}_{venueId} with status "active". */
    void assignVenueRole(const juce::String& uid,
                         const juce::String& venueId,
                         const juce::String& role,
                         const juce::String& venueName,
                         const juce::String& venueCity,
                         const juce::String& userEmail,
                         WriteCallback onDone);

    /** Admin-side direct password set -- never logged anywhere. */
    void setUserPassword(const juce::String& uid, const juce::String& newPassword, WriteCallback onDone);

    /** Disables the Firebase Auth account; data is left intact (reversible). */
    void deactivateUser(const juce::String& uid, WriteCallback onDone);

    /** Re-enables a previously deactivated account. */
    void reactivateUser(const juce::String& uid, WriteCallback onDone);

    /** Irreversible: deletes the Firebase Auth account, the hosts doc, and
        every user-venue-lookup row for this uid. confirmEmail is
        re-validated server-side against the account's real email. */
    void hardDeleteUser(const juce::String& uid, const juce::String& confirmEmail, WriteCallback onDone);

    /** Not a privileged Cloud Function -- wraps
        FirestoreClient::sendPasswordResetEmail(), a public endpoint. */
    void sendPasswordResetEmail(const juce::String& email, WriteCallback onDone);

private:
    CustomerAdminService() = default;
    ~CustomerAdminService() = default;

    /** POSTs {"data": dataObj} to the named callable with
        Authorization: Bearer <idToken>, and parses the onCall response
        envelope ({"result":...} or {"error":{status,message}}).
        Synchronous -- callers run this on a background thread. */
    struct CallResult
    {
        bool         ok = false;
        juce::var    result;
        juce::String errorMessage;
    };
    static CallResult callCloudFunction(const juce::String& functionName, const juce::var& dataObj);
    static juce::String functionUrl(const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE(CustomerAdminService)
};
