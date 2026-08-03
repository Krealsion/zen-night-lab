#!/usr/bin/env bash
# Collect the Zengine artifacts this experiment actually consumes into
# vendor/zengine/lib (tracked in git — an experiment that cannot be re-run is
# not evidence).
#
# DELIBERATELY NOT "every .so in the build tree". Zengine builds 15; this
# marathon references four, and tracking the other eleven would put ~57 MB of
# unread binaries (a 10 MB SDL skin, the snake package, the old marathon's
# replacement fixtures) into a public repository to no purpose. If a later
# experiment needs one, add it here with a reason — the list is the
# documentation. See README.md.
set -euo pipefail
M=/mnt/g/programming/cpp/Zen/playground/night-lab/workshop-marathon
DEST="$M/vendor/zengine/lib"
mkdir -p "$DEST"

WANTED=(
    zengine-timer.so          # the shipped service on the REAL monotonic clock
    zengine-timer-virtual.so  # the same service on a clock whose nap books the
                              # duration and returns — every witness runs here
    zengine-input.so          # the sole producer of key events (`run -i`, scribe)
    zengine-skin-tui-classic.so # the second skin, so a live skin SWAP is real
)

for name in "${WANTED[@]}"; do
    src="$(find "$M/vendor/zengine-build" -name "$name" -print -quit)"
    if [ -z "$src" ]; then
        echo "MISSING: $name (run setup.sh first)" >&2
        exit 1
    fi
    cp "$src" "$DEST/"
done
ls -la "$DEST"
