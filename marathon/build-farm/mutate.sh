#!/usr/bin/env bash
# The mutation harness for the build farm. Same discipline as the kitchen
# replay's, written out there and not repeated — including the guard that run
# paid for: a `perl -0pe` whose pattern matches nothing writes a byte-identical
# file and the line reads GREEN. `mutate()` compares and reports NOT-APPLIED.
#
# What this matrix attacks that the other two could not: THE QUEUE, THE
# RESUMPTION CONTRACT, and RECONCILIATION. Those are the three things that make
# this project structurally different from the download manager, so a suite that
# could not catch a lie about them would be measuring nothing that matters.
#
# Usage:  bash build-farm/mutate.sh [ids...]   (from the marathon root, under WSL)

set -u
M=/mnt/g/programming/cpp/Zen/playground/night-lab/marathon
SRC=$M/build-farm
BUILD=$M/build
BIN=$BUILD/build-farm/farm-tests
BACKUP=$BUILD/farm-mutation-backup
TMP=$BUILD/farm-mut-tmp.cpp
TIMEOUT=600
FILES="dispatcher.cpp worker.cpp harness.hpp"

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
    if ! cmake --build "$BUILD" -j"$(nproc)" > "$BUILD/farm-mut-build.log" 2>&1; then
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
mutate dispatcher.cpp 's/    void on\(const SubmitBuild& s, loom::Mail& mail\) \{/    void on(const SubmitBuild\& s, loom::Mail\& mail) {\n        { reject(mail, s.id, "CANARY"); return; }/'
run_one "00" "CANARY: the dispatcher rejects every build"

echo
echo "=== the operation itself ==="

mutate dispatcher.cpp 's/        tell\(mail, \*b,\n             BuildProgress\{b->id, s\.worker, s\.stage, s\.index \+ 1,\n                           static_cast<std::int64_t>\(kStageCount\), s\.attempt\}\);/        (void)mail;/'
run_one "01" "progress is never forwarded to the requester"

mutate dispatcher.cpp 's/                tell\(mail, done, BuildSucceeded\{done\.id, j\.worker, j\.detail, done\.attempt\}\);//'
run_one "02" "a finished build is dropped from the book with no terminal message"

mutate dispatcher.cpp 's/    void dispatch\(loom::Mail& mail\) \{/    void dispatch(loom::Mail\& mail) {\n        { (void)mail; return; }/'
run_one "03" "nothing is ever dispatched: every build waits forever"

mutate dispatcher.cpp 's/    bool busy\(const std::string& worker\) const \{/    bool busy(const std::string\& worker) const {\n        { (void)worker; return false; }/'
run_one "04" "the dispatcher piles work onto a worker that already has some"

mutate dispatcher.cpp 's/                if \(!b\.worker\.empty\(\)\) \{\n                    continue;\n                \}\n                if \(avoid_last/                if (!b.worker.empty()) {\n                    continue;\n                }\n                { bool newer = false; for (const Build\& q : state_.builds) { if (\&q > \&b \&\& q.worker.empty()) { newer = true; } } if (newer) { continue; } }\n                if (avoid_last/'
run_one "05" "the queue is LIFO: the newest waiting build goes first"

mutate dispatcher.cpp 's/            if \(b\.id == s\.id && b\.requester == requester\) \{/            if (b.id == s.id \&\& b.requester == requester \&\& false) {/'
run_one "06" "one requester may open the same build id twice"

echo
echo "=== the two answers to absence, and the resumption contract ==="

mutate dispatcher.cpp 's/            \+\+state_\.requeued_by_reconciliation;/            ++state_.requeued_by_reconciliation;\n            { b.patience = kAssignmentPatienceSweeps; ++at; continue; }/'
run_one "07" "RECONCILIATION does nothing: an arrival is no longer evidence"

mutate dispatcher.cpp 's/            \+\+state_\.requeued_by_sweep;/            ++state_.requeued_by_sweep;\n            { b.patience = kAssignmentPatienceSweeps; ++at; continue; }/'
run_one "08" "THE SWEEP does nothing: silence is no longer evidence"

mutate dispatcher.cpp 's/        if \(b\.attempt >= kMaxAttempts\) \{/        if (false) {/'
run_one "09" "attempts are unbounded: a build can be requeued forever"

mutate dispatcher.cpp 's/            fail\(mail, i, d\.worker, std::string\{\}, "worker declined: " \+ d\.reason\);/            { (void)requeue(mail, i, "declined"); dispatch(mail); return; }/'
run_one "10" "a DECLINE is treated as an absence and retried three times"

mutate worker.cpp 's/            state_\.holding\.push_back\(\n                Work\{j\.job, j\.project, j\.revision, j\.target, j\.attempt \+ 1, 0, 0\}\);/            state_.holding.push_back(\n                Work{j.job, j.project, j.revision, j.target, j.attempt, 0, 0});/'
run_one "11" "a resumed build does not count as a new attempt"

mutate worker.cpp 's/        for \(const Work& w : state_\.holding\) \{\n            hello\.holding\.push_back\(w\.job\);\n        \}/        hello.holding.push_back("1");\n        hello.holding.push_back("2");\n        hello.holding.push_back("3");/'
run_one "12" "a worker announces work it is not holding, so reconciliation believes a lie"

echo
echo "=== the preparation conversation ==="

mutate worker.cpp 's/        if \(p\.worker != kWorkerName\) \{/        if (false) {/'
run_one "13" "a candidate agrees to be a worker slot it is not"

mutate worker.cpp 's/            if \(j\.attempt >= kMaxAttempts\) \{/            if (false) {/'
run_one "14" "a candidate resumes a build that has already used up its attempts"

mutate harness.hpp 's/    void on\(const farm::WorkerNotReady& r, loom::Mail&\) \{\n        \+\+state_\.not_ready;\n        desk_->notes\.push_back\("candidate says NOT READY: " \+ r\.reason\);\n        offer\(loom::PreparationAnswer::Refused\);/    void on(const farm::WorkerNotReady\& r, loom::Mail\&) {\n        ++state_.not_ready;\n        desk_->notes.push_back("candidate says NOT READY: " + r.reason);\n        offer(loom::PreparationAnswer::Ready);/'
run_one "15" "THE SHIFT LEAD MISREPORTS a refusal as readiness"

echo
echo "=== declared-redundancy probes (a GREEN here is a REPORTED gap) ==="

mutate dispatcher.cpp 's/        if \(b == nullptr \|\| b->worker != s\.worker\) \{/        if (b == nullptr) {/'
run_one "16" "a StageDone is believed even when it names a worker the job never went to"

mutate dispatcher.cpp 's/            if \(b\.job != j\.job \|\| b\.worker != j\.worker\) \{/            if (b.job != j.job) {/'
run_one "17" "a JobDone is believed even when it names a worker the job never went to"

echo
echo "=== residue check ==="
restore
cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1
for marker in CANARY 'if (false)' '(void)worker; return false' 'hello.holding.push_back("1")'; do
    hits=$(grep -rn -- "$marker" "$SRC"/*.cpp "$SRC"/*.hpp 2>/dev/null | wc -l)
    echo "  marker '$marker' remaining in sources: $hits"
done
echo
echo "done."
