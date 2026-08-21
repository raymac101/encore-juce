/*
  ==============================================================================

    MetadataQuotaService.cpp

  ==============================================================================
*/

#include "MetadataQuotaService.h"
#include "FirestoreClient.h"
#include "../Firebase/FirebaseConfig.h"

namespace
{
    // Same onCall calling convention as InvitationService.cpp's local
    // callCloudFunction -- not worth a shared header for ~35 lines.
    juce::String functionUrl (const juce::String& name)
    {
        return "https://us-central1-" + FirebaseConfig::projectId + ".cloudfunctions.net/" + name;
    }

    struct CallResult
    {
        bool         ok = false;
        juce::var    result;
        juce::String errorMessage;
    };

    CallResult callCloudFunction (const juce::String& functionName)
    {
        CallResult out;

        juce::DynamicObject::Ptr bodyObj = new juce::DynamicObject();
        bodyObj->setProperty ("data", juce::var (new juce::DynamicObject()));
        const auto body = juce::JSON::toString (juce::var (bodyObj.get()), false);

        const auto idToken = FirestoreClient::getInstance().getFreshIdToken();

        juce::URL url (functionUrl (functionName));
        int statusCode = 0;
        auto stream = url.withPOSTData (body).createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                .withConnectionTimeoutMs (15000)
                .withHttpRequestCmd ("POST")
                .withExtraHeaders ("Content-Type: application/json\r\n"
                                   "Accept: application/json\r\n"
                                   "Authorization: Bearer " + idToken)
                .withStatusCode (&statusCode));

        if (stream == nullptr)
        {
            out.errorMessage = "Network error contacting " + functionName;
            return out;
        }

        const auto responseText = stream->readEntireStreamAsString();
        const auto parsed = juce::JSON::parse (responseText);

        if (parsed.isObject() && parsed.hasProperty ("error"))
        {
            auto err = parsed.getProperty ("error", {});
            out.errorMessage = err.getProperty ("message", "Request failed").toString();
            return out;
        }

        if (statusCode >= 200 && statusCode < 300 && parsed.isObject())
        {
            out.ok = true;
            out.result = parsed.getProperty ("result", {});
            return out;
        }

        out.errorMessage = "Unexpected response (HTTP " + juce::String (statusCode) + ") from " + functionName;
        return out;
    }
}

//==============================================================================
MetadataQuotaService& MetadataQuotaService::getInstance()
{
    static MetadataQuotaService instance;
    return instance;
}

void MetadataQuotaService::getStatus (Callback onDone)
{
    juce::Thread::launch ([onDone]()
    {
        const auto call = callCloudFunction ("getMetadataQuotaStatus");

        Status s;
        s.ok = call.ok;
        s.errorMessage = call.errorMessage;

        if (call.ok)
        {
            s.usedCalls = (int) call.result.getProperty ("usedCalls", 0);
            s.cap       = (int) call.result.getProperty ("cap", 1000);
            s.remaining = (int) call.result.getProperty ("remaining", 0);
        }

        if (onDone)
            juce::MessageManager::callAsync ([onDone, s] { onDone (s); });
    });
}
