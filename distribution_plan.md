# Distribute Encore Karaoke: Installers + Auto-Update

## Context

Encore Karaoke has no packaging, signing, or update infrastructure today — confirmed by direct inspection: `CMakeLists.txt` has no CPack/install()/signing steps, there's no `.github/workflows/`, and no update-check code exists anywhere in `Source/`. The only "signing" that exists is an ad-hoc `codesign --force --deep --sign -` command recorded in `.claude/settings.local.json`, which is purely a local sandbox workaround for running unsigned Debug builds on this dev machine — irrelevant to real distribution.

The goal: customers download a real installer from your website, and the running app checks for updates and offers to install them — without ever risking an interruption mid-show, since this app's core use case is a live, unrecoverable KJ event.

Confirmed decisions:
- Both Windows and macOS get installers.
- Neither code-signing credential exists yet — the plan calls this out as a prerequisite, not something automatable.
- Distribution: your own website + Firebase Hosting (same Firebase project already used for Firestore/Auth) for installer files and the update manifest. No license keys/activation — free download.
- Update checks happen **only at launch**, before login — never mid-session, so a live show can never be interrupted by an update prompt.
- On finding an update: download it in the background, then ask before restarting/installing — never fully silent, never forced.
- Release builds are automated via GitHub Actions on a version tag (repo is already on GitHub: `raymac101/encore-juce`).

Existing pieces this plan builds on directly:
- Versioning: `project(EncoreJUCE VERSION 1.0.0)` (CMakeLists.txt:5) is the semver source of truth. `cmake/IncrementBuildNumber.cmake` auto-increments `build_number.txt` and generates `BuildInfo.h` with `ENCORE_VERSION_WITH_BUILD` ("1.0.0.160"), already `#include`d in `Source/Main.cpp` and `Source/UI/LoginWindow.cpp`.
- `Main.cpp::initialise()` already has a clean insertion point: right after the plugin-scan-mode check (`PluginHostService::handleScanCommandLineIfPresent()`, ~line 52) and before `showLoginWindow()` (~line 67) — before any login/venue bootstrap, exactly where a launch-time update check belongs.
- Threading pattern to reuse: background `juce::Thread::launch` for network I/O, results marshalled to the message thread via `juce::MessageManager::callAsync` (documented in CLAUDE.md, used throughout `Services/`).
- `*Service` singleton pattern to match (`RequestService`, `QueueService`, `PluginHostService`).
- `BUNDLE_ID "com.viracicom.encore"` (CMakeLists.txt) — needed for macOS notarization identifiers.

## Prerequisites — user action items (not automatable, do these before the first real release)

