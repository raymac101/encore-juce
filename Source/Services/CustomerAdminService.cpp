/*
  ==============================================================================

    CustomerAdminService.cpp

  ==============================================================================
*/

#include "CustomerAdminService.h"
#include "FirestoreClient.h"
#include "../Firebase/FirebaseConfig.h"

namespace
{
    CustomerAdminService::HostSummary hostSummaryFromVar(const juce::var& v)
    {
        CustomerAdminService::HostSummary h;
        h.userId       = v.getProperty("userId", "").toString();
        h.email        = v.getProperty("email", "").toString();
        h.stageName    = v.getProperty("stageName", "").toString();
        h.fullName     = v.getProperty("fullName", "").toString();
        h.country      = v.getProperty("country", "").toString();
        h.city         = v.getProperty("city", "").toString();
        h.signUpDate   = v.getProperty("signUpDate", "").toString();
        h.lastLogin    = v.getProperty("lastLogin", "").toString();
        h.role         = v.getProperty("role", "").toString();
        h.accountStatus = v.getProperty("accountStatus", "active").toString();
        h.loginCount   = (int) v.getProperty("loginCount", 0);
        h.authOnly     = (bool) v.getProperty("authOnly", false);
        return h;
    }

    std::vector<CustomerAdminService::HostSummary> hostSummariesFromResult(const juce::var& result)
    {
        std::vector<CustomerAdminService::HostSummary> out;
        auto results = result.getProperty("results", {});
        if (auto* arr = results.getArray())
            for (auto& item : *arr)
                out.push_back(hostSummaryFromVar(item));
        return out;
    }
}

//==============================================================================
CustomerAdminService& CustomerAdminService::getInstance()
{
    static CustomerAdminService instance;
    return instance;
}

juce::String CustomerAdminService::functionUrl(const juce::String& name)
{
    return "https://us-central1-" + FirebaseConfig::projectId + ".cloudfunctions.net/" + name;
}

CustomerAdminService::CallResult CustomerAdminService::callCloudFunction(const juce::String& functionName,
                                                                          const juce::var& dataObj)
{
    CallResult out;

    juce::DynamicObject::Ptr bodyObj = new juce::DynamicObject();
    bodyObj->setProperty("data", dataObj);
    const auto body = juce::JSON::toString(juce::var(bodyObj.get()), false);

    // getFreshIdToken() (not getIdToken()) -- refreshes first if the
    // cached token is close to expiry. Without this, a long-idle session
    // sends a stale token and every call here fails as 401 unauthenticated.
    const auto idToken = FirestoreClient::getInstance().getFreshIdToken();

    juce::URL url(functionUrl(functionName));
    int statusCode = 0;
    auto stream = url.withPOSTData(body).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
            .withConnectionTimeoutMs(15000)
            .withHttpRequestCmd("POST")
            .withExtraHeaders("Content-Type: application/json\r\n"
                              "Accept: application/json\r\n"
                              "Authorization: Bearer " + idToken)
            .withStatusCode(&statusCode));

    if (stream == nullptr)
    {
        out.errorMessage = "Network error contacting " + functionName;
        return out;
    }

    const auto responseText = stream->readEntireStreamAsString();
    const auto parsed = juce::JSON::parse(responseText);

    if (parsed.isObject() && parsed.hasProperty("error"))
    {
        auto err = parsed.getProperty("error", {});
        out.errorMessage = err.getProperty("message", "Request failed").toString();
        return out;
    }

    if (statusCode >= 200 && statusCode < 300 && parsed.isObject())
    {
        out.ok = true;
        out.result = parsed.getProperty("result", {});
        return out;
    }

    out.errorMessage = "Unexpected response (HTTP " + juce::String(statusCode) + ") from " + functionName;
    return out;
}

