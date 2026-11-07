/*
  ==============================================================================

    ProfileSync.h

    Shares one public identity between Encore and the TAGG mobile app.

    Both apps sign in against the same Firebase project, so a person has ONE
    Firebase Auth uid across both. TAGG keeps its profile at `users/{uid}`;
    Encore keeps host-only data (role, companyId, license expiry, login
    counters...) at `hosts/{uid}`. The public identity -- stage name, avatar,
    full name, city, country, gender, birthday -- lives in `users/{uid}` and
    is mirrored into `hosts/{uid}` so existing Encore readers keep working.

      - Login:        syncOnLogin() pulls TAGG's identity into the Host
                      (TAGG wins), back-fills anything TAGG is missing from
                      the host doc, and creates `users/{uid}` for a host who
                      has never used TAGG.
      - Profile save: pushSharedFields() writes the edits to `users/{uid}`.
      - Sign-up:      createTaggProfile() creates `users/{uid}` alongside
                      the new `hosts/{uid}`.

    Every function here is SYNCHRONOUS network I/O -- call from a background
    thread only (same contract as FirestoreClient).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/Host.h"

namespace ProfileSync
{
    /** Encore stores grid avatars as "assets/icon/<id>.png"; TAGG stores the
        same artwork as "preset:<id>". Converts the former to the latter and
        passes anything else (preset refs, https photo URLs) through as-is.
        Returns empty for Encore's placeholder avatar so it never overwrites
        a real TAGG avatar. */
    juce::String toTaggAvatar (const juce::String& encoreAvatar);

    /** True if `stageName` is free in TAGG (case-insensitive), or already
        belongs to `uid`. TAGG requires stage names to be unique. On a
        network failure this returns true -- a uniqueness check must not
        block saving a profile while offline-ish. */
    bool isStageNameAvailable (const juce::String& stageName, const juce::String& uid);

    /** Merges the TAGG profile into `host` (see header comment) and mirrors
        the merged identity back into `hosts/{uid}` when it changed. Best
        effort: any network failure leaves `host` exactly as it was. */
    void syncOnLogin (Host& host);

    /** PATCHes the shared identity fields from `host` into `users/{uid}`.
        The stage name is only written when `includeStageName` is true (the
        caller has already checked it is available). */
    bool pushSharedFields (const Host& host, bool includeStageName);

    /** Creates `users/{uid}` for a brand-new Encore account, using the same
        base shape TAGG's own sign-up writes. No-op if one already exists. */
    void createTaggProfile (const Host& host);
}
