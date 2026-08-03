/*
  ==============================================================================

    FirestoreClient.cpp

  ==============================================================================
*/

#include "FirestoreClient.h"
#include "../Firebase/FirebaseConfig.h"

namespace
{
juce::String decodeBase64UrlToString (juce::String value)
{
    value = value.replaceCharacter ('-', '+').replaceCharacter ('_', '/');
    while ((value.length() % 4) != 0)
        value << '=';

    juce::MemoryOutputStream output;
    if (! juce::Base64::convertFromBase64 (output, value))
        return {};

    auto block = output.getMemoryBlock();
    const auto* data = static_cast<const char*> (block.getData());
    return juce::String::fromUTF8 (data, (int) block.getSize());
}

juce::var parseJwtPayload (const juce::String& jwt)
{
    auto payload = jwt.fromFirstOccurrenceOf (".", false, false)
                      .upToFirstOccurrenceOf (".", false, false);
    if (payload.isEmpty())
        return juce::var();

    auto json = decodeBase64UrlToString (payload);
    if (json.isEmpty())
        return juce::var();

    juce::var parsed;
    auto result = juce::JSON::parse (json, parsed);
    if (result.failed() || ! parsed.isObject())
        return juce::var();

    return parsed;
}
}

//==============================================================================
FirestoreClient& FirestoreClient::getInstance()
{
    static FirestoreClient inst;
    return inst;
}

void FirestoreClient::signOut()
{
    const juce::ScopedLock lock(stateLock_);
    idToken_.clear();
    refreshToken_.clear();
    localId_.clear();
    email_.clear();
    displayName_.clear();
    tokenIssuedAt_ = {};
    tokenLifetimeSeconds_ = 0;
}

juce::var FirestoreClient::getAuthClaims() const
{
    const juce::String token = getIdToken();
    if (token.isEmpty())
        return juce::var();

    return parseJwtPayload (token);
}

//==============================================================================
void FirestoreClient::ensureFreshToken()
{
    juce::String refreshTok;
    {
        const juce::ScopedLock lock(stateLock_);
        if (idToken_.isEmpty() || refreshToken_.isEmpty())
            return;

        const auto elapsedSeconds = (juce::Time::getCurrentTime() - tokenIssuedAt_).inSeconds();
        const auto lifetime = tokenLifetimeSeconds_ > 0 ? tokenLifetimeSeconds_ : 3600;

        // Refresh 5 minutes before actual expiry so in-flight requests don't
        // race a token that expires mid-call.
        if (elapsedSeconds < (lifetime - 300))
            return;

        refreshTok = refreshToken_;
    }

    juce::URL url("https://securetoken.googleapis.com/v1/token?key=" + FirebaseConfig::apiKey);
    const juce::String form = "grant_type=refresh_token&refresh_token="
                             + juce::URL::addEscapeChars(refreshTok, true);

    int status = 0;
    auto resp = httpJsonRaw(url, "POST", form, &status, {}, "application/x-www-form-urlencoded");

    if (status >= 200 && status < 300 && resp.isObject())
    {
        const juce::ScopedLock lock(stateLock_);
        idToken_       = resp.getProperty("id_token", idToken_).toString();
        refreshToken_  = resp.getProperty("refresh_token", refreshToken_).toString();
        tokenIssuedAt_ = juce::Time::getCurrentTime();
        tokenLifetimeSeconds_ = resp.getProperty("expires_in", juce::String(tokenLifetimeSeconds_)).toString().getIntValue();
    }
    else
    {
        DBG("FirestoreClient: token refresh failed, status=" << status);
    }
}

juce::var FirestoreClient::httpJson(const juce::URL& url,
                                    const juce::String& httpMethod,
                                    const juce::String& jsonBody,
                                    int* httpStatus,
                                    juce::StringArray extraHeaders)
{
    ensureFreshToken();
    return httpJsonRaw(url, httpMethod, jsonBody, httpStatus, extraHeaders, "application/json");
}

