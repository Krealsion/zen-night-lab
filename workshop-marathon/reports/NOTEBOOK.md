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

## 2026-08-02 — 2. Gate 1: something exists

**What I tried:** the smallest honest Workshop — a thin shell (the snake host's
shape, deliberately), an operator weave that turns Manager answers into
published `PartUp`/`PartFailed` facts, a loadable registry that accumulates
them and answers asks, a log skin that paints every SurfaceText slot, and a
project file (`ProjectSpec`) admitted through the one gate. First toy:
**lighthouse** — a TimedWeave lamp sweeping a beam, alive and visible.

**What Zen provided without being asked twice:** the entire operating layer.
`mount_control` + `mount_manager` + target-scoped grants + `send_as` boot is a
complete, message-driven launch surface; the compat JSON codec + the gate is a
complete project-file admission story (hand-written files admit — the
`content_id` in the envelope is optional on input, canonicalized on output);
loaded weaves are indistinguishable peers; the virtual-clock timer service made
the test suite exact. Boot ORDER genuinely did not matter (lamp loaded before
its timer service still swept — TimedWeave re-asks on TimerReady, as promised).

**What I invented locally:** description (`ProjectSpec`/`PartSpec`), launch
facts (`PartUp`/`PartFailed`), the ask (`QueryRunning`→`RunningReport`), the
stop wish (`StopWish`, honored by the operator — a bounded run is ordinary
intent, not a host hack), the log skin, artifact stem→path resolution.

**What failed, honestly:** my first run hung forever. The governor (a native
TimedWeave) was mounted with a grant that allowed only its StopWish — its
binding layer's `EnsureTimer` to the timer role was CapabilityDenied at every
delivery, invisible to the sender BY LAW (send-fate seam), and I had no tap
installed to see it. Diagnosed by reasoning, not observation. Two lessons
recorded: (1) the fix is a reusable `allow_timed_weave(grant)` — the reach a
native TimedWeave costs is knowable and should be spelled once; (2) **the
inspector is not a luxury** — my first real bug of the marathon was exactly
the class of silence Gate 2 exists to make visible. (P-007)

**Witnesses:** `tests/test_workshop.cpp`, 16 cases green — spec-gate round
trip + wrong-shape + garbage refusals; the beam MOVED across published frames
(virtual clock); the registry's authenticated answer (answers_ask) matched
launched reality (3 up, 1 failed); the ghost part's failure was a published
fact carrying the loader's words. Live run: `workshop run lighthouse
--for-seconds 6` — 4 up, 0 failed, 3 sweeps, clean governor stop.

**Gate 1 verdict: GREEN.** The gate's question — does Zen naturally support
*creation*, or only execution after manual assembly? — splits cleanly:
OPERATING a composed world is deeply native (the Manager door is a complete
vocabulary); DESCRIBING a creation is a thin local layer that composed out of
existing substrate (codec + gate + door) in one sitting. The description layer
is Workshop-local vocabulary and shows no pressure to sink lower yet.

---

## 2026-08-02 — 3. Gate 2: the machine has no secrets

**What I built:** the S-3 tap bridge (the one privileged hand that watches the
bus and immediately re-publishes everything as ordinary gated `BusFact`
intent) + `workshop-inspector`, an ordinary loadable weave that tallies the
facts, keeps refusal/recent rings, paints a live "inspector" slot line, and
answers `QueryEvents` with authenticated answers. Refusals render through
`explain.hpp` — the diagnostics table mechanized from the RefusalReason
vocabulary (Gate 9's seed). `--watch` (raw tap) and `--refuse` (deliberate
NotAccepted, then explained) added to `run`.

**What the runtime hands an observer:** more than expected. `BusEvent` carries
the stamped office (`authored_role`, historical fact, never recomputed), the
sender-life diagnostics ("authored by a life that has since ended" is READABLE,
not inferred), and expected/actual requester life+incarnation on answer
refusals. The observation surface is genuinely introspection-shaped.

**Two honest teachings the demo forced:**
- *Queue order is not load order.* My first refusal demo enqueued its poke
  "after the boots" and it delivered before any load had happened — the
  steward's door is itself message-composed (Manager → control door is a
  second hop). NoSuchTarget, target=0, plainly on the tap. Fixed by firing
  from the operator when the last boot ANSWER arrives (the answer, not the
  wish). This is MSG-01 discipline teaching real architecture.
- *The inspector honestly misses what predates it* (MSG-06: publish picks
  recipients at enqueue). A refusal from before the inspector's birth was on
  the tap but never in its tallies — the score-weave stance, arriving
  uninvited and correct.

**Witnesses (25 cases green):** the starved-TimedWeave's CapabilityDenied
EnsureTimer — the exact Gate 1 bug, recreated deliberately — shown by the
inspector from the actual runtime event (I1); a role-holder's authenticated
answer relayed with EMPTY authored_role — office authorship never invented
from membership (I2, canary #3 pre-armed); the inspector's own answer
authenticated + its live line via ordinary Surface intent (I3). Live demo:
`run lighthouse --refuse` → "refused 1 … NotAccepted QueryRunning … the target
never declared that shape".

**Not yet exposed (inspector v2 material, owed to later gates):** grants/reach
(Gate 8), replacement transaction state (Gate 3/4), timer relationships,
artifact status. Recorded, not forgotten.

**Gate 2 verdict: GREEN.** You can learn what this system is doing by
listening to it, in its own vocabulary, without a debugger. The one privilege
(the tap is host-tier) is recorded as S-3 and the bridge pattern converts it
into ordinary intent at zero substrate friction (P-008).

---

## DELIGHT

- **2026-08-02, Gate 1:** `workshop describe` prints your hand-written project
  file back *as the gate accepted it* — canonicalized, content-id included.
  You see the system's understanding of your words, not an echo of them. Free
  behavior, straight out of `compat::serialize(to_value(admitted))`; nothing
  was built to make it possible. Also: boot order genuinely not mattering
  (the lamp loaded before its clock and swept anyway) felt like the substrate
  keeping a promise nobody usually keeps.

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
