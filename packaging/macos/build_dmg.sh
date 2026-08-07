#!/bin/sh
# Encore Karaoke - macOS distribution build: codesign + notarize + staple + dmg
#
# Run from CI (.github/workflows/release.yml) after a Release build, e.g.:
#   packaging/macos/build_dmg.sh \
#     "build/EncoreJUCE_artefacts/Release/Encore Karaoke.app" \
#     1.1.0 \
#     "Developer ID Application: Your Name (TEAMID)"
#
# Requires a "Developer ID Application" certificate already imported into
# the active keychain, plus notarization credentials via EITHER of:
#   - NOTARY_KEYCHAIN_PROFILE (local/manual runs) -- a profile already
#     created once via `xcrun notarytool store-credentials <name>`.
#   - APPLE_API_KEY_ID, APPLE_API_ISSUER_ID, APPLE_API_KEY_PATH (CI runs --
#     see .github/workflows/release.yml for how the GitHub secrets are
#     wired into a .p8 file written to disk earlier in the job).
#
# Deliberately a signed + notarized .dmg, not a .pkg: Encore needs no
# install scripts and writes nothing to a protected location, so a plain
# drag-to-Applications .dmg is the simplest correct format (see
# distribution_plan.md Architecture §6).

set -eu

APP_PATH="$1"
VERSION="$2"
SIGNING_IDENTITY="$3"

if [ -z "${APP_PATH}" ] || [ -z "${VERSION}" ] || [ -z "${SIGNING_IDENTITY}" ]; then
    echo "usage: build_dmg.sh <path-to-.app> <version> <codesign-identity>" >&2
    exit 1
fi
if [ ! -d "${APP_PATH}" ]; then
    echo "error: ${APP_PATH} does not exist" >&2
    exit 1
fi

APP_NAME="$(basename "${APP_PATH}" .app)"
DIST_DIR="$(cd "$(dirname "${APP_PATH}")/../../.." && pwd)/dist"
mkdir -p "${DIST_DIR}"

STAGING_DIR="$(mktemp -d)"
trap 'rm -rf "${STAGING_DIR}"' EXIT

echo "==> Codesigning ${APP_PATH}"
codesign --force --deep --options runtime \
    --sign "${SIGNING_IDENTITY}" \
    --timestamp \
    "${APP_PATH}"

codesign --verify --deep --strict --verbose=2 "${APP_PATH}"

echo "==> Zipping for notarization submission"
NOTARIZE_ZIP="${STAGING_DIR}/${APP_NAME}-notarize.zip"
ditto -c -k --keepParent "${APP_PATH}" "${NOTARIZE_ZIP}"

echo "==> Submitting to notarytool (waits for Apple's result)"
if [ -n "${NOTARY_KEYCHAIN_PROFILE:-}" ]; then
    xcrun notarytool submit "${NOTARIZE_ZIP}" \
        --keychain-profile "${NOTARY_KEYCHAIN_PROFILE}" \
        --wait
else
    xcrun notarytool submit "${NOTARIZE_ZIP}" \
        --key "${APPLE_API_KEY_PATH}" \
        --key-id "${APPLE_API_KEY_ID}" \
        --issuer "${APPLE_API_ISSUER_ID}" \
        --wait
fi

echo "==> Stapling notarization ticket"
xcrun stapler staple "${APP_PATH}"
spctl -a -vv -t install "${APP_PATH}"

echo "==> Building .dmg"
DMG_STAGING="${STAGING_DIR}/dmg-root"
mkdir -p "${DMG_STAGING}"
ditto "${APP_PATH}" "${DMG_STAGING}/${APP_NAME}.app"
ln -s /Applications "${DMG_STAGING}/Applications"

DMG_PATH="${DIST_DIR}/EncoreKaraoke-${VERSION}-mac.dmg"
rm -f "${DMG_PATH}"
hdiutil create -volname "${APP_NAME}" \
    -srcfolder "${DMG_STAGING}" \
    -ov -format UDZO \
    "${DMG_PATH}"

echo "==> Codesigning the .dmg itself"
codesign --force --sign "${SIGNING_IDENTITY}" --timestamp "${DMG_PATH}"

echo "==> Done: ${DMG_PATH}"
