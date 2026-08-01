#!/usr/bin/env bash
# The mutation harness for the download manager. Same discipline as the kitchen
# replay's — the rules are written out there and are not repeated here — with one
# addition worth naming: several mutations below attack the CONTINUITY CONTRACT
# rather than a computation, because "what crosses a replacement" is this
# project's architectural claim and a suite that could not catch a lie about it
# would be measuring nothing.
#
# Usage:  bash download-manager/mutate.sh    (from the marathon root, under WSL)

set -u
M=/mnt/g/programming/cpp/Zen/playground/night-lab/marathon
SRC=$M/download-manager
BUILD=$M/build
BIN=$BUILD/download-manager/downloads-tests
BACKUP=$BUILD/dl-mutation-backup
TMP=$BUILD/dl-mut-tmp.cpp
TIMEOUT=600
FILES="service.cpp client.hpp harness.hpp"

cd "$M" || exit 1
mkdir -p "$BACKUP"

# THE GUARD THIS HARNESS DID NOT HAVE, AND THE RUN THAT PAID FOR IT.
# A `perl -0pe` whose pattern matches NOTHING exits 0 and writes a byte-identical
# file. The mutation is then never applied, the suite passes for the most boring
# reason there is, and the line reads GREEN -- indistinguishable from "the term is
# unwatched". Night One's version of this bug was perl failing to WRITE; this one
# is perl writing the same thing. Both wear the same costume.
#
# So: compare, and refuse to run at all if nothing changed. A NOT-APPLIED line is
# a broken mutation, never evidence about the code.
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

# Optional filter: `bash <script> 14 15` re-runs only those mutations (plus the
# baseline and the canary, which are the trust anchors and are never skipped).
# This exists because a NOT-APPLIED or BUILD-FAILED line has to be repaired and
# re-run, and a RED line provably does not: a mutation that fails to apply leaves
# a byte-identical tree, which can only ever produce the baseline result.
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
    if ! cmake --build "$BUILD" -j"$(nproc)" > "$BUILD/dl-mut-build.log" 2>&1; then
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
mutate service.cpp 's/    void on\(const StartDownload& s, loom::Mail& mail\) \{/    void on(const StartDownload\& s, loom::Mail\& mail) {\n        { refuse(mail, s.ticket, "CANARY"); return; }/'
run_one "00" "CANARY: the service refuses every transfer"

echo
echo "=== the operation itself ==="

mutate service.cpp 's/            mail\.send\(client_of\(t\), DownloadProgress\{t\.ticket, t\.bytes_done, t\.total_bytes\},\n                      static_cast<std::uint64_t>\(t\.correlation\)\);//'
run_one "01" "progress is never reported: the client watches a silent transfer"

mutate service.cpp 's/    void finish_completed\(loom::Mail& mail, std::size_t at\) \{\n        const Transfer t = state_\.transfers\[at\];/    void finish_completed(loom::Mail\& mail, std::size_t at) {\n        const Transfer t = state_.transfers[at];\n        { (void)mail; erase(at, t.ticket, t.client); return; }/'
run_one "02" "a finished transfer is dropped from the book with no terminal message"

mutate service.cpp 's/            if \(t\.breaks_at > 0 && t\.bytes_done >= t\.breaks_at\) \{/            if (false) {/'
run_one "03" "a source that goes bad is transferred anyway, and reported complete"

mutate service.cpp 's/        const Source\* src = find_source\(s\.source\);\n        if \(src == nullptr\) \{/        static const Source kAnything{"anything", 128, 0};\n        const Source* src = find_source(s.source);\n        if (src == nullptr) { src = \&kAnything; }\n        if (false) {/'
run_one "04" "the service accepts any source name and invents a size for it"

mutate service.cpp 's/            if \(t\.ticket == s\.ticket && t\.client == client\) \{/            if (t.ticket == s.ticket \&\& t.client == client \&\& false) {/'
run_one "05" "one client may open the same ticket twice"

mutate service.cpp 's/        if \(state_\.transfers\.size\(\) >= kMaxOpenTransfers\) \{/        if (false) {/'
run_one "06" "the book has no bound at all"

mutate service.cpp 's/            const std::int64_t done = t\.bytes_done;/            { state_.transfers.erase(state_.transfers.begin() + static_cast<std::ptrdiff_t>(i)); say(mail, loom::Ack{}); return; }\n            const std::int64_t done = t.bytes_done;/'
run_one "07" "a cancelled transfer vanishes without the ending the client was owed"

echo
echo "=== the continuity contract (this project's architectural claim) ==="

mutate service.cpp 's/        discharge_inherited\(mail\);//'
run_one "08" "inherited debts are never discharged: the client waits forever"

mutate service.cpp 's/            mail\.send\(client,\n                      DownloadFailed\{o\.ticket, o\.bytes_done,/            mail.send(client,\n                      DownloadCompleted{o.ticket, o.bytes_done, digest_of(o.source, o.total_bytes)},\n                      static_cast<std::uint64_t>(o.correlation));\n            if (false) mail.send(client,\n                      DownloadFailed{o.ticket, o.bytes_done,/'
run_one "09" "THE LIE: the successor claims it finished a transfer it never had the bytes for"

mutate service.cpp 's/            if \(p\.verify_sources && find_source\(o\.source\) == nullptr\) \{/            if (false) {/'
run_one "10" "a candidate accepts debt naming sources it cannot serve"

mutate service.cpp 's/        if \(p\.inherit\.size\(\) > kMaxInheritedObligations\) \{/        if (false) {/'
run_one "11" "a candidate accepts more debt than an honest predecessor could have owed"

mutate service.cpp 's/        for \(const Obligation& o : debts\) \{\n            const loom::WeaveId client\{parse_u64\(o\.client\)\};/        for (const Obligation\& o : debts) {\n            const loom::WeaveId client{parse_u64(o.client) + 1};/'
run_one "12" "inherited debts are reported to the wrong client"

echo
echo "=== which half of the operation is attested ==="

mutate service.cpp 's/            for \(std::size_t i = 0; i < held_\.size\(\); \+\+i\) \{\n                if \(held_\[i\]\.ticket != t\.ticket \|\| held_\[i\]\.client != t\.client\) \{\n                    continue;\n                \}/            for (std::size_t i = 0; i < held_.size(); ++i) {\n                if (true) {\n                    continue;\n                }/'
run_one "13" "the holds-the-answer build never spends the right it held"

mutate client.hpp 's/        const bool honest = c\.digest == digest_of\(source_of\(c\.ticket\), c\.bytes\);/        const bool honest = true;/'
run_one "14" "the client stops checking the bytes it was told arrived"

echo
echo "=== residue check ==="
restore
cmake --build "$BUILD" -j"$(nproc)" > /dev/null 2>&1
for marker in CANARY 'if (false)' 'kAnything' 'parse_u64(o.client) + 1'; do
    hits=$(grep -rn -- "$marker" "$SRC"/*.cpp "$SRC"/*.hpp 2>/dev/null | wc -l)
    echo "  marker '$marker' remaining in sources: $hits"
done
echo
echo "done."
