/*
  ==============================================================================

    LoginFlowController.cpp

  ==============================================================================
*/

#include "LoginFlowController.h"
#include "../Services/UserPreferences.h"

namespace
{
    using FC = FirestoreClient;

    //--- Firestore queries -----------------------------------------------------
    juce::var stringFilter(const juce::String& fieldPath, const juce::String& op, const juce::var& value)
    {
        // { fieldFilter: { field: { fieldPath }, op, value } }
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

    juce::var fromCollection(const juce::String& collection)
    {
        juce::DynamicObject::Ptr fc = new juce::DynamicObject();
        fc->setProperty("collectionId", collection);

        juce::Array<juce::var> arr;
        arr.add(juce::var(fc.get()));
        return arr;
    }

    juce::var buildQuery(const juce::String& collection, juce::var where)
    {
        juce::DynamicObject::Ptr q = new juce::DynamicObject();
        q->setProperty("from", fromCollection(collection));
        if (! where.isVoid())
            q->setProperty("where", where);
        return juce::var(q.get());
    }

    //--- Host loading / creation ---------------------------------------------
    Host hostFromDoc(const juce::var& doc, const juce::String& uid)
    {
        Host h;
        h.userId    = FC::readString(doc, "userId").toStdString();
        if (h.userId.empty()) h.userId = uid.toStdString();
        h.email     = FC::readString(doc, "email").toStdString();
        h.profileId = FC::readString(doc, "profileId").toStdString();
        h.avatarUrl = FC::readString(doc, "avatarUrl").toStdString();
        h.stageName = FC::readString(doc, "stageName").toStdString();
        h.fullName  = FC::readString(doc, "fullName").toStdString();
        h.country   = FC::readString(doc, "country").toStdString();
        h.city      = FC::readString(doc, "city").toStdString();
        h.gender    = FC::readString(doc, "gender").toStdString();
        h.birthday  = FC::readString(doc, "birthday").toStdString();
        h.signUpDate = FC::readString(doc, "signUpDate").toStdString();
        h.lastLogin  = FC::readString(doc, "lastLogin").toStdString();
        auto roleStr = FC::readString(doc, "role").toStdString();
        if (! roleStr.empty())
            h.role = AccessRightsUtil::stringToUserRole(roleStr);
        return h;
    }

    bool isFirstHostEver()
    {
        // Cheap probe: list `hosts` with a 1-document page.
        auto docs = FC::getInstance().listCollection("hosts", 1);
        return docs.isEmpty();
    }

    Host loadOrCreateHost(const juce::String& uid, const juce::String& email)
    {
        auto path = "hosts/" + uid;
        int status = 0;
        auto doc = FC::getInstance().getDocument(path, &status);

        if (status == 200 && doc.hasProperty("fields"))
            return hostFromDoc(doc, uid);

        // No host doc — create one. First user ever ⇒ EnterpriseAdmin.
        const bool first = isFirstHostEver();
        const UserRole role = first ? UserRole::EnterpriseAdmin : UserRole::Host;

        auto now = juce::Time::getCurrentTime();
        juce::Time expiry(now.toMilliseconds() + (juce::int64) 365 * 24 * 60 * 60 * 1000);

        auto fields = FC::makeFields({
            { "userId",     FC::stringValue(uid) },
            { "email",      FC::stringValue(email) },
            { "profileId",  FC::stringValue("") },
            { "avatarUrl",  FC::stringValue("assets/images/AvatarWhite.png") },
            { "stageName",  FC::stringValue("Anonymous") },
            { "fullName",   FC::stringValue(FC::getInstance().getDisplayName()) },
            { "birthday",   FC::stringValue("") },
            { "country",    FC::stringValue("") },
            { "city",       FC::stringValue("") },
            { "gender",     FC::stringValue("") },
            { "signUpDate", FC::stringValue(now.toISO8601(true)) },
            { "lastLogin",  FC::stringValue(now.toISO8601(true)) },
            { "role",       FC::stringValue(juce::String(AccessRightsUtil::userRoleToString(role))) },
            { "accessExpirationDate", FC::timestampValue(expiry) }
        });

        // Use uid as the document id so future getDocument(hosts/<uid>) works.
        FC::getInstance().createDocument("hosts", fields, uid);

        Host h;
        h.userId    = uid.toStdString();
        h.email     = email.toStdString();
        h.role      = role;
        h.stageName = "Anonymous";
        h.signUpDate = now.toISO8601(true).toStdString();
        h.lastLogin  = now.toISO8601(true).toStdString();
        return h;
    }

    //--- Associations / invitations -------------------------------------------
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

    std::vector<UserVenueAssociation> queryAssociations(const juce::String& uid)
    {
        // Mirrors VenueService.getVenuesForUser() in the Angular app:
        // query the `user-venue-lookup` collection, filtered by userId &
        // status='active'. Doc id is `${userId}_${venueId}`.
        juce::Array<juce::var> filters;
        filters.add(stringFilter("userId", "EQUAL", FC::stringValue(uid)));
        filters.add(stringFilter("status", "EQUAL", FC::stringValue("active")));
        auto query = buildQuery("user-venue-lookup", compositeAnd(filters));

        std::vector<UserVenueAssociation> out;
        for (auto& d : FC::getInstance().runQuery({}, query))
        {
            UserVenueAssociation a;
            auto name = d.getProperty("name", "").toString();
            a.id = name.fromLastOccurrenceOf("/", false, false);
            a.userId    = FC::readString(d, "userId");
            a.venueId   = FC::readString(d, "venueId");
            a.venueName = FC::readString(d, "venueName");
            a.role      = AccessRightsUtil::stringToUserRole(FC::readString(d, "role").toStdString());
            a.isActive  = (FC::readString(d, "status") == "active");
            a.acceptedDate   = FC::readTime(d, "joinedDate");
            a.lastAccessDate = FC::readTime(d, "lastActive");
            out.push_back(std::move(a));
        }
        return out;
    }

    std::vector<VenueInvitation> queryPendingInvitations(const juce::String& email)
    {
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

    void touchLastAccess(const juce::String& venueId, const juce::String& uid)
    {
        // user-venue-lookup uses a deterministic doc id of `${uid}_${venueId}`.
        const auto docId = uid + "_" + venueId;
        auto fields = FC::makeFields({
            { "lastActive", FC::timestampValue(juce::Time::getCurrentTime()) }
        });
        const auto path = "user-venue-lookup/" + docId
                        + "?updateMask.fieldPaths=lastActive";
        FC::getInstance().patchDocument(path, fields);
    }

    //--- Result dispatch -------------------------------------------------------
    void postOnMessageThread(std::function<void()> fn)
    {
        juce::MessageManager::callAsync(std::move(fn));
    }
}

//==============================================================================
void LoginFlowController::runPostAuthFlow(ResultCallback onResult, ErrorCallback onError)
{
    juce::Thread::launch([onResult = std::move(onResult), onError = std::move(onError)]()
    {
        try
        {
            auto& fc = FirestoreClient::getInstance();
            if (! fc.isSignedIn())
            {
                postOnMessageThread([onError]() { onError("Not signed in"); });
                return;
            }

            const auto uid   = fc.getUserId();
            const auto email = fc.getEmail().toLowerCase();

            // 1) Host bootstrap
            Host host = loadOrCreateHost(uid, email);

            // 2) Read stored venueId from prefs
            const auto storedVenueId = UserPreferences::getInstance().getVenueId();

            // 3) Associations + pending invitations
            auto associations = queryAssociations(uid);
            auto invitations  = queryPendingInvitations(email);

            const bool canCreate = (host.role == UserRole::Admin
                                  || host.role == UserRole::EnterpriseAdmin
                                  || host.role == UserRole::Tester);

            DBG("[LoginFlow] storedVenueId=" << storedVenueId
                << " associations=" << (int) associations.size()
                << " invitations="  << (int) invitations.size()
                << " role=" << juce::String(AccessRightsUtil::userRoleToString(host.role)));

            Result result;
            result.host              = host;
            result.canCreateVenue    = canCreate;
            result.configuredVenueId = storedVenueId;

            // 4) Apply the scenario tree (mirrors Angular start.component logic)
            //    - Multiple associations → ALWAYS show picker (the configured
            //      venue is just badged in the list).
            //    - Single association → auto-load it.
            //    - Zero associations → request access (if stored venueId set
            //      and user isn't an admin) / await invitation otherwise.
            if (associations.size() > 1)
            {
                result.outcome      = Outcome::PickVenue;
                result.associations = std::move(associations);
            }
            else if (associations.size() == 1)
            {
                UserPreferences::getInstance().setVenueId(associations[0].venueId);
                touchLastAccess(associations[0].venueId, uid);
                result.outcome = Outcome::VenueLoaded;
                result.venueId = associations[0].venueId;
            }
            else if (storedVenueId.isNotEmpty())
            {
                const bool adminOverride =
                    (host.role == UserRole::EnterpriseAdmin || host.role == UserRole::Tester);

                if (adminOverride)
                {
                    touchLastAccess(storedVenueId, uid);
                    result.outcome = Outcome::VenueLoaded;
                    result.venueId = storedVenueId;
                }
                else
                {
                    result.outcome = Outcome::RequestAccess;
                    result.venueId = storedVenueId;
                }
            }
            else
            {
                result.outcome     = Outcome::AwaitInvitation;
                result.invitations = std::move(invitations);
            }

            postOnMessageThread([onResult, result = std::move(result)]() mutable
            {
                onResult(std::move(result));
            });
        }
        catch (const std::exception& e)
        {
            const juce::String msg(e.what());
            postOnMessageThread([onError, msg]() { onError(msg); });
        }
        catch (...)
        {
            postOnMessageThread([onError]() { onError("Unexpected error in login flow"); });
        }
    });
}

void LoginFlowController::selectVenue(const juce::String& venueId,
                                      std::function<void()> onDone)
{
    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        UserPreferences::getInstance().setVenueId(venueId);
        const auto uid = FirestoreClient::getInstance().getUserId();
        if (uid.isNotEmpty())
            touchLastAccess(venueId, uid);

        if (onDone)
            postOnMessageThread(onDone);
    });
}

