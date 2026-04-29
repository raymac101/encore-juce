/*
  ==============================================================================

    FirestoreClient.cpp

  ==============================================================================
*/

#include "FirestoreClient.h"
#include "../Firebase/FirebaseConfig.h"

//==============================================================================
FirestoreClient& FirestoreClient::getInstance()
{
    static FirestoreClient inst;
    return inst;
}

void FirestoreClient::signOut()
{
    idToken_.clear();
    refreshToken_.clear();
    localId_.clear();
    email_.clear();
    displayName_.clear();
    tokenIssuedAt_ = {};
    tokenLifetimeSeconds_ = 0;
}

//==============================================================================
juce::var FirestoreClient::httpJson(const juce::URL& url,
                                    const juce::String& httpMethod,
                                    const juce::String& jsonBody,
                                    int* httpStatus,
                                    juce::StringArray extraHeaders)
{
    juce::URL u = url;
    if (jsonBody.isNotEmpty())
        u = u.withPOSTData(jsonBody);

    juce::StringArray headers;
    headers.add("Content-Type: application/json");
    headers.add("Accept: application/json");
    if (isSignedIn())
        headers.add("Authorization: Bearer " + idToken_);
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

    auto body = stream->readEntireStreamAsString();
    if (body.isEmpty())
        return juce::var();

    juce::var parsed;
    auto result = juce::JSON::parse(body, parsed);
    if (result.failed())
    {
        DBG("FirestoreClient: JSON parse failed (" << status << "): " << body.substring(0, 400));
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
                                          const juce::String& documentId)
{
    juce::String path = FirebaseConfig::firestoreBaseUrl() + "/" + collectionPath;
    if (documentId.isNotEmpty())
        path += "?documentId=" + juce::URL::addEscapeChars(documentId, true);

    juce::URL url(path);

    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty("fields", fields);

    int status = 0;
    return httpJson(url, "POST", juce::JSON::toString(juce::var(body.get())), &status);
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
