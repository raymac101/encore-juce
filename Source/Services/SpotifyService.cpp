/*
  ==============================================================================

    SpotifyService.cpp

  ==============================================================================
*/

#include "SpotifyService.h"
#include "UserPreferences.h"

namespace
{
    // Spotify's Auth/Web API surface -- stable for years, but worth a quick
    // check against their current docs if either ever starts returning
    // unexpected errors (same caveat as this session's ElevenLabs integration).
    constexpr const char* kAuthUrl  = "https://accounts.spotify.com/authorize";
    constexpr const char* kTokenUrl = "https://accounts.spotify.com/api/token";
    constexpr const char* kApiBase  = "https://api.spotify.com/v1";
    constexpr const char* kScopes   = "user-read-playback-state user-modify-playback-state playlist-read-private";

    // Fixed, not OS-assigned -- this exact redirect URI must be registered
    // in the Spotify Dashboard when the host creates their developer app.
    constexpr int kRedirectPort = 8899;
    constexpr const char* kRedirectUri = "http://127.0.0.1:8899/callback";

    constexpr int kConnectTimeoutMs = 10000;
    // A real human login (credentials, maybe 2FA, then the consent screen)
    // can easily take a couple of minutes -- 2 minutes was measured too
    // tight and caused the listener to close the port out from under a
    // slow-but-legitimate login, producing a browser-side "connection
    // refused" right when the redirect finally arrived. 10 minutes costs
    // nothing if abandoned (one idle background thread + an open loopback
    // port), so it's generous on purpose.
    constexpr int kOAuthTimeoutMs   = 600000;

    juce::String randomUrlSafeString (int length)
    {
        static const char kChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        juce::Random rng (juce::Time::currentTimeMillis() ^ (juce::int64) (juce::pointer_sized_int) &length);
        juce::String out;
        for (int i = 0; i < length; ++i)
            out += juce::String::charToString ((juce::juce_wchar) kChars[rng.nextInt (64)]);
        return out;
    }

    // Base64 with URL-safe substitutions and no padding, per RFC 7636.
    juce::String base64Url (const void* data, size_t numBytes)
    {
        auto s = juce::Base64::toBase64 (data, numBytes);
        return s.replaceCharacter ('+', '-').replaceCharacter ('/', '_').removeCharacters ("=");
    }

    juce::String pkceChallengeFor (const juce::String& verifier)
    {
        const juce::SHA256 hash (verifier.toRawUTF8(), (size_t) verifier.getNumBytesAsUTF8());
        const auto raw = hash.getRawData();
        return base64Url (raw.getData(), raw.getSize());
    }

    juce::String formEncode (const std::vector<std::pair<juce::String, juce::String>>& params)
    {
        juce::StringArray parts;
        for (auto& p : params)
            parts.add (p.first + "=" + juce::URL::addEscapeChars (p.second, true));
        return parts.joinIntoString ("&");
    }

    // Pulls a query-string parameter's value out of an HTTP request line like
    // "GET /callback?code=AQD...&state=xyz HTTP/1.1". Returns {} if absent.
    juce::String extractQueryParam (const juce::String& requestLine, const juce::String& name)
    {
        const auto firstLine = requestLine.upToFirstOccurrenceOf ("\r\n", false, false);
        const auto qIndex = firstLine.indexOfChar ('?');
        const auto spIndex = firstLine.indexOfChar (' ', juce::jmax (0, qIndex));
        if (qIndex < 0)
            return {};
        const auto query = spIndex > qIndex ? firstLine.substring (qIndex + 1, spIndex)
                                            : firstLine.substring (qIndex + 1);
        for (auto& pair : juce::StringArray::fromTokens (query, "&", ""))
        {
            const auto eq = pair.indexOfChar ('=');
            if (eq < 0) continue;
            if (pair.substring (0, eq) == name)
                return juce::URL::removeEscapeChars (pair.substring (eq + 1));
        }
        return {};
    }

