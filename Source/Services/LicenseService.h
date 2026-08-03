/*
  ==============================================================================

    LicenseService.h

    Per-venue license gate: `licenses/{venueId}` (deterministic doc id —
    venueId is already a unique Firestore auto-ID from VenueService::addVenue,
    so no query is needed to address it). Not a payment/subscription system —
    a venue's license is created valid by the onboarding wizard and can only
    become invalid via a manual Firestore edit (or a future admin screen);
    this service only creates and checks it.

    A venue with NO license doc at all is treated as valid — this grandfathers
    every venue that existed before this feature shipped.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class LicenseService
{
public:
    static LicenseService& getInstance();

    using WriteCallback = std::function<void(bool ok, juce::String error)>;
    using CheckCallback = std::function<void(bool valid, juce::String reason)>;

    /** Create `licenses/{venueId}`, valid, expiring 5 years from now
        (matches the legacy generation style). Call right after a venue is
        created. */
    void createLicenseForVenue(const juce::String& venueId,
                               const juce::String& venueName,
                               WriteCallback onDone = nullptr);

    /** Missing doc -> valid=true (grandfathered). Existing doc -> valid only
        if isValid==true AND not expired. Runs on a background thread;
        callback fires on the message thread. */
    void checkVenueLicense(const juce::String& venueId, CheckCallback onDone);

    /** Synchronous core of checkVenueLicense — must NOT be called from the
        message thread. Exposed so callers already running on their own
        background thread (LoginFlowController::runPostAuthFlow) can reuse
        the exact same check without a redundant nested juce::Thread::launch. */
    static void checkVenueLicenseSync(const juce::String& venueId, bool& outValid, juce::String& outReason);

private:
    LicenseService() = default;
    JUCE_DECLARE_NON_COPYABLE(LicenseService)
};
