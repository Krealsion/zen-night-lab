#!/usr/bin/env bash
# The mutation harness for the lobby. Same discipline as the kitchen replay's,
# written out there — including the NOT-APPLIED guard.
#
# What this matrix attacks: THE TRUTH TABLE. Every mutation below breaks one cell
# of "can the receiver tell?", because that table is the entire deliverable and a
# suite that could not catch a lie about it would be measuring nothing.
#
# Usage:  bash lobby/mutate.sh [ids...]   (from the marathon root, under WSL)

set -u
M=/mnt/g/programming/cpp/Zen/playground/night-lab/marathon
SRC=$M/lobby
BUILD=$M/build
BIN=$BUILD/lobby/lobby-tests
BACKUP=$BUILD/lobby-mutation-backup
TMP=$BUILD/lobby-mut-tmp.cpp
TIMEOUT=600
FILES="matchmaker.cpp registry.cpp player.hpp harness.hpp"

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
    if ! cmake --build "$BUILD" -j"$(nproc)" > "$BUILD/lobby-mut-build.log" 2>&1; then
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
mutate registry.cpp 's/    void on\(const JoinLobby& j, loom::Mail& mail\) \{/    void on(const JoinLobby\& j, loom::Mail\& mail) {\n        { (void)j; (void)mail.answer(JoinRefused{"CANARY"}); return; }/'
run_one "00" "CANARY: the registry refuses every join"

echo
echo "=== the truth table ==="

mutate matchmaker.cpp 's/                mail\.send\(who, MatchCreated\{match, names, "server-" \+ match\}\);\n            \}\n        \}\n        \/\/ Observers get the weaker fact\. See MatchStarted\./                (void)who;\n            }\n        }/'
run_one "01" "PUSH announces nothing: a real match never reaches the players"

mutate matchmaker.cpp 's/        for \(std::size_t i = 0; i < held_\.size\(\); \+\+i\) \{\n            if \(held_\[i\]\.weave != weave\) \{\n                continue;\n            \}/        for (std::size_t i = 0; i < held_.size(); ++i) {\n            if (true) {\n                continue;\n            }/'
run_one "02" "PULL never spends the right it held: the attested match becomes ordinary"

mutate player.hpp 's/        if \(strict_ && !attested_now\) \{/        if (false) {/'
run_one "03" "A STRICT PLAYER STOPS CHECKING: the policy knob does nothing"

mutate matchmaker.cpp 's/        if \(c\.ready\.size\(\) < kMatchSize \|\| c\.ready\.size\(\) != c\.ready_weaves\.size\(\)\) \{/        if (c.ready.empty() || c.ready.size() != c.ready_weaves.size()) {/'
run_one "04" "PUSH matches with fewer ready players than the house rule"

mutate matchmaker.cpp 's/        if \(!kPull \|\| state_\.seekers\.size\(\) < kMatchSize\) \{/        if (!kPull || state_.seekers.empty()) {/'
run_one "05" "PULL matches with fewer seekers than the house rule"

mutate registry.cpp 's/            if \(m\.name == j\.player\) \{/            if (false) {/'
run_one "06" "two players may sit in the lobby under one name"

mutate registry.cpp 's/                if \(state_\.members\[i\]\.name == name\) \{/                if (false) {/'
run_one "07" "the registry ignores MatchStarted: matched players stay in the lobby forever"

echo
echo "=== the office, its personal capacity, and the cost of the workaround ==="

mutate matchmaker.cpp 's/        for \(const std::string& w : p\.weaves\) \{\n            const loom::WeaveId who\{parse_u64\(w\)\};\n            if \(who\.valid\(\)\) \{\n                mail\.send\(who, MatchCreated\{p\.match, p\.players, "server-" \+ p\.match\}\);\n            \}\n        \}/        for (const std::string\& w : p.weaves) {\n            spend(mail, w, MatchCreated{p.match, p.players, "server-" + p.match});\n        }/'
run_one "08" "THE PERSONAL STATEMENT BORROWS THE OFFICE'S ATTESTATION"

mutate matchmaker.cpp 's/        described\.held_answer_rights = static_cast<std::int64_t>\(held_\.size\(\)\);/        described.held_answer_rights = 0;/'
run_one "09" "the office hides how many players it promised an attestation it cannot hand over"

mutate matchmaker.cpp 's/            state_\.seekers\.push_back\(Seeker\{w\.player, w\.weave, w\.correlation\}\);\n            \+\+state_\.stranded;/            ++state_.stranded;/'
run_one "10" "a replaced PULL matchmaker drops its inherited players entirely"

mutate matchmaker.cpp 's/        if \(p\.match_size != static_cast<std::int64_t>\(kMatchSize\)\) \{/        if (false) {/'
run_one "11" "a candidate agrees to a house rule it does not implement"

mutate harness.hpp 's/    void on\(const lob::MatchmakerNotReady& r, loom::Mail&\) \{\n        \+\+state_\.not_ready;\n        desk_->notes\.push_back\("candidate says NOT READY: " \+ r\.reason\);\n        offer\(loom::PreparationAnswer::Refused\);/    void on(const lob::MatchmakerNotReady\& r, loom::Mail\&) {\n        ++state_.not_ready;\n        desk_->notes.push_back("candidate says NOT READY: " + r.reason);\n        offer(loom::PreparationAnswer::Ready);/'
run_one "12" "THE WARDEN MISREPORTS a refusal as readiness"

echo
echo "=== residue check ==="
restore
cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1
for marker in CANARY 'if (false)' 'if (true) {' 'held_answer_rights = 0'; do
    hits=$(grep -rn -- "$marker" "$SRC"/*.cpp "$SRC"/*.hpp 2>/dev/null | wc -l)
    echo "  marker '$marker' remaining in sources: $hits"
done
echo
echo "done."
