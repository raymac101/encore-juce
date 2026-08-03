/*
  ==============================================================================

    InvitationService.cpp

  ==============================================================================
*/

#include "InvitationService.h"
#include "FirestoreClient.h"

namespace
{
    using FC = FirestoreClient;

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

void InvitationService::acceptInvitation(const VenueInvitation& invitation,
                                         const juce::String& uid,
                                         WriteCallback onDone)
{
    if (invitation.venueId.isEmpty() || uid.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Missing venueId/uid"); });
        return;
    }

    const VenueInvitation inv = invitation;
    juce::Thread::launch([inv, uid, onDone = std::move(onDone)]()
    {
        // 1) Grant the association — doc id `{uid}_{venueId}` matches the
        //    convention LoginFlowController::queryAssociations() reads.
        const auto now = juce::Time::getCurrentTime();
        auto assocFields = FC::makeFields({
            { "userId",    FC::stringValue(uid) },
            { "venueId",   FC::stringValue(inv.venueId) },
            { "venueName", FC::stringValue(inv.venueName) },
            { "role",      FC::stringValue(juce::String(AccessRightsUtil::userRoleToString(inv.role))) },
            { "status",    FC::stringValue("active") },
            { "joinedDate",FC::timestampValue(now) },
            { "lastActive",FC::timestampValue(now) }
        });
        const auto assocDocId = uid + "_" + inv.venueId;
        bool assocOk = false;
        FC::getInstance().createDocument("user-venue-lookup", assocFields, assocDocId, &assocOk);

        if (! assocOk)
        {
            if (onDone) juce::MessageManager::callAsync([onDone]
                { onDone(false, "Could not create venue association"); });
            return;
        }

        // 2) Patch the invitation doc — isAccepted / acceptedDate.
        if (inv.id.isNotEmpty())
        {
            auto patchFields = FC::makeFields({
                { "isAccepted",   FC::booleanValue(true) },
                { "acceptedDate", FC::timestampValue(now) }
            });
            const auto path = "venueInvitations/" + inv.id
                             + "?updateMask.fieldPaths=isAccepted&updateMask.fieldPaths=acceptedDate";
            FC::getInstance().patchDocument(path, patchFields);
        }

        if (onDone)
            juce::MessageManager::callAsync([onDone] { onDone(true, {}); });
    });
}
