# Project 3 — Build farm

**Verdict: GREEN.** 28 cases / 147 assertions. Mutation results below.

**Pinned against:** Loom `78d64ea`, Zengine `f6a4c69`, ABI v4.

---

## Purpose

The download manager already showed that *request → acknowledged responsibility → progress →
terminal* is expressible. Repeating it would have proved nothing. So this project exists to ask
the **comparison** question:

> Two independent implementations of the same conversation shape. Did they grow the same non-domain
> bookkeeping — or did the domain change the answer?

It is built to be structurally different in four ways a domain actually demands, and the fourth is
the sharp one.

---

## Domain model

Two tiers, not one: a **dispatcher** that owns the promise and the queue, and **workers** with
capacity 1 that own the work. A build moves through five named stages. Targets are a deterministic
catalogue; two of them die in a named stage.

| | download manager | build farm |
|---|---|---|
| tiers | one service role | dispatcher + N worker roles |
| concurrency | everything at once | **a queue**; workers hold one build |
| progress | a byte count (continuous) | a named stage (discrete, closed vocabulary) |
| replacement continuity | the obligation crosses, the work dies | **the intent crosses; the work is redone** |

---

## THE SHARP ONE — whether continuity is possible is a property of the DOMAIN

The download manager could not carry work across a replacement, because a half-downloaded file
**is** its bytes. A build is different in a way that has nothing to do with Loom:

> **A build is re-derivable from its intent.** Project, revision, target — three strings — and
> running it again produces the same artifact.

So this farm makes the opposite choice and states its cost on the wire: an interrupted build is
**RESUMED from the beginning as a new ATTEMPT**, and `BuildProgress::attempt` exists so a requester
watching `compile` turn back into `fetch` can tell a restart from a regression.

Three projects, three different answers to the same substrate question:

| | what crosses | the conversation |
|---|---|---|
| kitchen | the **work** (`passes_left` — progress that *describes* work) | continues |
| download manager | the **obligation** (who is owed a terminal message) | **ends**, in an honest failure |
| build farm | the **intent** (three strings) | **restarts**, as a numbered attempt |

None of those is more correct. The substrate offered the same ceremony to all three; the domain
decided.

---

## TWO ANSWERS TO ABSENCE, and they cover different cases

The kitchen had one — a watchdog — because a station that vanished produced *nothing at all*. This
farm has two, and knowing which covers which is the second reason it is not the kitchen again.

| | **reconciliation** | **the sweep** |
|---|---|---|
| the evidence | a worker ARRIVES and lists what it actually holds | a worker is SILENT for N sweeps |
| covers | replacement (which always produces an announcement) | disappearance (which produces nothing) |
| latency | immediate | `kAssignmentPatienceSweeps` |
| trusts | an **unauthenticated publication** | only its own clock |

Neither subsumes the other. Both are pinned by cases, and mutations 07 and 08 disable one each.

---

## Positive vertical

Submit → accepted (with the truth about the queue: `queued behind N`) → five stages of progress →
succeeded, with the artifact named. Two workers run two builds in parallel; one worker makes the
second build wait and then run.

And the replacement vertical: a worker is replaced mid-build through the handle, the build resumes
as attempt 2 on the successor and completes, **while the other worker's build runs to completion
throughout and never notices** — which is what "the farm remains available" has to mean.

---

## Hostile cases

| case | what happened |
|---|---|
| a target that dies in `link` | failed, with the stage named, and **not retried** |
| a target no worker has a recipe for | **declined** — a judgement, so failed once rather than retried three times |
| a duplicate build id | rejected |
| a build missing project/revision/target | rejected, counting which of the three it named |
| the 17th open build | rejected against the published bound |
| withdrawing a queued build | acknowledged; the requester still gets its one terminal message |
| withdrawing a running build | the worker is told to stop and is genuinely freed |
| withdrawing a build nobody has | `zen.Refused` |
| a forged `WorkerReady` | `InvalidReadiness`; the transaction stays `Preparing` |
| a candidate asked to be a worker it is not | authentic refusal → `CandidateRefused` |
| a candidate built for another toolchain | authentic refusal, after asking from inside the seal |
| a candidate asked to resume a build at max attempts | authentic refusal |
| a `JobDone` naming a worker the job never went to | ignored — the itinerary is the half we *can* check |

**Two that land, deliberately:**

> **`MEASURED, NOT WAVED AT: a forged JobDone ends somebody else's build`** — fourth independent
> sighting of the role-provenance gap.

