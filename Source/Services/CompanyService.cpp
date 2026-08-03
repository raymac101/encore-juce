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