juce::var FirestoreClient::httpJsonRaw(const juce::URL& url,
                                       const juce::String& httpMethod,
                                       const juce::String& body,
                                       int* httpStatus,
                                       juce::StringArray extraHeaders,
                                       const juce::String& contentType)
{
    juce::URL u = url;
    if (body.isNotEmpty())
        u = u.withPOSTData(body);

    const juce::String bearerToken = getIdToken();

    juce::StringArray headers;
    headers.add("Content-Type: " + contentType);
    headers.add("Accept: application/json");
    if (bearerToken.isNotEmpty())
        headers.add("Authorization: Bearer " + bearerToken);
    headers.addArray(extraHeaders);

    auto headersStr = headers.joinIntoString("\r\n");

    int status = 0;
    juce::StringPairArray responseHeaders;

    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(15000)
                    .withExtraHeaders(headersStr)
                    .withHttpRequestCmd(httpMethod)
                    .withResponseHeaders(&responseHeaders)
                    .withStatusCode(&status);

    std::unique_ptr<juce::InputStream> stream(u.createInputStream(opts));
    if (httpStatus != nullptr) *httpStatus = status;

    if (stream == nullptr)
    {
        DBG("FirestoreClient: connection failed for " << u.toString(false));
        return juce::var();
    }

    auto responseBody = stream->readEntireStreamAsString();
    if (responseBody.isEmpty())
        return juce::var();

    juce::var parsed;
    auto result = juce::JSON::parse(responseBody, parsed);
    if (result.failed())
    {
        DBG("FirestoreClient: JSON parse failed (" << status << "): " << responseBody.substring(0, 400));
        return juce::var();
    }
    return parsed;
}

//==============================================================================
// Auth
static FirestoreClient::AuthResult parseAuthError(const juce::var& v)
{
    FirestoreClient::AuthResult r;
    r.ok = false;
    if (auto* err = v.getProperty("error", juce::var()).getDynamicObject())
    {
        r.errorCode    = err->getProperty("message").toString();
        r.errorMessage = err->getProperty("message").toString();
    }
    else
    {
        r.errorMessage = "Unknown authentication error";
    }
    return r;
}

FirestoreClient::AuthResult FirestoreClient::signInWithEmailPassword(const juce::String& email,
                                                                     const juce::String& password)
{
    juce::URL url(FirebaseConfig::authBaseUrl
                  + "/accounts:signInWithPassword?key=" + FirebaseConfig::apiKey);

    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty("email", email);
    body->setProperty("password", password);
    body->setProperty("returnSecureToken", true);

    int status = 0;
    auto resp = httpJson(url, "POST", juce::JSON::toString(juce::var(body.get())), &status);

    if (status >= 200 && status < 300 && resp.isObject())
    {
        const juce::ScopedLock lock(stateLock_);
        idToken_      = resp.getProperty("idToken", "").toString();
        refreshToken_ = resp.getProperty("refreshToken", "").toString();
        localId_      = resp.getProperty("localId", "").toString();
        email_        = resp.getProperty("email", email).toString();
        displayName_  = resp.getProperty("displayName", "").toString();
        tokenIssuedAt_ = juce::Time::getCurrentTime();
        tokenLifetimeSeconds_ = (int) resp.getProperty("expiresIn", 3600);
        return { true, false, {}, {} };
    }
    return parseAuthError(resp);
}

FirestoreClient::AuthResult FirestoreClient::signUpWithEmailPassword(const juce::String& email,
                                                                     const juce::String& password)
{
    juce::URL url(FirebaseConfig::authBaseUrl
                  + "/accounts:signUp?key=" + FirebaseConfig::apiKey);

    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty("email", email);
    body->setProperty("password", password);
    body->setProperty("returnSecureToken", true);

    int status = 0;
    auto resp = httpJson(url, "POST", juce::JSON::toString(juce::var(body.get())), &status);

    if (status >= 200 && status < 300 && resp.isObject())
    {
        const juce::ScopedLock lock(stateLock_);
        idToken_      = resp.getProperty("idToken", "").toString();
        refreshToken_ = resp.getProperty("refreshToken", "").toString();
        localId_      = resp.getProperty("localId", "").toString();
        email_        = resp.getProperty("email", email).toString();
        displayName_  = resp.getProperty("displayName", "").toString();
        tokenIssuedAt_ = juce::Time::getCurrentTime();
        tokenLifetimeSeconds_ = (int) resp.getProperty("expiresIn", 3600);
        return { true, true, {}, {} };
    }
    return parseAuthError(resp);
}

FirestoreClient::AuthResult FirestoreClient::sendPasswordResetEmail(const juce::String& email)
{
    juce::URL url(FirebaseConfig::authBaseUrl
                  + "/accounts:sendOobCode?key=" + FirebaseConfig::apiKey);

    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty("requestType", "PASSWORD_RESET");
    body->setProperty("email", email);

    int status = 0;
    auto resp = httpJson(url, "POST", juce::JSON::toString(juce::var(body.get())), &status);

    if (status >= 200 && status < 300 && resp.isObject())
        return { true, false, {}, {} };

    return parseAuthError(resp);
}

bool FirestoreClient::deleteCurrentAccount()
{
    const juce::String token = getFreshIdToken();
    if (token.isEmpty())
        return true; // nothing signed in to delete

    juce::URL url(FirebaseConfig::authBaseUrl + "/accounts:delete?key=" + FirebaseConfig::apiKey);

    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty("idToken", token);

    int status = 0;
    httpJson(url, "POST", juce::JSON::toString(juce::var(body.get())), &status);
    return status >= 200 && status < 300;
}

