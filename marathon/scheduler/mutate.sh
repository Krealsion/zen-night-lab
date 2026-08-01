#!/usr/bin/env bash
# The mutation harness for the maintenance scheduler. Same discipline as the
# kitchen replay's, written out there — including the NOT-APPLIED guard.
#
# What this matrix attacks: THE COMPOSITION. The authored rhythm, the activation
# hook, the two kinds of cancellation, and both replacements — this package's own
# service and somebody else's. Plus the defect the suite found on its first run
# (a one-shot leaving the book before its answer came back), which now has a
# mutation of its own so it cannot come back unnoticed.
#
# ⚠ ONE MORE RULE, PAID FOR HERE: ANCHOR MUTATIONS ON CODE, NEVER ON PROSE.
# A source comment containing an apostrophe ("the fleet's current state") inside a
# single-quoted shell string CLOSES THE STRING. Every following line's quoting
# shifts, and bash dies on a syntax error several mutations later -- after earlier
# ones have already run and printed results that look perfectly normal. The EXIT
# trap still restores the tree, so nothing is left mutated; what is lost is every
# mutation after the bad line, silently, unless somebody reads the exit code.
#
# Usage:  bash scheduler/mutate.sh [ids...]   (from the marathon root, under WSL)

set -u
M=/mnt/g/programming/cpp/Zen/playground/night-lab/marathon
SRC=$M/scheduler
BUILD=$M/build
BIN=$BUILD/scheduler/maint-tests
BACKUP=$BUILD/maint-mutation-backup
TMP=$BUILD/maint-mut-tmp.cpp
TIMEOUT=900
FILES="scheduler.cpp worker.cpp harness.hpp"

cd "$M" || exit 1
mkdir -p "$BACKUP"

MUT_APPLIED=1
mutate() {
    local file="$1"; shift
    if ! perl -0pe "$@" "$SRC/$file" > "$TMP"; then
        MUT_APPLIED=0
        return 1
    fi
    if cmp -s "$TMP" "$SRC/$file"; then
        MUT_APPLIED=0
        return 1
    fi
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
ONLY="${*:-}"
selected() {
    [ -z "$ONLY" ] && return 0
    [ "$1" = "--" ] && return 0
    [ "$1" = "00" ] && return 0
    case " $ONLY " in *" $1 "*) return 0 ;; esac
    return 1
}

run_one() {
    local id="$1" desc="$2"
    if ! selected "$id"; then
        MUT_APPLIED=1
        restore
        return
    fi
    if [ "$MUT_APPLIED" -eq 0 ]; then
        echo "$id  NOT-APPLIED    $desc   | the pattern matched nothing -- NOT a green"
        MUT_APPLIED=1
        restore
        return
    fi
    if ! cmake --build "$BUILD" -j"$(nproc)" > "$BUILD/maint-mut-build.log" 2>&1; then
        echo "$id  BUILD-FAILED   $desc"
        restore
        cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1
        return
    fi
    local out rc cases summary
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
        echo "$id  TRUNCATED      $desc   | ran $cases of $BASE_CASES cases"
    elif [ $rc -ne 0 ]; then
        echo "$id  RED            $desc   | $summary"
    else
        echo "$id  GREEN          $desc   | $summary"
    fi
    restore
    cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1
    MUT_APPLIED=1
}

echo "=== baseline (must be GREEN) ==="
run_one "--" "unmutated"
echo "    baseline case count: $BASE_CASES"

echo
echo "=== canary (must be RED, or this harness is lying) ==="
mutate scheduler.cpp 's/    void on\(const ScheduleCheck& c, loom::Mail& mail\) \{/    void on(const ScheduleCheck\& c, loom::Mail\& mail) {\n        { (void)c; (void)mail.answer(loom::Refused{"CANARY"}); return; }/'
run_one "00" "CANARY: the scheduler refuses every schedule"

echo
echo "=== the authored rhythm and the activation hook ==="

mutate scheduler.cpp 's/        \+\+state_\.sweeps;\n        for \(Schedule& s : state_\.book\) \{/        ++state_.sweeps;\n        if (true) { return; }\n        for (Schedule\& s : state_.book) {/'
run_one "01" "the rhythm beats and nothing is ever asked"

mutate scheduler.cpp 's/        mail\.publish\(SchedulerOpen\{static_cast<std::int64_t>\(timers\(\)\.size\(\)\),\n                                   static_cast<std::int64_t>\(state_\.book\.size\(\)\)\}\);/        (void)mail;/'
run_one "02" "the activation hook does no domain work: nothing announces itself"

