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
    juce::var    getAuthClaims() const;

    void signOut();

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

    /** Open the system browser to start an OAuth sign-in for the given
        provider ("google.com" or "apple.com"). Real desktop OAuth requires a
        local HTTP listener to receive the redirect; that infrastructure is
        not yet wired up in JUCE, so for now we surface a friendly error and
        log a TODO. Returns ok=false with errorMessage set. */
    AuthResult signInWithOAuthProvider(const juce::String& providerId);

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

    /** Quick helper: GET a collection (no filtering). */
    juce::Array<juce::var> listCollection(const juce::String& collectionPath,
                                          int pageSize = 100);

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
        ensureFreshToken itself (which must not trigger another refresh). */
    juce::var httpJsonRaw(const juce::URL& url,
                          const juce::String& httpMethod,
                          const juce::String& body,
                          int* httpStatus,
                          juce::StringArray extraHeaders,
                          const juce::String& contentType);

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
