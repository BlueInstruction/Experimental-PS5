#!/usr/bin/env bash
# ============================================================================
# Deterministic bootstrap of libadrenotools (+ liblinkernsbypass handling).
#
# Uses the Winlator-Ludashi fork (Pipetto-crypto) at the exact commit the
# Winlator ecosystem ships (adrenotools @ 8483dfd), which carries the newer
# Adreno driver-version support (e.g. 0.762.x on Adreno 750) on top of
# bylaws' original. Materialized OUTSIDE the repository at a pinned commit,
# same discipline as tools/fetch_fexcore.sh.
# ============================================================================
set -euo pipefail

PIN_SHA="8483dfdaa2abf97ee89ad0e5f337e7b508550c6b"
UPSTREAM="https://github.com/Pipetto-crypto/libadrenotools.git"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${PX5_ADRENOTOOLS_ROOT:-$REPO_ROOT/../../deps/adrenotools}"

if [ -f "$DEST/.pin" ] && grep -qx "$PIN_SHA" "$DEST/.pin" 2>/dev/null; then
    echo "[fetch_adrenotools] up to date: $DEST (${PIN_SHA:0:12})"
    exit 0
fi

echo "[fetch_adrenotools] cloning $UPSTREAM @ ${PIN_SHA:0:12} -> $DEST"
TMP="$DEST.tmp.$$"
rm -rf "$TMP"
git clone --quiet "$UPSTREAM" "$TMP"
git -C "$TMP" checkout --quiet "$PIN_SHA"

GOT="$(git -C "$TMP" rev-parse HEAD)"
case "$GOT" in
    "$PIN_SHA") ;;
    *)
        echo "[fetch_adrenotools] ERROR: pin mismatch — got $GOT" >&2
        rm -rf "$TMP"; exit 1 ;;
esac

# Materialize any submodules the fork declares (idempotent).
git -C "$TMP" submodule update --init --quiet || true

rm -rf "$DEST"
mkdir -p "$(dirname "$DEST")"
mv "$TMP" "$DEST"
printf '%s\n' "$PIN_SHA" > "$DEST/.pin"
echo "[fetch_adrenotools] ready: $DEST"