    // Mirrors FirestoreClient::httpJsonRaw's proven shape exactly --
    // withPOSTData() + withHttpRequestCmd() together for any verb with a
    // body, withHttpRequestCmd() alone for GET/DELETE.
    juce::var httpCall (const juce::URL& url, const juce::String& method, const juce::String& body,
                        const juce::String& contentType, const juce::StringArray& extraHeaders, int* statusOut)
    {
        juce::URL u = url;
        if (body.isNotEmpty())
            u = u.withPOSTData (body);

        juce::StringArray headers;
        if (contentType.isNotEmpty())
            headers.add ("Content-Type: " + contentType);
        headers.addArray (extraHeaders);

        int status = 0;
        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (kConnectTimeoutMs)
                        .withExtraHeaders (headers.joinIntoString ("\r\n"))
                        .withHttpRequestCmd (method)
                        .withStatusCode (&status);

        std::unique_ptr<juce::InputStream> stream (u.createInputStream (opts));
        if (statusOut != nullptr) *statusOut = status;
        if (stream == nullptr)
            return {};

        const auto responseBody = stream->readEntireStreamAsString();
        if (responseBody.isEmpty())
            return {};
        return juce::JSON::parse (responseBody);
    }

    // One-shot local HTTP listener for the OAuth redirect. Lives only for
    // the duration of a single connect() call.
    class LoopbackListener
    {
    public:
        bool start (int port) { return socket_.createListener (port, "127.0.0.1"); }
        void cancel() { socket_.close(); }

        // Blocks until a browser redirect carrying a "code" or "error"
        // query parameter arrives (or cancel() is called from another
        // thread). Returns the raw HTTP request line, or an empty string
        // if the listener was cancelled/failed.
        //
        // Browsers routinely open more than one connection to a freshly
        // navigated-to origin -- most commonly a GET /favicon.ico alongside
        // the real navigation request, sometimes arriving on a separate
        // TCP connection that gets accepted before the real one. A single
        // accept-and-done call can grab that spurious request, respond to
        // it, and return -- leaving the real callback connection sitting
        // fully connected (TCP handshake complete) but never serviced,
        // which is exactly what a browser "hangs, then eventually errors"
        // looks like from the outside. Looping past anything that isn't
        // the real callback fixes that.
        juce::String waitForRedirect()
        {
            for (;;)
            {
                std::unique_ptr<juce::StreamingSocket> client (socket_.waitForNextConnection());
                if (client == nullptr)
                    return {};

                // Drain the FULL request (request line + every header) before
                // writing a response and letting `client` go out of scope
                // (closing the socket). A real browser request -- long
                // authorization code in the URL, plus the usual dozen+
                // Chrome headers -- easily exceeds a single recv(), and
                // closing a socket while the OS still has unread incoming
                // bytes buffered makes it send a hard RST instead of a
                // clean FIN. Browsers surface that as a connection error
                // (seen in practice as ERR_SOCKET_NOT_CONNECTED), not just
                // "the response arrived a little late" -- so the fix is to
                // keep reading until nothing more arrives for a short
                // stretch, not just until we've seen enough to extract the
                // code.
                juce::MemoryBlock requestData;
                char chunk[4096];
                for (;;)
                {
                    if (client->waitUntilReady (true, 200) != 1)
                        break; // nothing more arrived within 200ms -- request is fully sent
                    const int bytesRead = client->read (chunk, (int) sizeof (chunk), false);
                    if (bytesRead <= 0)
                        break;
                    requestData.append (chunk, (size_t) bytesRead);
                    if (requestData.getSize() > 65536)
                        break; // safety cap, not a realistic request size
                }

                if (requestData.isEmpty())
                    continue;

                const juce::String requestText (juce::String::fromUTF8 (
                    static_cast<const char*> (requestData.getData()), (int) requestData.getSize()));
                const bool isRealCallback = requestText.contains ("code=") || requestText.contains ("error=");

                if (isRealCallback)
                {
                    static const char* kResponseBody =
                        "<html><body style=\"font-family:sans-serif;text-align:center;padding-top:4em;\">"
                        "<h2>Encore Karaoke</h2><p>You're connected -- you can close this tab and return to Encore.</p>"
                        "</body></html>";
                    const juce::String response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                                                  + juce::String (kResponseBody);
                    client->write (response.toRawUTF8(), (int) response.getNumBytesAsUTF8());
                    return requestText;
                }

                // Anything else (favicon.ico, a bare preconnect, etc.) --
                // send a quick, harmless response and keep waiting.
                const juce::String ignoredResponse = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
                client->write (ignoredResponse.toRawUTF8(), (int) ignoredResponse.getNumBytesAsUTF8());
            }
        }

    private:
        juce::StreamingSocket socket_;
    };
}

//==============================================================================
SpotifyService& SpotifyService::getInstance()
{
    static SpotifyService instance;
    return instance;
}

bool SpotifyService::isConnected() const
{
    return UserPreferences::getInstance().getSpotifyRefreshToken().isNotEmpty();
}

