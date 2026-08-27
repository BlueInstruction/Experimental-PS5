#!/usr/bin/env bash
# ============================================================================
# Deterministic bootstrap of the upstream FEX-Emu/FEX (FEXCore) source tree.
#
# The upstream engine sources are NO LONGER vendored inside this repository —
# they used to add ~21,000 tracked files / >1 GB of foreign history. Instead,
# every consumer (local dev shell or CI runner) materializes an exact copy at
# a pinned commit via this single entry point, which keeps builds auditable
# and bit-for-bit reproducible across machines.
#
#   Local :  ./tools/fetch_fexcore.sh
#            (default destination: <repo>/../../deps/FEX)
#   CI    :  PX5_FEXCORE_ROOT=$PWD/deps/FEX ./tools/fetch_fexcore.sh
#
# To upgrade the pin, bump PIN_TAG/PIN_SHA below to a released monthly tag
# (https://github.com/FEX-Emu/FEX/tags) and re-run this script plus one full
# rebuild. AGENT.md documents the compatibility contract with our compat/
# layer (std::atomic_ref polyfill, disabled LTO, lld, static FEXCore target).
# ============================================================================
set -euo pipefail

PIN_TAG="FEX-2607"
PIN_SHA="1cc4b93e7a71c883ec021b71359f136394dc1f3c"
UPSTREAM="https://github.com/FEX-Emu/FEX.git"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${PX5_FEXCORE_ROOT:-$REPO_ROOT/../../deps/FEX}"

if [ -f "$DEST/.fex-pin" ] && grep -qx "$PIN_SHA" "$DEST/.fex-pin" 2>/dev/null; then
    echo "[fetch_fexcore] up to date: $DEST ($PIN_TAG @ ${PIN_SHA:0:12})"
    exit 0
fi

echo "[fetch_fexcore] cloning $UPSTREAM @ $PIN_TAG -> $DEST"
TMP="$DEST.tmp.$$"
rm -rf "$TMP"
git clone --quiet --depth 1 --branch "$PIN_TAG" "$UPSTREAM" "$TMP"

GOT="$(git -C "$TMP" rev-parse HEAD)"
case "$GOT" in
    "$PIN_SHA") ;;
    *)
        echo "[fetch_fexcore] ERROR: pin mismatch — got $GOT, want $PIN_SHA" >&2
        rm -rf "$TMP"
        exit 1
        ;;
esac

rm -rf "$DEST"
mkdir -p "$(dirname "$DEST")"
mv "$TMP" "$DEST"
printf '%s\n%s\n' "$PIN_SHA" "$PIN_TAG" > "$DEST/.fex-pin"
echo "[fetch_fexcore] ready: $DEST"
