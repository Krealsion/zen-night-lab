# Project 1 — Kitchen replay

**Verdict: GREEN.**
39 cases / 196 assertions · `repro-answer-seam` exits 0 · mutations **14 RED, 2 GREEN**, canary red
on a full run, no build failures, no timeouts, no truncations, residue clean.

**Pinned against:** Loom `78d64ea`, Zengine `f6a4c69`, ABI v4. Night One ran on Loom `d7dd974`,
Zengine `93eef58`, ABI v3, and is preserved unmodified at `../../original/`.

---

## Purpose

Night One built a job kitchen and left seven named seams. A year of substrate work later, the
question is not "does the kitchen still work" but:

> **What did that work actually change for an application that already existed?**

So this is a **replay**, not a Kitchen 2. The domain, the three roles, the two conversation styles
and the architectural question are Night One's, kept deliberately recognisable. Every difference is
either a substrate change or a new substrate capability the application had to say something to
use.

The kitchen's own question is unchanged and was re-proven from scratch:

> Loom ends a conversation when a participant dies, and tells the other party nothing. Can a
> kitchen keep an honest promise for **every** order it accepts — including orders whose cook
> disappears mid-dish — using only existing public Loom and Zengine behaviour?

---

## Domain model

Three roles, eight loadable weaves, one vocabulary.

| role | owns | replaceable by |
|---|---|---|
| `kitchen.expediter` | the ticket book: capacity, the promise, the watchdog, the final word | graceful swap (letter) |
| `kitchen.policy` | routing: preference, fallback, and the reason. Pure. | swap, either kind |
| `kitchen.station.*` | cooking: a menu, a pass rate, progress | **both** ceremonies — that is the new material |

Two conversation styles, on purpose: the **receipt** is Loom's authenticated answer deferred across
the policy round trip (trustworthy, un-inheritable); the **outcome** (`Served`/`OrderLost`) is an
ordinary directed message (survives replacement, carries no attestation).

---

## Zen surface used

| surface | how it appeared |
|---|---|
| role-addressed delivery | every service address; the whole reason replacement is invisible to callers |
| direct delivery | outcomes to a diner, by id, from a stringified `WeaveId` in the book |
| publication | `StationOpen` — presence is announced, never discovered |
| authenticated **immediate** answer | `mail.answer()` for receipts-that-refuse, `PrepDeclined`, `KitchenStatus`, and the whole preparation conversation |
| authenticated **deferred** answer | the receipt across the policy round trip; the candidate's readiness under `consult` |
| answer provenance (`answers_ask`) | the expediter's wall against a forged `RouteChoice`; the diner's wall on a receipt; the station's wall on a bequest |
| `zen.Ack` / `zen.Refused` / `zen.Result` | steward answers and the diagnostic |
| Weave Manager swap (graceful + hard) | the letter ceremony, unchanged from Night One |
| `loom::PreparedReplacement` | **new** — the whole of section 9 of the suite |
| Zengine Timer `StartRoleTimer` | the watchdog sweep and the station pass, both role-addressed |
| `zen.PrepareShutdown` / `Bequest` / `ClaimBequest` | continuity under the graceful ceremony |

Never used, and the absence is itself a result: **ordinary reply**. Nothing in this kitchen ever
wanted "reply to whoever sent this" — every reply is either an *answer* (because it must be
provably an answer) or a *role-addressed send* (because it must survive replacement). Between those
two, `reply` had no job.

---

## Positive vertical

An order placed by a diner is routed by a swappable policy, cooked by a station on a Timer beat,
and served — with a receipt naming the resolved choice and its reason, and an outcome that always
arrives. The demo runs the whole thing on the **real monotonic clock**; the suite runs it on the
Timer package's virtual clock so every deadline is an exact integer nobody waited for.

And the new one:

```
station v1 serving orders
  -> owner asks the LIVE incumbent to describe its work        (ordinary ask, changes nothing)
  -> loom::PreparedReplacement.start()                          (candidate loaded SEALED)
  -> .ask(PrepareStation{station, carried work, consult})       (domain payload, no txn id)
  -> candidate answers StationReady, authentically              (or asks back, from inside the seal)
  -> owner offers the delivery it is holding; the BUS judges
  -> .commit(sequence)  ->  AdmissionPending, incumbent still serving
  -> one dispatch: activation first, then the role moves
  -> station v2 serves new orders; the dish crossed
```

