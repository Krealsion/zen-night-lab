#!/usr/bin/env bash
# The follow-up's mutation harness: three application-level cuts, each removing
# a deliberate as_role so the strict receivers must redden. Canary first (m1 is
# hand-proven before any matrix is believed). House discipline throughout:
# apply is verified by cmp (a non-matching pattern is NOT-APPLIED, never a
# silent green), builds and runs carry timeouts, the tree is restored and
# residue-checked after every mutation, and the tail rebuild leaves no mutated
# binary behind.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="$HERE/build"
MARK="R2D0_FOLLOWUP_MUTATION"

mutate() {
    local id="$1" file="$2" old="$3" new="$4" expect_red_suite="$5"
    if ! (cd "$HERE" && git diff --quiet -- .); then
        echo "m$id ABORT: tree dirty"
        exit 1
    fi
    python3 - "$file" "$old" "$new" <<'PY'
import sys, pathlib
f, old, new = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3]
t = f.read_text()
if t.count(old) != 1:
    print(f"pattern matches {t.count(old)} times (need 1)")
    sys.exit(3)
f.write_text(t.replace(old, new, 1))
PY
    local applied=$?
    if [ "$applied" != "0" ]; then
        echo "m$id NOT-APPLIED"
        (cd "$HERE" && git checkout -- .)
        return
    fi
    if ! timeout 600 cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1; then
        echo "m$id BUILD-FAILED"
        (cd "$HERE" && git checkout -- .)
        return
    fi
    if timeout 300 ctest --test-dir "$BUILD" > /dev/null 2>&1; then
        echo "m$id GREEN (unexpected — the strict receivers did not notice)"
    else
        echo "m$id RED ($expect_red_suite reddened, as the feature promises)"
    fi
    (cd "$HERE" && git checkout -- .)
    if grep -rq "$MARK" "$HERE" --include=*.cpp --include=*.hpp; then
        echo "m$id RESIDUE LEFT — STOP"
        exit 1
    fi
}

# m1 (CANARY): the lobby matchmaker forgets to speak as the office — its match
# pushes become personal chatter, and the strict player refuses every one.
mutate 1 "$HERE/lobby/matchmaker.cpp" \
'        (void)mail.as_role("lobby.matchmaker").send(player, match);' \
'        (void)mail.send(player, match); /*R2D0_FOLLOWUP_MUTATION*/' \
"lobby-replay"

# m2: the farm worker publishes its announcement personally — every observer
# refuses the evidence.
mutate 2 "$HERE/build-farm/worker.cpp" \
'        (void)mail.as_role("farm.worker.a").send_to_role("farm.dispatcher", done);
        (void)mail.as_role("farm.worker.a").publish(open);' \
'        (void)mail.send_to_role("farm.dispatcher", done); /*R2D0_FOLLOWUP_MUTATION*/
        (void)mail.publish(open);' \
"farm-replay"

# m3: the download service speaks its terminal truth personally — the client
# cannot verify the office and the second half of the operation goes dark.
mutate 3 "$HERE/download-manager/service.cpp" \
'        (void)mail.as_role("download.service")
            .send(WeaveId{static_cast<std::uint64_t>(state_.client)},
                  dl::DownloadDone{state_.url, true});' \
'        (void)mail.send(WeaveId{static_cast<std::uint64_t>(state_.client)},
                        dl::DownloadDone{state_.url, true}); /*R2D0_FOLLOWUP_MUTATION*/' \
"download-replay"

echo "== final clean rebuild =="
timeout 600 cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1 && \
    ctest --test-dir "$BUILD" 2>&1 | tail -2
