#!/usr/bin/env bash
# The mutation harness: does this suite actually CATCH anything?
#
# A green suite proves nothing about a suite. Each mutation below breaks exactly
# one claim the kitchen makes, rebuilds the WHOLE binary, and runs the WHOLE
# suite under a timeout. A mutation that stays green is reported as green — it
# means the term it touched is unwatched (or genuinely redundant), and saying so
# is the point.
#
# Discipline, learned the hard way elsewhere and applied here:
#   * THE CANARY RUNS FIRST. If an obviously fatal change comes back green, the
#     harness itself is broken and every later result is worthless.
#   * ABSOLUTE PATHS. A relative path silently resolves somewhere else.
#   * A TIMEOUT IS NOT A PASS. It is reported as TIMEOUT, distinctly.
#   * THE ASSERTION COUNT IS PRINTED for every run. Identical counts across
#     every mutation is the tell that nothing was rebuilt.
#
# Usage:  bash tests/mutate.sh        (from the night-lab root, under WSL)

set -u
LAB=/mnt/g/programming/cpp/Zen/playground/night-lab
BUILD=$LAB/build
BIN=$BUILD/kitchen/night-lab-tests
TIMEOUT=300

cd "$LAB" || exit 1
mkdir -p "$LAB/build/mutation-backup"

# In-place perl editing fails on the drvfs mount from WSL (the rename step
# cannot complete), so every mutation is written through a temp file and copied
# back. Discovered by the CANARY coming back BUILD-FAILED instead of RED, which
# is precisely the job of a canary: the first run of this harness edited nothing.
mutate() {
    local file="$1"; shift
    perl -0pe "$@" "$LAB/kitchen/$file" > /tmp/nl-mut-src.cpp || return 1
    cp /tmp/nl-mut-src.cpp "$LAB/kitchen/$file"
}

restore() {
    for f in expediter.cpp station.cpp policy.cpp; do
        if [ -f "$LAB/build/mutation-backup/$f" ]; then
            cp "$LAB/build/mutation-backup/$f" "$LAB/kitchen/$f"
        fi
    done
}
for f in expediter.cpp station.cpp policy.cpp; do
    cp "$LAB/kitchen/$f" "$LAB/build/mutation-backup/$f"
done
trap restore EXIT

run_one() {
    local id="$1" desc="$2"
    if ! cmake --build "$BUILD" -j"$(nproc)" > /tmp/nl-mut-build.log 2>&1; then
        echo "$id  BUILD-FAILED  $desc"
        restore
        return
    fi
    local out rc
    out=$(timeout $TIMEOUT "$BIN" 2>&1)
    rc=$?
    local counts
    counts=$(printf '%s\n' "$out" | grep -E '^\[doctest\] assertions' | tr -s ' ')
    if [ $rc -eq 124 ]; then
        echo "$id  TIMEOUT       $desc"
    elif [ $rc -ne 0 ]; then
        echo "$id  RED           $desc   | $counts"
    else
        echo "$id  GREEN         $desc   | $counts"
    fi
    restore
}

echo "=== baseline (must be GREEN) ==="
run_one "--" "unmutated"

echo
echo "=== canary (must be RED, or this harness is lying) ==="
mutate policy.cpp 's/(void on\(const RouteQuery& q, loom::Mail& mail\) \{\n)/$1        { refuse(mail, q, "CANARY"); return; }\n/'
run_one "00" "CANARY: the policy refuses everything"

echo
echo "=== the mutations ==="

mutate expediter.cpp 's/tell_diner\(mail, t,\n                           OrderLost\{t\.order_id, t\.station,/(void)0; if (false) tell_diner(mail, t,\n                           OrderLost{t.order_id, t.station,/'
run_one "01" "the watchdog drops a lost ticket silently instead of saying so"

mutate expediter.cpp 's/if \(--t\.patience_left > 0\) \{/if (t.patience_left > 0) {/'
run_one "02" "the watchdog never spends patience (no deadline at all)"

mutate station.cpp 's/            letter\.tickets\.push_back\(t\);/            (void)t;/'
run_one "03" "a station writes its heir an EMPTY letter"

mutate expediter.cpp 's/            answer_receipt\(mail, t\.job,\n                           OrderReceipt\{t\.order_id, kRoutedRefused, "",\n                                        "the expediter was replaced while this order was still "\n                                        "being routed; nothing was started"\}\);/            release_promise(t.job, mail);/'
mutate expediter.cpp 's/(    void hold_late\(HeldOutcome outcome\) \{)/    void release_promise(const std::string\& job, loom::Mail\& mail) {\n        for (Promise\& p : promises_) { if (p.job == job) { loom::release_deferred(p.answer, mail); } }\n    }\n$1/'
run_one "04" "the outgoing expediter RELEASES its open promises instead of closing them"

mutate expediter.cpp 's/        if \(bootstrapping_\) \{\n            hold_late\(HeldOutcome\{false, p\.job, p\.station, p\.dish, "", mail\.correlation\(\)\}\);\n            return;\n        \}\n//'
run_one "05" "no handover window: a plate arriving mid-handover is handled at once"

mutate expediter.cpp 's/    void on\(const RouteChoice& c, loom::Mail& mail\) \{\n        if \(!mail\.answers_ask\(\)\) \{\n            \+\+state_\.ignored;\n            return;\n        \}\n/    void on(const RouteChoice\& c, loom::Mail\& mail) {\n/'
run_one "06" "the expediter believes any RouteChoice with the right correlation"

mutate expediter.cpp 's/    void strike\(const std::string& station\) \{/    void strike(const std::string\& station) {\n        if (true) { return; }/'
run_one "07" "a station that lost a dish is never struck from the roster"

mutate policy.cpp 's/        const bool required = q\.fallback == kFallbackNone;/        const bool required = false;/'
run_one "08" "the policy ignores a REQUIRED preference and re-routes anyway"

mutate station.cpp 's/        return 0; \/\/ 0 means "not on this menu" — never a zero-pass instant dish/        return 1;/'
run_one "09" "a station cooks anything it is handed, menu or not"

mutate policy.cpp 's/        answer_across_the_seam\(mail, RouteChoice\{q\.order_id, station, resolved,/        mail.answer(RouteChoice{q.order_id, station, resolved,/'
run_one "10" "THE SEAM: the policy answers with Mail::answer instead of the workaround"

echo
echo "=== declared-redundancy probes (a GREEN here is a REPORTED gap) ==="

mutate expediter.cpp 's/        if \(t == nullptr \|\| t->station\.empty\(\) \|\| t->station != station\) \{/        if (t == nullptr || t->station.empty()) {/'
run_one "11" "a plate is accepted even when it names a station the job never went to"

mutate expediter.cpp 's/            if \(!known\) \{/            if (true) {/'
run_one "12" "an inherited roster OVERWRITES a station that announced during the handover"

echo
echo "done."
