#!/bin/bash
# Embeds the whole transitive Homebrew dependency tree rooted at whatever
# the app executable links against (currently just librubberband, which
# itself links libsamplerate -- also from Homebrew) into the app bundle,
# repointing every reference at @rpath instead of the absolute Homebrew
# path CMakeLists.txt's find_library() picks up at configure time.
#
# Why this matters: a Debug build ad-hoc-signed with no hardened runtime
# never checks who signed a loaded dylib, so the absolute Homebrew path
# works fine there. But packaging/macos/build_dmg.sh signs with
# `--options runtime` (hardened runtime, required for notarization), which
# enforces library validation -- it refuses to load a dylib whose code
# signature doesn't match the app's own Team ID. Homebrew's builds aren't
# signed with this project's Developer ID, so a signed/notarized build
# aborts at launch with a DYLD "Library not loaded ... different Team IDs"
# crash -- first for librubberband itself, then (once that's fixed) one
# level deeper for its own libsamplerate dependency. Embedding the whole
# chain and letting build_dmg.sh's codesign --deep sign it all under our
# identity fixes it for good, and for any future transitive dependency
# without needing to hand-add it here.
set -eu

EXECUTABLE="$1"
FRAMEWORKS_DIR="$2"

mkdir -p "${FRAMEWORKS_DIR}"

seen=""

is_seen() {
    case " ${seen} " in
        *" $1 "*) return 0 ;;
        *) return 1 ;;
    esac
}

homebrew_deps_of() {
    otool -L "$1" | tail -n +2 | awk '{print $1}' | grep '^/opt/homebrew' || true
}

# Recursively embeds $1 (an absolute Homebrew path) exactly once, then
# embeds and repoints its own Homebrew dependencies the same way.
embed_one() {
    local dep="$1"
    local name
    name="$(basename "${dep}")"

    if is_seen "${name}"; then
        return
    fi
    seen="${seen} ${name}"

    local dest="${FRAMEWORKS_DIR}/${name}"
    cp -f "${dep}" "${dest}"
    chmod u+w "${dest}"
    # Homebrew's file can carry extended attributes (e.g. quarantine/
    # resource-fork metadata) that codesign rejects on a loose file inside
    # a bundle ("resource fork, Finder information, or similar detritus
    # not allowed"). cp preserves xattrs from the source, so strip them.
    xattr -c "${dest}" 2>/dev/null || true
    install_name_tool -id "@rpath/${name}" "${dest}"

    local subdep subname
    while IFS= read -r subdep; do
        [ -z "${subdep}" ] && continue
        embed_one "${subdep}"
        subname="$(basename "${subdep}")"
        install_name_tool -change "${subdep}" "@rpath/${subname}" "${dest}" 2>/dev/null || true
    done < <(homebrew_deps_of "${dep}")

    # install_name_tool invalidates whatever signature the file had
    # (Homebrew's own, in this case). Xcode's automatic CodeSign build
    # phase only signs the top-level app bundle -- it does not re-sign
    # loose dylibs sitting in Contents/Frameworks -- so without this the
    # bundle fails codesign --verify --deep and macOS refuses to launch it
    # at all (SIGKILL "Code Signature Invalid"), even for a plain ad-hoc
    # Debug build with no hardened runtime. Ad-hoc self-sign here is enough
    # to satisfy that; packaging/macos/build_dmg.sh's later `codesign
    # --deep --sign <Developer ID>` overwrites this with the real identity
    # for Release/notarized builds, so this doesn't conflict with that flow.
    codesign --force --sign - "${dest}"
}

dep=""
while IFS= read -r dep; do
    [ -z "${dep}" ] && continue
    embed_one "${dep}"
    name="$(basename "${dep}")"
    install_name_tool -change "${dep}" "@rpath/${name}" "${EXECUTABLE}" 2>/dev/null || true
done < <(homebrew_deps_of "${EXECUTABLE}")

# Idempotent -- ignore "rpath already exists" on incremental rebuilds.
install_name_tool -add_rpath "@executable_path/../Frameworks" "${EXECUTABLE}" 2>/dev/null || true

if [ -z "${seen}" ]; then
    echo "embed_rubberband.sh: no Homebrew-linked libraries found on ${EXECUTABLE}, nothing to do"
else
    echo "embed_rubberband.sh: embedded [${seen}] into ${FRAMEWORKS_DIR}"
fi
