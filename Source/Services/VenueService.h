/*
  ==============================================================================

    VenueService.h

    Loads + caches the active VenueItem from Firestore. All network calls run
    on a background thread; callbacks are dispatched on the message thread so
    UI components can safely repaint.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/VenueItem.h"

class VenueService
{
public:
    static VenueService& getInstance();

    using LoadCallback = std::function<void(bool ok, VenueItem venue, juce::String error)>;

    /** Asynchronously load `venues/<venueId>` from Firestore. The callback
        fires on the message thread. The loaded venue is cached and made
        available via getCurrent(). */
    void loadVenue(const juce::String& venueId, LoadCallback onDone);

    /** Returns the most recently loaded venue (may be empty / invalid if
        none has been loaded yet). */
    VenueItem getCurrent() const;

    /** Returns the venueId of the currently loaded venue, or empty. */
    juce::String getCurrentVenueId() const;

    using SaveCallback = std::function<void(bool ok, juce::String error)>;

    /** PATCH the editable fields of `venues/<venueId>` from the given
        VenueItem. Only the fields the settings page can actually mutate are
        sent (name, address, city, country, code, codePlus, queue rules,
        display flags, logoUrl, background) — other fields on the document
        are left untouched. Updates the cache on success. Callback fires on
        the message thread. */
    void saveVenue(const juce::String& venueId,
                   const VenueItem& venue,
                   SaveCallback onDone);

    void clear();

private:
    VenueService() = default;

    mutable juce::CriticalSection lock_;
    VenueItem current_;
    juce::String currentId_;

    JUCE_DECLARE_NON_COPYABLE(VenueService)
};