mutate scheduler.cpp 's/        for \(Schedule& s : state_\.book\) \{\n            ask_worker\(mail, s\);\n        \}\n    \}\n\n    \/\/ ---- the operator/        (void)mail;\n    }\n\n    \/\/ ---- the operator/'
run_one "03" "the activation hook skips the fleet's first checks"

mutate scheduler.cpp 's/            } else \{\n                \/\/ A one-shot is finished the moment its answer lands, and not one\n                \/\/ beat sooner\.\n                state_\.book\.erase\(state_\.book\.begin\(\) \+ static_cast<std::ptrdiff_t>\(i\)\);\n            \}/            }/'
run_one "04" "a one-shot never leaves the book, so it runs forever"

mutate scheduler.cpp 's/        if \(s\.pending\) \{\n            return; \/\/ one question at a time; the answer is what moves it on\n        \}\n        s\.pending = true;/        s.pending = true;/'
run_one "05" "THE DEFECT THE SUITE FOUND: a schedule is asked again while its answer is in flight"

mutate scheduler.cpp 's/        audit_\.cancel\(mail\);/        (void)mail;/'
run_one "06" "cancelling the AUTHORED binding does nothing"

mutate scheduler.cpp 's/                state_\.book\.erase\(state_\.book\.begin\(\) \+ static_cast<std::ptrdiff_t>\(i\)\);\n                \(void\)mail\.answer\(loom::Ack\{\}\);/                (void)mail.answer(loom::Ack{});/'
run_one "07" "cancelling a DOMAIN schedule does nothing but say yes"

echo
echo "=== the worker, and both replacements ==="

mutate worker.cpp 's/        if \(!knows\(r\.machine\)\) \{/        if (false) {/'
run_one "08" "the worker services machines it has never heard of"

mutate worker.cpp 's/            if \(!knows\(m\)\) \{/            if (false) {/'
run_one "09" "a candidate worker accepts a fleet it cannot service"

mutate harness.hpp 's/    void on\(const maint::MaintWorkerNotReady& r, loom::Mail&\) \{\n        \+\+state_\.worker_not_ready;\n        desk_->notes\.push_back\("worker candidate says NOT READY: " \+ r\.reason\);\n        offer\(loom::PreparationAnswer::Refused\);/    void on(const maint::MaintWorkerNotReady\& r, loom::Mail\&) {\n        ++state_.worker_not_ready;\n        desk_->notes.push_back("worker candidate says NOT READY: " + r.reason);\n        offer(loom::PreparationAnswer::Ready);/'
run_one "10" "THE ENGINEER MISREPORTS this package's own refusal as readiness"

mutate harness.hpp 's/    void on\(const timer::TimerCandidateDeclined& d, loom::Mail&\) \{\n        \+\+state_\.clock_declined;\n        desk_->notes\.push_back\("CLOCK candidate DECLINED: " \+ d\.reason\);\n        offer\(loom::PreparationAnswer::Refused\);/    void on(const timer::TimerCandidateDeclined\& d, loom::Mail\&) {\n        ++state_.clock_declined;\n        desk_->notes.push_back("CLOCK candidate DECLINED: " + d.reason);\n        offer(loom::PreparationAnswer::Ready);/'
run_one "11" "THE ENGINEER MISREPORTS SOMEBODY ELSE'S SERVICE's refusal as readiness"

# MASKED, AND HERE IS THE PAIRED CUT THAT PROVES IT. Mutation 09 alone stays
# GREEN because a second term refuses first: the narrow worker services two
# machines and is handed four, so the CAPACITY bound answers before the
# per-machine check is ever reached. Cutting one half of a two-term protection
# and reporting the green as "unwatched" would be exactly the mistake the
# discipline exists to prevent -- so cut BOTH, and the pair is load-bearing.
mutate worker.cpp 's/            if \(!knows\(m\)\) \{/            if (false) {/'
mutate worker.cpp 's/        if \(p\.fleet\.size\(\) > kKnownCount\) \{/        if (false) {/'
run_one "12" "PAIRED CUT: a candidate accepts a fleet it can neither hold nor service"

echo
echo "=== residue check ==="
restore
cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1
for marker in CANARY 'if (false)' 'if (true) { return; }'; do
    hits=$(grep -rn -- "$marker" "$SRC"/*.cpp "$SRC"/*.hpp 2>/dev/null | wc -l)
    echo "  marker '$marker' remaining in sources: $hits"
done
echo
echo "done."
