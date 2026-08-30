/*
  ==============================================================================

    FirestoreClient.h

    Minimal Firebase Auth + Firestore REST client used by the login flow.
    All public calls are SYNCHRONOUS and intended to be invoked from a
    background thread (see LoginFlowController which runs them on a juce::Thread).

    Auth   : https://identitytoolkit.googleapis.com/v1
    Firestore: https://firestore.googleapis.com/v1/projects/<id>/databases/(default)/documents

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

class FirestoreClient
{
public:
    static FirestoreClient& getInstance();

    //==============================================================================
    // Authentication state
    bool        isSignedIn() const noexcept { const juce::ScopedLock l(stateLock_); return idToken_.isNotEmpty(); }
    juce::String getUserId() const          { const juce::ScopedLock l(stateLock_); return localId_; }
    juce::String getEmail() const           { const juce::ScopedLock l(stateLock_); return email_; }
    juce::String getDisplayName() const     { const juce::ScopedLock l(stateLock_); return displayName_; }
    juce::String getIdToken() const         { const juce::ScopedLock l(stateLock_); return idToken_; }

    /** The long-lived Firebase refresh token from the current session, if
        any. Safe to persist locally (unlike the password) for a "stay
        signed in" feature -- see signInWithRefreshToken(), the login-time
        counterpart that bootstraps a whole new session from one of these
        without ever touching the password. */
    juce::String getRefreshToken() const    { const juce::ScopedLock l(stateLock_); return refreshToken_; }

    /** Like getIdToken(), but refreshes first if the current token is close
        to expiry (see ensureFreshToken()). Use this instead of getIdToken()
        whenever the token is about to be sent somewhere else that verifies
        it server-side (e.g. as an Authorization header on a Cloud Functions
        call) -- getIdToken() alone can hand back a token that's already
        expired if the caller doesn't otherwise go through httpJson(). Safe
        to call from any background thread. */
    juce::String getFreshIdToken()          { ensureFreshToken(); return getIdToken(); }
    juce::var    getAuthClaims() const;

    void signOut();

    /** "Reconnect Now"-style escape hatch: forgets about any requests
        currently counted against httpJsonRaw()'s in-flight cap (see its
        comment), so a fresh manual reconnect attempt gets a real, immediate
        chance instead of being silently refused because earlier stalled
        requests -- which may take the OS's own TCP stack several minutes to
        actually give up on -- are still counted as "in flight". Safe to
        call from the message thread. */
    static void resetStalledRequestBudget();

    //==============================================================================
    // Auth: email + password
    struct AuthResult
    {
        bool         ok = false;
        bool         isNewAccount = false;     // true after a successful sign-up
        juce::String errorCode;                // e.g. "EMAIL_NOT_FOUND"
        juce::String errorMessage;             // human-readable
    };

    AuthResult signInWithEmailPassword(const juce::String& email,
                                       const juce::String& password);
    AuthResult signUpWithEmailPassword(const juce::String& email,
                                       const juce::String& password);

    /** Bootstraps a brand-new session purely from a previously-saved
        refresh token (securetoken.googleapis.com's refresh grant) -- the
        "stay signed in" counterpart to signInWithEmailPassword(), used at
        app launch instead of asking for the password again. Unlike
        ensureFreshToken() (which only refreshes a session that's already
        signed in), this works from a completely empty auth state.
        errorMessage is empty on failure (an expired/revoked token is an
        expected, silent case here, not something to surface to the user --
        callers should just fall back to the normal login form). */
    AuthResult signInWithRefreshToken(const juce::String& refreshToken);

    /** Open the system browser to start an OAuth sign-in for the given
        provider ("google.com" or "apple.com"). Real desktop OAuth requires a
        local HTTP listener to receive the redirect; that infrastructure is
        not yet wired up in JUCE, so for now we surface a friendly error and
        log a TODO. Returns ok=false with errorMessage set. */
    AuthResult signInWithOAuthProvider(const juce::String& providerId);

    /** Sends a Firebase "reset your password" email to the given address via
        Identity Toolkit's accounts:sendOobCode (requestType PASSWORD_RESET).
        This is a public endpoint keyed only by the Web API key — it works
        for any target email without the caller needing to BE that user or
        hold any special privilege (the same mechanism a public "forgot
        password" link relies on). Used by the Customer Admin tool, but is
        not itself a privileged operation. */
    AuthResult sendPasswordResetEmail(const juce::String& email);

    /** Deletes the CURRENTLY SIGNED-IN Firebase Auth account via Identity
        Toolkit's self-service accounts:delete, authenticated with the
        current idToken -- no admin privilege needed, since a user can
        always delete their own account. Does not itself call signOut() or
        touch any Firestore documents; callers are responsible for both.
        Returns true on success, or if no one was signed in to begin with. */
    bool deleteCurrentAccount();

    //==============================================================================
    // Firestore: documents
    /** GET projects/.../documents/<path>. Returns the raw response var, or
        an empty var on 404. Sets `httpStatus` if non-null. */
    juce::var getDocument(const juce::String& path, int* httpStatus = nullptr);

    /** PATCH projects/.../documents/<path>. `fields` is a Firestore-format
        object ({ field: { stringValue: ... } }) — use the helpers below. */
    bool patchDocument(const juce::String& path, const juce::var& fields);

    /** POST projects/.../documents/<collectionPath>?documentId=<id> (id optional).
        `outOk` (if non-null) is set to true only on an actual 2xx response —
        Firestore error bodies (401/403/etc.) parse as valid JSON objects, so
        callers must check `outOk` rather than the returned var's isObject(). */
    juce::var createDocument(const juce::String& collectionPath,
                             const juce::var& fields,
                             const juce::String& documentId = {},
                             bool* outOk = nullptr);

    /** DELETE projects/.../documents/<path>. Returns true on 2xx (also true
        on 404 — the doc is gone either way). */
    bool deleteDocument(const juce::String& path);

    /** POST :runQuery on the parent path. `structuredQuery` is the Firestore
        StructuredQuery JSON object. Returns the array of documents. */
    juce::Array<juce::var> runQuery(const juce::String& parentPath,
                                    const juce::var& structuredQuery);

    /** Quick helper: GET a collection (no filtering). An empty array is
        returned both when the collection genuinely has no documents and
        when the request itself failed (dropped connection, timeout, auth
        error, transient server error) -- those two cases are otherwise
        indistinguishable to the caller. Pass `ok` to tell them apart: it's
        set true only on a real 2xx response, false on any request failure,
        so a caller that polls on a timer (e.g. QueueService's watcher) can
        skip acting on a failed fetch instead of treating it as "now
        empty". */
    juce::Array<juce::var> listCollection(const juce::String& collectionPath,
                                          int pageSize = 100,
                                          bool* ok = nullptr);

    //==============================================================================
    // Firestore value helpers — convert between native juce/std types and the
    // Firestore REST representation ({ stringValue: ..., booleanValue: ... }).
    static juce::var stringValue(const juce::String& s);
    static juce::var booleanValue(bool b);
    static juce::var integerValue(juce::int64 v);
    static juce::var doubleValue(double v);
    static juce::var timestampValue(juce::Time t);
    static juce::var nullValue();

    /** Build a Firestore document object: { fields: { name: { stringValue:..} } } */
    static juce::var makeFields(std::initializer_list<std::pair<juce::String, juce::var>> entries);

    /** Read a typed field from a Firestore document var (output of getDocument). */
    static juce::String  readString (const juce::var& doc, const juce::String& field);
    static bool          readBool   (const juce::var& doc, const juce::String& field, bool dflt = false);
    static juce::int64   readInt    (const juce::var& doc, const juce::String& field, juce::int64 dflt = 0);
    static juce::Time    readTime   (const juce::var& doc, const juce::String& field);

    /** Strip the "fields" wrapper into a plain object of { fieldName: value }. */
    static juce::var unwrapFields(const juce::var& document);

