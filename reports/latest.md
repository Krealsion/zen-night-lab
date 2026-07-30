# Night Lab — morning report

**Session:** 2026-07-30 · **Experiment:** the job kitchen
**Pinned against:** Loom `d7dd974`, Zengine `93eef58` (see `vendor/README.md`)
**Status:** working. 27 cases / 78 assertions green, plus the seam reproducer. 12 of 13 mutations red.

---

## Experiment

**A job kitchen: three roles, six loadable weaves, one vocabulary.**

- `kitchen.expediter` — owns the ticket book: capacity, the promise, a watchdog, and the final word.
  Knows no recipes and makes no routing decision.
- `kitchen.policy` — owns routing: preference, fallback, and the reason. Pure, a function of the
  query, replaceable by swap. Two builds from one source (`house` honours the diner, `rush` honours
  the specialist).
- `kitchen.station.*` — owns cooking: a menu, a pass rate, and progress. Three builds from one
  source (`grill`, `grill-2`, `fryer`).

### The architectural question

Loom's law is that **death ends a conversation**. But death is *silent to the other party*. An
ordinary weave cannot ask who holds a role, cannot learn that a weave was unloaded, and cannot see
the fate of its own send — so a cook that vanishes mid-dish produces exactly nothing: no refusal,
no event, no word.

> **Can a kitchen therefore keep an honest promise for every order it accepts — including orders
> whose cook disappears mid-dish — using only existing public Loom and Zengine behaviour?**

Secondary questions the same shape happened to answer: does the Timer package's
*request → menu → resolved choice → receipt* order model transplant to a second, unrelated domain?
Can domain policy really be swapped underneath a running service? What can and cannot a careful
consumer check about an arriving message?

---

## Result

**Working — and the answer is a qualified yes, with one substrate gap paid around and one
architectural gap that a project-local solution cannot close.**

Yes: every promise this kitchen makes ends in a message to the diner. The mechanism is three
ordinary things — a bounded ticket book, a role-addressed watchdog beat, and the discipline that
*every* ticket leaves the book through a message. Silence is never an outcome.

The price is stated plainly: **absence is only ever discovered by a promise going unkept.** The
kitchen cannot know a station left; it can only notice that a dish it handed over never came back,
and then say so and strike the station from its roster. That is not a workaround the kitchen chose
over a better option — it is the *only* option the substrate offers today.

### The sharpest thing the experiment found

Two conversation styles behave differently under replacement, and the difference is not cosmetic:

| | the receipt | the outcome (`Served` / `OrderLost`) |
|---|---|---|
| mechanism | Loom's authenticated answer, **deferred** across the policy round trip | an ordinary directed message |
| can the recipient verify the sender? | **yes** — `Mail::answers_ask()` is Loom's word | **no** — nothing to check |
| does it survive the sender being replaced? | **no** — an answer right belongs to the life that earned it | **yes** |

