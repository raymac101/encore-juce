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

    /** Called on the message thread once the manifest has been checked,
        exactly once per call -- unlike checkForUpdates(), which stays
        silent when there's nothing newer. Does not download anything;
        follow a "true" result with downloadUpdateNow() before offering to
        restart. Used by the menu's manual "Check for Updates" action. */
    using ManifestCheckCallback = std::function<void (bool hasUpdate, juce::String version, juce::String releaseNotesUrl)>;
    void checkForUpdatesManual (ManifestCheckCallback onResult);

    /** Downloads and sha256-verifies the update most recently found by
        checkForUpdatesManual(). Calls onDone(true) once
        isUpdateReadyToInstall() is safe to act on (i.e. restartAndInstall()
        can be called), onDone(false) on any failure (network, checksum
        mismatch, no prior successful manual check). Runs on a background
        thread; onDone is always called back on the message thread. */
    using DownloadCallback = std::function<void (bool success)>;
    void downloadUpdateNow (DownloadCallback onDone);

    bool isUpdateReadyToInstall() const noexcept { return readyToInstall_.load(); }
    juce::String getPendingVersion() const { return pendingVersion_; }

    /** Runs the app's normal shutdown sequence, then hands off to a tiny
        detached helper that waits for this process to fully exit before
        launching the verified installer -- never races the installer
        against our own teardown (Windows in particular locks the running
        exe). Must be called from the message thread. No-op (returns false)
        if no verified installer is pending; returns true once shutdown has
        been kicked off. */
    bool restartAndInstall();

private:
    UpdateService() = default;
    ~UpdateService() = default;

    static bool isRemoteVersionNewer (const juce::String& remoteVersion, const juce::String& localVersion);
    static juce::File getUpdatesDirectory();

    /** Shared by checkForUpdates() and checkForUpdatesManual(): fetches and
        parses the manifest, and fills `out` if a newer build is available
        for this platform. Returns false (leaving `out` untouched) for
        anything unparsable/missing/not-newer -- callers must treat that as
        "nothing to offer," never as an error. Runs synchronously; callers
        are responsible for calling it from a background thread. */
    struct ManifestUpdateInfo
    {
        juce::URL fileUrl;
        juce::String sha256Hex;
        juce::String version;
        juce::String releaseNotesUrl;
    };
    static bool fetchManifestUpdateInfo (ManifestUpdateInfo& out);

    /** Downloads fileUrl, verifies it against expectedSha256Hex, and on
        success marks it ready to install (readyToInstall_/
        pendingInstallerFile_/pendingVersion_) and -- if onReady is set --
        invokes it on the message thread. Returns true on success, false on
        any failure (every failure path is a silent false: network hiccup,
        malformed download, checksum mismatch). Runs synchronously; callers
        are responsible for calling it from a background thread. */
    bool downloadAndVerify (const juce::URL& fileUrl,
                            const juce::String& expectedSha256Hex,
                            const juce::String& version,
                            const juce::String& releaseNotesUrl,
                            ReadyCallback onReady);

    std::atomic<bool> readyToInstall_ { false };
    juce::File pendingInstallerFile_;
    juce::String pendingVersion_;

    // Set by a successful checkForUpdatesManual() call; consumed by
    // downloadUpdateNow(). Both only ever run one at a time in practice
    // (gated by the user dismissing/acting on the "Check for Updates"
    // dialog before triggering the next step), so no extra locking here.
    juce::URL    pendingCheckedFileUrl_;
    juce::String pendingCheckedSha256_;
    juce::String pendingCheckedVersion_;
    juce::String pendingCheckedReleaseNotesUrl_;

    JUCE_DECLARE_NON_COPYABLE (UpdateService)
};
