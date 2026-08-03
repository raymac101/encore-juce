/*
  ==============================================================================

    FirebaseManager.h

    Central coordinator for all Firebase operations.

    Architecture
    ────────────
    • FirestoreClient  — REST one-shots (login, venue load, queue writes).
                         Already fully implemented; FirebaseManager does NOT
                         duplicate those APIs.
    • Firebase C++ SDK — real-time snapshot listeners for the queue and venue
                         documents.  Requires:
                           macOS:   ${FIREBASE_SDK_ROOT}/libs/darwin/
                           Windows: ${FIREBASE_SDK_ROOT}/libs/windows/
                         (see EncoreJUCE.jucer for link flags)

    Thread model
    ────────────
    Firebase SDK callbacks arrive on an internal Firebase thread.
    FirebaseManager marshals every callback onto the JUCE message thread before
    notifying application code, so all callbacks are safe to use for UI updates.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/VenueItem.h"
#include "../Models/QueueItem.h"
#include "../Models/Singers.h"

// Forward-declare SDK types so we don't drag the full headers into every TU.
namespace firebase        { class App; }
namespace firebase::auth  { class Auth; }
namespace firebase::firestore {
    class Firestore;
    class ListenerRegistration;
}

//==============================================================================
class FirebaseManager
{
public:
    //==========================================================================
    enum class ConnectionStatus { Disconnected, Connecting, Connected, Error };

    //==========================================================================
    // Callback types — all invoked on the JUCE message thread.
    using ConnectionCallback  = std::function<void(ConnectionStatus, juce::String msg)>;
    using QueueCallback       = std::function<void(std::vector<Singers>)>;
    using VenueCallback       = std::function<void(VenueItem)>;
    using ErrorCallback       = std::function<void(juce::String context, juce::String msg)>;

    //==========================================================================
    static FirebaseManager& getInstance();

    //==========================================================================
    // Lifecycle

    /** Initialise the Firebase C++ SDK with the compile-time credentials from
        FirebaseConfig.h.  Must be called once before any other method, on the
        message thread. */
    void initialise();
    void shutdown();

    bool isInitialised()  const noexcept { return initialised_.load(); }
    bool isAuthenticated() const noexcept { return authenticated_.load(); }

    //==========================================================================
    // Authentication

    struct AuthResult
    {
        bool         ok           = false;
        bool         isNewAccount = false;
        juce::String userId;
        juce::String email;
        juce::String errorCode;
        juce::String errorMessage;
    };

    using AuthCallback = std::function<void(AuthResult)>;

    /** Sign in with email + password.  Runs on a background thread; callback
        fires on the message thread. */
    void signInWithEmail(const juce::String& email,
                         const juce::String& password,
                         AuthCallback onDone);

    /** Sign up a new account.  Same thread model. */
    void signUpWithEmail(const juce::String& email,
                         const juce::String& password,
                         AuthCallback onDone);

    /** Opens the system browser for OAuth (Google).  The local redirect
        listener is not yet wired; returns an error with a user-visible message
        directing them to use email/password instead. */
    void signInWithGoogle(AuthCallback onDone);

    void signOut();

    juce::String getCurrentUserId()   const;
    juce::String getCurrentUserEmail() const;

    //==========================================================================
    // Venue

    /** Load venues/<venueId> once (one-shot, not a listener).
        Caches the result internally; call getCurrentVenue() afterwards. */
    void loadVenue(const juce::String& venueId,
                   std::function<void(bool ok, VenueItem, juce::String error)> onDone);

    VenueItem    getCurrentVenue()   const;
    juce::String getCurrentVenueId() const;

    //==========================================================================
    // Real-time listeners

    /** Start a Firestore snapshot listener on
        venues/<venueId>/queue.
        The listener fires immediately with the current data, then again on
        every server-side change.  Implicitly stops any previous listener. */
    void startQueueListener(const juce::String& venueId, QueueCallback onChange);

    /** Start a Firestore snapshot listener on venues/<venueId>.
        Same semantics as startQueueListener. */
    void startVenueListener(const juce::String& venueId, VenueCallback onChange);

    /** Stop the queue listener (if running). */
    void stopQueueListener();

    /** Stop the venue listener (if running). */
    void stopVenueListener();

    //==========================================================================
    // Queue writes  (async, callback on message thread)

    using WriteCallback = std::function<void(bool ok, juce::String error)>;

    void addSongToQueue    (const juce::String& venueId, const QueueItem& item,   WriteCallback = nullptr);
    void removeSongFromQueue(const juce::String& venueId, const QueueItem& item,  WriteCallback = nullptr);
    void updateQueueItem   (const juce::String& venueId, const QueueItem& item,   WriteCallback = nullptr);
    void deleteSinger      (const juce::String& venueId, const juce::String& singerName, WriteCallback = nullptr);

    //==========================================================================
    // Callback registration

    void setConnectionCallback(ConnectionCallback cb) { connectionCb_ = std::move(cb); }
    void setErrorCallback(ErrorCallback cb)           { errorCb_       = std::move(cb); }

private:
    FirebaseManager() = default;
    ~FirebaseManager();

    //==========================================================================
    void notifyConnection(ConnectionStatus s, const juce::String& msg = {});
    void notifyError(const juce::String& ctx, const juce::String& msg);

    // Parse a Firestore collection snapshot into grouped Singers.
    static std::vector<Singers> parseSingers(const void* querySnapshotPtr);

    // Parse a Firestore document snapshot into a VenueItem.
    static VenueItem parseVenue(const void* documentSnapshotPtr);

    //==========================================================================
    // SDK object ownership — raw pointers because the SDK manages lifetime
    // through its own factory functions.
    firebase::App*                         app_       = nullptr;
    firebase::auth::Auth*                  auth_      = nullptr;
    firebase::firestore::Firestore*        db_        = nullptr;

    // Listener registrations — must be removed before db_ is destroyed.
    std::unique_ptr<firebase::firestore::ListenerRegistration> queueReg_;
    std::unique_ptr<firebase::firestore::ListenerRegistration> venueReg_;

    //==========================================================================
    std::atomic<bool> initialised_   { false };
    std::atomic<bool> authenticated_ { false };

    mutable juce::CriticalSection lock_;
    VenueItem    currentVenue_;
    juce::String currentVenueId_;
    juce::String currentUserId_;
    juce::String currentUserEmail_;

    ConnectionCallback connectionCb_;
    ErrorCallback      errorCb_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirebaseManager)
};
