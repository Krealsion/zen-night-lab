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

## 2026-08-02 — 4. Gate 3: reach inside while it is alive

**Three alterations, all through ordinary doors, all witnessed (41 cases green):**
- **Knob poke (A1):** creations now DECLARE their reach-in points (`knobs` in
  ProjectSpec v2 — an honest schema evolution, v1 files refuse at the gate by
  law). A `zen.PokeWrite` widened the live beam 21→41 mid-run; sweeps did not
  reset. Discovered and pinned: **a poke reply is ordinary correlated speech,
  not an answer** — the construction layer replies with an ordinary send, so
  the consumer's authority is the stamped sender + its own correlation.
  `answers_ask()` false by design. (The FACT/DERIVED discipline just got its
  first live example of "same-looking shapes, different standing.")
- **Reload in place (A2):** `ReloadWeave`, same artifact — the sweep count AND
  the poked width both survived the incarnation bump. State rides the gate;
  nothing pretended, nothing lost, and the alteration a user made by hand is
  part of the state that survives. That is a genuinely pleasant property:
  *your pokes are as durable as the program's own memory.*
- **Swap a Workshop organ (A3):** the REGISTRY was hard-swapped through the
  same Manager door any toy part uses (Gate 10 down-payment). The office
  survived the officeholder (role-addressed ask answered by the successor,
  authenticated); the successor's memory held exactly one fact — the
  operator's announcement of the swap that created it. Continuity truthfully
  NOT preserved, and the report says so instead of papering it.

**Interactive layer added** (`run -i`): the operator hears KeyPressed and
reaches in with 1/p/r/q — skin cycle via SwapWeave, declared-knob pokes,
reload-in-place. To be exercised with real keys at the pilot.

**Gate 3 verdict: GREEN** (verified-successor ceremony deliberately deferred
to Gate 4, where a code edit will need it). Answer to the gate's question:
from above, live alteration feels like *ordinary messaging* — send a shape,
hear the answer, publish the fact. No kernel machinery leaked through the
floor; the floor is messages all the way down.

---

## 2026-08-02 — 5. Gate 4: the same thing at two heights (narrow vertical)

**The slice (50 cases green):** one creation, two live representations.
- **Code height (H1):** the lamp's glyph is a source-level property. The
  "edit" is real compiled code (`lighthouse-lamp-star.so`, same source, one
  define) and it traveled `ReloadWeave` into the RUNNING lamp behind the same
  stable id: the beam changed `#`→`*` mid-sweep and the sweep count crossed
  the edit intact. Interactively, `u` runs the actual `cmake --build` then the
  same reload — an honest edit-compile-reload loop from inside the run.
- **Schematic height (H2):** `schematic_lines()` renders the creation from
  its ADMITTED description + the operator's live trackers, published as
  ordinary Surface intent (slots `schematic.NN`; the log skin styles them
  bare — styling is the skin's business). A schematic-level operation (the
  knob) changed the RUNTIME, and the re-rendered schematic and the running
  frames agreed on the new truth. `workshop view` prints the same lines for a
  creation at rest ("described" vs "live" is labelled).

**Where the equivalence honestly ends (recorded, not hidden):**
1. The schematic edit set is small: select-implementation (skin) and
   change-property (knob). No add/remove-component from the schematic yet.
2. Edits are keyboard-mediated against a rendered view — legible, but not
   direct manipulation of drawn nodes. The SDL direct-manipulation question
   is also gated by the substrate itself (the SDL window cannot hear in V1).
3. Only contract-preserving code edits ride `reload_from`. A contract-CHANGING
   edit needs the prepared-replacement ceremony, which the Workshop has not
   yet driven — named as the next deepening, not glossed.
4. The schematic derives from description + operator trackers, not from
   observed message topology (the runtime-graph-semantics question stays open
   for the toys to vote on).
5. In-pump rebuilds STALL THE WORLD (the `u` key freezes the sweep until the
   compiler returns). BORING FRICTION: a build-watcher weave is the mature
   shape; v1 says what it does and does it in the open.

**Gate 4 verdict: GREEN for the narrow slice as specified** — one real
vertical, one meaningful change from each height, ends recorded precisely.
The "easy way vs real way" felt genuinely continuous for properties and
implementations; the wall (contract changes, structural edits) is named.

---

## 2026-08-02 — 6. Gates 5 & 6: the garden, and the regrets

**Four materially different toys now exist** (64 cases green):
1. **lighthouse** — single part, continuous, visual, the first vertical.
2. **pond** — eight fireflies from ONE artifact + a canvas that discovers its
   pond by listening; Kuramoto sync as declared data. Chosen to attack; it
   landed three hits (launcher name/stem conflation — fixed; knobs FOUGHT —
   wants broadcast + continuous; SurfaceText FOUGHT — wants a canvas).
3. **scribe** — a TOOL, not a game: a plain event-driven WeaveBase with no
   rhythm at all. Its votes: the Input package has NO TEXT STORY (scancodes +
   convenience names; no shift/case — the pad's honest ceiling), and even a
   rhythmless tool drags the clock in because the Input weave polls on Timer
   beats. "Event-driven only" does not exist in current Zen.
4. **constellation** — a night sky made ENTIRELY of other toys' parts: pond
   stars (pull=0 — same code, different creature, purely by declared `set`),
   pond canvas as the sky, lighthouse lamp as the beacon. **Zero new code —
   the project file IS the toy.** Forced garden-wide artifact resolution.

**Gate 6 witnessed in the same stroke:** the composed world runs both toys'
vocabularies on one bus; the registry's authenticated report shows the reuse
plainly (foreign stems under local instance names); and a modification to the
reused artifact (the star-glyph lamp) reached its consumer LIVE via reload —
constellation's beacon changed character mid-sweep. Reuse-knowledge today =
stem provenance in launch facts; deeper lineage (origin, hashes) is Gate 7's.

**Gate 5 verdict: GREEN** — the garden is diverse, and later toys genuinely
embarrassed earlier abstractions (knobs and the launcher took real hits; the
vote table has fought-votes with stories). **Gate 6 verdict: GREEN** for
compose/reuse/modify-observe; "the Workshop knows it was reused" is honest at
stem level and thin below that — recorded.

---

## 2026-08-02 — 7. PLAY DETOUR: echo — the machine swims with the fireflies

Built because the question was irresistible, not for coverage: a weave that
eats the S-3 bridge's BusFact stream and flashes into the pond every N
delivered facts. The machine's own heartbeat as a creature.

**What purposeless play discovered that the gates did not:**
- The observation vocabulary consumed by a TOY (not an inspector) worked with
  zero friction — P-008's predicted second sighting, landed. The vocabulary
  is starting to smell engine-shaped rather than Workshop-shaped; one more
  independent consumer makes it a PACKAGE PRESSURE candidate.
- The machine-fly is nearly a metronome: the beat chain dominates bus
  traffic, so "every 150 deliveries" beats at the Timer's own pace — the
  detour accidentally VISUALIZED the substrate's breathing. Nobody planned
  an instrument; the pond became one.
- The loop closes: fireflies hear the machine's flashes and get nudged by
  them; the machine counts the traffic the pond makes. A feedback creature
  built in 40 lines with no new substrate — composition depth the structured
  gates never asked for.
- Its cadence is a declared knob ("machine cadence": 150/40/400) — reach into
  the feedback loop while it runs.

Verdict on the project's claim that purposelessness is valuable: **supported
by evidence in this instance** — the detour produced a package-pressure
sighting and an unplanned instrument in under an hour.

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
