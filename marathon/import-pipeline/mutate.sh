#!/usr/bin/env bash
# The mutation harness for the import pipeline. Same discipline as the kitchen
# replay's, written out there — including the NOT-APPLIED guard, which by now has
# caught four broken patterns across three projects and is the reason any GREEN
# below can be read as evidence at all.
#
# What this matrix attacks: the CONVERSATION SHAPE. A menu that can be guessed
# past, a menu identity that can be ignored, a choice whose speaker is not
# checked, and a successor that carries a promise it never made. Those are this
# project's four claims and a suite that could not catch a lie about them would
# be measuring nothing.
#
# Usage:  bash import-pipeline/mutate.sh [ids...]   (from the marathon root, WSL)

set -u
M=/mnt/g/programming/cpp/Zen/playground/night-lab/marathon
SRC=$M/import-pipeline
BUILD=$M/build
BIN=$BUILD/import-pipeline/import-tests
BACKUP=$BUILD/import-mutation-backup
TMP=$BUILD/import-mut-tmp.cpp
TIMEOUT=600
FILES="importer.cpp requester.hpp harness.hpp"

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
    if ! cmake --build "$BUILD" -j"$(nproc)" > "$BUILD/import-mut-build.log" 2>&1; then
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
mutate importer.cpp 's/    void on\(const ImportAsset& r, loom::Mail& mail\) \{/    void on(const ImportAsset\& r, loom::Mail\& mail) {\n        { refuse(mail, r.ticket, "CANARY"); return; }/'
run_one "00" "CANARY: the importer refuses every request"

echo
echo "=== the conversation shape ==="

mutate importer.cpp 's/        \(void\)mail\.answer\(menu_for\(c\)\);/        (void)mail;/'
run_one "01" "the menu is never offered: the requester is asked to choose from silence"

mutate importer.cpp 's/        if \(exact == nullptr && by_codec == nullptr\) \{/        if (exact == nullptr \&\& by_codec == nullptr) { exact = \&kind->options[0]; }\n        if (false) {/'
run_one "02" "AN UNKNOWN SPELLING IS GUESSED AT instead of refused"

mutate importer.cpp 's/        if \(c\.menu != conv->menu\) \{/        if (false) {/'
run_one "03" "the menu identity is ignored, so a stale choice is acted on"

mutate importer.cpp 's/            if \(x\.requester == who\) \{\n                conv = &x;\n            \}/            conv = \&x;/'
run_one "04" "THE WALL: the importer stops checking WHO is choosing"

mutate importer.cpp 's/        if \(!conv->resolved_to\.empty\(\)\) \{/        if (false) {/'
run_one "05" "a second choice overwrites a decision already made"

mutate importer.cpp 's/        if \(kind->count == 0\) \{/        if (false) {/'
run_one "06" "a file that admits nothing is offered an EMPTY MENU instead of a refusal"

mutate importer.cpp 's/            \+\+state_\.receipts;\n            mail\.send\(who,\n                      ImportReceipt\{/            ++state_.receipts;\n            if (false) mail.send(who,\n                      ImportReceipt{/'
run_one "07" "the work finishes and the requester is never told"

echo
echo "=== what crosses a replacement, and what must not ==="

mutate importer.cpp 's/            c\.menu = mint_menu\(\);\n            state_\.open\.push_back\(c\);\n            \+\+state_\.reoffered;/            c.menu = "m1";\n            state_.open.push_back(c);\n            ++state_.reoffered;/'
run_one "08" "THE PROMISE CROSSES: the successor reuses its predecessor's menu identity"

mutate importer.cpp 's/            if \(!p\.resolved_to\.empty\(\)\) \{/            if (false) {/'
run_one "09" "a decision that already crossed is thrown away and re-asked"

mutate importer.cpp 's/            c\.resolved_to = p\.resolved_to;/            c.resolved_to.clear();/'
run_one "10" "the resolved choice does not cross, so the requester is asked twice"

mutate importer.cpp 's/            if \(p\.verify_files && find_file\(c\.file\) == nullptr\) \{/            if (false) {/'
run_one "11" "a candidate adopts conversations about files it cannot read"

mutate importer.cpp 's/        if \(p\.adopt\.size\(\) > kMaxAdoptedConversations\) \{/        if (false) {/'
run_one "12" "a candidate adopts more conversations than an honest predecessor could have held"

mutate harness.hpp 's/    void on\(const imp::ImporterNotReady& r, loom::Mail&\) \{\n        \+\+state_\.not_ready;\n        desk_->notes\.push_back\("candidate says NOT READY: " \+ r\.reason\);\n        offer\(loom::PreparationAnswer::Refused\);/    void on(const imp::ImporterNotReady\& r, loom::Mail\&) {\n        ++state_.not_ready;\n        desk_->notes.push_back("candidate says NOT READY: " + r.reason);\n        offer(loom::PreparationAnswer::Ready);/'
run_one "13" "THE CURATOR MISREPORTS a refusal as readiness"

mutate importer.cpp 's/        adopt_numbering\(p\.next_menu\);
        \(void\)mail\.answer\(ImporterReady/        (void)mail.answer(ImporterReady/'
run_one "14" "THE NAMESPACE DOES NOT CROSS: a successor mints a menu name its predecessor already used"

echo
echo "=== residue check ==="
restore
cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1
for marker in CANARY 'if (false)' 'c.menu = "m1"' 'c.resolved_to.clear()'; do
    hits=$(grep -rn -- "$marker" "$SRC"/*.cpp "$SRC"/*.hpp 2>/dev/null | wc -l)
    echo "  marker '$marker' remaining in sources: $hits"
done
echo
echo "done."