//==============================================================================
void CustomerAdminService::searchUsers(const juce::String& query, const juce::String& cursor, SearchCallback onDone)
{
    juce::Thread::launch([query, cursor, onDone]
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("query", query);
        if (cursor.isNotEmpty())
            data->setProperty("cursor", cursor);

        auto res = callCloudFunction("adminSearchUsers", juce::var(data.get()));

        juce::MessageManager::callAsync([onDone, res]
        {
            if (! onDone) return;
            if (! res.ok) { onDone(false, {}, {}, res.errorMessage); return; }
            onDone(true, hostSummariesFromResult(res.result),
                   res.result.getProperty("nextCursor", "").toString(), {});
        });
    });
}

void CustomerAdminService::listUnassignedHosts(const juce::String& cursor, SearchCallback onDone)
{
    juce::Thread::launch([cursor, onDone]
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        if (cursor.isNotEmpty())
            data->setProperty("cursor", cursor);

        auto res = callCloudFunction("adminListUnassignedHosts", juce::var(data.get()));

        juce::MessageManager::callAsync([onDone, res]
        {
            if (! onDone) return;
            if (! res.ok) { onDone(false, {}, {}, res.errorMessage); return; }
            onDone(true, hostSummariesFromResult(res.result),
                   res.result.getProperty("nextCursor", "").toString(), {});
        });
    });
}

void CustomerAdminService::listVenues(VenuesCallback onDone)
{
    juce::Thread::launch([onDone]
    {
        auto res = callCloudFunction("adminListVenues", juce::var (new juce::DynamicObject()));

        juce::MessageManager::callAsync([onDone, res]
        {
            if (! onDone) return;
            if (! res.ok) { onDone(false, {}, res.errorMessage); return; }

            std::vector<CustomerAdminService::VenueSummary> venues;
            auto results = res.result.getProperty ("results", {});
            if (auto* arr = results.getArray())
            {
                for (auto& item : *arr)
                {
                    VenueSummary v;
                    v.id          = item.getProperty ("id", "").toString();
                    v.name        = item.getProperty ("name", "").toString();
                    v.address     = item.getProperty ("address", "").toString();
                    v.city        = item.getProperty ("city", "").toString();
                    v.country     = item.getProperty ("country", "").toString();
                    v.code        = item.getProperty ("code", "").toString();
                    v.codePlus    = item.getProperty ("codePlus", "").toString();
                    v.adminEmail  = item.getProperty ("adminEmail", "").toString();
                    v.numSongs    = (int)  item.getProperty ("numSongs", 0);
                    v.numSingers  = (int)  item.getProperty ("numSingers", 0);
                    v.numStrikes  = (int)  item.getProperty ("numStrikes", 0);
                    v.repeatSongs = (bool) item.getProperty ("repeatSongs", false);
                    v.autoapprove = (bool) item.getProperty ("autoapprove", false);
                    venues.push_back (v);
                }
            }
            onDone(true, venues, {});
        });
    });
}

void CustomerAdminService::deleteVenue (const juce::String& venueId, const juce::String& confirmName, WriteCallback onDone)
{
    juce::Thread::launch([venueId, confirmName, onDone]
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty ("venueId", venueId);
        data->setProperty ("confirmName", confirmName);

        auto res = callCloudFunction ("adminDeleteVenue", juce::var (data.get()));
        juce::MessageManager::callAsync([onDone, res] { if (onDone) onDone (res.ok, res.errorMessage); });
    });
}

