/*
  ==============================================================================

    LicenseService.cpp

  ==============================================================================
*/

#include "LicenseService.h"
#include "FirestoreClient.h"

namespace
{
    using FC = FirestoreClient;

    juce::String generateLicenseKey()
    {
        static const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        auto& rng = juce::Random::getSystemRandom();
        juce::String out;
        for (int i = 0; i < 24; ++i)
            out += juce::String::charToString(charset[rng.nextInt(62)]);
        return out;
    }
}

//==============================================================================
LicenseService& LicenseService::getInstance()
{
    static LicenseService instance;
    return instance;
}

void LicenseService::createLicenseForVenue(const juce::String& venueId,
                                           const juce::String& venueName,
                                           WriteCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, venueName, onDone = std::move(onDone)]()
    {
        const auto now = juce::Time::getCurrentTime();
        const juce::Time expiry(now.toMilliseconds() + (juce::int64) 5 * 365 * 24 * 60 * 60 * 1000);

        auto fields = FC::makeFields({
            { "venueId",    FC::stringValue(venueId) },
            { "venueName",  FC::stringValue(venueName) },
            { "licenseKey", FC::stringValue(generateLicenseKey()) },
            { "expiryDate", FC::integerValue(expiry.toMilliseconds()) },
            { "isValid",    FC::booleanValue(true) }
        });

        bool ok = false;
        FC::getInstance().createDocument("licenses", fields, venueId, &ok);

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("createDocument failed")); });
    });
}

void LicenseService::checkVenueLicenseSync(const juce::String& venueId, bool& outValid, juce::String& outReason)
{
    outValid = true;
    outReason = {};

    if (venueId.isEmpty())
        return;

    int status = 0;
    auto doc = FC::getInstance().getDocument("licenses/" + venueId, &status);

    // No license doc at all -> grandfathered valid (venue predates this
    // feature, or the license was never created for some other reason).
    if (status != 200 || ! doc.hasProperty("fields"))
        return;

    auto fields = doc.getProperty("fields", juce::var());
    auto isValidField = fields.getProperty("isValid", juce::var());
    const bool isValid = isValidField.hasProperty("booleanValue")
                            ? (bool) isValidField.getProperty("booleanValue", false)
                            : false;

    auto expiryField = fields.getProperty("expiryDate", juce::var());
    juce::int64 expiryMs = 0;
    if (expiryField.hasProperty("integerValue"))
        expiryMs = expiryField.getProperty("integerValue", "0").toString().getLargeIntValue();

    const bool expired = expiryMs > 0 && expiryMs < juce::Time::getCurrentTime().toMilliseconds();

    if (! isValid)
    {
        outValid = false;
        outReason = "This venue's license is not active. Contact your account owner.";
        return;
    }

    if (expired)
    {
        outValid = false;
        outReason = "This venue's license has expired. Contact your account owner.";
        return;
    }
}

void LicenseService::checkVenueLicense(const juce::String& venueId, CheckCallback onDone)
{
    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        bool valid = true;
        juce::String reason;
        checkVenueLicenseSync(venueId, valid, reason);

        if (onDone)
            juce::MessageManager::callAsync([onDone, valid, reason]()
                { onDone(valid, reason); });
    });
}