void SpotifyService::disconnect()
{
    {
        const juce::ScopedLock sl (lock_);
        accessToken_ = {};
        accessTokenExpiryMs_ = 0;
    }
    UserPreferences::getInstance().setSpotifyRefreshToken ({});
    UserPreferences::getInstance().setSpotifyAccountName ({});
}

void SpotifyService::connect (const juce::String& clientId,
                              std::function<void (bool, juce::String, juce::String)> onDone)
{
    juce::Thread::launch ([this, clientId, onDone]
    {
        auto fail = [onDone] (const juce::String& error)
        {
            if (onDone)
                juce::MessageManager::callAsync ([onDone, error] { onDone (false, {}, error); });
        };

        if (clientId.trim().isEmpty())
        {
            fail ("Enter your Spotify Client ID first.");
            return;
        }

        const auto codeVerifier = randomUrlSafeString (64);
        const auto codeChallenge = pkceChallengeFor (codeVerifier);
        const auto state = randomUrlSafeString (16);

        auto listener = std::make_shared<LoopbackListener>();
        if (! listener->start (kRedirectPort))
        {
            fail ("Could not start local listener on port " + juce::String (kRedirectPort)
                 + " -- is another app using it? Close it and try again.");
            return;
        }

        auto cancelled = std::make_shared<std::atomic<bool>> (false);
        juce::Thread::launch ([listener, cancelled]
        {
            juce::Thread::sleep (kOAuthTimeoutMs);
            if (! cancelled->load())
                listener->cancel();
        });

        juce::URL authUrl (kAuthUrl);
        authUrl = authUrl.withParameter ("client_id", clientId.trim())
                          .withParameter ("response_type", "code")
                          .withParameter ("redirect_uri", kRedirectUri)
                          .withParameter ("code_challenge_method", "S256")
                          .withParameter ("code_challenge", codeChallenge)
                          .withParameter ("scope", kScopes)
                          .withParameter ("state", state);
        authUrl.launchInDefaultBrowser();

        const auto requestLine = listener->waitForRedirect();
        *cancelled = true;

        if (requestLine.isEmpty())
        {
            fail ("Timed out waiting for Spotify sign-in.");
            return;
        }

        const auto returnedState = extractQueryParam (requestLine, "state");
        if (returnedState != state)
        {
            fail ("Security check failed (state mismatch) -- please try again.");
            return;
        }

        const auto code = extractQueryParam (requestLine, "code");
        if (code.isEmpty())
        {
            const auto err = extractQueryParam (requestLine, "error");
            fail (err.isNotEmpty() ? ("Spotify sign-in failed: " + err) : "Spotify sign-in was cancelled.");
            return;
        }

        const auto body = formEncode ({
            { "grant_type",    "authorization_code" },
            { "code",          code },
            { "redirect_uri",  kRedirectUri },
            { "client_id",     clientId.trim() },
            { "code_verifier", codeVerifier }
        });

        int status = 0;
        const auto tokenResponse = httpCall (juce::URL (kTokenUrl), "POST", body,
                                             "application/x-www-form-urlencoded", {}, &status);

        if (status != 200)
        {
            fail ("Could not exchange authorization code (HTTP " + juce::String (status) + ").");
            return;
        }

        const auto newAccessToken  = tokenResponse.getProperty ("access_token", "").toString();
        const auto newRefreshToken = tokenResponse.getProperty ("refresh_token", "").toString();
        const int expiresIn        = (int) tokenResponse.getProperty ("expires_in", 3600);

        if (newAccessToken.isEmpty() || newRefreshToken.isEmpty())
        {
            fail ("Spotify didn't return the expected tokens.");
            return;
        }

        {
            const juce::ScopedLock sl (lock_);
            clientId_ = clientId.trim();
            accessToken_ = newAccessToken;
            accessTokenExpiryMs_ = juce::Time::getCurrentTime().toMilliseconds() + (juce::int64) (expiresIn - 60) * 1000;
        }
        UserPreferences::getInstance().setSpotifyClientId (clientId.trim());
        UserPreferences::getInstance().setSpotifyRefreshToken (newRefreshToken);

        int meStatus = 0;
        const auto meJson = apiCall ("/me", "GET", {}, &meStatus);
        const auto accountName = meStatus == 200 ? meJson.getProperty ("display_name", "").toString() : juce::String();

        if (onDone)
            juce::MessageManager::callAsync ([onDone, accountName] { onDone (true, accountName, {}); });
    });
}

