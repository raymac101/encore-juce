# Windows handoff — Encore distribution setup

Written 2026-07-11 on macOS, last updated 2026-07-17. This is a status
snapshot, not a plan — see `distribution_plan.md` for the full design/risk
assessment. If you're a fresh Claude Code session on Windows: read this
file plus `distribution_plan.md`, then pick up at "What to do on Windows"
below. The macOS session's memory/conversation does not carry over
automatically — this file is the continuity mechanism.

## Where things actually live (as of 2026-07-17 — this superseded two
## earlier decisions, don't trust older commit messages/docs about this)

- **Manifest + installer files both live in Firebase Storage**, not
  Firebase Hosting and not a website. Bucket `tagg-9ee2b.appspot.com`,
  path `Installers/`. Public read for that one path only is granted by
  `firebase/storage.rules` (deployed and confirmed working — a request for
  a not-yet-uploaded path returned `404`, not `403`, proving the rule
  itself is live).
- Download URL format (used by `Source/Services/UpdateService.cpp`'s
  `kManifestUrl` and must be used for every file you upload):
  ```
  https://firebasestorage.googleapis.com/v0/b/tagg-9ee2b.appspot.com/o/Installers%2F<filename>?alt=media
  ```
  (`%2F` is an encoded `/` — the path segment after `/o/` is the object's
  full path with `/` encoded, not a literal subfolder in the URL.)
- Repo-root `firebase.json`/`.firebaserc`/`hosting/` (Firebase **Hosting**)
  and the `https://viracicom.com/download/` (self-hosted website) idea
  that came before it are both **dead ends now** — vestigial, not deleted,
  but don't use either.
- **Version numbers are now `1.1.<build_number>`**, not a fixed `1.1.0`.
  `CMakeLists.txt` reads `build_number.txt`'s current value at *configure*
  time (before `project()`) and uses it directly as the patch component —
  so `ProjectInfo::versionString`, the macOS bundle version, and the
  Windows exe version resource all agree on one real number, and it's
  what `UpdateService.cpp` compares against the manifest. The counter
  auto-advances on every `cmake --preset ...` (not on every `cmake
  --build`) — check `build_number.txt` after configuring to see what
  version you're about to build. The old build-time-only increment script
  (`cmake/IncrementBuildNumber.cmake`) is gone; don't look for it.

## What's done (built and verified on macOS)

- **Client code**: `Source/Services/UpdateService.h/.cpp`, the update
  banner in `MainComponent`, the launch-time hook in `Main.cpp`.
- **Packaging scripts**: `packaging/windows/installer.iss` (Inno Setup,
  not yet run/tested on real Windows), `packaging/macos/build_dmg.sh`.
- **CI**: `.github/workflows/release.yml`, triggered on `v*.*.*` tags —
  its recorded release URLs were updated to the Firebase Storage format
  above, but no Azure secrets exist yet (see below), so the Windows job
  would still fail partway through if triggered today.
- **macOS signing — fully wired, proven, and shipped for real**:
  - Apple Developer ID Application identity:
    `Developer ID Application: Viracicom Entertainment Group LTD (9A3YNFA752)`.
  - App Store Connect API key: Key ID `S26YU8G2D2`, Issuer ID
    `ebb366d2-f9b3-4fb3-b692-15662edb7093`.
  - All 7 `APPLE_*` GitHub secrets are set on `raymac101/encore-juce` —
    **do not re-derive or re-paste these values anywhere**, they're
    already safely in GitHub Secrets.
  - `Certificates.p12` (if you go hunting) is the **Apple Development**
    cert (Xcode local testing), *not* Developer ID Application.
  - **Real release built 2026-07-17**: `dist/EncoreKaraoke-1.1.174-mac.dmg`
    — notarization `status: Accepted`, stapled, `spctl` reports
    `accepted / source=Notarized Developer ID`. `dist/manifest.json` has
    the matching sha256 and the real Storage download URL, ready to
    upload alongside the dmg. Zero Gatekeeper warnings for customers.

## What to do on Windows

1. **Pull latest first** — this handoff file, the CMakeLists.txt version
   restructuring, the new `kManifestUrl`, and `firebase/storage.rules` are
   all only useful if your Windows checkout has them.
