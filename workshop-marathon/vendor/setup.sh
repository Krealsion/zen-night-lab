#!/usr/bin/env bash
# Vendor the pinned substrate for workshop-marathon (Night Lab III).
# Technique inherited from marathon/vendor (pinning discipline is shareable).
# Loom is built and INSTALLED from an archive of its pin; Zengine is built from
# an archive of its pin against that install — the stranger's path — and its
# own ctest green is the proof the vendored substrate works end to end.
set -euo pipefail
M=/mnt/g/programming/cpp/Zen/playground/night-lab/workshop-marathon
LOOM_PIN=61b2915
ZENGINE_PIN=0356f02

echo "== 1/5 archive Loom @$LOOM_PIN"
rm -rf "$M/vendor/loom-src" && mkdir -p "$M/vendor/loom-src"
git -C /mnt/g/programming/cpp/Zen/Loom archive "$LOOM_PIN" | tar -x -C "$M/vendor/loom-src"

echo "== 2/5 build Loom"
cmake -S "$M/vendor/loom-src" -B "$M/vendor/loom-build" -DCMAKE_BUILD_TYPE=Debug > "$M/vendor/loom-configure.log" 2>&1
cmake --build "$M/vendor/loom-build" -j"$(nproc)" > "$M/vendor/loom-build.log" 2>&1

echo "== 3/5 install Loom"
cmake --install "$M/vendor/loom-build" --prefix "$M/vendor/loom-install" > "$M/vendor/loom-install.log" 2>&1

echo "== 4/5 archive + build Zengine @$ZENGINE_PIN (stranger path)"
rm -rf "$M/vendor/zengine-src" && mkdir -p "$M/vendor/zengine-src"
git -C /mnt/g/programming/cpp/Zen/Zengine archive "$ZENGINE_PIN" | tar -x -C "$M/vendor/zengine-src"
cmake -S "$M/vendor/zengine-src" -B "$M/vendor/zengine-build" -DCMAKE_PREFIX_PATH="$M/vendor/loom-install" > "$M/vendor/zengine-configure.log" 2>&1
cmake --build "$M/vendor/zengine-build" -j"$(nproc)" > "$M/vendor/zengine-build.log" 2>&1

echo "== 5/5 Zengine ctest (the vendored substrate's green)"
ctest --test-dir "$M/vendor/zengine-build" --output-on-failure > "$M/vendor/zengine-ctest.log" 2>&1 || { echo "CTEST FAILED — see zengine-ctest.log"; exit 1; }
echo "VENDOR COMPLETE"
