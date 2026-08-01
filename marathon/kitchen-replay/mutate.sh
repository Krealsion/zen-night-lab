#!/usr/bin/env bash
# The mutation harness for the kitchen replay: does this suite actually CATCH
# anything?
#
# A green suite proves nothing about a suite. Each mutation below breaks exactly
# one claim the kitchen makes, rebuilds the WHOLE binary, and runs the WHOLE
# suite under a timeout. A mutation that stays green is REPORTED as green — it
# means the term it touched is unwatched (or genuinely redundant), and saying so
# is the point.
#
# The discipline, every rule of it paid for by a harness that lied first:
#
#   * THE CANARY RUNS FIRST. If an obviously fatal change comes back green, the
#     harness is broken and every later verdict is worthless.
#   * NEVER CHECK A BUILD'S RETURN CODE THROUGH A PIPE. `cmake --build | tail`
#     reports tail's status, so a mutation that does not compile silently runs
#     the PREVIOUS mutation's binary.
#   * A SUMMARY IS NOT A COMPLETED RUN. doctest prints its summary even after the
#     process dies partway, so a crash wears the costume of a clean red. Any run
#     with fewer CASES than the baseline is TRUNCATED, not RED.
#   * IDENTICAL COUNTS ACROSS EVERY MUTATION is the tell that nothing rebuilt.
#     They are printed for every line so the tell is visible.
#   * A TIMEOUT IS NOT A PASS. It is reported distinctly. A wedged pump is a
#     finding.
#   * RESTORING SOURCES IS NOT RESTORING THE TREE. The restore rebuilds, so the
#     next lane cannot inherit the last mutation's binary.
#   * ABSOLUTE PATHS, and edits through a temp file: in-place `perl -0pi` cannot
#     complete its rename on the drvfs mount from WSL, so it silently edits
#     nothing.
#
# Usage:  bash kitchen-replay/mutate.sh     (from the marathon root, under WSL)

set -u
M=/mnt/g/programming/cpp/Zen/playground/night-lab/marathon
SRC=$M/kitchen-replay
BUILD=$M/build
BIN=$BUILD/kitchen-replay/kitchen-tests
BACKUP=$BUILD/mutation-backup
TMP=$BUILD/mut-tmp.cpp
TIMEOUT=300
FILES="expediter.cpp station.cpp policy.cpp harness.hpp"

cd "$M" || exit 1
mkdir -p "$BACKUP"

mutate() {
    local file="$1"; shift
    perl -0pe "$@" "$SRC/$file" > "$TMP" || return 1
    cp "$TMP" "$SRC/$file"
}

restore() {
    for f in $FILES; do
        if [ -f "$BACKUP/$f" ]; then
            cp "$BACKUP/$f" "$SRC/$f"
        fi
    done
}
for f in $FILES; do
    cp "$SRC/$f" "$BACKUP/$f"
done
trap restore EXIT

BASE_CASES=0

run_one() {
    local id="$1" desc="$2"
    if ! cmake --build "$BUILD" -j"$(nproc)" > "$BUILD/mut-build.log" 2>&1; then
        echo "$id  BUILD-FAILED   $desc"
        restore
        cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1
        return
    fi
    local out rc cases asserts summary
    out=$(timeout $TIMEOUT "$BIN" 2>&1)
    rc=$?
    summary=$(printf '%s\n' "$out" | grep -E '^\[doctest\] (test cases|assertions)' | tr -s ' ' \
              | tr '\n' ' ')
    cases=$(printf '%s\n' "$out" | grep -E '^\[doctest\] test cases' \
            | sed -E 's/.*test cases: *([0-9]+).*/\1/')
    cases=${cases:-0}
    if [ "$id" = "--" ]; then
        BASE_CASES=$cases
    fi
    if [ $rc -eq 124 ]; then
        echo "$id  TIMEOUT        $desc"
    elif [ "$cases" -lt "$BASE_CASES" ]; then
        # The process died partway. A crash is not evidence about the property.
        echo "$id  TRUNCATED      $desc   | ran $cases of $BASE_CASES cases"
    elif [ $rc -ne 0 ]; then
        echo "$id  RED            $desc   | $summary"
    else
        echo "$id  GREEN          $desc   | $summary"
    fi
    restore
    # Restoring sources is not restoring the tree.
    cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1
}

echo "=== baseline (must be GREEN) ==="
run_one "--" "unmutated"
echo "    baseline case count: $BASE_CASES"

echo
echo "=== canary (must be RED, or this harness is lying) ==="
mutate policy.cpp 's/(void on\(const RouteQuery& q, loom::Mail& mail\) \{\n)/$1        { refuse(mail, q, "CANARY"); return; }\n/'
run_one "00" "CANARY: the policy refuses everything"

echo
echo "=== the kitchen's own claims (Night One's, re-tested) ==="

mutate expediter.cpp 's/                tell_diner\(mail, t,\n                           OrderLost\{t\.order_id, t\.station,/                (void)0; if (false) tell_diner(mail, t,\n                           OrderLost{t.order_id, t.station,/'
run_one "01" "the watchdog drops a lost ticket silently instead of saying so"

