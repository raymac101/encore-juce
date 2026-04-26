/*
  ==============================================================================

    FirebaseConfig.h

    Compile-time Firebase project constants (matches the Angular project's
    src/environments/environment.ts so we hit the same Firestore data).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace FirebaseConfig
{
    // Match the Angular/Electron project so JUCE talks to the same Firestore.
    inline const juce::String apiKey      = "AIzaSyDwZeeqR5NLHc02PyiKP6eh1WniEZSkc8s";
    inline const juce::String projectId   = "tagg-9ee2b";
    inline const juce::String authDomain  = "tagg-9ee2b.firebaseapp.com";

    // Identity Toolkit (Firebase Auth) REST base.
    inline const juce::String authBaseUrl =
        "https://identitytoolkit.googleapis.com/v1";

    // Firestore REST base for this project's (default) database.
    inline juce::String firestoreBaseUrl()
    {
        return "https://firestore.googleapis.com/v1/projects/"
             + projectId + "/databases/(default)/documents";
    }
}
