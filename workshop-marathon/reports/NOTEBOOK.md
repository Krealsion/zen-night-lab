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

## 2026-08-02 — 1. Gate 0 complete

- Read the full public corpus (vision → CONTEXT → terminology → 6 guides → 9
  reference pages → 6 law files → Zengine README/AGENTS + Timer corpus).
  ~2.5 hours of a human's reading; the corpus is compact on purpose.
- Verdict **GREEN with named debts** — full answers in `GATE0-ONBOARDING.md`;
  five pressure entries opened (P-001..P-005).
- What I was trying to do: answer the 12 onboarding questions without source.
  10 of 12 landed from guides/reference alone. The two that didn't: the exact
  unexported-target set (CMakeLists) and the Zengine consumption story (absent).
- Surprises worth recording now:
  - The substrate is *already* introspection-shaped: BusEvent taps, a journal,
    `construct_blind` ("the console's road"), `ZEN_EXPOSE` Poke doors, manifests
    with nested-shape references. Gate 2 has real ground to stand on.
  - The Weave Manager control door means lifecycle commands are *messages* from
    a granted operator weave — the snake host is already "a thin shell + an
    ordinary weave operating the world". The Workshop wants to be born that way.
  - The SDL window cannot hear (V1, structural). The visual Workshop will paint
    into SDL or TUI but *listen* through the terminal. This is a constraint to
    exhibit honestly, not to code around secretly.
- Next: vendor the pinned substrate (old marathon's technique), then Gate 1 —
  first toy through the smallest honest Workshop.

---

## DELIGHT

- **2026-08-02, Gate 0:** the laws' DOES-NOT-MEAN sections. Four times during
  one cold read they killed a wrong assumption *before* it was written into a
  design (commit≠committed; holding≠authoring; replacement≠continuity;
  correlation≠authentication). Documentation that anticipates your specific
  future mistake is rare. Not manufactured: this materially changed the
  Workshop plan (the operator-weave shape came straight from reading MSG-02's
  "holding a Switchboard is being the host").

---

## BORING FRICTION

(reserved; the non-architectural stuff that stops beginners)
