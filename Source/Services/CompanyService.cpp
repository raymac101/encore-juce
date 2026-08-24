/*
  ==============================================================================

    CompanyService.cpp

  ==============================================================================
*/

#include "CompanyService.h"
#include "FirestoreClient.h"

namespace
{
    using FC = FirestoreClient;

    juce::String docIdFromName(const juce::String& docName)
    {
        return docName.fromLastOccurrenceOf("/", false, false);
    }

    juce::var fieldsFromCompany(const Company& c)
    {
        return FC::makeFields({
            { "name",        FC::stringValue(juce::String(c.name)) },
            { "status",      FC::stringValue(juce::String(c.status)) },
            { "ownerUserId", FC::stringValue(juce::String(c.ownerUserId)) },
            { "address",     FC::stringValue(juce::String(c.address)) },
            { "city",        FC::stringValue(juce::String(c.city)) },
            { "country",     FC::stringValue(juce::String(c.country)) },
            { "logoUrl",     FC::stringValue(juce::String(c.logoUrl)) },
            { "updatedAt",   FC::timestampValue(juce::Time::getCurrentTime()) }
        });
    }

    //--- Firestore queries (mirrors LoginFlowController.cpp's file-local helpers) ---
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

    juce::var buildCollectionGroupQuery(const juce::String& collection, juce::var where)
    {
        juce::DynamicObject::Ptr fc = new juce::DynamicObject();
        fc->setProperty("collectionId", collection);
        fc->setProperty("allDescendants", true);
        juce::Array<juce::var> from;
        from.add(juce::var(fc.get()));

        juce::DynamicObject::Ptr q = new juce::DynamicObject();
        q->setProperty("from", from);
        if (! where.isVoid())
            q->setProperty("where", where);
        return juce::var(q.get());
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
}

//==============================================================================
CompanyService& CompanyService::getInstance()
{
    static CompanyService instance;
    return instance;
}

void CompanyService::createCompany(const Company& company, CreateCallback onDone)
{
    if (company.name.empty() || company.ownerUserId.empty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, {}, "Missing name/owner"); });
        return;
    }

    Company c = company;
    juce::Thread::launch([c, onDone = std::move(onDone)]()
    {
        auto fields = fieldsFromCompany(c);
        bool ok = false;
        auto resp = FC::getInstance().createDocument("companies", fields, {}, &ok);
        const auto newId = ok ? docIdFromName(resp.getProperty("name", "").toString())
                              : juce::String();
        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, ok, newId]()
            {
                onDone(ok, newId, ok ? juce::String() : juce::String("createDocument failed"));
            });
        }
    });
}

void CompanyService::addCompanyMember(const juce::String& companyId,
                                      const juce::String& userId,
                                      const juce::String& role,
                                      WriteCallback onDone)
{
    if (companyId.isEmpty() || userId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Missing companyId/userId"); });
        return;
    }

    juce::Thread::launch([companyId, userId, role, onDone = std::move(onDone)]()
    {
        auto fields = FC::makeFields({
            { "userId",    FC::stringValue(userId) },
            { "role",      FC::stringValue(role) },
            { "status",    FC::stringValue("active") },
            { "updatedAt", FC::timestampValue(juce::Time::getCurrentTime()) },
            { "updatedBy", FC::stringValue(FC::getInstance().getUserId()) }
        });

        const bool ok = FC::getInstance().createDocument(
            "companies/" + companyId + "/members", fields, userId).isObject();

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("createDocument failed")); });
    });
}

void CompanyService::findMembershipForUser(const juce::String& userId, MembershipCallback onDone)
{
    if (userId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, {}, {}); });
        return;
    }

    juce::Thread::launch([userId, onDone = std::move(onDone)]()
    {
        auto query = buildCollectionGroupQuery("members",
            stringFilter("userId", "EQUAL", FC::stringValue(userId)));

        auto docs = FC::getInstance().runQuery({}, query);

        juce::String companyId, role;
        bool found = false;
        if (! docs.isEmpty())
        {
            const auto& d = docs.getReference(0);
            const auto name = d.getProperty("name", "").toString();
            // .../companies/{companyId}/members/{uid} -- companyId is the
            // third-from-last path segment.
            auto segments = juce::StringArray::fromTokens(name, "/", "");
            if (segments.size() >= 3)
                companyId = segments[segments.size() - 3];

            role  = FC::readString(d, "role");
            found = companyId.isNotEmpty();
        }

        if (onDone)
            juce::MessageManager::callAsync([onDone, found, companyId, role]()
                { onDone(found, companyId, role); });
    });
}

void CompanyService::getVenueMembers(const juce::String& venueId, VenueMembersCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(true, {}, {}); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        auto query = buildQuery("user-venue-lookup",
            stringFilter("venueId", "EQUAL", FC::stringValue(venueId)));
        auto docs = FC::getInstance().runQuery({}, query);

        std::vector<VenueMember> out;
        out.reserve((size_t) docs.size());
        for (auto& d : docs)
        {
            VenueMember m;
            m.userId = FC::readString(d, "userId");
            m.role   = FC::readString(d, "role");
            m.status = FC::readString(d, "status");
            if (m.userId.isEmpty())
                continue;

            // Best-effort enrichment -- small N per venue, matches the N+1
            // style already used elsewhere in this codebase (e.g. the
            // venue-enabled check in LoginFlowController).
            auto hostDoc = FC::getInstance().getDocument("hosts/" + m.userId);
            m.email     = FC::readString(hostDoc, "email");
            m.stageName = FC::readString(hostDoc, "stageName");

            out.push_back(std::move(m));
        }

        if (onDone)
            juce::MessageManager::callAsync([onDone, out = std::move(out)]() mutable
                { onDone(true, std::move(out), {}); });
    });
}

void CompanyService::removeVenueMember(const juce::String& venueId, const juce::String& userId, RemoveVenueMemberCallback onDone)
{
    if (venueId.isEmpty() || userId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Missing venueId/userId"); });
        return;
    }

    juce::Thread::launch([venueId, userId, onDone = std::move(onDone)]()
    {
        const bool ok = FC::getInstance().deleteDocument("user-venue-lookup/" + userId + "_" + venueId);
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("deleteDocument failed")); });
    });
}