2. **Build:**
   ```bat
   cmake --preset windows-release
   cmake --build build --config Release
   ```
   Check `build_number.txt` after the configure step — that's the exact
   build number that becomes this build's version (e.g. if it now says
   `176`, the app is `1.1.176`). Use that same number in the next step.
3. **Package with Inno Setup** (install it first: https://jrsoftware.org/isinfo.php):
   ```bat
   iscc packaging\windows\installer.iss /DMyAppVersion=1.1.<build> /DSourceExe="build\EncoreJUCE_artefacts\Release\Encore Karaoke.exe"
   ```
   Output: `dist\EncoreKaraoke-1.1.<build>-win64.exe`. This is a real,
   working installer, but **unsigned** — Windows SmartScreen will show a
   hard "unrecognized publisher" warning until Azure Trusted Signing is
   set up (see below). Confirmed acceptable for now per explicit decision
   on 2026-07-17 — you're building this unsigned deliberately.
4. **Compute its sha256** (PowerShell): `Get-FileHash "dist\EncoreKaraoke-1.1.<build>-win64.exe" -Algorithm SHA256`
5. **Upload to Firebase Storage** (`firebase login` first if needed, then
   use the Console at
   console.firebase.google.com/project/tagg-9ee2b/storage/tagg-9ee2b.appspot.com/files/~2FInstallers
   — drag the `.exe` in) at path `Installers/EncoreKaraoke-1.1.<build>-win64.exe`.
6. **Update `dist/manifest.json`** (or download the current live one from
   Storage first, if a Mac release already promoted a newer version) —
   fill in the `windows` block:
   ```json
   "windows": {
     "url": "https://firebasestorage.googleapis.com/v0/b/tagg-9ee2b.appspot.com/o/Installers%2FEncoreKaraoke-1.1.<build>-win64.exe?alt=media",
     "sha256": "<the hash from step 4, lowercase hex>"
   }
   ```
   Keep `latestVersion` and the `macos` block whatever they already are
   unless you're intentionally promoting a new version. Re-upload
   `manifest.json` to `Installers/manifest.json`, overwriting the old one.
7. **Set `manifest.json`'s Cache-Control metadata to `no-cache, max-age=0`**
   in the Storage console after uploading (click the file → edit
   metadata). Without this, Firebase's CDN can serve a stale cached copy
   for a while, delaying how fast a promoted update actually reaches
   customers.

### If/when you want it actually signed (recommended before real customers)

1. **Azure Trusted Signing account** — first-time identity verification
   can take a few days, start early. Need: Tenant ID, Client ID, Client
   Secret, the signing endpoint URL, account name, cert profile name.
2. **Set the secrets** (needs `gh auth login` on the Windows machine too
   — separate login per machine):
   ```
   gh secret set AZURE_TENANT_ID
   gh secret set AZURE_CLIENT_ID
   gh secret set AZURE_CLIENT_SECRET
   gh secret set AZURE_TRUSTED_SIGNING_ENDPOINT
   gh secret set AZURE_TRUSTED_SIGNING_ACCOUNT
   gh secret set AZURE_TRUSTED_SIGNING_CERT_PROFILE
   ```
3. Once both platforms' secrets exist, `git tag vX.Y.Z && git push origin vX.Y.Z`
   triggers the full CI pipeline instead of the manual steps above —
   though note the CI's Windows/macOS jobs still assume you want CI to be
   the one building; manual local builds (like this handoff describes)
   remain valid too and don't need CI at all.

## Repo-relative files to know about

- `distribution_plan.md` — the approved plan, full risk assessment (note:
  its Firebase-Hosting-based examples are now historical, not current).
- `.github/workflows/release.yml` — CI, all secret names it expects are in
  its comments/env blocks.
- `packaging/windows/installer.iss`, `packaging/macos/build_dmg.sh`
- `firebase/storage.rules` — the actual live access-control for downloads.
- `dist/manifest.json` — the current manifest content, ready to upload
  (macOS side already filled in as of 2026-07-17).
- `scripts/promote-release.sh`, `releases/releases.json`,
  `hosting/encore/manifest.json`, `firebase.json`, `.firebaserc` — all
  still reference the old Firebase-Hosting plan; don't use without
  reworking them for Storage first.