mutate expediter.cpp 's/if \(--t\.patience_left > 0\) \{/if (t.patience_left > 0) {/'
run_one "02" "the watchdog never spends patience (no deadline at all)"

mutate expediter.cpp 's/    void on\(const RouteChoice& c, loom::Mail& mail\) \{\n        if \(!mail\.answers_ask\(\)\) \{\n            \+\+state_\.ignored;\n            return;\n        \}\n/    void on(const RouteChoice\& c, loom::Mail\& mail) {\n/'
run_one "03" "the expediter believes any RouteChoice with the right correlation"

mutate expediter.cpp 's/        if \(bootstrapping_\) \{\n            hold_late\(HeldOutcome\{false, p\.job, p\.station, p\.dish, "", mail\.correlation\(\)\}\);\n            return;\n        \}\n//'
run_one "04" "no handover window: a plate arriving mid-handover is handled at once"

mutate expediter.cpp 's/            answer_receipt\(mail, t\.job,\n                           OrderReceipt\{t\.order_id, kRoutedRefused, "",\n                                        "the expediter was replaced while this order was still "\n                                        "being routed; nothing was started"\}\);/            release_promise(t.job, mail);/'
mutate expediter.cpp 's/(    void hold_late\(HeldOutcome outcome\) \{)/    void release_promise(const std::string\& job, loom::Mail\& mail) {\n        for (Promise\& p : promises_) { if (p.job == job) { loom::release_deferred(p.answer, mail); } }\n    }\n$1/'
run_one "05" "the outgoing expediter RELEASES its open promises instead of closing them"

mutate station.cpp 's/        return 0; \/\/ 0 means "not on this menu" — never a zero-pass instant dish/        return 1;/'
run_one "06" "a station cooks anything it is handed, menu or not"

mutate policy.cpp 's/        const bool required = q\.fallback == kFallbackNone;/        const bool required = false;/'
run_one "07" "the policy ignores a REQUIRED preference and re-routes anyway"

echo
echo "=== the REPLAY's own claims (the prepared-replacement ceremony) ==="

mutate harness.hpp 's/    void on\(const kitchen::StationNotReady& r, loom::Mail&\) \{\n        \+\+state_\.not_ready;\n        desk_->notes\.push_back\("candidate says NOT READY: " \+ r\.reason\);\n        offer\(loom::PreparationAnswer::Refused\);/    void on(const kitchen::StationNotReady\& r, loom::Mail\&) {\n        ++state_.not_ready;\n        desk_->notes.push_back("candidate says NOT READY: " + r.reason);\n        offer(loom::PreparationAnswer::Ready);/'
run_one "08" "THE COORDINATOR MISREPORTS a refusal as readiness"

mutate station.cpp 's/        if \(p\.station != kStationName\) \{\n            refuse_prep\(mail, "this artifact is station /        if (false) {\n            refuse_prep(mail, "this artifact is station /'
run_one "09" "a candidate agrees to be a station it is not"

mutate station.cpp 's/            if \(passes_for\(t\.dish\) == 0\) \{\n                refuse_prep/            if (false) {\n                refuse_prep/'
run_one "10" "a candidate accepts carried work it cannot cook"

mutate station.cpp 's/    void on\(const PrepareStation& p, loom::Mail& mail\) \{/    void on(const PrepareStation\& p, loom::Mail\& mail) {\n        { (void)p; become_ready(mail, {}); return; }/'
run_one "11" "the preparation DROPS the carried work and says Ready anyway"

mutate harness.hpp 's/        desk_->offers\.push_back\(desk_->upgrade->offer_current_answer\(answer\)\);/        (void)answer; desk_->offers.push_back(loom::TxnResult{true, desk_->upgrade->id(), loom::TxnReason::None});/'
run_one "12" "the owner never offers anything to the gate, and reports success"

echo
echo "=== declared-redundancy probes (a GREEN here is a REPORTED gap) ==="

mutate expediter.cpp 's/        if \(t == nullptr \|\| t->station\.empty\(\) \|\| t->station != station\) \{/        if (t == nullptr || t->station.empty()) {/'
run_one "13" "a plate is accepted even when it names a station the job never went to"

mutate expediter.cpp 's/            if \(!known\) \{/            if (true) {/'
run_one "14" "an inherited roster OVERWRITES a station that announced during the handover"

mutate station.cpp 's/        const auto letter = loom::claim_item<StationHandoff>\(envelope\.items\[0\]\);\n        if \(!letter \|\| letter->station != kStationName\) \{/        const auto letter = loom::claim_item<StationHandoff>(envelope.items[0]);\n        if (!letter) {/'
run_one "15" "a station adopts a letter written for a different station"

echo
echo "=== residue check ==="
restore
cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1
for marker in CANARY 'if (false)' 'release_promise' 'PreparationAnswer::Ready);' ; do
    hits=$(grep -rn -- "$marker" "$SRC"/*.cpp "$SRC"/*.hpp 2>/dev/null | grep -v '^Binary' | wc -l)
    echo "  marker '$marker' remaining in sources: $hits"
done
echo "  (the last one is expected to appear once: the owner's honest Ready path)"
echo
echo "done."
