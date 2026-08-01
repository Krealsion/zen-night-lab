# Night Lab Marathon — final report

**Six applications, six greens, one substrate.**

**Pinned against:** Loom `78d64ea`, Zengine `f6a4c69`, **ABI v4**. Neither core repository was
modified. Night One is preserved unaltered at `../original/` and was re-verified there.

> The marathon succeeds when we no longer have to ask *"what should Zen build next?"*, because six
> different applications have answered. They did — and the loudest answer was not the one the brief
> expected.

---

## 1. Portfolio status

| # | project | verdict | cases / assertions | mutations | canary |
|---|---|---|---:|---|---|
| 1 | kitchen-replay | **GREEN** | 39 / 196 | 14 RED, 2 GREEN | RED on a full 39-case run |
| 2 | download-manager | **GREEN** | 30 / 153 | 14 RED, 0 GREEN | RED |
| 3 | build-farm | **GREEN** | 29 / 153 | 16 RED, 1 GREEN | RED |
| 4 | import-pipeline | **GREEN** | 26 / 134 | 15 RED, 0 GREEN | RED |
| 5 | lobby | **GREEN** | 17 / 99 | *see §11* | RED |
| 6 | scheduler | **GREEN** | 18 / 113 | *see §11* | RED |
| | **total** | **6 GREEN, 0 BLOCKED** | **159 / 848** | | |

Plus `repro-answer-seam` (project 1), which exits 0 and is a `ctest` case of its own.

**No project ended PROVEN BLOCKED.** Project 5 was the candidate and deliberately did not: the
distinction it exists to test *is* expressible, at a price, and recording the **price** is more
useful than recording a wall. See §5.

**Every GREEN mutation is a reported gap, not a shrug** — each is named in §11 with its reason
and, where it is masked, with the mechanism that masks it.

---

## 2. Order chosen, and why each next

| after | chose | because |
|---|---|---|
| — | **kitchen-replay** | Recalibration before anything else. Everything later depends on knowing what current Zen actually does, and Night One left seven named seams to re-test. It also produced the authoring idiom the other five reused. |
| 1 | **download-manager** | Night One *named* the promise book and could not validate it. A second consumer that stresses it where the kitchen did not — public progress, many operations, cancellation — is the smallest thing that turns one sighting into two. |
| 2 | **build-farm** | The comparison checkpoint needs two implementations to compare. Built to be structurally different on four axes chosen in advance, so any agreement between them would be *evidence* rather than habit. |
| 3 | **import-pipeline** | With responsibility settled, the open question was the *other* named shape: order/menu resolution had two sightings (Timer, kitchen) and needed a third from a domain where the menu is discovered rather than declared. |
| 4 | **lobby** | Four projects had by then written the same sentence about role provenance and treated it as bookkeeping. The next thing worth knowing was whether it is a *security* fact — and that needs a domain where believing a forgery costs something real. |
| 5 | **scheduler** | Last, deliberately: the composition question ("do two pleasant APIs stay pleasant together?") is only answerable once both are familiar, and the accumulated idiom made the *friction* legible instead of drowned in novelty. |

**No optional seventh project, and the brief's own test is why.** A seventh must be chosen *to
distinguish between two architectural interpretations*, with the hypothesis stated first. After six
the evidence never split two ways: the candidates were either **unanimous** (describe-then-hand-over,
6/6; role provenance, 5/6), **unanimously absent** (sequence owner and outcome ergonomics, 0/6), or
**disagreeing for a reason already understood** (the promise book's three consumers differ on
continuity policy, and a seventh domain would add a fourth opinion rather than break a tie). The one
thing a seventh could still settle is whether an extracted `PromiseBook` reduces ceremony or
relocates it — and that is an *extraction experiment*, not an application, so it is ranked as errand
5 in §8 instead.

---

## 3. PreparedReplacement verdict

> **Yes. Night Lab preferred the handle, and not because it is shorter.**

