/*
  ==============================================================================

    QueueRotation.h

    Pure, Firestore/UI-agnostic logic for the singer Round Robin (RR) and
    the derived Queue view.

    Model
    ─────
    The RR is a stable list ordered by `order`; its first member is the
    "start of the round" (the host by default, but the KJ can drag anyone,
    host included, anywhere in the round). Singers are appended to the
    bottom the first time they add a song. The
    RR only changes via join, removal, or manual KJ drag-reorder -- never
    as a side effect of someone starting or finishing a performance.

    The Queue (what's actually shown as "up next") is not a separately
    stored list -- it's the RR sorted by stable order, rotated to start at
    whichever singer is the current "anchor" (who's up next). When the
    anchor singer finishes, the anchor simply advances to the next RR
    member, circularly (wrapping past the last member back to the round start).

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

    /** Ascending by stable `order` (ties go to the host). The front of
        the result is the start of the round. Does not mutate `order`. */
    std::vector<Singers> sortByStableOrder(std::vector<Singers> singers);

    /** Stamps `order` = 0..N-1 by current vector position. `rr` must
        already be sorted by stable order (see sortByStableOrder). Call only after a
        genuine RR-structure change (join/remove/drag) -- never on a
        song finishing or starting. */
    void renumberStableOrder(std::vector<Singers>& rr);

    /** The current anchor's singer id: whoever has `rotationOrder` 0,
        ties broken by `order`. Falls back to the round start (front of
        sortByStableOrder) when nobody does -- e.g. a brand-new venue, the
        host's unranked (-1) doc, or corrupt/missing data. */
    juce::String findAnchorId(const std::vector<Singers>& singers);

    /** `rr` must already be sorted by stable order (see
        sortByStableOrder). Rotates it to start at the singer identified by
        `anchorId`; falls back to starting at index 0 if that id isn't
        found in `rr` (e.g. they were just removed). */
    std::vector<Singers> deriveDisplayQueue(const std::vector<Singers>& rr,
                                            const juce::String& anchorId);

    /** Stamps `rotationOrder` on every element of `rr` (in place) to its
        rank in deriveDisplayQueue(rr, anchorId) -- 0 for the anchor. */
    void stampDerivedRanks(std::vector<Singers>& rr, const juce::String& anchorId);

    /** "Someone's turn ended" primitive (song finished, or the anchor
        singer was removed). `rr` must be sorted by stable order
        (see sortByStableOrder). Returns the id of whoever has the next-higher `order` after
        `departedOrder`, wrapping around to the lowest existing `order`
        (the round start) if nothing higher exists. Returns an empty string
        if `rr` has fewer than 2 members. Does not mutate `rr`. */
    juce::String advanceAnchor(const std::vector<Singers>& rr, int departedOrder);

    /** The mirror image of advanceAnchor(), for a KJ manually rotating the
        queue backwards (no turn actually ended). `rr` must be sorted
        by stable order. Returns the id of whoever has the
        next-lower `order` before `currentOrder`, wrapping around to the
        highest existing `order` if nothing lower exists. Returns an empty
        string if `rr` has fewer than 2 members. Does not mutate `rr`. */
    juce::String retreatAnchor(const std::vector<Singers>& rr, int currentOrder);

    struct DragRemapResult
    {
        std::vector<Singers> newRR;   // Canonical order, round start at [0].
        juce::String newAnchorId;
    };

    /** Maps a KJ drag performed on the currently DISPLAYED (already
        rotated) queue back into RR-space. Splices the drag directly within
        `displayedQueue`, then reconstructs the canonical RR by rotating the
        result so the round start stays first, and sets the new anchor to
        whoever ends up at the front of the edited displayed list (i.e.
        dragging elsewhere in the list never changes who's up next, unless
        the KJ specifically drags the front singer away from position 0).

        If the dragged singer was the round start, the singer after them
        takes over that slot. With `takeRoundStartSlot`, a singer dropped
        directly above the round start becomes the new round start (a KJ
        drop onto the "Start of Round" slot); without it they land at the
        end of the round instead (e.g. "Move to Next"). */
    DragRemapResult remapFromDisplayDrag(const std::vector<Singers>& displayedQueue,
                                         int fromIndex, int toIndex,
                                         bool takeRoundStartSlot = false);
}