void LoginFlowController::requestVenueAccess(const juce::String& venueId,
                                             const juce::String& venueName,
                                             const juce::String& message,
                                             std::function<void(bool, juce::String)> onDone)
{
    juce::Thread::launch([venueId, venueName, message, onDone = std::move(onDone)]()
    {
        auto& fc = FirestoreClient::getInstance();
        const auto uid    = fc.getUserId();
        const auto email  = fc.getEmail().toLowerCase();
        const auto display = fc.getDisplayName().isEmpty() ? email : fc.getDisplayName();

        // Avoid duplicates
        juce::Array<juce::var> filters;
        filters.add(stringFilter("venueId",          "EQUAL", FC::stringValue(venueId)));
        filters.add(stringFilter("requestedByUserId","EQUAL", FC::stringValue(uid)));
        filters.add(stringFilter("status",           "EQUAL", FC::stringValue("pending")));
        auto query = buildQuery("venueJoinRequests", compositeAnd(filters));

        if (! fc.runQuery({}, query).isEmpty())
        {
            postOnMessageThread([onDone]() { if (onDone) onDone(true, "Request already pending"); });
            return;
        }

        auto now = juce::Time::getCurrentTime();
        juce::Time expires(now.toMilliseconds() + (juce::int64) 7 * 24 * 60 * 60 * 1000);

        auto fields = FC::makeFields({
            { "venueId",            FC::stringValue(venueId) },
            { "venueName",          FC::stringValue(venueName) },
            { "requestedByUserId",  FC::stringValue(uid) },
            { "requestedByEmail",   FC::stringValue(email) },
            { "requestedByName",    FC::stringValue(display) },
            { "message",            FC::stringValue(message) },
            { "status",             FC::stringValue("pending") },
            { "requestDate",        FC::timestampValue(now) },
            { "expirationDate",     FC::timestampValue(expires) }
        });

        auto created = fc.createDocument("venueJoinRequests", fields);
        const bool ok = created.isObject() && created.hasProperty("name");
        postOnMessageThread([onDone, ok]()
        {
            if (onDone) onDone(ok, ok ? juce::String() : "Could not send the request");
        });
    });
}