**178 semantic facade operations across six projects. Zero raw prepared-replacement operations in
application code.** Both numbers are grep counts, not estimates — an earlier draft of this report
said 237 by counting the handle's *accessors* (`id()`, `candidate()`, `incumbent()`, `started()`)
as operations. They are 24 more calls, almost all of them in test assertions, and they are counted
separately in EVIDENCE.md. The number that matters is unchanged and exact: no `begin_prepared_replacement`,
`ask_candidate_to_prepare`, `accept_preparation_answer`, `commit_prepared_replacement`,
`abort_prepared_replacement`, `host_lifecycle_authority`, `load_candidate`, `seal_weave` or
`admit_candidate` appears in any of the six.

**Where it fell through to raw machinery: nowhere.** The one nonzero anywhere in the audit is two
uses of `upgrade().id()` in project 6, and it is not a fall-through — `timer::PrepareTimerHandover`
carries a transaction field for wire legibility (its own header says it is *not* authority), so an
application driving a **third-party service** through the handle reaches for the id the handle
already exposes as diagnostics. Classification: **third-party vocabulary that predates the handle.**
Not missing sugar.

**Why "naturally preferable" and not merely "shorter":**

1. **Nobody was ever tempted by the raw path.** Six applications, six domains, six replacement
   stories, and not one of them wanted something the handle would not do — although the brief
   explicitly invited a raw control vertical as a control.
2. **It made the domain decisions the only decisions left.** In every project the application code
   reads: start, ask *this domain question*, offer *what I am holding*, commit *when I decide*. Six
   different domains, and the only thing that varied was the domain.
3. **It did not care whose service it was replacing.** Project 6 drives this package's own worker
   and **the Zengine Timer** through the same five calls, from one coordinator class, and the diff
   between the two paths is four lines.