> **`THE NEW ATTACK SURFACE: a forged arrival DESTROYS a healthy build`** — and this one is new.
> Reconciliation is faster than a watchdog *and trusts more*. A `WorkerOpen` is an unauthenticated
> publication; the kitchen's roster had the same weakness and the worst a forgery could do there
> was **invent** a station. Here **an announcement is evidence that work was lost**, so a forged
> one is a claim that somebody else's build is dead. Measured as it actually behaves, and it is
> worse than a restart: the dispatcher requeues, re-offers the build to the worker that never lost
> it, that worker declines as busy, and a decline is a judgement — so the build is **failed
> outright** with a reason its requester cannot make sense of.
>
> One unauthenticated publication, one destroyed build. The mechanism is the application's; the
> fact that an announcement cannot be attributed is the substrate's.

---

## Two defects the suite found in this project's own design

Both worth recording because they are the kind of thing a domain grows on its own.

1. **A requeued build was re-offered to the worker it came back from.** Fixed with a two-pass
   dispatch: the first pass avoids `last_worker`, the second places whatever is left anywhere free,
   so "prefer somebody else" never becomes "wait forever" on a one-worker farm.

2. **The attempt number had two owners.** The dispatcher mints attempt+1 when it requeues; a
   candidate *worker* mints attempt+1 when it resumes through a preparation, and the dispatcher is
   never told. The terminal message therefore said *"after 1 attempt"* while every progress line
   said *attempt 2*. Fixed by making the party that **starts** an attempt the one that owns the
   number, and carrying it on every worker→dispatcher message. This is a genuinely general lesson
   for prepared replacement: **when a candidate can restart work during preparation, any counter
   about that work has a second author nobody wired up.**

---

## Replacement behaviour

Same shape as the other two projects: describe the live incumbent, hand the description to the
candidate inside the preparation ask, offer the answer, commit. Activation-first asserted from a
tap with a mark taken at commit.

What is described is **intent only** — `AssignedJob` has no stage and no step, deliberately. A
successor handed a stage cursor would be claiming to have compiled something it has never seen.

---

## Sugar audit

| | count |
|---|---|
| `PreparedReplacement` facade operations used | `start` 8, `ask` 7, `offer_current_answer` 2, `commit` 3, `abort` 1, `state` 11, `take_outcome` 7 |
| **raw prepared-replacement operations in app code** | **0** |
| manual transaction ids in domain payloads | **0** |
| manual lifecycle authority wiring | **0** |
| manual candidate cleanup | **0** |
| manual outcome filtering | **0** |

---

## Comparison checkpoint (the brief's question)

Did the download manager and the build farm independently create the same bookkeeping?

| | download | farm | same? |
|---|---|---|---|
| acknowledgement | ✓ `DownloadAccepted` | ✓ `BuildAccepted` (+ `queued_behind`) | **✓ same** |
| responsibility identity | one naming (`ticket`) | **two** (`id` / `job`), because there is an inner boundary | ~ |
| progress | continuous, public | discrete, public, **and attempt-numbered** | ~ |
| terminal result | ✓ | ✓ | **✓ same** |
| cancellation | ✓ demanded by the model | ✓ demanded by the model | **✓ same** |
| replacement continuity | obligation crosses | intent crosses | **× opposite** |
| bounded book + visible refusal | ✓ | ✓ | **✓ same** |
| "every entry leaves through a message" | ✓ | ✓ | **✓ same** |
| stringified `WeaveId` | ✓ | ✓ | **✓ same** |
| absence detection | none needed (one tier) | **two mechanisms** | × |

**Five identical, and they are all the promise-book's.** The parts that differ are all *domain*
decisions. That is the strongest thing this pair of projects says: the reusable core is the
bookkeeping around a promise, and everything anybody might have wanted to put *inside* it —
continuity policy, progress shape, identity naming — is exactly what the two domains disagreed
about.

**Extraction was NOT attempted.** The brief permits a Night-Lab-local helper at this point. The
evidence says the candidate is real (see EVIDENCE.md) but the two implementations' *differences*
are load-bearing, and a helper that abstracted them would be a fifth opinion nobody asked for. The
final report ranks it rather than building it.

---

## Verdict

```
GREEN
```

## What this project votes for

| candidate | vote | evidence |
|---|---|---|
| promise/responsibility book | **✓** | third independent consumer; five identical bookkeeping elements |
| role authorship (provenance) | **✓✓** | fourth sighting, and the first where a forgery **destroys** rather than merely misreports |
| order/menu resolution | **×** | no menu: a target either has a recipe or it does not |
| activation hold/replay | **×** | no bootstrap race; the successor's startup act is an announcement, which is safe to make late |
| activation-sequence owner | ~ | one operator, one sequence |
| outcome observation ergonomics | **×** | pleasant a third time |
| describe-then-hand-over | **✓** | third sighting, third purpose (intent, not work, not obligation) |
| **counter-with-two-authors** *(new)* | **✓** | the attempt-number defect: a candidate that restarts work during preparation becomes a second author of any counter about it |