---

## Hostile cases

| case | what refused, and why |
|---|---|
| forged `RouteChoice`, correct shape, correct correlation | `answers_ask()` — Loom's word, which no weave can manufacture |
| forged `Plated` for an unopened job | no such ticket |
| forged `Plated` naming the wrong station | the itinerary the expediter *can* check |
| forged `StationOpen` claiming a menu | believed (the roster is unauthenticated) — and harmless, because the station is the authority on its own menu and declines the `Prep` |
| a station named by nobody real | the send is refused where the sender cannot see it; the **watchdog** turns that silence into a word |
| forged `StationReady` with **no conversation open** | `InvalidReadiness`, deterministically — nothing else in that world could move the state |
| forged `StationReady` **during** a real conversation | `InvalidReadiness`; the candidate's own answer still lands afterwards |
| the fryer asked to become the grill | **authentic refusal** → `CandidateRefused`; the incumbent never learned it happened |
| a preparation carrying work the candidate cannot cook | authentic refusal, for a domain reason |
| an offer with no transaction in flight | counted, not crashed |
| a handle dropped mid-flight | nothing: no abort, no unload, no pump |

**One hostile case is still, deliberately, a pass:**

> **`STILL MEASURED, STILL NOT WAVED AT: a forged Plated finishes someone else's dish`**

A rogue holding nothing but an ordinary `allow_to_any` grant for `Plated` — a shape any weave may
legitimately hold — ends an order it has nothing to do with. **Night One's finding B reproduces
unchanged.** See *Core blockers* below.

---

## Replacement behaviour

The sharpest result of the replay. **There are now two ceremonies and they are disjoint.**

| | graceful swap (Weave Manager) | prepared replacement |
|---|---|---|
| talks to | the **outgoing** holder | the **incoming** holder |
| preserves work in flight | **yes** (the letter) | **no** — the incumbent is never told |
| verifies the successor | **no** | **yes** — it answers for itself, authentically |
| window with an unasked holder | yes | **none** — admission and activation are one event |
| the incumbent learns | `zen.PrepareShutdown` | **nothing at all** |

Neither gives both properties, and **nothing in either ceremony hints that the other exists**. An
author who reaches for prepared replacement because it is the newer, safer-sounding one silently
gets hard-swap semantics for work in flight. This kitchen only noticed because it already had a
watchdog that turns silence into a word — case *"the incumbent is NEVER TOLD, so its work is
gone"* pins exactly that.

**The composition, and it works.** The preparation window is the one interval in which the
incumbent is alive **and** the successor is reachable. So the owner asks the incumbent to
*describe* its work — an ordinary question that changes nothing, unlike `PrepareShutdown` — and
hands the description to the candidate inside the preparation ask. Case *"THE COMPOSITION"* proves
the dish crosses, exactly once, finished by a weave that never received the `Prep`.

Nothing in Loom did this. The substrate offers verification **or** continuity; the composition is
the application's, and it is invisible.

---

## Authoring friction

The full entries are in `../FRICTION.md`. The ones this project raised:

- **F1** — "activation first" means the candidate's first delivery **as part of the world**. The
  preparation conversation is delivered before it, and must be. I wrote the naive assertion first
  and it failed.
- **F2** — the handle is the host's (`Switchboard&`), the offer must happen inside the
  coordinator's delivery (a weave's). Every prepared replacement therefore grows a raw
  `PreparedReplacement*` across that boundary, re-pointed by hand.
- **F3** — the bus authenticates **that** the candidate answered and **who** said it, not **what it
  said**. The Ready/Refused verdict rides on the coordinator's honesty. Mutation 08 keeps it
  visible.
- **F4** — the two ceremonies are disjoint; only the application can compose them.
- **F5** — a weave still cannot see the fate of its own send. Unchanged. The entire watchdog exists
  for this.
- **F6** — reading a loaded weave's own state from the host needs a hand-built schema.
- **F7** — with a beat chain running, the beat is the queue's clock. Two test failures that looked
  like logic bugs were arithmetic.

---

## Completion evidence (the replay's own questions)

### Old ceremony versus replay

