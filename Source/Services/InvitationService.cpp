/*
  ==============================================================================

    InvitationService.cpp

  ==============================================================================
*/

#include "InvitationService.h"
#include "FirestoreClient.h"
#include "../Firebase/FirebaseConfig.h"

namespace
{
    using FC = FirestoreClient;

    //--- Cloud Function calling -- small local copy of the same pattern
    //    CustomerAdminService uses (its own version is private to that
    //    class); not worth a shared header for ~40 lines. ---------------
    struct CallResult
    {
        bool         ok = false;
        juce::var    result;
        juce::String errorMessage;
    };

    juce::String functionUrl(const juce::String& name)
    {
        return "https://us-central1-" + FirebaseConfig::projectId + ".cloudfunctions.net/" + name;
    }

    CallResult callCloudFunction(const juce::String& functionName, const juce::var& dataObj)
    {
        CallResult out;

        juce::DynamicObject::Ptr bodyObj = new juce::DynamicObject();
        bodyObj->setProperty("data", dataObj);
        const auto body = juce::JSON::toString(juce::var(bodyObj.get()), false);

        // getFreshIdToken() (not getIdToken()) -- refreshes first if the
        // cached token is close to expiry.
        const auto idToken = FC::getInstance().getFreshIdToken();

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

    //--- Firestore structured-query builders (small local copies — mirrors
    //    the equivalent private helpers in LoginFlowController.cpp; not
    //    worth sharing a header for ~15 lines of generic filter-building). ---
    juce::var stringFilter(const juce::String& fieldPath, const juce::String& op, const juce::var& value)
    {
        juce::DynamicObject::Ptr field = new juce::DynamicObject();
        field->setProperty("fieldPath", fieldPath);

        juce::DynamicObject::Ptr filter = new juce::DynamicObject();
        filter->setProperty("field", juce::var(field.get()));
        filter->setProperty("op", op);
        filter->setProperty("value", value);

        juce::DynamicObject::Ptr wrap = new juce::DynamicObject();
        wrap->setProperty("fieldFilter", juce::var(filter.get()));
        return juce::var(wrap.get());
    }

    juce::var compositeAnd(juce::Array<juce::var> filters)
    {
        juce::DynamicObject::Ptr c = new juce::DynamicObject();
        c->setProperty("op", "AND");
        c->setProperty("filters", filters);

        juce::DynamicObject::Ptr wrap = new juce::DynamicObject();
        wrap->setProperty("compositeFilter", juce::var(c.get()));
        return juce::var(wrap.get());
    }

    juce::var buildQuery(const juce::String& collection, juce::var where)
    {
        juce::DynamicObject::Ptr fc = new juce::DynamicObject();
        fc->setProperty("collectionId", collection);
        juce::Array<juce::var> from;
        from.add(juce::var(fc.get()));

        juce::DynamicObject::Ptr q = new juce::DynamicObject();
        q->setProperty("from", from);
        if (! where.isVoid())
            q->setProperty("where", where);
        return juce::var(q.get());
    }

    VenueInvitation invitationFromDoc(const juce::var& doc)
    {
        VenueInvitation inv;
        auto name = doc.getProperty("name", "").toString();
        inv.id = name.fromLastOccurrenceOf("/", false, false);
        inv.venueId          = FC::readString(doc, "venueId");
        inv.venueName        = FC::readString(doc, "venueName");
        inv.invitedUserEmail = FC::readString(doc, "invitedUserEmail");
        inv.invitedByEmail   = FC::readString(doc, "invitedByEmail");
        inv.invitedByName    = FC::readString(doc, "invitedByName");
        inv.role = AccessRightsUtil::stringToUserRole(FC::readString(doc, "role").toStdString());
        inv.invitationDate   = FC::readTime(doc, "invitationDate");
        inv.expirationDate   = FC::readTime(doc, "expirationDate");
        inv.acceptedDate     = FC::readTime(doc, "acceptedDate");
        inv.isAccepted       = FC::readBool(doc, "isAccepted", false);
        inv.isExpired        = FC::readBool(doc, "isExpired", false);
        inv.notes = FC::readString(doc, "notes");
        return inv;
    }
}

//==============================================================================
InvitationService& InvitationService::getInstance()
{
    static InvitationService instance;
    return instance;
}

void InvitationService::addVenueMember(const juce::String& venueId, const juce::String& email,
                                       const juce::String& role, AddMemberCallback onDone)
{
    if (venueId.isEmpty() || email.isEmpty() || role.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, false, "venueId, email, and role are required"); });
        return;
    }

    juce::Thread::launch([venueId, email, role, onDone = std::move(onDone)]()
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("venueId", venueId);
        data->setProperty("email", email);
        data->setProperty("role", role);
        const auto res = callCloudFunction("addVenueMember", juce::var(data.get()));

        const bool activated = res.ok && (bool) res.result.getProperty("activated", false);
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok = res.ok, activated, error = res.errorMessage]
                { onDone(ok, activated, error); });
    });
}

std::vector<VenueInvitation> InvitationService::queryPendingInvitationsSync(const juce::String& email)
{
    if (email.isEmpty())
        return {};

    juce::Array<juce::var> filters;
    filters.add(stringFilter("invitedUserEmail", "EQUAL", FC::stringValue(email.toLowerCase())));
    filters.add(stringFilter("isAccepted",       "EQUAL", FC::booleanValue(false)));
    filters.add(stringFilter("isExpired",        "EQUAL", FC::booleanValue(false)));
    auto query = buildQuery("venueInvitations", compositeAnd(filters));

    std::vector<VenueInvitation> out;
    for (auto& d : FC::getInstance().runQuery({}, query))
        out.push_back(invitationFromDoc(d));
    return out;
}

void InvitationService::findPendingInvitations(const juce::String& email, FindCallback onDone)
{
    juce::Thread::launch([email, onDone = std::move(onDone)]()
    {
        auto out = queryPendingInvitationsSync(email);
        if (onDone)
            juce::MessageManager::callAsync([onDone, out = std::move(out)]() mutable
                { onDone(std::move(out)); });
    });
}

bool InvitationService::acceptInvitationSync(const juce::String& invitationId, juce::String* outError)
{
    if (invitationId.isEmpty())
    {
        if (outError) *outError = "Missing invitationId";
        return false;
    }

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("invitationId", invitationId);
    const auto res = callCloudFunction("acceptVenueInvitation", juce::var(data.get()));

    if (! res.ok && outError)
        *outError = res.errorMessage;
    return res.ok;
}

void InvitationService::acceptInvitation(const VenueInvitation& invitation, WriteCallback onDone)
{
    const auto invitationId = invitation.id;
    juce::Thread::launch([invitationId, onDone = std::move(onDone)]()
    {
        juce::String error;
        const bool ok = acceptInvitationSync(invitationId, &error);
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok, error] { onDone(ok, error); });
    });
}

void InvitationService::claimAllPendingSync(const juce::String& email)
{
    for (auto& inv : queryPendingInvitationsSync(email))
        acceptInvitationSync(inv.id);
}
