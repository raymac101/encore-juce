#!/bin/sh
# Encore Karaoke - promote a built release to the live update manifest.
#
# This is the deliberate manual gate from distribution_plan.md Architecture
# §1 / Risk #1: CI (.github/workflows/release.yml) builds, signs, and
# uploads every tagged release automatically, but nothing reaches a real
# customer's app until this script is run BY HAND, from your own machine
# (never from CI -- see Risk #4). Rollback is just running this again with
# a previous version:
#
#   scripts/promote-release.sh 1.1.0      # ship 1.1.0
#   scripts/promote-release.sh 1.0.0      # instant rollback if 1.1.0 is bad
#
# Requires: jq, Node.js, the Firebase CLI logged in (`firebase login`) with
# access to the project configured in .firebaserc, and
# GOOGLE_APPLICATION_CREDENTIALS pointing at a Firebase/GCP service account
# key JSON with Storage Object Admin on the project's default bucket (same
# key used for the FIREBASE_SERVICE_ACCOUNT GitHub secret -- generate one
# from Firebase Console -> Project Settings -> Service Accounts if you don't
# have it handy; safe to delete the file again once this script exits).
#
# The running app's launch-time update check (UpdateService.cpp's
# kManifestUrl) reads the manifest from Firebase STORAGE
# (Installers/manifest.json), a different product/URL entirely from Firebase
# HOSTING -- `firebase deploy --only hosting` alone never reaches it. This
# script updates both: Hosting (kept for whatever else references it) and,
# critically, the Storage object the app actually checks.

set -eu

VERSION="${1:-}"
if [ -z "${VERSION}" ]; then
    echo "usage: $(basename "$0") <version>   (e.g. 1.1.0)" >&2
    echo "Available versions in releases/releases.json:" >&2
    jq -r '.[].version' "$(dirname "$0")/../releases/releases.json" >&2 || true
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RELEASES_FILE="${REPO_ROOT}/releases/releases.json"
MANIFEST_FILE="${REPO_ROOT}/hosting/encore/manifest.json"

ENTRY="$(jq -c --arg v "${VERSION}" '.[] | select(.version == $v)' "${RELEASES_FILE}")"
if [ -z "${ENTRY}" ]; then
    echo "error: version ${VERSION} not found in ${RELEASES_FILE}" >&2
    echo "Have you run the CI release workflow for v${VERSION} yet?" >&2
    exit 1
fi

echo "==> Promoting ${VERSION} to the live manifest"
echo "${ENTRY}" | jq '{
    latestVersion: .version,
    releaseNotesUrl: .releaseNotesUrl,
    platforms: .platforms
}' > "${MANIFEST_FILE}"

cat "${MANIFEST_FILE}"

echo "==> Deploying hosting/ to Firebase Hosting"
( cd "${REPO_ROOT}" && firebase deploy --only hosting )

if [ -z "${GOOGLE_APPLICATION_CREDENTIALS:-}" ]; then
    echo "error: GOOGLE_APPLICATION_CREDENTIALS is not set -- the app's actual" >&2
    echo "update check (Firebase Storage's Installers/manifest.json) was NOT" >&2
    echo "updated. Hosting alone does not reach it. Re-run with:" >&2
    echo "  GOOGLE_APPLICATION_CREDENTIALS=/path/to/key.json $0 ${VERSION}" >&2
    exit 1
fi

echo "==> Uploading manifest to Firebase Storage (Installers/manifest.json -- what the app actually reads)"
node "$(dirname "$0")/upload-to-storage.cjs" "${MANIFEST_FILE}" "Installers/manifest.json" "application/json"

echo "==> Done. ${VERSION} is now live. To roll back: $(basename "$0") <previous-version>"