| | Night One | replay |
|---|---|---|
| `answering.hpp` — the `Mail::answer` workaround | **67 lines** + 7 call sites | **deleted**, 0 call sites |
| `expediter.cpp` | 704 | 685 |
| `policy.cpp` | 244 | 252 *(+ the answer ticket is now checked)* |
| `station.cpp` | 315 | 467 *(+152: all of it the new preparation conversation)* |
| `vocabulary.hpp` | 360 | 420 *(+60: all of it preparation vocabulary)* |
| suite | 27 cases / 78 assertions | **39 / 196** |

The substrate removed a 67-line workaround and its explanation. Everything that grew is new
capability the application chose to use.

### Raw prepared-replacement calls remaining in application code

**Zero.** Measured, not asserted: no `begin_prepared_replacement`, `ask_candidate_to_prepare`,
`accept_preparation_answer`, `commit_prepared_replacement`, `abort_prepared_replacement`,
`host_lifecycle_authority`, `load_candidate`, `seal_weave` or `admit_candidate` appears anywhere in
this project. Facade operations used: `start` ×5, `ask` ×9, `offer_current_answer` ×2 (one honest,
one for the forgery), `commit` ×6, `abort` ×1, `state` ×15, `take_outcome` ×8, plus `candidate`,
`incumbent`, `id`, `started` for assertions.

### Old seams CLOSED

1. **`Mail::answer()` is native-only and fails silently across the `.so` seam.** **CLOSED**, and
   measured three ways by `repro_answer_seam.cpp`: the door exists (ABI v4), Night One's workaround
   still works, **and a second answer to the same request is refused with the weave told about
   it**. The third probe is the one that matters — the original complaint was never "answer does
   nothing", it was that a weave could not tell.
2. **`ZenHostApi` has no `answer`.** Closed by the same change.

### Old seams STILL PRESENT

3. **Loom attests answers and lifecycle; it does not attest role-holding.** Reproduces unchanged.
   A rogue with an ordinary grant finishes someone else's dish. Second sighting was Timer R2B-0;
   this is now the **third**.
4. **A weave cannot see the fate of its own send.** Unchanged.
5. **A message can reach an heir before its own `zen.Activated`.** **Still true for the graceful
   ceremony** — mutation 04 (remove the handover window) is RED, which means the race is live and
   the window is load-bearing. It is *structurally impossible* under prepared replacement, where
   admission and activation are one event.
6. **`timer::TimedWeave` and the activation moment are mutually exclusive.** Untested here — all
   four weaves still write the raw Timer protocol for the same reason. Project 6 tests it directly.
7. **The Emit-derived grant is the wrong shape for test-only nudges.** Reproduced in project 2 (F8).

### New seams discovered

8. **The two replacement ceremonies are disjoint** (F4) — architecture pressure.
9. **`PreparationAnswer` is the coordinator's word, not the candidate's** (F3) — the substrate
   proves an authentic answer arrived; the verdict's *meaning* is application-side.
10. **"Activation first" is ambiguous in prose** (F1) — paper cut, but it produced a wrong test.

---

## Repeated patterns

Named, **not** extracted (one project is one sighting):

- **the promise book** — a bounded book of open promises, each with an addressee, a correlation and
  a patience; a role-addressed sweep; the invariant that *every* entry leaves the book through a
  message. Night One named it. It is here again because it is the same application.
- **request → menu → resolved choice → receipt** — refusal as an outcome and never a menu choice,
  unknown spellings refused rather than guessed.
- **describe-then-hand-over** — the preparation-window composition (F4).
- **a `WeaveId` stringified as decimal Text**, because the wire's `Int` is signed.

---

## Verdict

```
GREEN
```

## What this project votes for

| candidate | vote | evidence |
|---|---|---|
| role-authored provenance | **✓** | the forged-`Plated` case passes as a defect, third independent sighting overall |
| promise/responsibility book | ✓ | the whole expediter, unchanged from Night One |
| order/menu resolution | ✓ | routing, unchanged from Night One |
| activation hold/replay | ✓ | mutation 04 RED — the race is live under the graceful ceremony |
| activation-sequence owner | ~ | the caller supplies the number; nothing here wanted an owner, one operator, one sequence |
| outcome observation ergonomics | × | `state()` / `take_outcome()` were pleasant; nothing was awkward |
| **describe-then-hand-over** *(new)* | ✓ | F4 — the ceremonies are disjoint and the application bridged them |
