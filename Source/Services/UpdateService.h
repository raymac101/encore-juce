/*
  ==============================================================================

    UpdateService.h

    Launch-time-only update check. Fetches a small static JSON manifest
    (hosted on Firebase Hosting, no auth required), compares its
    "latestVersion" against this build's version, and if newer, downloads +
    sha256-verifies the platform installer in the background.

    This is the ONLY place an update is ever checked for — there is no
    periodic re-check timer. Encore runs during live, unrecoverable KJ shows,
    so every failure path here (network down, timeout, malformed manifest,
    checksum mismatch, a manifest that's actually older than this build)
    must resolve to "no update available" silently: no dialog, no retry
    loop, no delay to the login window. See CLAUDE.md's threading model —
    all network I/O here runs on a background juce::Thread::launch, with
    the completion callback marshalled to the message thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>

class UpdateService
{
public:
    static UpdateService& getInstance();

    /** Called on the message thread once a newer build has been downloaded
        and its sha256 verified against the manifest. Never called if there's
        no update, or if any step failed -- callers should treat "never
        called" as the normal case, not an error. */
    using ReadyCallback = std::function<void (juce::String newVersion, juce::String releaseNotesUrl)>;

    /** Fire-and-forget. Safe to call once, early, at startup. Does not block
        and never delays the caller. */
    void checkForUpdates (ReadyCallback onReady);

    bool isUpdateReadyToInstall() const noexcept { return readyToInstall_.load(); }
    juce::String getPendingVersion() const { return pendingVersion_; }

    /** Runs the app's normal shutdown sequence, then hands off to a tiny
        detached helper that waits for this process to fully exit before
        launching the verified installer -- never races the installer
        against our own teardown (Windows in particular locks the running
        exe). Must be called from the message thread. No-op if no verified
        installer is pending. */
    void restartAndInstall();

private:
    UpdateService() = default;
    ~UpdateService() = default;

    static bool isRemoteVersionNewer (const juce::String& remoteVersion, const juce::String& localVersion);
    static juce::File getUpdatesDirectory();

    void downloadAndVerify (const juce::URL& fileUrl,
                            const juce::String& expectedSha256Hex,
                            const juce::String& version,
                            const juce::String& releaseNotesUrl,
                            ReadyCallback onReady);

    std::atomic<bool> readyToInstall_ { false };
    juce::File pendingInstallerFile_;
    juce::String pendingVersion_;

    JUCE_DECLARE_NON_COPYABLE (UpdateService)
};
