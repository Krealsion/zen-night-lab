# Project 6 — Maintenance scheduler

**Verdict: GREEN.** 18 cases / 113 assertions. Mutation results in FINAL-REPORT.md.

**Pinned against:** Loom `78d64ea`, Zengine `f6a4c69`, ABI v4.

---

## Purpose

Not "does Timer work" — that is the Timer package's own suite's job.

> **What does a moderately ordinary application feel like when the Timer binding layer and prepared
> replacement have to coexist? Did two individually pleasant APIs stay pleasant together?**

So this project uses both sugars at full strength and nothing else: a `timer::TimedWeave` with three
authored bindings and an `on_timed_activation` hook, and `loom::PreparedReplacement` used **twice** —
on this package's own worker, and on **the Timer service itself**, through the Timer package's own
`PrepareTimerHandover` / `TimerCandidatePrepared` vocabulary.

---

## Night One's friction 4 is CLOSED, and it is measured

Night One found that `timer::TimedWeave` and the activation moment were **mutually exclusive**: the
binding owned `on(zen.Activated)`, a derived handler suppressed it **silently** through C++ name
hiding, and three of four weaves in that kitchen wanted the moment and none could use the binding.

Now:

- `on_timed_activation()` exists, runs after the bindings reconciled and only for an activation the
  cursor accepted, and this scheduler does **real domain work** in it — it announces itself and
  runs the fleet's first checks, because a fresh scheduler knows nothing about the fleet's state and
  should not wait out a full period to find out.
- A derived raw activation handler is now a **compile-time refusal that names the alternative**.
- A missing `using TimedWeave::on;` is a hard compile error with a sentence in it.

Case *"the yard opens"* asserts the hook ran, did its work, and reported **three** reconciled
bindings — so the ordering the layer promises is pinned rather than assumed.

---

## Did the two sugars compose? **Yes — and in one direction, for free.**

> **Replacing the Timer service underneath the bindings required no application code at all.**

The new clock publishes `TimerReady`; the binding layer reconciles; every authored binding is
re-established. Case *"THE COMPOSITION"* replaces the clock mid-schedule and the domain rhythm
simply keeps running. That is the nicest thing in this project and nobody had to write it.

And the handle did not care whose service it was replacing. The **same** five calls — `start`,
`ask`, `offer_current_answer`, `commit`, `take_outcome` — drove both this package's worker and a
third-party Timer. The only application-visible difference is which domain shape goes into `ask` and
which handler offers the answer, and for the Timer both of those are **the Timer package's**, not
this application's. One coordinator class holds both, side by side, and the diff between the two
paths is four lines.

---

## The friction found instead: the binding table is authored, not dynamic

A scheduler's whole job is to run things on a rhythm the **operator** chooses. None of those rhythms
can be a binding:

- `reconcile` belongs to the binding layer — correctly; an author reconciling would be a second
  scheduler;
- the layer reconciles on an accepted activation or a `TimerReady`;
- so a binding declared after construction sits `Waiting` and the Timer service never hears of it.

So the scheduler declares **one** authored beat and counts it — which is exactly what the kitchen's
expediter did with the raw protocol, two projects and one sugar layer ago.

**This is a boundary, not a defect.** It is worth knowing because the binding layer *reads* like the
general answer to "I want something to happen periodically" and is in fact the answer to "**this
weave** has a fixed rhythm".

Case *"THE BINDING TABLE IS AUTHORED, NOT DYNAMIC"* measures it (a late binding never fires), and
its companion measures the consequence nobody would guess:

> **A binding declared at run time starts firing the moment the Timer service is replaced** — because
> that is when the layer next reconciles.

---

## Positive vertical

`ScheduleCheck` → the authored beat counts it down → `RunCheck` to the worker → an authenticated
`CheckResult` → a `HealthReport` to the party that asked. One repeating schedule, one one-shot, one
authored one-shot that fires and becomes `Spent`, one authored one-shot that is **cancelled**, and
a role-addressed authored beat a successor would inherit.

---

## A defect the suite found in this project's own design

**A one-shot left the book the moment it was ASKED.** The worker's answer then matched no schedule,
and the party that asked for the action was **never told what happened to it** — the exact
silent-failure shape four earlier projects were built to refuse, reintroduced by accident in the
sixth.

A schedule now leaves the book when its **result** lands, not when its question goes out, and a
`pending` flag stops the rhythm asking twice. Mutation 05 keeps it that way.

Worth naming as a general lesson: **"remove it when you have asked" is only correct for work whose
answer you do not owe anybody.**

---

## Hostile cases

| case | what happened |
|---|---|
| a period of zero | refused |
| the 13th schedule | refused against the published bound |
| cancelling a schedule nobody has | `zen.Refused` |
| a machine no worker services | reported **UNHEALTHY with a reason**, never dropped |
| a candidate worker that services only part of the fleet | **authentic refusal** → `CandidateRefused` |
| **a candidate CLOCK that declines** (`zengine-timer-declines`) | **authentic refusal from somebody else's service**, and the rhythm never stuttered |
| aborting either replacement | the incumbent is untouched, on both |
| `NoRoleHolder` / `CandidateLoad` / `IncumbentBusy` | inspected exactly |
| an offer with no transaction in flight | counted, not crashed |

---

## Sugar audit

| | count |
|---|---|
| facade operations used | `start` 9, `ask` 8, `offer_current_answer` 2, `commit` 3, `abort` 2, `state` 9, `take_outcome` 8 |
| raw prepared-replacement operations in app code | **0** |
| **manual transaction ids in domain payloads** | **2 — explained below** |
| manual lifecycle authority wiring | **0** |
| manual candidate cleanup | **0** |
| manual outcome filtering | **0** |

**The nonzero, classified.** `timer::PrepareTimerHandover` carries a `transaction` field. Its own
header says plainly that it is **not authority** and exists so an operator reading the wire can pair
the two halves; the transaction is keyed from the coordinator's own record. Driving it through the
handle therefore means reaching for `upgrade().id()`, which the handle documents as *"diagnostics,
logging, tests and unusual integration"*.

Classification: **third-party vocabulary that predates the handle** — not missing sugar, not an
incorrect boundary, and not test-only. It is the one place in six projects where an application
touched a transaction id, and the reason is that somebody else's package asked for it.

---

## Verdict

```
GREEN
```

## What this project votes for

| candidate | vote | evidence |
|---|---|---|
| activation-sequence owner | **×** | tested and did not appear even here, with two replacements of two different services in one program: one operator, one counter, nothing contended |
| outcome observation ergonomics | **×** | fifth project, still pleasant. Delete it from the roadmap |
| promise/responsibility book | ~ | a bounded book with visible refusal — but every step is somebody's answer, so nothing can go silent and no watchdog was needed |
| describe-then-hand-over | **✓** | sixth sighting |
| role authorship | ~ | not exercised: this scheduler's counterparty is a role, but nothing hostile speaks the protocol here |
| **the binding table is authored, not dynamic** *(new)* | **✓** | first sighting, and the reason a scheduler falls back to counting beats |
| **"remove it when asked" is a silent-failure shape** *(new)* | **✓** | the one-shot defect |