FirestoreClient::AuthResult FirestoreClient::signInWithOAuthProvider(const juce::String& providerId)
{
    // Real desktop OAuth requires:
    //   1. Open the system browser to https://<authDomain>/__/auth/handler
    //      with provider=<providerId>, client_id, redirect_uri=http://127.0.0.1:<port>
    //   2. Run a one-shot local HTTP listener on <port> to capture the
    //      authorization code / id_token.
    //   3. POST it to identitytoolkit.googleapis.com/v1/accounts:signInWithIdp
    //      to mint a Firebase id token.
    //
    // The local-listener piece is non-trivial and not yet wired up. For now,
    // open the browser as a hint and return a friendly error so the UI can
    // fall back to email/password.
    juce::URL hint("https://" + FirebaseConfig::authDomain + "/__/auth/handler"
                   "?providerId=" + providerId);
    hint.launchInDefaultBrowser();

    AuthResult r;
    r.ok = false;
    r.errorCode    = "OAUTH_NOT_IMPLEMENTED";
    r.errorMessage = providerId == "google.com"
        ? "Google sign-in isn't fully wired up in this build yet. Please use email & password."
        : "Apple sign-in isn't fully wired up in this build yet. Please use email & password.";
    return r;
}

//==============================================================================
// Firestore documents
juce::var FirestoreClient::getDocument(const juce::String& path, int* httpStatus)
{
    juce::URL url(FirebaseConfig::firestoreBaseUrl() + "/" + path);
    int status = 0;
    auto v = httpJson(url, "GET", {}, &status);
    if (httpStatus != nullptr) *httpStatus = status;
    if (status == 404) return juce::var();
    return v;
}

bool FirestoreClient::patchDocument(const juce::String& path, const juce::var& fields)
{
    juce::URL url(FirebaseConfig::firestoreBaseUrl() + "/" + path);

    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty("fields", fields);

    int status = 0;
    httpJson(url, "PATCH", juce::JSON::toString(juce::var(body.get())), &status);
    return status >= 200 && status < 300;
}

juce::var FirestoreClient::createDocument(const juce::String& collectionPath,
                                          const juce::var& fields,
                                          const juce::String& documentId,
                                          bool* outOk)
{
    juce::String path = FirebaseConfig::firestoreBaseUrl() + "/" + collectionPath;
    if (documentId.isNotEmpty())
        path += "?documentId=" + juce::URL::addEscapeChars(documentId, true);

    juce::URL url(path);

    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty("fields", fields);

    int status = 0;
    auto resp = httpJson(url, "POST", juce::JSON::toString(juce::var(body.get())), &status);
    if (outOk != nullptr)
        *outOk = (status >= 200 && status < 300);
    return resp;
}

bool FirestoreClient::deleteDocument(const juce::String& path)
{
    juce::URL url(FirebaseConfig::firestoreBaseUrl() + "/" + path);
    int status = 0;
    httpJson(url, "DELETE", {}, &status);
    return (status >= 200 && status < 300) || status == 404;
}

juce::Array<juce::var> FirestoreClient::runQuery(const juce::String& parentPath,
                                                 const juce::var& structuredQuery)
{
    juce::String path = FirebaseConfig::firestoreBaseUrl();
    if (parentPath.isNotEmpty())
        path += "/" + parentPath;
    path += ":runQuery";

    juce::URL url(path);

    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty("structuredQuery", structuredQuery);

    int status = 0;
    auto resp = httpJson(url, "POST", juce::JSON::toString(juce::var(body.get())), &status);

    juce::Array<juce::var> docs;
    if (auto* arr = resp.getArray())
    {
        for (auto& entry : *arr)
        {
            auto document = entry.getProperty("document", juce::var());
            if (document.isObject())
                docs.add(document);
        }
    }
    return docs;
}

juce::Array<juce::var> FirestoreClient::listCollection(const juce::String& collectionPath,
                                                       int pageSize)
{
    juce::URL url(FirebaseConfig::firestoreBaseUrl() + "/" + collectionPath
                  + "?pageSize=" + juce::String(pageSize));
    int status = 0;
    auto resp = httpJson(url, "GET", {}, &status);

    juce::Array<juce::var> docs;
    if (auto* arr = resp.getProperty("documents", juce::var()).getArray())
    {
        for (auto& d : *arr)
            docs.add(d);
    }
    return docs;
}

//==============================================================================
// Firestore value helpers
juce::var FirestoreClient::stringValue(const juce::String& s)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty("stringValue", s);
    return juce::var(o.get());
}