bool SpotifyService::refreshAccessToken()
{
    const auto refreshToken = UserPreferences::getInstance().getSpotifyRefreshToken();
    const auto clientId = UserPreferences::getInstance().getSpotifyClientId();
    if (refreshToken.isEmpty() || clientId.isEmpty())
        return false;

    const auto body = formEncode ({
        { "grant_type",    "refresh_token" },
        { "refresh_token", refreshToken },
        { "client_id",     clientId }
    });

    int status = 0;
    const auto response = httpCall (juce::URL (kTokenUrl), "POST", body,
                                    "application/x-www-form-urlencoded", {}, &status);
    if (status != 200)
        return false;

    const auto newAccessToken = response.getProperty ("access_token", "").toString();
    if (newAccessToken.isEmpty())
        return false;

    const int expiresIn = (int) response.getProperty ("expires_in", 3600);
    // Spotify only returns a new refresh_token occasionally -- keep the old
    // one unless a replacement is actually provided.
    const auto newRefreshToken = response.getProperty ("refresh_token", "").toString();

    {
        const juce::ScopedLock sl (lock_);
        clientId_ = clientId;
        accessToken_ = newAccessToken;
        accessTokenExpiryMs_ = juce::Time::getCurrentTime().toMilliseconds() + (juce::int64) (expiresIn - 60) * 1000;
    }
    if (newRefreshToken.isNotEmpty())
        UserPreferences::getInstance().setSpotifyRefreshToken (newRefreshToken);

    return true;
}

bool SpotifyService::ensureFreshAccessToken()
{
    {
        const juce::ScopedLock sl (lock_);
        if (accessToken_.isNotEmpty() && juce::Time::getCurrentTime().toMilliseconds() < accessTokenExpiryMs_)
            return true;
    }
    return refreshAccessToken();
}

juce::var SpotifyService::apiCall (const juce::String& endpointPath, const juce::String& method,
                                   const juce::String& jsonBody, int* statusOut)
{
    if (! ensureFreshAccessToken())
    {
        if (statusOut) *statusOut = 401;
        return {};
    }

    auto callOnce = [&] (int* status) -> juce::var
    {
        juce::String bearer;
        {
            const juce::ScopedLock sl (lock_);
            bearer = accessToken_;
        }
        return httpCall (juce::URL (juce::String (kApiBase) + endpointPath), method, jsonBody,
                         jsonBody.isNotEmpty() ? "application/json" : juce::String(),
                         { "Authorization: Bearer " + bearer }, status);
    };

    int status = 0;
    auto result = callOnce (&status);

    if (status == 401 && refreshAccessToken())
        result = callOnce (&status);

    if (statusOut) *statusOut = status;
    return result;
}

void SpotifyService::fetchPlaylists (std::function<void (bool, std::vector<PlaylistInfo>, juce::String)> onDone)
{
    juce::Thread::launch ([this, onDone]
    {
        std::vector<PlaylistInfo> playlists;
        juce::String nextPath = "/me/playlists?limit=50";

        while (nextPath.isNotEmpty())
        {
            int status = 0;
            const auto page = apiCall (nextPath, "GET", {}, &status);
            if (status != 200)
            {
                if (onDone)
                    juce::MessageManager::callAsync ([onDone, status]
                    {
                        onDone (false, {}, "Could not load playlists (HTTP " + juce::String (status) + ").");
                    });
                return;
            }

            const auto items = page.getProperty ("items", {});
            for (int i = 0; i < items.size(); ++i)
            {
                PlaylistInfo info;
                info.uri  = items[i].getProperty ("uri", "").toString();
                info.name = items[i].getProperty ("name", "").toString();
                if (info.uri.isNotEmpty())
                    playlists.push_back (std::move (info));
            }

            const auto next = page.getProperty ("next", juce::var());
            if (next.isVoid() || next.toString().isEmpty())
                break;
            // "next" is a full URL -- strip the API base since apiCall() adds it back.
            nextPath = next.toString().fromFirstOccurrenceOf (kApiBase, false, false);
        }

        if (onDone)
            juce::MessageManager::callAsync ([onDone, playlists] { onDone (true, playlists, {}); });
    });
}