void CustomerAdminService::getUserProfile(const juce::String& uid, ProfileCallback onDone)
{
    juce::Thread::launch([uid, onDone]
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("uid", uid);

        auto res = callCloudFunction("adminGetUserProfile", juce::var(data.get()));

        UserProfile profile;
        if (! res.ok)
        {
            profile.error = res.errorMessage;
        }
        else
        {
            profile.ok = true;
            profile.host = hostSummaryFromVar(res.result.getProperty("host", {}));

            auto legacy = res.result.getProperty("legacyProfile", {});
            if (! legacy.isVoid() && ! legacy.isUndefined())
            {
                profile.hasLegacyProfile = true;
                profile.legacyProfileRaw = legacy;
            }

            auto venues = res.result.getProperty("venues", {});
            if (auto* arr = venues.getArray())
            {
                for (auto& v : *arr)
                {
                    VenueAssociation a;
                    a.id        = v.getProperty("id", "").toString();
                    a.venueId   = v.getProperty("venueId", "").toString();
                    a.venueName = v.getProperty("venueName", "").toString();
                    a.venueCity = v.getProperty("venueCity", "").toString();
                    a.role      = v.getProperty("role", "").toString();
                    profile.venues.push_back(a);
                }
            }

            auto auth = res.result.getProperty("auth", {});
            if (auth.isObject())
            {
                profile.hasAuthRecord = true;
                profile.authDisabled  = (bool) auth.getProperty("disabled", false);
                profile.authLastSignInTime = auth.getProperty("lastSignInTime", "").toString();
                profile.authCreationTime   = auth.getProperty("creationTime", "").toString();
                auto providers = auth.getProperty("providers", {});
                if (auto* pArr = providers.getArray())
                    for (auto& p : *pArr)
                        profile.authProviders.add(p.toString());
            }
        }

        juce::MessageManager::callAsync([onDone, profile] { if (onDone) onDone(profile); });
    });
}

void CustomerAdminService::assignVenueRole(const juce::String& uid,
                                           const juce::String& venueId,
                                           const juce::String& role,
                                           const juce::String& venueName,
                                           const juce::String& venueCity,
                                           const juce::String& userEmail,
                                           WriteCallback onDone)
{
    juce::Thread::launch([uid, venueId, role, venueName, venueCity, userEmail, onDone]
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("uid", uid);
        data->setProperty("venueId", venueId);
        data->setProperty("role", role);
        data->setProperty("venueName", venueName);
        data->setProperty("venueCity", venueCity);
        data->setProperty("userEmail", userEmail);

        auto res = callCloudFunction("adminAssignVenueRole", juce::var(data.get()));
        juce::MessageManager::callAsync([onDone, res] { if (onDone) onDone(res.ok, res.errorMessage); });
    });
}

void CustomerAdminService::setUserPassword(const juce::String& uid, const juce::String& newPassword, WriteCallback onDone)
{
    juce::Thread::launch([uid, newPassword, onDone]
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("uid", uid);
        data->setProperty("newPassword", newPassword);

        auto res = callCloudFunction("adminSetUserPassword", juce::var(data.get()));
        juce::MessageManager::callAsync([onDone, res] { if (onDone) onDone(res.ok, res.errorMessage); });
    });
}

void CustomerAdminService::deactivateUser(const juce::String& uid, WriteCallback onDone)
{
    juce::Thread::launch([uid, onDone]
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("uid", uid);

        auto res = callCloudFunction("adminDeactivateUser", juce::var(data.get()));
        juce::MessageManager::callAsync([onDone, res] { if (onDone) onDone(res.ok, res.errorMessage); });
    });
}

void CustomerAdminService::reactivateUser(const juce::String& uid, WriteCallback onDone)
{
    juce::Thread::launch([uid, onDone]
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("uid", uid);

        auto res = callCloudFunction("adminReactivateUser", juce::var(data.get()));
        juce::MessageManager::callAsync([onDone, res] { if (onDone) onDone(res.ok, res.errorMessage); });
    });
}

void CustomerAdminService::hardDeleteUser(const juce::String& uid, const juce::String& confirmEmail, WriteCallback onDone)
{
    juce::Thread::launch([uid, confirmEmail, onDone]
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("uid", uid);
        data->setProperty("confirmEmail", confirmEmail);

        auto res = callCloudFunction("adminHardDeleteUser", juce::var(data.get()));
        juce::MessageManager::callAsync([onDone, res] { if (onDone) onDone(res.ok, res.errorMessage); });
    });
}

void CustomerAdminService::sendPasswordResetEmail(const juce::String& email, WriteCallback onDone)
{
    juce::Thread::launch([email, onDone]
    {
        auto result = FirestoreClient::getInstance().sendPasswordResetEmail(email);
        juce::MessageManager::callAsync([onDone, result]
        {
            if (onDone) onDone(result.ok, result.errorMessage);
        });
    });
}
