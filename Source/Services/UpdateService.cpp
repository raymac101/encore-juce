/*
  ==============================================================================

    UpdateService.cpp

  ==============================================================================
*/

#include "UpdateService.h"

#if JUCE_WINDOWS
 #include <windows.h>
#elif JUCE_MAC
 #include <unistd.h>
#endif

namespace
{
    // TODO: point this at the real Firebase Hosting URL once Hosting is
    // enabled on the project (see the distribution plan's Prerequisites).
    constexpr const char* kManifestUrl = "https://download.karaokeworld.net/encore/manifest.json";
    constexpr int kConnectionTimeoutMs = 6000;

    // Parses "X.Y.Z" into three non-negative ints. Returns false (and leaves
    // out untouched) for anything malformed -- callers must treat that as
    // "not comparable," never as "newer."
    bool parseVersion (const juce::String& v, int (&out)[3])
    {
        const auto parts = juce::StringArray::fromTokens (v, ".", {});
        if (parts.size() < 3)
            return false;

        for (int i = 0; i < 3; ++i)
        {
            const auto p = parts[i].trim();
            if (p.isEmpty() || ! p.containsOnly ("0123456789"))
                return false;
            out[i] = p.getIntValue();
        }
        return true;
    }
}

//==============================================================================
UpdateService& UpdateService::getInstance()
{
    static UpdateService instance;
    return instance;
}

bool UpdateService::isRemoteVersionNewer (const juce::String& remoteVersion, const juce::String& localVersion)
{
    int remote[3];
    int local[3];

    // Malformed manifest field, or a version string we can't parse -- fail
    // safe and report "nothing newer" rather than guessing.
    if (! parseVersion (remoteVersion, remote) || ! parseVersion (localVersion, local))
        return false;

    for (int i = 0; i < 3; ++i)
    {
        if (remote[i] > local[i]) return true;
        if (remote[i] < local[i]) return false;
    }

    return false; // equal versions -- never offer a "downgrade" or a no-op reinstall
}

juce::File UpdateService::getUpdatesDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("EncoreKaraoke")
                   .getChildFile ("updates");

    if (! dir.exists())
        dir.createDirectory();

    return dir;
}

void UpdateService::checkForUpdates (ReadyCallback onReady)
{
    juce::Thread::launch ([this, onReady]
    {
        // Every early-return below is a deliberate silent no-op: this runs
        // unconditionally on every launch, so a network hiccup, a malformed
        // manifest, or a slow server must never surface as an error or delay
        // getting into the app.
        const juce::URL manifestUrl (kManifestUrl);

        int statusCode = 0;
        auto stream = manifestUrl.createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs (kConnectionTimeoutMs)
                .withStatusCode (&statusCode));

        if (stream == nullptr || statusCode != 200)
            return;

        const auto manifestText = stream->readEntireStreamAsString();
        const auto parsed = juce::JSON::parse (manifestText);
        if (! parsed.isObject())
            return;

        const auto latestVersion = parsed.getProperty ("latestVersion", "").toString();
        if (latestVersion.isEmpty())
            return;

        const juce::String localVersion = ProjectInfo::versionString;
        if (! isRemoteVersionNewer (latestVersion, localVersion))
            return;

        juce::String platformKey;
       #if JUCE_WINDOWS
        platformKey = "windows";
       #elif JUCE_MAC
        platformKey = "macos";
       #endif
        if (platformKey.isEmpty())
            return; // no installer story for this platform

        const auto platforms = parsed.getProperty ("platforms", {});
        if (! platforms.isObject())
            return;

        const auto platformEntry = platforms.getProperty (platformKey, {});
        if (! platformEntry.isObject())
            return;

        const auto fileUrlString  = platformEntry.getProperty ("url", "").toString();
        const auto expectedSha256 = platformEntry.getProperty ("sha256", "").toString();
        if (fileUrlString.isEmpty() || expectedSha256.isEmpty())
            return;

        const juce::URL fileUrl (fileUrlString);
        if (! fileUrl.getScheme().equalsIgnoreCase ("https"))
            return; // HTTPS only, no exceptions -- manifest and installer alike

        const auto releaseNotesUrl = parsed.getProperty ("releaseNotesUrl", "").toString();
        downloadAndVerify (fileUrl, expectedSha256, latestVersion, releaseNotesUrl, onReady);
    });
}

void UpdateService::downloadAndVerify (const juce::URL& fileUrl,
                                        const juce::String& expectedSha256Hex,
                                        const juce::String& version,
                                        const juce::String& releaseNotesUrl,
                                        ReadyCallback onReady)
{
    // Still running on checkForUpdates' background thread.
    int statusCode = 0;
    auto stream = fileUrl.createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs (kConnectionTimeoutMs)
            .withStatusCode (&statusCode));

    if (stream == nullptr || statusCode != 200)
        return;

    const auto destDir = getUpdatesDirectory();
    const auto fileName = fileUrl.getFileName();
    auto destFile = destDir.getChildFile (fileName.isNotEmpty() ? fileName
                                                                 : ("EncoreKaraoke-update-" + version));
    destFile.deleteFile();

    {
        juce::FileOutputStream out (destFile);
        if (! out.openedOk())
            return;

        juce::HeapBlock<char> buffer (1 << 16);
        for (;;)
        {
            const auto bytesRead = stream->read (buffer.getData(), 1 << 16);
            if (bytesRead <= 0)
                break;
            out.write (buffer.getData(), (size_t) bytesRead);
        }
        out.flush();
    }

    if (! destFile.existsAsFile())
        return;

    // Verify before ever trusting this file -- a corrupted or tampered
    // download must never reach the "ready to install" state.
    const juce::SHA256 actualHash (destFile);
    if (! actualHash.toHexString().equalsIgnoreCase (expectedSha256Hex))
    {
        destFile.deleteFile();
        return;
    }

    pendingInstallerFile_ = destFile;
    pendingVersion_ = version;
    readyToInstall_ = true;

    if (onReady)
        juce::MessageManager::callAsync ([onReady, version, releaseNotesUrl]
        {
            onReady (version, releaseNotesUrl);
        });
}

void UpdateService::restartAndInstall()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (! readyToInstall_.load() || ! pendingInstallerFile_.existsAsFile())
        return;

    const auto installerPath = pendingInstallerFile_.getFullPathName();

    // Never race the installer against our own teardown: hand off to a
    // small detached helper that waits for this exact process to fully exit
    // before it launches the installer, rather than racing a direct
    // startAsProcess() call against quit() (Windows in particular refuses to
    // overwrite/replace a still-running exe).
   #if JUCE_WINDOWS
    const auto pid = (unsigned long) ::GetCurrentProcessId();
    const juce::String script =
        "Wait-Process -Id " + juce::String (pid) + " -ErrorAction SilentlyContinue; "
        "Start-Process -FilePath '" + installerPath.replace ("'", "''") + "'";

    juce::ChildProcess helper;
    helper.start (juce::StringArray { "powershell.exe", "-WindowStyle", "Hidden", "-Command", script });
   #elif JUCE_MAC
    const auto pid = (long) ::getpid();
    const juce::String script =
        "while kill -0 " + juce::String (pid) + " 2>/dev/null; do sleep 0.2; done; "
        "open \"" + installerPath.replace ("\"", "\\\"") + "\"";

    juce::ChildProcess helper;
    helper.start (juce::StringArray { "/bin/sh", "-c", script });
   #endif

    // Reuses the exact same graceful shutdown path as the window's own
    // close button (Main.cpp's MainWindow::closeButtonPressed()) -- no
    // separate teardown logic to keep in sync.
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
