#!/usr/bin/env bash
# The safety view must carry the runtime's own containment words, and must
# never paint an enforced badge this host cannot earn (canary #10 tripwire).
set -euo pipefail
WORKSHOP="$1"
OUT="$("$WORKSHOP" safety lighthouse)"
echo "$OUT" | grep -q "no OS sandbox" || { echo "missing the honest containment line"; exit 1; }
if echo "$OUT" | grep -qiE "fully sandboxed|containment: enforced|sandbox: on"; then
    echo "the safety view painted a shield it cannot earn"
    exit 1
fi
echo "safety view honest"