4. **It never hid a refusal.** Every substrate refusal any project encountered was inspected by its
   own name — `NoRoleHolder`, `CandidateLoad` (carrying the loader's own words), `IncumbentBusy`,
   `AlreadyStarted`, `InvalidReadiness`, `CandidateRefused`, `ExplicitAbort` — and not one had to be
   reconstructed from a bool. Each project's *domain* refusals ride on top as their own text.

**The one thing the handle does not do, and it is not sugar's job:** it drives *one* conversation
with the incoming holder and knows nothing about the outgoing one. Every project had to invent
`describe-then-hand-over` by hand. See §4.

---

## 4. Repeated abstraction candidates, ranked

### EARNED

**1. `describe-then-hand-over` — 6/6 sightings, and it is a substrate gap wearing a pattern's
clothes.**

There are two replacement ceremonies and they are **disjoint**:

| | graceful swap (Weave Manager) | prepared replacement |
|---|---|---|
| talks to | the **outgoing** holder | the **incoming** holder |
| preserves work | yes (the letter) | **no** — the incumbent is never told |
| verifies the successor | **no** | yes |
| window with an unasked holder | yes | **none** |

Neither gives both, and **nothing in either hints the other exists**. So all six projects
independently discovered the same bridge: *the preparation window is the one interval in which the
incumbent is alive and the successor is reachable*, and asked the incumbent to **describe** itself —
an ordinary question that changes nothing, unlike `PrepareShutdown`.

What crossed differed every time, and that is why this is not one helper:

| project | what crossed | the conversation |
|---|---|---|
| kitchen | the **work** | continues |
| download | the **obligation** to report a failure | ends |
| build farm | the **intent** | restarts as attempt N+1 |
| import | the **question** | reopens with a new menu |
| lobby | the **fact that somebody is waiting** (and a count of what cannot cross) | degrades |
| scheduler | the **fleet and a tally** | continues |

**Do not extract a helper. The repeated thing is a hole.**

**2. Role authorship — 5 independent sightings, and it is the only CORE-DESIGN candidate.** See §5.

**3. "Which half do you attest?" — 3 sightings, and it is a decision, not code.** Loom grants one
authenticated answer per request. Every long operation must choose which single message it defends,
and the download manager measured that the sum is exactly 1 in both directions. No abstraction can
help; a *sentence in the docs* can.

### PROMISING

**4. The promise / responsibility book — 3 consumers, and the three disagree.** All three grew: a
bounded book, a visible refusal at the bound, an addressee + correlation per entry, and the
invariant that *every entry leaves the book through a message*. **Five identical elements.**
But the parts anyone would want *inside* it — continuity policy, progress shape, identity naming,
whether a watchdog is needed at all — are exactly what the domains disagreed about. **Extraction was
deliberately not attempted**; a helper would have had to take a position on all four.

**5. Minted-identity namespaces — 3 sightings, 2 of them defects.** The kitchen carries `next_job`
and got it right. The build farm's `attempt` had **two authors**. The import pipeline's `next_menu`
did not cross at all, and a successor minted a name a requester was still holding. The rule:
*the identity is per-incarnation; the namespace must not be.*

**6. Order/menu resolution — 2 in this marathon (+Timer = 3).** Reproduced exactly in the import
pipeline, including *refusal-is-not-a-menu-entry* and *unknown-spellings-refused*, and the
**resolved choice** finally earned its own step. But four of six domains tested it and it was
genuinely absent — this is a shape for services that must *offer* something they discovered, not a
general one.

**7. Stringified `WeaveId` on the wire — 4 sightings.** Every project that needed to write an
address into a payload spelled it as decimal Text, with the same comment about `Int` being signed.

### NAME ONLY

- **Activation hold/replay** — 1 sighting (the kitchen's, re-confirmed live by mutation 04). Five
  other projects had no bootstrap race, because prepared replacement makes admission *be* the
  activation.
- **The binding table should be dynamic** — 1 sighting (scheduler).
- **Attestation for observers** — 1 sighting (lobby), and it folds into role authorship.

### REJECTED

- **Activation-sequence owner — 0 sightings in six.** Five `~` and one `×`. Every project has one
  operator and one counter; nothing contended, *including* the project that replaces two different
  services in one program.
- **Outcome observation ergonomics — 0 sightings in six.** `state()` and `take_outcome()` were
  pleasant every time. Nobody wrote a line of glue. **Delete it from the roadmap.**

---

## 5. Core blockers

### BLOCKER 1 — Loom attests answers and lifecycle; it does not attest ROLE-HOLDING

**Five independent sightings**, escalating in consequence:

| project | the forgery | what it costs |
|---|---|---|
| kitchen | `Plated` | a dish is "served" that nobody cooked |
| download | `DownloadCompleted` | an operation ends; only a domain digest catches it |
| build farm | `JobDone` | a build "succeeds" with a fabricated artifact |
| build farm | `WorkerOpen` | **a healthy build is destroyed** — an announcement is *evidence* here |
| lobby | `MatchCreated` | **a player leaves for an attacker's server** |

**Minimal reproducer:** `lobby/test_lobby.cpp`, `PUSH: AN UNRELATED WEAVE CAN CREATE A MATCH`. A
weave holding nothing but an ordinary `allow_to_any` grant for a public shape sends one message and
two players leave the lobby.

**Current behaviour.** A receiver can check the correlation (a number it chose) and the bus-stamped
sender (a `WeaveId`). It cannot ask whether that sender **held role R when it spoke**, and
`send_to_role` describes where a message *went*, never what capacity it came *from*.

**Why application-level composition cannot solve it.** Two workarounds exist and both were built
and measured:

- *Ask a registry who holds the role.* The registry's belief comes from an **unauthenticated
  announcement**. Turtles.
- *Invert push to pull* — make the office **answer each receiver's own request**, so the statement is
  Loom's attested answer. **It works** (`PULL: the match a player acts on is Loom's answer…`). Its
  price, measured:
  1. `kMaxDeferredAnswers` is **64 and belongs to one Loom** — a lobby of 65 waiting players
     exhausts every other weave's ability to hold a conversation (`HOLDING AN ANSWER SPENDS ONE
     LOOM'S CAPACITY`);
  2. an answer right belongs to the life that earned it, so **a replaced office strands every waiting
     receiver** — and a strict receiver then refuses **the honest successor** for exactly the reason
     it refuses a forger (`THE COST`);
  3. it does nothing at all for **observers**: a publication can never be attested to anybody
     (`THE REGISTRY IS A RECEIVER TOO`).

**The narrowest apparent seam.** A **delivery fact** of exactly the kind `Mail` already carries
twice (`answers_ask()`, `lifecycle_attested()`):

> *"the sender of this delivery held role R at the moment it was sent"*

Nothing above the bus can synthesise it. The import pipeline is the control that proves the shape:
it is the one project that **can** perform the check, and the reason is exact — its counterparty is
a specific weave rather than an office.

**Affected projects:** 5 of 6 (kitchen, download, build farm, lobby; import as the inverted
control). **Severity: high** — it is the only finding whose worst case is a user going somewhere an
attacker chose.

### BLOCKER 2 — a weave cannot see the fate of its own send

**Re-tested and unchanged.** `send_to_role` to an unheld role is refused and the sender is told
nothing: no ticket outcome, no event. Only a host tap can see it.

**Reproducer:** `kitchen-replay/test_kitchen.cpp`, `a station named by nobody real`.

**Why composition cannot solve it:** it cannot. Every project that needed to notice absence built a
**clock** — the kitchen's watchdog, the farm's sweep, the scheduler's patience. The build farm found
the one partial alternative (**reconciliation**: a replacement produces an announcement, so requeue
at once) and also found its cost — it trusts an unauthenticated publication, which is Blocker 1.

**Affected projects:** 3 of 6. **Severity: medium** — every application can work around it, and
every application must.

### NOT A BLOCKER, and worth saying: what got fixed

Night One's sharpest finding — **`Mail::answer()` is native-only and fails silently across the `.so`
seam** — is **CLOSED**, and `repro_answer_seam.cpp` measures three things rather than one: the door
exists, the old workaround still works, and **a second answer is refused with the weave told about
it**. The third probe is the one that matters: the original complaint was never *"answer does
nothing"*, it was that a weave could not tell.

Night One's friction 4 — **`TimedWeave` and the activation moment are mutually exclusive** — is
**CLOSED**. `on_timed_activation()` exists, the scheduler does real domain work in it, and a derived
raw activation handler is now a compile-time refusal that names the alternative.

---

## 6. Sugar findings — what still feels hard to write

### Loom ceremony

- **The handle is the host's; the conversation is a weave's** (F2, 6/6). `PreparedReplacement`
  needs `Switchboard&`; `offer_current_answer` must be called from inside the coordinator's
  delivery. So every replacement grows a raw `PreparedReplacement*` across that boundary,
  re-pointed by hand, null-checked in the handler. Loom's own suite does the same thing.
- **The bus authenticates THAT the candidate answered, not WHAT it said** (F3). The Ready/Refused
  verdict is the coordinator's mapping. Defensible, undocumented, and reads stronger than it is.
- **"Activation first" means the first delivery *as part of the world*** (F1). The preparation
  conversation is delivered before it and must be. An author who has only read the phrase writes
  the wrong assertion — this one did.

### Package-specific ceremony

- **The Timer binding table is authored, not dynamic** (F14). A weave whose rhythm is *data* cannot
  express it as bindings and falls back to counting one authored beat — which is what the kitchen
  did with the raw protocol before the sugar existed.
- **A third-party preparation vocabulary carries a transaction id** (F15), so an application driving
  it through the handle touches `id()`. The only nonzero in the audit.

### Real domain decisions (not friction, and correctly left alone)

- **Which half of an operation to attest** — you get one.
- **Whether work can cross a replacement at all** — a property of the domain, not the substrate: a
  build is re-derivable from three strings, a half-downloaded file is its bytes.
- **Requeue or fail** — the farm retries absences and fails judgements, and the distinction is real.

### C++ / framework tax

- Missing `using TimedWeave::on;` (a *good* error — it says what to do).
- doctest forbids `&&`/`||` inside an assertion (F17).
- Reading a loaded weave's state from the host needs a hand-built schema (F6).

---

## 7. Surprises

**The substrate was better than expected, three times.**

1. **`PreparedReplacement` needed no help at all.** Zero raw operations across six projects and 237
   facade calls. The expectation going in was that *some* project would need to reach under it; none
   did, including the one that replaces a third-party service.
2. **The Timer binding layer already handles Timer replacement, for free.** Replace the clock
   underneath a `TimedWeave` and the new service's `TimerReady` reconciles every authored binding.
   Not one line of application code. Nobody designed that composition; it fell out.
3. **The seal is a security property, not just a lifecycle one.** Of the lobby's five speakers, the
   *predecessor* is the one the substrate handles completely: a retired incumbent is sealed by the
   admission and cannot speak into the world at all.

**An anticipated abstraction did not repeat.**

- **Activation hold/replay** was expected to be the marathon's third sighting — Night One had two
  independent ones (Timer R2B-0 and the kitchen) and predicted a Loomstd home. It appeared **once**,
  in the project that inherited it. Prepared replacement removes the race by construction: admission
  *is* activation, so there is no window in which a message can reach an unactivated heir.

**Two patterns thought identical turned out different.**

- **Continuity.** "What crosses a replacement" looked like one question with one answer. It has
  four, and which one is correct is decided by the *domain*: work, obligation, intent, or question.
- **Absence detection.** The kitchen's watchdog and the farm's reconciliation look like the same
  mechanism. They cover **disjoint** cases — an arrival versus a silence — and neither subsumes the
  other. The farm needs both.

**And one unwelcome surprise.** The mutation harness lied in a *third* new way (F16): a pattern that
matches nothing writes a byte-identical file and reads GREEN. It was caught by a mutation
contradicting an assertion, repaired in all six harnesses, and then immediately caught four more
broken patterns. Night One's version of this bug was perl failing to *write*; this one is perl
writing the *same thing*.

---

## 8. Recommended next five errands

Ranked by consumers × severity × leverage ÷ risk. **Core work is not automatically first.**

### 1. Role-holding provenance as a delivery fact — *core, high, 5 consumers*

`Mail::sender_held_role()` or equivalent: one more delivery fact beside `answers_ask()` and
`lifecycle_attested()`. Five independent sightings; the only finding whose worst case is a user
going somewhere an attacker chose; both application-level workarounds built and their prices
measured. **Risk: medium** — it is a new attestation on the delivery path, which is the most
carefully guarded code in the substrate.

### 2. Write down what prepared replacement does NOT do — *docs, high leverage, near-zero risk*

Six projects independently discovered that the two ceremonies are disjoint. `replacing-a-service-
safely.md` should say, in one paragraph: *the incumbent is never told; work in flight is lost unless
your application arranges continuity during the preparation window; here is the shape that does
it.* Plus the three prose corrections: what "activation first" means (F1), that `PreparationAnswer`
is the coordinator's mapping (F3), and that `defer_answer()` spends a **Loom-wide** resource (F10).
**This is the highest value-per-hour item in the list.**

### 3. A `describe`-side ceremony for prepared replacement — *core-adjacent, medium, 6 consumers*

Not a helper — a *place*. Every project invented an ordinary `DescribeX` → `XDescribed` exchange with
the live incumbent because there is nowhere for it. What crosses must stay the application's
(the four answers differ), but the **timing and the guarantee** — "ask the incumbent while it is
still serving, hand the result to the candidate in the ask" — are the same six times. **Risk: low**;
it composes two things that already exist.

### 4. Make `defer_answer()`'s cost visible at the call site — *core, medium, 2 consumers*

`kMaxDeferredAnswers` is one Loom's. Two projects hit it: the download manager measured the
exhaustion, the lobby found it is the price of the only provenance workaround available. Options
range from a doc sentence through a per-weave sub-bound to a `remaining()` query. **Risk: low.**
**Do not raise the bound** — the bound is right; its invisibility is the problem.

### 5. Night-Lab-local `PromiseBook` prototype — *lab-only, medium, 3 consumers*

The brief permitted extraction after the build farm and it was deliberately declined, because the
three consumers disagree on four axes. The right next step is a **prototype with both original
implementations kept beside it** and a measurement of whether it reduces non-domain ceremony or
merely relocates it. **Risk: low** (it is Night Lab), **leverage: uncertain** — which is exactly why
it should be built as an experiment and not as a package.

---

## 9. What NOT to build

**1. An activation-sequence owner.** Zero sightings in six projects. Five marked it *adjacent but
different*; the sixth — which performs two replacements of two different services in one program —
marked it *absent*. Nothing ever contended for the number. It is a solution to a problem no
application in this portfolio has.

**2. Anything around outcome observation.** `state()` and `take_outcome()` were pleasant in all six.
Not one line of glue was written around them anywhere. This one should be **deleted from the
roadmap**, not deferred.

**3. A generic `PromiseBook` in Loomstd.** Three consumers, five identical elements — and four axes
of genuine disagreement (continuity policy, progress shape, identity naming, whether absence
detection is needed at all). A shared helper would have to take a position on all four, and each
position would be wrong for at least one consumer. If it is built, it belongs in Night Lab with both
originals kept for comparison.

**4. A shared "order/menu" vocabulary.** Three sightings including the Timer — but four of six
domains **tested it and it was genuinely absent**. It is the shape for services that must offer
something they discovered, and most services do not.

**5. A hold-and-replay bootstrap layer.** Night One predicted this would be the third sighting and
a Loomstd candidate. It appeared once. Prepared replacement removed the race by construction.

**6. A test-fixture library.** Six identical host fixtures is real (F12) — and a repeating *test
fixture* is much weaker evidence than a repeating *application*. Two of its six pieces are worth
watching (the beat-budget pump and the delivery-order tap, both invented rather than copied); the
other four are ordinary host wiring.

---

## 10. Source state

```
Loom      78d64ea   UNTOUCHED   (git status clean; no marathon commit touches it)
Zengine   f6a4c69   UNTOUCHED   (git status clean)
Night Lab           see below
original/           preserved byte-for-byte; rebuilt at its new path and re-verified
                    at 27 cases / 78 assertions, ctest 2/2 -- exactly its own report's numbers
```

Every mutation harness restores its sources and rebuilds, and each ends with a residue grep for its
own marker text. Verified clean at the end of the run.

---

## 11. Mutation results in full, and every non-RED line explained

Roughly ninety mutations across six matrices. Every one rebuilds the **whole** binary and runs the
**whole** suite under a timeout; every matrix begins with a hand-chosen canary that must come back
RED on a **full** case count, prints the case/assertion counts on every line so identical counts are
visible, treats a run with fewer cases than baseline as `TRUNCATED` rather than RED, and ends with a
residue grep for its own marker text.

### The GREENs — three, each a reported gap

| project | mutation | why it stayed green | what was done |
|---|---|---|---|
| kitchen | *an inherited roster OVERWRITES a station that announced during the handover* | **genuinely unwatched.** Making an announcement land inside the handover window deterministically needs machinery Night One judged not worth building, and this replay agreed. The term is true by construction. | **reported**, as Night One reported it |
| kitchen | *a station adopts a letter written for a different station* | **MASKED, and the masking mechanism is the substrate's:** a `Bequest` is delivered by the Weave Manager only to the claimant of the role it names, and a station's role determines its name. Expressing the attack needs a *forged attested answer*, which the honest API cannot produce — the unsayable-attack case. | **reported as masked**, with the mechanism named |
| build farm | *a StageDone is believed even when it names a worker the job never went to* | **masked by the terminal check:** mutation 17 cuts the same term on `JobDone` and is RED, so the itinerary rule *is* watched — on the message where believing it costs something. A mis-attributed progress line changes no outcome. | **reported as masked** |

### The repairs — and what needed re-running, and what did not

Four kinds of non-result showed up, and all four are harness defects rather than evidence:

| kind | count | cause |
|---|---:|---|
| `NOT-APPLIED` | 4 | a pattern that matched nothing (**F16**) — including two where an unescaped `++` in a regex is a possessive quantifier rather than two plus signs |
| `BUILD-FAILED` | 3 | a cut that orphaned a variable or parameter under `-Werror` |
| `TRUNCATED` | 1 | a cut that left a loop reading past a shorter vector — a crash, not a property |
| `GREEN`-but-unexpressible | 1 | a mutation whose attack the scenario could not stage |

**Every one was repaired and re-run.** What did **not** need re-running, and the reasoning is worth
keeping: *a mutation that fails to apply leaves a byte-identical tree, which can only produce the
baseline result* — so every RED verdict in every matrix is unaffected by the F16 repair and stands
as recorded. Only non-RED lines were re-run. Both mutation harnesses that needed it also gained an
id filter (`bash <project>/mutate.sh 04 05`) so a repaired line can be re-run without redoing a
matrix, and the canary is never skipped by the filter.

### Two coverage gaps found by mutations and closed by cases, not by notes

The discipline says *unwatched ≠ redundant, and the fix is a new case*:

- **build farm** — *the queue is LIFO* stayed green because two builds cannot distinguish LIFO from
  FIFO: the first is dispatched immediately and only one ever waits. A three-build case with one
  worker was added; the mutation is now RED.
- **import pipeline** — *a candidate adopts more conversations than the bound* stayed green because
  nothing ever handed one more than the bound. A case was added; the mutation is now RED.

### The classification the brief asks for

Not every claim in this portfolio is defended by a mutation, and the ones that are not are defended
better:

- **Semantic RED** — the ordinary case, and the bulk of the ~90. A mutation expresses a *behaviour*
  ("match with however many are here", "believe any RouteChoice", "claim you finished a transfer you
  never had the bytes for") rather than the deletion of a line, and a case goes red.
- **MASKED** — three, each named above with the mechanism that masks it, and never reported as a
  pass. Where the masking mechanism is the substrate's (the role-keyed `Bequest`), expressing the
  attack would need a *forged attested answer* — which the honest API cannot produce. That is the
  unsayable-attack case, and the honest thing is to say so rather than to write a test that passes
  while testing nothing.
- **COMPILE-ENFORCED** — no mutation needed, and no mutation possible:
  - `PreparedReplacement` is non-copyable and non-default-constructible (`static_assert`s in Loom's
    own suite);
  - a `TimedWeave` that forgets `using TimedWeave::on;` does not compile, and says why in a
    sentence;
  - a `TimedWeave` that declares its own raw `on(zen.Activated)` does not compile, and names
    `on_timed_activation` as the alternative. **This is the one Night One asked for**, and it is the
    strongest kind of answer: the old failure was *silent*, and the new one is a paragraph.
- **STRUCTURALLY IMPOSSIBLE** — the hold-and-replay race. Under prepared replacement admission
  **is** activation, in one envelope, so there is no window in which a message can reach an
  unactivated heir. Five of six projects needed no bootstrap window at all, and that is not because
  they were careful.
- **COUNT-DETECTED** — every matrix prints case and assertion counts on every line, treats
  `cases < baseline` as `TRUNCATED`, and would show identical counts across every mutation if
  nothing had rebuilt. All three tells fired at least once during this run.