void SpotifyService::playPlaylist (const juce::String& playlistUri, std::function<void (bool, juce::String)> onDone)
{
    juce::Thread::launch ([this, playlistUri, onDone]
    {
        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty ("context_uri", playlistUri);

        int status = 0;
        apiCall ("/me/player/play", "PUT", juce::JSON::toString (juce::var (body.get())), &status);

        if (onDone)
            juce::MessageManager::callAsync ([onDone, status]
            {
                if (status == 204 || status == 200)
                    onDone (true, {});
                else if (status == 404)
                    onDone (false, "No active Spotify device -- open Spotify on this computer, your phone, or a speaker first.");
                else
                    onDone (false, "Could not start playback (HTTP " + juce::String (status) + ").");
            });
    });
}

void SpotifyService::play (std::function<void (bool, juce::String)> onDone)
{
    juce::Thread::launch ([this, onDone]
    {
        int status = 0;
        apiCall ("/me/player/play", "PUT", {}, &status);
        if (onDone)
            juce::MessageManager::callAsync ([onDone, status]
            {
                onDone (status == 204 || status == 200,
                       (status == 204 || status == 200) ? juce::String()
                           : (status == 404 ? "No active Spotify device." : "Could not resume playback."));
            });
    });
}

void SpotifyService::pause (std::function<void (bool, juce::String)> onDone)
{
    juce::Thread::launch ([this, onDone]
    {
        int status = 0;
        apiCall ("/me/player/pause", "PUT", {}, &status);
        if (onDone)
            juce::MessageManager::callAsync ([onDone, status]
            {
                onDone (status == 204 || status == 200, (status == 204 || status == 200) ? juce::String() : "Could not pause playback.");
            });
    });
}

void SpotifyService::skipNext (std::function<void (bool, juce::String)> onDone)
{
    juce::Thread::launch ([this, onDone]
    {
        int status = 0;
        apiCall ("/me/player/next", "POST", {}, &status);
        if (onDone)
            juce::MessageManager::callAsync ([onDone, status]
            {
                onDone (status == 204 || status == 200, (status == 204 || status == 200) ? juce::String() : "Could not skip track.");
            });
    });
}

void SpotifyService::skipPrevious (std::function<void (bool, juce::String)> onDone)
{
    juce::Thread::launch ([this, onDone]
    {
        int status = 0;
        apiCall ("/me/player/previous", "POST", {}, &status);
        if (onDone)
            juce::MessageManager::callAsync ([onDone, status]
            {
                onDone (status == 204 || status == 200, (status == 204 || status == 200) ? juce::String() : "Could not go to the previous track.");
            });
    });
}

void SpotifyService::setVolume (int percent0to100, std::function<void (bool, juce::String)> onDone)
{
    const int clamped = juce::jlimit (0, 100, percent0to100);
    juce::Thread::launch ([this, clamped, onDone]
    {
        int status = 0;
        apiCall ("/me/player/volume?volume_percent=" + juce::String (clamped), "PUT", {}, &status);
        if (onDone)
            juce::MessageManager::callAsync ([onDone, status]
            {
                onDone (status == 204 || status == 200, (status == 204 || status == 200) ? juce::String() : "Could not set volume.");
            });
    });
}

void SpotifyService::getPlaybackState (std::function<void (bool, PlaybackState, juce::String)> onDone)
{
    juce::Thread::launch ([this, onDone]
    {
        int status = 0;
        const auto json = apiCall ("/me/player", "GET", {}, &status);

        PlaybackState state;

        if (status == 204 || (status == 200 && json.isVoid()))
        {
            // 204 = no active device.
            if (onDone)
                juce::MessageManager::callAsync ([onDone, state] { onDone (true, state, {}); });
            return;
        }

        if (status != 200)
        {
            if (onDone)
                juce::MessageManager::callAsync ([onDone, state, status]
                {
                    onDone (false, state, "Could not read Spotify playback state (HTTP " + juce::String (status) + ").");
                });
            return;
        }

        state.hasActiveDevice = true;
        state.isPlaying = (bool) json.getProperty ("is_playing", false);
        state.positionSeconds = (double) json.getProperty ("progress_ms", 0) / 1000.0;
        state.volumePercent = (int) json.getProperty ("device", juce::var()).getProperty ("volume_percent", 100);

        const auto item = json.getProperty ("item", juce::var());
        if (item.isObject())
        {
            state.trackName = item.getProperty ("name", "").toString();
            state.durationSeconds = (double) item.getProperty ("duration_ms", 0) / 1000.0;

            const auto artists = item.getProperty ("artists", juce::var());
            if (artists.isArray() && artists.size() > 0)
                state.artistName = artists[0].getProperty ("name", "").toString();
        }

        if (onDone)
            juce::MessageManager::callAsync ([onDone, state] { onDone (true, state, {}); });
    });
}
