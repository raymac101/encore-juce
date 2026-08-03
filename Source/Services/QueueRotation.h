/*
  ==============================================================================

    QueueRotation.h

    Pure, Firestore/UI-agnostic logic for the singer Round Robin (RR) and
    the derived Queue view.

    Model
    ─────
    The RR is a stable list: the host is always first and immovable, and
    singers are appended to the bottom the first time they add a song. The
    RR only changes via join, removal, or manual KJ drag-reorder -- never
    as a side effect of someone starting or finishing a performance.

    The Queue (what's actually shown as "up next") is not a separately
    stored list -- it's the RR sorted by stable order, rotated to start at
    whichever singer is the current "anchor" (who's up next). When the
    anchor singer finishes, the anchor simply advances to the next RR
    member, circularly (wrapping past the last member back to the host).

    Persistence
    ───────────
    `Singers::order` is the RR's stable position. `Singers::rotationOrder`
    is a derived cache of "rank in the current Queue view" (0 = anchor),
    restamped by stampDerivedRanks() whenever something changes; it also
    doubles as how the anchor is *recovered* after a reload (see
    findAnchorId) -- no separate persisted "anchor" field is needed.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include "../Models/Singers.h"

namespace QueueRotation
{
    /** Index of the host in `singers`, or -1 if none present. */
    int findHostIndex(const std::vector<Singers>& singers);

    /** Ascending by stable `order`, with the host always forced first
        regardless of its stored order value. Does not mutate `order`. */
    std::vector<Singers> sortByStableOrder(std::vector<Singers> singers);

    /** Stamps `order` = 0..N-1 by current vector position. `rr` must
        already be host-first (see sortByStableOrder). Call only after a
        genuine RR-structure change (join/remove/drag) -- never on a
        song finishing or starting. */
    void renumberStableOrder(std::vector<Singers>& rr);

    /** The current anchor's singer id: whoever has the smallest
        non-negative `rotationOrder`, ties broken by `order`. Falls back to
        the host (or the first entry, if no host is present) when nothing
        qualifies -- e.g. a brand-new venue or corrupt/missing data. */
    juce::String findAnchorId(const std::vector<Singers>& singers);

    /** `rr` must already be sorted host-first by stable order (see
        sortByStableOrder). Rotates it to start at the singer identified by
        `anchorId`; falls back to starting at index 0 if that id isn't
        found in `rr` (e.g. they were just removed). */
    std::vector<Singers> deriveDisplayQueue(const std::vector<Singers>& rr,
                                            const juce::String& anchorId);

    /** Stamps `rotationOrder` on every element of `rr` (in place) to its
        rank in deriveDisplayQueue(rr, anchorId) -- 0 for the anchor. */
    void stampDerivedRanks(std::vector<Singers>& rr, const juce::String& anchorId);

    /** "Someone's turn ended" primitive (song finished, or the anchor
        singer was removed). `rr` must be sorted host-first by stable
        order. Returns the id of whoever has the next-higher `order` after
        `departedOrder`, wrapping around to the lowest existing `order`
        (always the host) if nothing higher exists. Returns an empty string
        if `rr` has fewer than 2 members. Does not mutate `rr`. */
    juce::String advanceAnchor(const std::vector<Singers>& rr, int departedOrder);

    struct DragRemapResult
    {
        std::vector<Singers> newRR;   // Canonical order, host at [0].
        juce::String newAnchorId;
    };

    /** Maps a KJ drag performed on the currently DISPLAYED (already
        rotated) queue back into RR-space. Splices the drag directly within
        `displayedQueue`, then reconstructs the canonical RR by rotating the
        result so the host is first, and sets the new anchor to whoever
        ends up at the front of the edited displayed list (i.e. dragging
        elsewhere in the list never changes who's up next, unless the KJ
        specifically drags the front singer away from position 0). */
    DragRemapResult remapFromDisplayDrag(const std::vector<Singers>& displayedQueue,
                                         int fromIndex, int toIndex);
}
