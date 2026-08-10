/*
  ==============================================================================

    SpotifyService.h

    Remote-controls Spotify from the Ribbon's Background Music panel
    (Source/UI/BackgroundMusicLibraryPanel.h) via Spotify's Web API. This is
    deliberately NOT audio playback through this app -- Spotify (like Apple
    Music) is DRM-locked, so a third-party desktop app can only authenticate
    and drive playback on whatever Spotify app/device the host already has
    running (desktop client, phone, a Connect speaker); the audio itself
    never passes through Encore's own mixer. Playback-control endpoints
    require the host's account to have Spotify Premium.

    Auth is Authorization Code + PKCE (no client secret needed, safe for a
    desktop app with no secure secret storage):
      1) generateAuthUrl() builds the browser URL + starts a one-shot local
         HTTP listener on 127.0.0.1 to catch the redirect.
      2) The host logs in / consents in their system browser.
      3) The listener captures the authorization code, this service
         exchanges it for an access token + refresh token.
      4) The refresh token is persisted (UserPreferences::getSpotifyRefreshToken,
         same security posture as the ElevenLabs API key -- not additionally
         encrypted); the access token is kept in memory only and silently
         refreshed as needed.

    All network/file work runs on a background juce::Thread; callbacks are
    always marshalled back to the message thread, matching every other
    *Service in this codebase.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

class SpotifyService
{
public:
    static SpotifyService& getInstance();

    struct PlaylistInfo
    {
        juce::String uri;    // e.g. "spotify:playlist:37i9dQZF1..."
        juce::String name;
    };

    struct PlaybackState
    {
        bool isPlaying = false;
        bool hasActiveDevice = false;   // false = nothing playing anywhere; host needs to open Spotify first
        juce::String trackName;
        juce::String artistName;
        double positionSeconds = 0.0;
        double durationSeconds = 0.0;
        int volumePercent = 100;
    };

    /** True once a refresh token is on file -- doesn't guarantee it's still
        valid (Spotify can revoke it), just that connect() has succeeded
        before. */
    bool isConnected() const;

    /** Clears the stored refresh token and in-memory access token. Does not
        call Spotify -- there's no "revoke" endpoint to hit; this just makes
        Encore forget the connection. */
    void disconnect();

    /** Runs the full OAuth flow: opens the system browser, waits for the
        redirect on a local loopback listener, exchanges the code for
        tokens, and fetches the account display name. onDone is always
        called, on the message thread. */
    void connect (const juce::String& clientId,
                 std::function<void (bool ok, juce::String accountName, juce::String error)> onDone);

    /** The host's own playlists (paginated internally). onDone is always
        called, on the message thread. */
    void fetchPlaylists (std::function<void (bool ok, std::vector<PlaylistInfo> playlists, juce::String error)> onDone);

    /** Starts `playlistUri` playing on whichever device is currently
        active. Fails with a clear "no active device" error if the host
        doesn't have Spotify open anywhere. */
    void playPlaylist (const juce::String& playlistUri, std::function<void (bool ok, juce::String error)> onDone);

    void play (std::function<void (bool ok, juce::String error)> onDone);
    void pause (std::function<void (bool ok, juce::String error)> onDone);
    void skipNext (std::function<void (bool ok, juce::String error)> onDone);
    void skipPrevious (std::function<void (bool ok, juce::String error)> onDone);
    void setVolume (int percent0to100, std::function<void (bool ok, juce::String error)> onDone);

    /** Polled by MainComponent (~every 3s, matching RequestService's
        cadence) while Spotify is the active background-music source, then
        fed into the same RibbonMenu::setBackgroundState/setBackgroundTrackInfo
        calls the local player already drives. */
    void getPlaybackState (std::function<void (bool ok, PlaybackState state, juce::String error)> onDone);

private:
    SpotifyService() = default;
    ~SpotifyService() = default;

    // Ensures a valid access token is in accessToken_, refreshing via the
    // stored refresh token first if necessary. Runs synchronously on
    // whichever background thread calls it -- every public method above
    // calls this at the start of its own juce::Thread::launch body.
    bool ensureFreshAccessToken();

    bool refreshAccessToken();

    // Generic authenticated Web API call. `method` is "GET"/"PUT"/"POST".
    // `jsonBody` empty means no body. Retries once after a token refresh on
    // a 401. Returns the parsed JSON response (may be a void var for 204s).
    juce::var apiCall (const juce::String& endpointPath, const juce::String& method,
                      const juce::String& jsonBody, int* statusOut);

    juce::String clientId_;
    juce::String accessToken_;       // in-memory only
    juce::int64 accessTokenExpiryMs_ = 0;

    juce::CriticalSection lock_;

    JUCE_DECLARE_NON_COPYABLE (SpotifyService)
};
