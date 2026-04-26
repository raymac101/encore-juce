/*
  ==============================================================================

    VenueService.cpp

  ==============================================================================
*/

#include "VenueService.h"
#include "FirestoreClient.h"

VenueService& VenueService::getInstance()
{
    static VenueService instance;
    return instance;
}

void VenueService::loadVenue(const juce::String& venueId, LoadCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone)
            juce::MessageManager::callAsync([onDone] { onDone(false, {}, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        int status = 0;
        auto doc = FirestoreClient::getInstance().getDocument("venues/" + venueId, &status);

        if (status != 200 || ! doc.hasProperty("fields"))
        {
            if (onDone)
            {
                juce::MessageManager::callAsync([onDone, status]()
                {
                    onDone(false, {}, "Venue not found (HTTP " + juce::String(status) + ")");
                });
            }
            return;
        }

        // Convert Firestore { fields: { name: { stringValue: "..." } } } into
        // a flat object, then re-use VenueItem::fromJson which already knows
        // how to coerce string-encoded numbers ("3", "5") into ints.
        auto unwrapped = FirestoreClient::unwrapFields(doc);
        auto json = juce::JSON::toString(unwrapped);
        auto venue = VenueItem::fromJson(json);
        venue.id = venueId.toStdString();

        {
            // Cache.
            VenueService& self = VenueService::getInstance();
            const juce::ScopedLock sl(self.lock_);
            self.current_   = venue;
            self.currentId_ = venueId;
        }

        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, venue]() mutable
            {
                onDone(true, std::move(venue), {});
            });
        }
    });
}

VenueItem VenueService::getCurrent() const
{
    const juce::ScopedLock sl(lock_);
    return current_;
}

juce::String VenueService::getCurrentVenueId() const
{
    const juce::ScopedLock sl(lock_);
    return currentId_;
}

void VenueService::clear()
{
    const juce::ScopedLock sl(lock_);
    current_   = {};
    currentId_ = {};
}

//==============================================================================
namespace
{
    using FC = FirestoreClient;

    // Editable fields the settings page can change. Listed once so the body
    // payload and the updateMask stay in sync.
    static const juce::StringArray kEditableFields = {
        "name", "address", "city", "country",
        "code", "codePlus",
        "numSongs", "numSingers", "numStrikes",
        "repeatSongs", "autoapprove",
        "background", "encoreBackground", "encodeBackgroundSize",
        "showOnlineSongs", "showOnlineSongsEncore", "showMemoryStats",
        "logoUrl"
    };

    juce::var fieldsFromVenue(const VenueItem& v)
    {
        // Match the Angular schema: numSongs / numSingers / numStrikes /
        // background are stored as STRINGS in Firestore.
        return FC::makeFields({
            { "name",     FC::stringValue(juce::String(v.name)) },
            { "address",  FC::stringValue(juce::String(v.address)) },
            { "city",     FC::stringValue(juce::String(v.city)) },
            { "country",  FC::stringValue(juce::String(v.country)) },

            { "code",     FC::stringValue(juce::String(v.code)) },
            { "codePlus", FC::stringValue(juce::String(v.codePlus)) },

            { "numSongs",   FC::stringValue(juce::String(v.numSongs)) },
            { "numSingers", FC::stringValue(juce::String(v.numSingers)) },
            { "numStrikes", FC::stringValue(juce::String(v.numStrikes)) },

            { "repeatSongs", FC::booleanValue(v.repeatSongs) },
            { "autoapprove", FC::booleanValue(v.autoapprove) },

            { "background",           FC::stringValue(juce::String(v.background)) },
            { "encoreBackground",     FC::stringValue(juce::String(v.encoreBackground)) },
            { "encodeBackgroundSize", FC::stringValue(juce::String(v.encodeBackgroundSize)) },

            { "showOnlineSongs",       FC::booleanValue(v.showOnlineSongs) },
            { "showOnlineSongsEncore", FC::booleanValue(v.showOnlineSongsEncore) },
            { "showMemoryStats",       FC::booleanValue(v.showMemoryStats) },

            { "logoUrl", FC::stringValue(juce::String(v.logoUrl)) }
        });
    }
}

void VenueService::saveVenue(const juce::String& venueId,
                             const VenueItem& venue,
                             SaveCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone)
            juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, venue, onDone = std::move(onDone)]()
    {
        // Build path with an updateMask query string so only the editable
        // fields are written. Without this Firestore would clear any field
        // we don't include in the body.
        juce::StringArray maskParts;
        for (auto& f : kEditableFields)
            maskParts.add("updateMask.fieldPaths=" + f);

        const auto path = "venues/" + venueId + "?" + maskParts.joinIntoString("&");
        const auto fields = fieldsFromVenue(venue);

        const bool ok = FirestoreClient::getInstance().patchDocument(path, fields);

        if (ok)
        {
            VenueService& self = VenueService::getInstance();
            const juce::ScopedLock sl(self.lock_);
            self.current_   = venue;
            self.currentId_ = venueId;
        }

        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, ok]()
            {
                onDone(ok, ok ? juce::String() : juce::String("Save failed"));
            });
        }
    });
}