juce::var FirestoreClient::booleanValue(bool b)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty("booleanValue", b);
    return juce::var(o.get());
}

juce::var FirestoreClient::integerValue(juce::int64 v)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty("integerValue", juce::String(v));
    return juce::var(o.get());
}

juce::var FirestoreClient::doubleValue(double v)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty("doubleValue", v);
    return juce::var(o.get());
}

juce::var FirestoreClient::timestampValue(juce::Time t)
{
    // RFC3339 with millisecond precision: "2026-04-25T12:34:56.789Z"
    auto millis = t.toMilliseconds();
    juce::Time utc(millis);
    auto iso = utc.formatted("%Y-%m-%dT%H:%M:%S")
             + "." + juce::String(utc.getMilliseconds()).paddedLeft('0', 3) + "Z";
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty("timestampValue", iso);
    return juce::var(o.get());
}

juce::var FirestoreClient::nullValue()
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty("nullValue", juce::var());
    return juce::var(o.get());
}

juce::var FirestoreClient::makeFields(std::initializer_list<std::pair<juce::String, juce::var>> entries)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    for (auto& e : entries)
        o->setProperty(juce::Identifier(e.first), e.second);
    return juce::var(o.get());
}

//==============================================================================
juce::var FirestoreClient::unwrapFields(const juce::var& document)
{
    auto fields = document.getProperty("fields", juce::var());
    juce::DynamicObject::Ptr out = new juce::DynamicObject();
    if (auto* obj = fields.getDynamicObject())
    {
        for (auto& p : obj->getProperties())
        {
            const auto& v = p.value;
            if (v.hasProperty("stringValue"))   out->setProperty(p.name, v.getProperty("stringValue", ""));
            else if (v.hasProperty("booleanValue")) out->setProperty(p.name, v.getProperty("booleanValue", false));
            else if (v.hasProperty("integerValue")) out->setProperty(p.name, v.getProperty("integerValue", "").toString().getLargeIntValue());
            else if (v.hasProperty("doubleValue"))  out->setProperty(p.name, (double) v.getProperty("doubleValue", 0.0));
            else if (v.hasProperty("timestampValue")) out->setProperty(p.name, v.getProperty("timestampValue", ""));
            else if (v.hasProperty("nullValue"))    out->setProperty(p.name, juce::var());
            else                                    out->setProperty(p.name, v);
        }
    }
    return juce::var(out.get());
}

juce::String FirestoreClient::readString(const juce::var& doc, const juce::String& field)
{
    auto fields = doc.getProperty("fields", juce::var());
    return fields.getProperty(juce::Identifier(field), juce::var())
                 .getProperty("stringValue", "").toString();
}

bool FirestoreClient::readBool(const juce::var& doc, const juce::String& field, bool dflt)
{
    auto fields = doc.getProperty("fields", juce::var());
    auto v = fields.getProperty(juce::Identifier(field), juce::var());
    if (v.hasProperty("booleanValue"))
        return (bool) v.getProperty("booleanValue", dflt);
    return dflt;
}

juce::int64 FirestoreClient::readInt(const juce::var& doc, const juce::String& field, juce::int64 dflt)
{
    auto fields = doc.getProperty("fields", juce::var());
    auto v = fields.getProperty(juce::Identifier(field), juce::var());
    if (v.hasProperty("integerValue"))
        return v.getProperty("integerValue", "").toString().getLargeIntValue();
    return dflt;
}

juce::Time FirestoreClient::readTime(const juce::var& doc, const juce::String& field)
{
    auto fields = doc.getProperty("fields", juce::var());
    auto s = fields.getProperty(juce::Identifier(field), juce::var())
                   .getProperty("timestampValue", "").toString();
    if (s.isEmpty())
        return juce::Time();
    // Parse RFC3339 with optional fractional seconds and trailing Z.
    auto datePart = s.upToFirstOccurrenceOf("T", false, false);
    auto timePart = s.fromFirstOccurrenceOf("T", false, false)
                     .upToFirstOccurrenceOf("Z", false, false);
    int year=0, month=0, day=0, h=0, m=0, sec=0, ms=0;
    sscanf(datePart.toRawUTF8(), "%d-%d-%d", &year, &month, &day);
    auto fracIdx = timePart.indexOfChar('.');
    if (fracIdx >= 0)
    {
        sscanf(timePart.substring(0, fracIdx).toRawUTF8(), "%d:%d:%d", &h, &m, &sec);
        ms = timePart.substring(fracIdx + 1).getIntValue();
    }
    else
    {
        sscanf(timePart.toRawUTF8(), "%d:%d:%d", &h, &m, &sec);
    }
    return juce::Time(year, month - 1, day, h, m, sec, ms, false);
}