private:
    FirestoreClient() = default;
    ~FirestoreClient() = default;

    /** Refreshes idToken_ via the securetoken endpoint if it's close to
        expiry. No-op if signed out or if the current token still has more
        than 5 minutes of life left. Safe to call from any background thread;
        does not recurse into itself (uses httpJsonRaw directly). */
    void ensureFreshToken();

    /** Issues the HTTP request. Checks/refreshes the token first, then
        delegates to httpJsonRaw with the Firestore/Auth JSON content type. */
    juce::var httpJson(const juce::URL& url,
                       const juce::String& httpMethod,
                       const juce::String& jsonBody,
                       int* httpStatus,
                       juce::StringArray extraHeaders = {});

    /** Low-level request with no auto-refresh — used by httpJson and by
        ensureFreshToken itself (which must not trigger another refresh).
        `includeAuthHeader` (default true) controls whether a current
        idToken_ gets attached as "Authorization: Bearer ...". Identity
        Toolkit's own credential-establishing endpoints (signIn/signUp/
        password reset/refresh) are public, keyed only by the API key in
        the URL, and must be called with this false: they can run while a
        *different*, still-valid session's idToken_ is sitting around
        (e.g. LoginWindow's "stay signed in" background refresh, held back
        from navigating anywhere -- see LoginContent::attemptSavedSignIn),
        and attaching that unrelated token turned out to make Identity
        Toolkit reject the request outright ("Request had invalid
        authentication credentials") rather than just ignore it. */
    juce::var httpJsonRaw(const juce::URL& url,
                          const juce::String& httpMethod,
                          const juce::String& body,
                          int* httpStatus,
                          juce::StringArray extraHeaders,
                          const juce::String& contentType,
                          bool includeAuthHeader = true);

    // Guards all auth-state fields below: they're written by sign-in/sign-up/
    // refresh/sign-out and read by every background thread that issues a
    // Firestore/Auth request (QueueService, RequestService, VenueService,
    // AuditService, ...), so plain juce::String access here is a data race.
    mutable juce::CriticalSection stateLock_;
    juce::String idToken_;
    juce::String refreshToken_;
    juce::String localId_;
    juce::String email_;
    juce::String displayName_;
    juce::Time   tokenIssuedAt_;
    int          tokenLifetimeSeconds_ = 0;

    JUCE_DECLARE_NON_COPYABLE(FirestoreClient)
};
