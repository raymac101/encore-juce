# Windows handoff — Encore distribution setup

Written 2026-07-11 on macOS to continue this work on Windows. This is a
status snapshot, not a plan — see `distribution_plan.md` for the full
design/risk assessment. If you're a fresh Claude Code session on Windows:
read this file plus `distribution_plan.md`, then pick up at "What's left
on Windows" below. The macOS session's memory/conversation does not carry
over automatically — this file is the continuity mechanism.

## What's done (built and verified on macOS)

- **Client code** (compiles, links, runs — confirmed by launching the app):
  `Source/Services/UpdateService.h/.cpp`, the update banner in
  `MainComponent`, the launch-time hook in `Main.cpp`. A real bug (empty
  banner showing on every launch via `addAndMakeVisible` forcing visibility)
  was found and fixed — see git history on `Source/UI/MainComponent.cpp`.
- **Packaging scripts**: `packaging/windows/installer.iss` (Inno Setup,
  not yet run/tested on real Windows), `packaging/macos/build_dmg.sh`
  (tested for real, see below).
- **CI**: `.github/workflows/release.yml`, triggered on `v*.*.*` tags.
- **Firebase Hosting scaffolding**: `firebase.json`, `.firebaserc`
  (project ID still a placeholder), `hosting/encore/manifest.json`
  (inert placeholder — points at itself, no real update yet),
  `releases/releases.json` (empty — no releases recorded yet),
  `scripts/promote-release.sh` (the manual go-live/rollback gate).
- **macOS signing — fully wired and GitHub secrets are set**:
  - Apple Developer ID Application identity found and verified:
    `Developer ID Application: Viracicom Entertainment Group LTD (9A3YNFA752)`
  - App Store Connect API key: Key ID `S26YU8G2D2`, Issuer ID
    `ebb366d2-f9b3-4fb3-b692-15662edb7093` (file was at
    `/Volumes/MediaDrive/CodeProjects/Encore/Certificates/AuthKey_S26YU8G2D2.p8`
    on the Mac).
  - All 7 `APPLE_*` GitHub secrets are set on `raymac101/encore-juce`
    (`APPLE_DEVELOPER_ID_IDENTITY`, `APPLE_DEVELOPER_ID_CERT_P12`,
    `APPLE_DEVELOPER_ID_CERT_PASSWORD`, `APPLE_TEMP_KEYCHAIN_PASSWORD`,
    `APPLE_API_KEY_ID`, `APPLE_API_ISSUER_ID`, `APPLE_API_KEY_P8`) —
    verified via `gh secret list`. **Do not re-derive or re-paste these
    values anywhere** — they're already safely in GitHub Secrets, which is
    the only place they should live.
  - Note found along the way: `Certificates.p12` in that folder is your
    **Apple Development** cert (Xcode local testing), *not* Developer ID
    Application — don't confuse the two if you're hunting for certs again.
  - A local end-to-end test (build Release .app → `build_dmg.sh` → real
    codesign → real notarization submission → staple → .dmg) was kicked
    off with your real credentials. As of writing, Apple's notarization
    step was still "In Progress" (can take up to ~15-30 min) — check
    whether it finished and whether the resulting `.dmg` is valid
    (`spctl -a -vv -t install dist/EncoreKaraoke-0.0.1-test-mac.dmg`)
    before assuming macOS signing is fully proven end-to-end.

## What's left on Windows

1. **Azure Trusted Signing account** (distribution_plan.md Prerequisites
   #2) — not started yet. First-time identity verification for the
   certificate profile can take a few days, so start this early. Need:
   Tenant ID, Client ID, Client Secret, the signing endpoint URL, account
   name, and cert profile name.
2. **Set the Azure secrets** once you have them:
   ```
   gh secret set AZURE_TENANT_ID
   gh secret set AZURE_CLIENT_ID
   gh secret set AZURE_CLIENT_SECRET
   gh secret set AZURE_TRUSTED_SIGNING_ENDPOINT
   gh secret set AZURE_TRUSTED_SIGNING_ACCOUNT
   gh secret set AZURE_TRUSTED_SIGNING_CERT_PROFILE
   ```
   (You'll need `gh auth login` on the Windows machine too — separate login
   per machine.)
3. **Test the Windows installer script locally** (works today, unsigned,
   no Azure account needed yet):
   ```bat
   cmake --preset windows-release
   cmake --build build --config Release
   REM install Inno Setup first: https://jrsoftware.org/isinfo.php
   iscc packaging\windows\installer.iss /DMyAppVersion=0.0.1-test /DSourceExe="build\EncoreJUCE_artefacts\Release\Encore Karaoke.exe"
   ```
   Output lands in `dist\EncoreKaraoke-0.0.1-test-win64.exe`. Run it, confirm
   the install/uninstall/shortcuts work. This is a real, working, *unsigned*
   installer — Windows SmartScreen will warn on it until Azure Trusted
   Signing is wired in.
4. **Fill in remaining placeholders** (either machine, just needs doing
   once): `.firebaserc`'s `REPLACE_WITH_YOUR_FIREBASE_PROJECT_ID`, and
   `Source/Services/UpdateService.cpp`'s `kManifestUrl` (currently
   `download.karaokeworld.net`, which doesn't resolve — pick your real
   Hosting domain).
5. Once both platforms' secrets are set: tag a real version
   (`git tag vX.Y.Z && git push origin vX.Y.Z`) to trigger the full CI
   pipeline, per distribution_plan.md's "Cutting a release" flow.

## Repo-relative files to know about

- `distribution_plan.md` — the approved plan, full risk assessment.
- `.github/workflows/release.yml` — CI, all secret names it expects are in
  its comments/env blocks.
- `packaging/windows/installer.iss`, `packaging/macos/build_dmg.sh`
- `scripts/promote-release.sh`, `releases/releases.json`,
  `hosting/encore/manifest.json`, `firebase.json`, `.firebaserc`
