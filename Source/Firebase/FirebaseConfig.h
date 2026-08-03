/*
  ==============================================================================

    FirebaseConfig.h

    Compile-time Firebase project constants.  These match the Angular project's
    src/environments/environment.ts so the JUCE app talks to the same backend.

    The appId is required by the Firebase C++ SDK's AppOptions.
    Locate it in the Firebase console → Project Settings → Your apps → App ID.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace FirebaseConfig
{
    // ── Core credentials ────────────────────────────────────────────────────
    inline const juce::String apiKey      = "AIzaSyDwZeeqR5NLHc02PyiKP6eh1WniEZSkc8s";
    inline const juce::String projectId   = "tagg-9ee2b";
    inline const juce::String authDomain  = "tagg-9ee2b.firebaseapp.com";

    // Required by the Firebase C++ SDK AppOptions.
    // Format: "1:<project-number>:desktop:<hash>"
    // Find in Firebase console → Project Settings → General → Your apps.
    inline const juce::String appId       = "1:123456789:desktop:abcdef123456"; // TODO: replace

    // ── Optional SDK fields ──────────────────────────────────────────────────
    inline const juce::String databaseUrl     = "https://tagg-9ee2b-default-rtdb.firebaseio.com";
    inline const juce::String storageBucket   = "tagg-9ee2b.appspot.com";
    inline const juce::String messagingSenderId = "123456789"; // TODO: replace with real value

    // ── Derived REST bases (used by FirestoreClient) ─────────────────────────
    inline const juce::String authBaseUrl =
        "https://identitytoolkit.googleapis.com/v1";

    inline juce::String firestoreBaseUrl()
    {
        return "https://firestore.googleapis.com/v1/projects/"
             + projectId + "/databases/(default)/documents";
    }
}