1. **Apple Developer Program membership** ($99/yr) → generate a **Developer ID Application** certificate (for signing the `.app`) and an **App Store Connect API key** (`.p8` + Key ID + Issuer ID) for headless notarization via `xcrun notarytool` — avoids Apple-ID/2FA prompts in CI.
2. **Windows code signing**: sign up for **Azure Trusted Signing** (Microsoft's cloud HSM signing service — cheap, CI-friendly, avoids the hardware-token requirement modern CAs impose on traditional OV certs). Alternative if preferred: SignPath.io or SSL.com eSigner.
3. **Firebase Hosting**: enable the Hosting product in the existing Firebase project (one console click) and optionally map a custom subdomain (e.g. `viracicom.com/download`) to it.
4. Generate a **Firebase service account** (or CI token) for `firebase deploy` from GitHub Actions.

Until #1/#2 are done: macOS builds will be **blocked by Gatekeeper** on a fresh customer machine (not just a warning — an unsigned/unnotarized app is refused outright, requiring a manual right-click-Open workaround), and Windows builds will show a **SmartScreen "unrecognized publisher"** warning (dismissible via "More info → Run anyway", less severe than macOS). The plan below is written to work either way, but real customer distribution should wait for at least the Apple side.

## Architecture

### 1. Update manifest (static JSON on Firebase Hosting)

A single file, e.g. `https://viracicom.com/download/manifest.json`:
```json
{
  "latestVersion": "1.1.0",
  "releaseNotesUrl": "https://viracicom.com/download/notes/1.1.0.html",
  "platforms": {
    "windows": { "url": "https://viracicom.com/download/EncoreKaraoke-1.1.0-win64.exe", "sha256": "<hex>" },
    "macos":   { "url": "https://viracicom.com/download/EncoreKaraoke-1.1.0-mac.dmg",   "sha256": "<hex>" }
  }
}
```
No auth required — works before login, matches "free download, no gate."

**Deliberate manual gate**: CI uploads each release's installer + a version-specific manifest snippet automatically, but flipping `manifest.json`'s `latestVersion` to point at it is a **separate, explicit step** (a one-line `promote-release.sh <version>` script, not a hand-edit), not automatic on every tag push. This means a freshly-built release can be smoke-tested by you before any customer's app ever offers it — mirroring the "Bypass All Plugins" panic-button philosophy from the plugin-hosting plan: automation builds the release, a human decides when it goes live.

**Instant rollback (Risk #1)**: `promote-release.sh` always writes `manifest.json` from a small checked-in history file (`releases.json`, one entry per shipped version, never overwritten) rather than hand-editing the live file — so "undo" is `promote-release.sh <previous-version>`, a few seconds, no rebuild. Every previously-shipped installer stays on Hosting permanently (never deleted/overwritten by a later release) so a rollback always has something real to point at. Adopt a habit of only running the promote step on a weekday, never right before a weekend (when most gigs happen), and never promote a version you haven't personally run a full song+SFX+mixer smoke test on first.

### 2. Client: `Source/Services/UpdateService.h/.cpp` (new)

Singleton matching the existing `*Service` pattern:
- `checkForUpdates(std::function<void(UpdateInfo)> callback)` — background thread, HTTP GET the manifest via `juce::URL` (HTTPS only — Firebase Hosting default; never follow an `http://` redirect), parse with `juce::JSON::parse`, compare `latestVersion` against the app's semver (parsed from `ProjectInfo::versionString` / the existing `PROJECT_VERSION`, ignoring the trailing build-number component) via a small 3-int numeric semver-compare helper (never a string compare — `"1.9.0"` must correctly be < `"1.10.0"`). Callback fires on the message thread via `callAsync`.
- If newer: `downloadUpdate(...)` — background download of the current platform's installer to `juce::File::getSpecialLocation(userApplicationDataDirectory)/EncoreKaraoke/updates/`, verifies `juce::SHA256` of the downloaded file against the manifest's checksum before trusting it. Discards and re-downloads if a still-newer version appears before the pending one is installed.
- `isUpdateReadyToInstall()` / `getPendingInstallerPath()` accessors for the UI.

**Fail silent, always (Risk #2 — this runs unconditionally on every launch, for every install, forever)**: a network timeout (hard cap, e.g. 5s connect + 10s total via `juce::URL::InputStreamOptions`), a malformed/missing manifest field, a checksum mismatch, a parse failure, or a downgrade (`latestVersion` numerically ≤ installed version) must all resolve to "no update available" with zero UI, zero delay to `showLoginWindow()`, and zero logged error visible to the user — never a dialog, never a retry loop, never a blocking wait. This is a hard requirement, not a nice-to-have, precisely because unlike every other feature in the app it runs unconditionally before the user has done anything.

### 3. Launch-time hook (`Source/Main.cpp`)

Right after the scan-mode check, before `showLoginWindow()`: kick off `UpdateService::getInstance().checkForUpdates(...)` — fire-and-forget, non-blocking, does not delay `showLoginWindow()`. This is the **only** place a check ever happens — no periodic re-check timer — which is what makes "never interrupt a show" trivially true rather than something that needs runtime guarding.

### 4. UI: small non-blocking banner (new lightweight component, not a modal `AlertWindow`)

Shown in `MainComponent` once the update finishes downloading+verifying: "Update available — Restart to install now, or Later." Dismissible; reappears next launch if not installed. On "Restart now": run the app's existing safe-shutdown path (`AudioEngine::shutdown()`, etc. — whatever `JUCEApplication::shutdown()` already does), then hand off to the installer, then `quit()`. The installer runs its normal (few-click) UI rather than a silent/unattended install — more robust than chasing per-platform silent-install flags for a low-volume product, and still just one click for the user.

**Don't race the installer against our own exit (Risk #3)**: launching the installer via `juce::File::startAsProcess()` and then immediately calling `quit()` risks the installer starting (and on Windows, trying to overwrite/lock-check the still-running exe) before this process has actually finished tearing down — a real risk given `AudioEngine::shutdown()` closes an audio device and the plugin-scan pattern already shows this codebase can have short-lived child processes. Mitigation: spawn the installer from a tiny detached shell one-liner that waits on our own PID first (`cmd /c "waitfor /pid <ourpid> ... & start installer.exe"` on Windows via a helper batch script written to a temp file; `while kill -0 <ourpid> 2>/dev/null; do sleep 0.2; done; open installer.dmg` on macOS), not a direct child-process launch — so the installer only ever starts after this process has fully exited, and Inno Setup's `CloseApplications`/`AppMutex` remains a pure safety net rather than something load-bearing.

New localization keys in `Resources/Languages/en_US.txt` following the existing `key=value` pattern: `update.banner_downloading`, `update.banner_ready`, `update.btn_restart_now`, `update.btn_later`.

### 5. Windows installer: Inno Setup

New `packaging/windows/installer.iss` — installs to `Program Files\Encore Karaoke`, Start Menu + Desktop shortcuts, standard uninstaller, version pulled from a CI-passed `/DMyAppVersion=`. Chosen over WiX (much more verbose for a solo maintainer) and MSIX (drags in Store-style app-identity/signing requirements not needed here). Use Inno Setup's `CloseApplications`/`AppMutex` directives so the installer can close a still-running Encore itself as a safety net, even though the app's own "Restart now" flow already quits it first.

### 6. macOS distribution: signed + notarized `.dmg`

New `packaging/macos/build_dmg.sh` wrapping: `codesign --deep --sign "Developer ID Application: ..."` on the `.app`, `xcrun notarytool submit --wait` (API-key auth), `xcrun stapler staple`, then package into a `.dmg` (plain `hdiutil create`, no need for a `.pkg`/`productbuild` since Encore needs no install scripts or protected-location writes — a drag-to-Applications `.dmg` is the standard, simplest format for this).

### 7. CI: `.github/workflows/release.yml` (new)

Triggered on tags matching `v*.*.*`:
- **build-windows** (`windows-latest`): `cmake --preset windows-release` → build Release → sign the exe via Azure Trusted Signing action → run `iscc installer.iss` → sign the installer exe too → upload artifact.
- **build-macos** (`macos-latest`): `cmake --preset macos-release` → build Release → import Developer ID cert from a base64 secret into a temp keychain → codesign → notarize+staple → `build_dmg.sh` → upload artifact.
- **publish** (needs both): downloads both artifacts, computes sha256, writes a version-specific manifest snippet + uploads both installers to Firebase Hosting (`firebase deploy --only hosting`, service-account secret) and creates a GitHub Release with both files attached as a backup download source. Does **not** touch `manifest.json`'s live `latestVersion` — that's the manual promotion step from Architecture §1. **Hard-fails (Risk #6)** if either platform's artifact, sha256, or upload step is missing — never partially publishes one platform while silently skipping the other, so a release can't end up in a state where one OS's manifest entry points at a stale/missing file.

Secrets to add in GitHub repo settings (documented for the user to create; I cannot create these myself): Azure Trusted Signing credentials, `APPLE_DEVELOPER_ID_CERT_P12` (+ password), `APPLE_API_KEY_ID`/`APPLE_API_ISSUER_ID`/`APPLE_API_KEY_P8`, Firebase service-account JSON.

**Secrets hygiene (Risk #4)**: these credentials can produce OS-trusted signed installers, so treat them accordingly — restrict the release workflow to run only on tags pushed by repo maintainers (GitHub Actions already withholds secrets from fork-PR-triggered runs by default; don't add a `pull_request` trigger to this workflow), never `echo`/print secret values in any step (mask via GitHub's built-in secret redaction, don't defeat it by base64-round-tripping into a visible log), and keep the manifest "promote" step (Architecture §1) running from your own machine rather than inside CI — so even a fully compromised CI pipeline could sign a rogue installer but could never make a real customer's app offer it.

### 8. Firebase Hosting setup

New `firebase.json` + `.firebaserc` at repo root, `public` dir populated by CI with `manifest.json`, the versioned installer files, and optionally a bare-bones `index.html` download page. Your main marketing website (whatever it's built on) just links its "Download" button to this Hosting URL — it doesn't need to know anything about the update mechanism.

## Risk assessment (most → least risky) and how the plan mitigates each

Ranked by likelihood × severity × blast radius against this app's reality: a KJ's income depends on Encore working the moment they need it, with no IT support to call.

1. **A bad release reaching every customer with no fast way back.** Highest ranked because "checks only at launch" (mitigating mid-show interruption) doesn't fully save you here — a customer can still update right before a gig and find the new build broken, with no one else to fix it in the moment. Mitigated by: the manual "promote" gate (§1, `promote-release.sh`) so nothing ships without a deliberate human decision; instant rollback via a checked-in `releases.json` history so undoing a bad promotion is seconds, not a rebuild; every previous installer kept permanently on Hosting so rollback always has a real target; a personal habit of promoting only after a full smoke test and never right before a weekend.
2. **A bug in the update-checker itself, since unlike every other feature it runs unconditionally on every launch for every install forever.** A crash, hang, or stray dialog here has 100% blast radius across the whole install base — worse than almost any other class of bug. Mitigated by the explicit "fail silent, always" requirement in Architecture §2: hard network timeout, numeric (not string) semver comparison, any malformed/missing/downgrade case treated identically to "no update," zero UI and zero delay to login on any failure path. Verification section below adds an explicit test matrix for this (airplane mode, malformed JSON, hanging server).
3. **The installer racing our own process's shutdown, corrupting the existing install (Windows file-locking is strict — a botched upgrade could leave neither the old nor new version working).** Mitigated in Architecture §4: the installer is launched via a small detached wait-for-our-PID-then-launch helper rather than racing `startAsProcess()` against `quit()`, so it only ever starts after this process has fully exited; Inno Setup's `CloseApplications` stays a safety net, not something load-bearing.
4. **Signing credentials (Apple Developer ID, Azure Trusted Signing) leaking or being misused from CI**, since they can produce OS-trusted signed installers. Mitigated in the CI section (§7): workflow restricted to maintainer-pushed tags (no fork-PR trigger), no secret values ever echoed to logs, and — most importantly — the live-manifest "promote" step deliberately kept outside CI entirely, so even a fully compromised pipeline can sign a rogue build but can't make it reach a real customer.
5. **Distributing unsigned installers before certs exist.** Documented explicitly in Prerequisites with the exact Gatekeeper/SmartScreen friction each platform will show; macOS should be treated as blocking for real customer rollout, Windows as degraded-but-usable in the interim.
6. **One platform's release silently lagging or breaking without anyone noticing** (e.g. macOS build fails while Windows succeeds, and only Windows' manifest entry gets updated). Mitigated in §7: the `publish` CI job hard-fails if either platform's artifact/checksum/upload is incomplete, rather than partially succeeding.
7. **A corrupted or tampered-with download being installed.** Mitigated by sha256 verification (§2) before the client ever trusts a downloaded installer, plus HTTPS-only fetches for both the manifest and the installer itself.
8. **Firebase Hosting downtime or a custom-domain misconfiguration blocking update checks entirely.** Low impact given Risk #2's mitigation already requires this to fail silently and gracefully — worth noting only so it's never mistaken for an emergency; regular karaoke playback is completely unaffected either way.

## Explicitly out of scope

- License keys / activation (confirmed free distribution).
- Mac App Store / Microsoft Store (App Store sandboxing conflicts with the existing VST3 plugin-hosting and arbitrary folder-scanning features — not viable without major rework).
- Delta/binary-diff updates — full installer re-download each release is fine at this app's size.
- Forced/mandatory minimum-version enforcement — manifest schema leaves room for a future `minimumSupportedVersion` field, but nothing enforces it yet.
- Any change to the existing local ad-hoc dev-sandbox `codesign` workaround in `.claude/settings.local.json` — unrelated to real distribution signing.

## Verification

- Trigger a `v0.0.1-test`-style tag on a throwaway branch to confirm the CI pipeline builds, signs (once certs exist), and uploads both artifacts without touching the live manifest.
- Manually promote that test version in a staging manifest path (not the real `manifest.json`) and confirm a locally-run build detects it, downloads, verifies checksum, shows the banner, and installs cleanly on both platforms.
- Confirm an app launched with no network connectivity starts normally with no error dialog (update check must fail silently/gracefully) — also test a malformed-JSON manifest and a manifest server that accepts the connection but never responds (hang), confirming the hard timeout fires and login still isn't delayed.
- Unit-check the semver-compare helper directly against edge cases: `"1.9.0"` vs `"1.10.0"` (must treat 1.10.0 as newer), equal versions (must report "no update"), and a manifest `latestVersion` older than the installed version (must never offer a downgrade).
- Confirm launching while a song is actively playing never shows an update prompt (should be structurally impossible per the design, but verify).
- Confirm the `publish` CI job actually fails the whole run (not just one job) if the macOS or Windows artifact is missing/empty, by temporarily breaking one platform's build on a test branch.
- Exercise `promote-release.sh <version>` and then `promote-release.sh <previous-version>` back-to-back and confirm `manifest.json` round-trips exactly — this is the rollback path and must be proven before it's ever needed for real.
