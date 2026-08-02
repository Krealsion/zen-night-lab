#!/usr/bin/env bash
# Collect the built Zengine artifacts into vendor/zengine/lib (tracked in git —
# an experiment that cannot be re-run is not evidence). Run after setup.sh.
set -euo pipefail
M=/mnt/g/programming/cpp/Zen/playground/night-lab/workshop-marathon
DEST="$M/vendor/zengine/lib"
mkdir -p "$DEST"
find "$M/vendor/zengine-build" -name '*.so' -exec cp {} "$DEST/" \;
ls -la "$DEST"
