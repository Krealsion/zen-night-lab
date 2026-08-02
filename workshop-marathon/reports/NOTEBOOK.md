# Laboratory Notebook — Night Lab III

Append-oriented. Entries written as work happens, not reconstructed.
Short entries; truthful chronology over prose.

---

## 2026-08-02 — 0. Baseline

- Pins verified before anything else: Loom `61b2915` (clean), Zengine `0356f02` (clean),
  night-lab `917bb46` (clean), `ZEN_ABI_VERSION 5u` in `Loom/include/zen/kernel/abi.h`.
- Cold state attested: executor memory for this scope is empty; an early accidental
  glob/grep into the Zen tree was rejected by the user before returning anything.

**Known contamination, recorded rather than hidden:** `marathon/README.md` (which I was
told to read for METHOD only) itself contains one conclusion sentence — that five of six
Night Two projects wanted **role-holding provenance**, and that the authoring surface was
in good shape. I cannot unread it. Mitigation: I will not let it steer toy selection or
architecture; if provenance pressure appears here it must re-earn itself with this
experiment's own evidence, and REDISCOVERY.md must treat it as *tainted rediscovery*
unless the pressure arrives through an independent path.

**Method adopted from prior experiments (allowed: build/harness/pinning/discipline):**
- WSL/GCC builds only — the kernel is `dlopen`/POSIX ground. CMake + ctest.
- Vendor pinning: `git archive <pin> | tar -x` into `vendor/loom-src`, build + install
  locally; Zengine consumed as pinned headers + prebuilt `.so` artifacts.
- Per-area `mutate.sh` residue harnesses; labelled fakes only; reproducers instead of
  substrate patches; commit baselines before mutating them.

**Embargo list honored** (not read, will not read until postmortem): `marathon/EVIDENCE.md`,
`marathon/FRICTION.md`, `marathon/FINAL-REPORT.md`, `followups/role-authorship/REPORT.md`,
all `<project>/REPORT.md` phase reports, and the root-level `zen-*-report` style documents
unless CONTEXT.md explicitly routes to them.

Next: Gate 0 — cold onboarding. Sources in order: `Loom/zen-vision.md` (in full),
`Loom/docs/CONTEXT.md` and what it routes to, `Zengine/AGENTS.md`, `Zengine/docs/README.md`.

---

## DELIGHT

(reserved; used sparingly, never manufactured)

---

## BORING FRICTION

(reserved; the non-architectural stuff that stops beginners)