You get exactly one of authenticity and continuity per message, and Loom decides which. A promise
is therefore *not* a request/response: Loom authenticates the **acknowledgment**, and cannot
authenticate the **fulfilment**. The expediter closes what it cannot bequeath at the one moment it
is given (`zen.PrepareShutdown`), and bequeaths the rest as words. Both halves are pinned
(cases *"an expediter replaced gracefully keeps the promises it already made"* and *"an expediter
replaced MID-ROUTING closes the conversation it cannot bequeath"*).

### The paired demonstration that carries the whole point

Same moment, two ceremonies, visible from the diner's chair:

- **hard** replacement of a station mid-dish → `lost h1: station 'grill' took this job and never
  plated it within 40 sweeps; it has been struck from the roster`
- **graceful** replacement of the same station mid-dish → `served g1: brisket from grill`

The difference is the letter, carrying `passes_left` — progress, never a due time. Replacement
downtime is *paused*, which is continuity of a delay and must never be described as preservation of
a deadline. (That distinction is the Timer package's, borrowed because it was right.)

---

## Running it

```sh
cd /mnt/g/programming/cpp/Zen/playground/night-lab
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"

ctest --test-dir build --output-on-failure    # 2/2: kitchen (27 cases / 78 assertions) + answer-seam
./build/kitchen/kitchen-demo                  # the usage example, on the REAL monotonic clock, ~2s
./build/kitchen/repro-answer-seam             # the core-seam reproducer
bash tests/mutate.sh                          # 13 mutations; ~10 min
NIGHT_LAB_TRACE=1 ./build/kitchen/night-lab-tests   # every delivery and refusal, from the host tap
```

WSL/GCC only (the kernel is `dlopen`/POSIX ground). Nothing writes to `Zen/Loom` or `Zen/Zengine`;
the lab builds against a pinned snapshot under `vendor/`. That decision paid for itself the same
night: `Zen/Loom` picked up in-progress R2B-3b edits to `switchboard.hpp`/`.cpp` while this ran, and
the lab neither saw them nor was disturbed by them.

### The demo transcript (abridged)

```
-- 3. swap the routing policy underneath a running kitchen -------
  op      | swap policy -> rush -> 9

-- 4. the same order, a different brain -------------------------
  order   | a3: fries (prefer 'grill', fallback 'any_station')
  receipt | a3: routed_fallback @fryer -- [rush] the rush kitchen sends 'fries' to the
            specialist 'fryer' rather than the preferred 'grill'
  SERVED  | a3: fries from fryer

-- 5. ...but a REQUIRED preference binds even the policy that disagrees --
  order   | a4: fries (prefer 'grill', fallback '')
  receipt | a4: routed_preferred @grill -- [rush] station 'grill' was required and can cook 'fries'
  ...the grill walks out, mid-dish, saying nothing to anyone.
  op      | evict the grill -> REFUSED: open failed: .../no-such-weave.so
  LOST    | a4: station 'grill' took this job and never plated it within 40 sweeps;
            it has been struck from the roster
```

---

## What felt natural

**Roles.** Addressing a service by role and letting the holder be replaced underneath is so
frictionless that the policy swap needed no design at all — it is just `zen.SwapWeave` on
`kitchen.policy`, and the expediter never learns it happened. Three replaceable seats fell out of
the architecture rather than being built into it.

**`StartRoleTimer`.** A beat that belongs to a *slot* rather than to an incarnation is exactly the
right primitive for a watchdog and for a station's work pass. A replaced expediter inherits the
sweep without asking; a replaced station inherits the pulse. This is the single most load-bearing
Zengine idea in the experiment and it needed no adaptation.

**The letter (`PrepareShutdown` / `Bequest` / `ClaimBequest`).** Two independent weaves needed
continuity for completely different reasons, and both got it from the same three shapes with **no
Loom change and no wall**. The Timer package's *intent / progress / binding-lifecycle* triage
transplanted directly: the station carries only `passes_left`, the expediter carries only what a
re-ask cannot reconstruct. Naming those three kinds of state made every "should this cross?"
question answerable in one sentence.

**The order model.** *request → available menu → resolved choice → receipt*, with **refusal as an
outcome and never a menu choice**, and unknown spellings **refused rather than guessed**, moved from
timers to job routing without a single adjustment. That is now the **second** package wanting this
shape; the Timer's own note says a third is the trigger for a shared vocabulary.

**`Mail::answers_ask()`.** For the conversations where it *is* available it is complete and cheap.
The forged-`RouteChoice` case hands a rogue weave a perfectly-shaped answer with the correct
correlation and it changes nothing — the wall is Loom's word, not a secret.

**Refusals as values.** Every failure in this kitchen is a message with a stranger-readable reason,
because that is what every Loom surface already does. Nothing threw.

---

## Friction

**1. `Mail::answer()` is native-only and fails silently across the `.so` seam.** The single largest
time cost of the night. See *Possible core seam* — this is the finding, not a complaint.

**2. A message can reach an heir before the heir's own `zen.Activated`.** A bus trace caught a
`Plated` delivered to the incoming expediter **two turns ahead of** its activation and long before
its letter-claim resolved. Handling it then found no such job, ignored it as noise, and left the
inherited ticket to time out — *a promise broken by the handover itself.* The fix is the Timer
package's: a bounded hold-and-replay window that **opens at construction, not at activation**. This
is the second package to independently need that exact mechanism. Its consequence is also the same
one, and is named in the code: a weave that is loaded and never activated holds forever.

**3. A weave cannot see the fate of its own send.** `send_to_role` to an unheld role is refused, and
the sender is told nothing — no ticket outcome, no event. This is the reason the whole watchdog
exists. The host *can* see it, from a tap, which is why `NIGHT_LAB_TRACE=1` exists in the harness.

**4. `timer::TimedWeave` and the activation moment are mutually exclusive.** The binding is the
nicest thing in Zengine and it owns `on(zen.Activated)` to do its work. Any weave that needs that
moment for something else — claiming a letter, announcing itself — cannot use the binding: a derived
`on(const loom::Activated&, Mail&)` **suppresses** the base one through the using-declaration (C++
name hiding), so the timers silently stop being established. It is a *silent* miss, not a compile
error, and the `using TimedWeave::on;` static_assert does not catch it. Three of the four weaves in
this kitchen wanted the binding and none could use it. Recorded in `station.cpp` at the point of
decision.

**5. A grant derived from `Emit<...>` is the wrong shape for test-only nudges.** Three separate
"why is nothing happening?" debugging rounds were a harness weave being sent a shape that was not in
its own emit silhouette, so its `send_as` was refused where nobody could see it. The fix is right
(an explicit `mount_granted` grant, written out); the failure mode is the same invisible one as
friction 3.

**6. With a beat chain running, the beat is the queue's clock.** A multi-hop conversation costs
roughly one beat per hop, so a test that pumps "enough beats" for a three-hop exchange has to know
that. Several first-draft cases failed for this reason alone and looked like logic bugs.

**7. `loom::Bus::answer()`'s documented default is honest for the case it was written for and a lie
for the one that matters.** "A Bus that is not a live delivery truthfully answers nothing" is
correct for a shim or a future mailbox. `HostApiBus` *is* a live delivery. The comment made the
silence look intentional at every call site.

---

## Shortcuts and limitations

- **One labelled fake: the Timer's clock.** The suite loads `zengine-timer-virtual.so` — the Timer
  package's *own* suite artifact, the shipped service over a clock whose nap books the duration and
  returns. Same protocol, same table, same letter, same beat chain. It makes every deadline an exact
  integer nobody waited for. The demo runs the real `zengine-timer` on the real monotonic clock, so
  both are exercised. Labelled at the top of `tests/harness.hpp`.
- **`answer_across_the_seam` is a workaround, and reads like one.** `kitchen/answering.hpp` is one
  function, deliberately not named `answer` and deliberately not a member of anything, with the
  whole finding written above it. Delete the file the day the ABI grows an `answer` door.
- **The harness spends real capabilities but the host still starts them.** Diner orders and rogue
  forgeries go out via `send_as`/`send_as_to_role` — stamped as the weave, authorized against the
  weave's own grant. That is the Zengine snake-host pattern, not a root send, but it *is* the host
  poking a weave into speaking.
- **The rogue's grant is hand-written.** It holds ordinary `allow_to_any` for `Plated`,
  `StationOpen` and `RouteChoice` — shapes any weave may legitimately hold. That is the threat
  model, not a hole punched for the test.
- **Not tested, and inherited rather than re-proven:** the `ActivationCursor`'s attestation and
  duplicate rules (Zengine's own suite pins them; this lab uses the vendored header unchanged).
- **Not tested:** the ticket-book capacity bound (32) and the handover hold bound (16). Both refuse
  visibly in code; neither has a case.
- **Known unpinned term (a mutation stayed green, reported as a gap):** the expediter's letter
  adoption *merges* the roster rather than overwriting it, so a station that announced during the
  handover is not forgotten. Making an announcement land inside that window deterministically was
  not worth the machinery tonight; the term is true by construction and unwatched by test.
- **Single-threaded, in-process, `abuse`-tier.** No isolation host, no out-of-process weaves. The
  demo prints its containment note honestly.
- **The kitchen is not a service.** It has no persistence, no backpressure beyond its two bounds,
  and no notion of a diner that goes away.

### Verification completeness

Everything planned ran. **27 cases / 78 assertions**, all green; `ctest` 2/2 including the
reproducer. **13 mutation runs** (canary + 10 targeted + 2 declared-redundancy probes): **12 red,
1 green** — the green one is the roster-merge term named above.

⚠ **The mutation harness lied once, and the canary is what caught it.** The first run reported
`BUILD-FAILED` for every mutation including the canary: `perl -0pi` cannot complete its in-place
rename on the drvfs mount from WSL, so *nothing was ever edited*. Had the canary not been first and
not been required to go red, thirteen meaningless "GREEN" lines would have read as a clean sheet.
The harness now edits through a temp file and says so in a comment.

⚠ **A test can stop pinning the thing it was written for.** Mutation 05 (remove the handover window)
went red, then **green**, then red again: retargeting the continuity cases at a slower dish made the
plate land *after* the handover instead of inside it, so the race stopped being exercised while the
case kept passing. It is now pinned by its own case (*"a dish plated WHILE the kitchen is changing
hands is not dropped"*) that reproduces the original trace. A green suite is not evidence that a
case still tests what its name says.

---

## Possible reusable package

Two boundaries emerged; only one of them is small enough to be worth naming.

**The promise book (small, real, and not built).** The reusable core is not "a kitchen" — it is
about forty lines that any service making deferred promises needs:

> a bounded book of open promises, each with an addressee, a correlation, and a patience;
> a role-addressed sweep that spends patience;
> the invariant that *every* entry leaves the book through a message;
> and a `PrepareShutdown` that closes what cannot be bequeathed and bequeaths what can.

Everything domain-specific (routing, menus, passes) sits outside it. The honest trigger is a
**second** service that needs it — a download manager, an asset pipeline, anything where a worker
holds a job. Tonight there is one consumer, so this is a named seam and not a package.

**The order model (already named elsewhere; this is the second sighting).** *request → menu →
resolved choice → receipt*, with refusal-as-outcome and unknown-spelling-refused. The Timer package
authored it for continuity; this kitchen used the same shape for routing without adaptation. The
Timer's own note says a **third** package wanting it is the trigger for a shared vocabulary. This
report is the second data point, not a request to build it.

---

## Possible core seam

*Evidence only. Neither of these is approved architecture, and the lab changed nothing in Loom.*

### A. `Mail::answer()` does not cross the `.so` seam, and fails silently

**Reproducer:** `repro/answer_seam.cpp` — one source, two weave libraries differing only in which
door they answer through, plus a host with a bus tap. `./build/kitchen/repro-answer-seam`:

```
  repro-answer-direct     answers delivered on the bus: 0 | asker heard an attested answer: NO  | refusals: 0
  repro-answer-deferred   answers delivered on the bus: 1 | asker heard an attested answer: yes | refusals: 0
  => reproduced: the immediate authenticated answer is native-only.
```

**Mechanism.** `loom::Bus::answer()` has a base implementation that returns an invalid `Ticket` and
does nothing. `loom::detail::HostApiBus` (`zen/kernel/export.hpp`) — the Bus a loaded weave is
handed — overrides `send`, `publish`, `send_to_role`, `make_deferred_answer`, `spend_deferred` and
`release_deferred`, and **not** `answer`. `ZenHostApi` (`zen/kernel/abi.h`, v3) has `defer_answer`
and `answer_deferred`, and **no `answer`**. So a loaded weave inherits the base and says nothing —
no queue entry, no refusal event, and no signal except a `Ticket` that `zen/weave/weave.hpp`'s own
documented call shape (`mail.answer(msg);`) discards.

**How it presented, because the shape of the symptom is the point.** The routing policy received
every `RouteQuery` and answered every one; the expediter heard nothing, forever. The bus trace
showed the query delivered, **no answer, and no refusal** — the signature of a message that was
never enqueued. Neither participant could see it. Only a host-side tap could.

**Why a project-local solution is insufficient.** There is a local workaround and this lab uses it
(`kitchen/answering.hpp`: `defer_answer()` then spend immediately, both of which *do* cross the ABI
and the second of which returns a meaningful ticket). But the workaround cannot fix the two things
that matter:

1. **Every future loaded weave will hit this**, and will hit it as *silence* rather than as an
   error. The authenticated answer is the substrate's own answer to "who may reply to a role-
   addressed ask" — R2B-1's whole subject — and it is unavailable in the tier where most weaves
   will live, in the one way that cannot be noticed.
2. **A workaround in one package does not make the next author's code correct.** The natural,
   documented, obviously-right call is the one that silently does nothing.

**The smallest shapes a fix could take** (evidence, not a proposal): an `answer` entry in
`ZenHostApi` mirroring `send`; or, if the deferred path is deemed sufficient, `HostApiBus::answer`
overridden to *be* defer-and-spend, so the public API means the same thing on both sides of the
seam; or, at minimum, making the silence loud — the base `answer()` cannot know it is being called
from a live delivery, but `HostApiBus` can.

### B. Loom attests answers and lifecycle; it does not attest **role-holding**

**Reproducer:** the suite case *"MEASURED, NOT WAVED AT: a forged Plated finishes someone else's
dish"*. A rogue weave holding nothing but an ordinary `allow_to_any` grant for `Plated` — a shape
any weave may legitimately hold — sends one naming an open job, and the diner is served a dish no
station cooked.

**Mechanism.** The standing consumer obligation is *"match the correlation AND the bus-stamped
sender"*. For a role-addressed conversation across replacement, **the second half is not
performable**: the incarnation that plates is legitimately not the one that was sent the `Prep`, so
there is no sender the recipient could have pre-bound. An ordinary weave also cannot ask Loom who
holds a role (`QueryRole` is the kernel control door and needs `load_capability`, which is the
dangerous grant). So the expediter does everything available — the job must be open, past routing,
and the claimed station must be the one the job actually went to (that last term is pinned; mutation
11 reds) — and the forgery still lands.

**Why a project-local solution is insufficient.** The missing fact is not domain knowledge; it is a
*delivery fact* only Loom holds. `Mail` already carries two such attestations (`answers_ask()`,
`lifecycle_attested()`); this is a third of exactly the same kind — *"the sender of this delivery
held role R at the moment it was sent"* — and nothing above the bus can synthesise it.

**Honest scoping.** Today's tier is trusted-in-process, so this is not a live vulnerability; the
Timer package recorded the same class of gap in R2B-0 (a forged `Bequest` can aim a firing at a
third party) and reached the same conclusion. This report is a **second independent sighting** of
"role-addressed traffic has no provenance", from a different package, with a runnable reproducer.

---

## Questions for the humans

1. **Is the `answer` seam (A) a bug or a boundary?** If `Mail::answer` is *intended* to be
   native-only, the fix may be documentation plus a loud failure rather than an ABI door — but the
   current default comment reads as intentional in a case where it is not, and the failure is
   invisible. Either way, does it want an errand or a phase?
2. **Does role-holding provenance (B) belong to R2B, or does it wait for the identity phase?** It is
   the same shape as `answers_ask()` and `lifecycle_attested()`, it now has two independent
   sightings (Timer R2B-0, this kitchen), and it is the missing half of a consumer obligation the
   codebase already asks every consumer to perform.
3. **The hold-and-replay bootstrap window is now a pattern in two unrelated packages.** Is that the
   trigger for naming it (Loomstd-tier), or is two still a coincidence? Note that the *consequence*
   is also identical in both: a weave loaded and never activated holds forever.
4. **`TimedWeave` versus the activation moment (friction 4).** Three of four weaves here wanted the
   binding and none could use it, because the binding owns `on(zen.Activated)` and a derived handler
   suppresses it **silently**. Is there an appetite for an author hook (`on_activated()`) that the
   binding calls after reconciling — or should the collision at least become a compile error?
5. **Is the promise book worth a Loomstd home, or does it wait for a second consumer?** The lab's
   own instinct is: wait. Asking because that is a scope call, not a code call.

---

## Suggested next experiment

**A simulated download manager, on the promise book — as a second, adversarial consumer.**

It is the smallest thing that would test the one boundary this night *named but could not
validate*: whether "a bounded book of open promises with a sweep and a mandatory exit message" is
genuinely reusable or was just this kitchen's shape. A download manager stresses it differently in
three specific ways the kitchen never did:

- **partial progress that must be reported, not just preserved** — a station's `passes_left` was
  private; a download's bytes-so-far is something the requester wants *while it is happening*, which
  forces the question of a progress channel that is neither an answer nor an outcome;
- **many workers per job and many jobs per worker**, where the kitchen had one station per job;
- **cancellation from the requester's side**, which this kitchen deliberately has no vocabulary for
  and which Loom's `release_deferred` explicitly does not report (V1 has no cancellation vocabulary).

If the promise book survives that unchanged, it is a package. If it does not, the report says what
broke — which is a better answer than building it tonight would have been.

*(Recommended, not begun.)*
