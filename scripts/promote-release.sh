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
# Requires: jq, and the Firebase CLI logged in (`firebase login`) with
# access to the project configured in .firebaserc.

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

echo "==> Done. ${VERSION} is now live. To roll back: $(basename "$0") <previous-version>"
