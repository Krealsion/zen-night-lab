# Project 2 — Download manager

**Verdict: GREEN.**
30 cases / 153 assertions · mutations **14 RED, 0 GREEN** after a harness repair (see *Process*).

**Pinned against:** Loom `78d64ea`, Zengine `f6a4c69`, ABI v4.

---

## Purpose

The kitchen found, and said in prose, that *Loom authenticates the acknowledgment of a job and
never its fulfilment.* The kitchen's jobs were short and its progress was private, so it never had
to pay for that. This project exists to ask the same question of an operation that is long, whose
progress is the whole reason anyone is watching, and to answer it **with a number**:

> **Is the original answer capability the right thing to hold for the entire operation?**

---

## Domain model

One role, `download.service`, holding many transfers at once. Deliberately a *single* service tier
— the kitchen had three roles and the build farm has two, and this one has one, so "replacing the
service" and "replacing the worker" are the same act.

A client sends `StartDownload{ticket, source, destination}` and eventually observes
`DownloadAccepted` / `DownloadRefused`, several `DownloadProgress`, and exactly one of
`DownloadCompleted` / `DownloadFailed`. Sources are a deterministic in-memory catalogue; one of
them (`truncated.iso`) goes bad exactly halfway, so "how much was discarded" is a real number.

**Two services from one source**, differing in one decision and nothing else:

| | `download-service` | `download-service-holds` |
|---|---|---|
| spends its one answer on | the **acknowledgment** | the **ending** |
| everything else is | ordinary directed messages | ordinary directed messages |

---

## Zen surface used

| surface | how it appeared |
|---|---|
| authenticated **immediate** answer | `DownloadAccepted` (default build), every refusal in both builds, the diagnostic, the whole preparation conversation |
| authenticated **deferred** answer | the terminal message in the holds build; the candidate's readiness under `verify_sources` |
| ordinary direct send | progress, and whichever half did not get the answer |
| role-addressed delivery | every client→service message |
| publication | **never used.** Nothing in this domain is an announcement to nobody in particular; the service has exactly one client relationship at a time and speaks to it directly |
| `zen.Ack` / `zen.Refused` | the answer to a cancellation, which is a different conversation from the transfer it names |
| `answers_ask` | the operations desk's wall on `ObligationsDescribed`; the client's *measurement* rather than its wall |
| `loom::PreparedReplacement` | the whole of section 5 |
| Zengine Timer `StartRoleTimer` | one role-addressed beat moves every transfer |
| Weave Manager swap | **never used.** This project replaces its service only through the prepared ceremony |

---

## Positive vertical

A transfer is accepted, reports monotone progress across ten beats, and completes with a digest the
client checks against the source it actually asked for. Twelve run at once. Two clients name a
transfer the same way and do not collide.

---

## Hostile cases

| case | what happened |
|---|---|
| an unknown source | refused, with the source named |
| a duplicate ticket from one client | refused |
| no destination | refused — the service will not guess one |
| a source that goes bad partway | failed at byte 448, naming the bytes discarded |
| the 81st concurrent transfer | refused visibly against the published bound |
| withdrawing a transfer nobody has | `zen.Refused` with a stranger-readable reason |
| a forged `ServiceReady` | `InvalidReadiness`; the transaction stays `Preparing` |
| a preparation carrying debt for an unservable source | **authentic refusal** → `CandidateRefused` |
| a preparation carrying more debt than the bound | authentic refusal |
| a preparation whose catalogue disagrees with the operator's | authentic refusal, from inside the seal, after the candidate asked |
| an offer with no transaction in flight | counted, not crashed |
| a forged ending for a correlation nobody holds | ignored |

**And one that lands, deliberately:**

> **`MEASURED, NOT WAVED AT: a forged ending closes somebody else's operation`**

In the acknowledge-at-once build the terminal message carries no attestation, so a rogue with an
ordinary grant and a guessed correlation ends a live transfer. **The digest check is what catches
it — and a digest is domain cleverness, not a substrate fact.** Third independent sighting of the
role-provenance gap.

---

## THE MEASUREMENT — the answer to the project's question

### 1. You get one, and the choice is which half to leave undefended

Case **`YOU GET ONE`** asserts, for both builds, that `accepts_attested + terminals_attested == 1`,
and that the two builds attested *different* halves.

| | `download-service` | `download-service-holds` |
|---|---|---|
| acknowledgment attested | **yes** | no |
| ending attested | no | **yes** |
| progress attested | **never** — asserted, in both | **never** |

Progress can never be attested by anything: one authenticated answer per request, and progress is
many messages.

### 2. Holding the answer spends a resource that belongs to the whole Loom

`Switchboard::kMaxDeferredAnswers` is **64**, and the header says plainly it is *"how many
unfinished conversations one Loom will hold at once"*. So the suite:

1. asks an unrelated **bystander** weave to defer an answer — it can;
2. starts **seventy** concurrent transfers through the holds build (the service's own bound is 80,
   set deliberately larger so it is not the limit that binds);
3. observes transfers refused with *"this Loom has no unfinished-conversation slots left; the
   limit reached was the substrate's, not this service's"*;
4. asks the bystander again — **it cannot defer.**

And the control: the same seventy transfers through the acknowledge-at-once build leave the
bystander untouched.

**Answer to the project's question: no.** Not for a taste reason. Holding the answer capability for
the duration of an operation converts your concurrency limit into the substrate's, on behalf of
every other weave in the process. Nothing at the call site says so — `defer_answer()` reads like a
local decision.

### 3. …and the attested ending is the one thing that cannot survive a replacement

Case **`HOLDS-THE-ANSWER ACROSS A REPLACEMENT`**: the client that was promised an attested ending
gets an unattested one, because the answer right belonged to a life that has ended and there is no
representation of it a successor could be handed. The build that attests the *promise* has nothing
to lose here; the build that attests the *ending* loses exactly the thing it chose.

---

## Replacement behaviour

**The contract, chosen and not defaulted to:**

> A transfer that has not reached a terminal message when the service is replaced is **FAILED**,
> explicitly, by the successor, naming how many bytes were discarded.

It would have been easy to write *"the successor continues the download"* and nothing would have
caught the lie: the successor would have re-fetched from zero while reporting inherited progress.
So **the bytes do not cross and are not pretended to**. What crosses is the **obligation** — who is
owed a terminal message, about what, and how far it had got.

Mechanically this is the kitchen's *describe-then-hand-over* composition (F4) reached
independently and used for the opposite purpose:

| | kitchen | download manager |
|---|---|---|
| the preparation window carries | the **work** | the **obligation** |
| because | progress was three integers that *described* work | progress **is** the bytes |

The successor discharges its inherited debts **at activation**, not during preparation — a sealed
candidate cannot speak to anybody but its coordinator, and a candidate that told clients their
downloads had failed and then was never admitted would have lied on behalf of a service still
running perfectly.

**Cancellation was implemented, and the model demanded it.** Without it, a client that has given up
is still owed a terminal message, and the only way for it to stop being owed one is for the service
to invent a deadline it has no basis for. The client withdrawing is the honest alternative — a
message it sends, not a state the service guesses.

---

## Sugar audit

| | count |
|---|---|
| `PreparedReplacement` facade operations used | `start` 6, `ask` 8, `offer_current_answer` 2, `commit` 3, `abort` 1, `state` 12, `take_outcome` 8 |
| **raw prepared-replacement operations in app code** | **0** |
| manual transaction ids in domain payloads | **0** |
| manual lifecycle authority wiring | **0** |
| manual candidate cleanup | **0** |
| manual outcome filtering | **0** |

---

## Authoring friction

Full entries in `../FRICTION.md`.

- **F2 (2nd sighting)** — the coordinator/handle raw pointer. Written independently; identical
  shape.
- **F4 (2nd sighting)** — the two ceremonies are disjoint; the same composition, opposite purpose.
- **F8 (2nd sighting)** — the `Emit`-derived grant is the wrong shape for a test-only nudge, and
  the failure is invisible. It cost a debugging round here exactly as it did on Night One: the case
  read as *"the bystander was never denied"* when the truth was *"the bystander never ran"*.
- **F9 (2nd sighting)** — a `WeaveId` does not fit on the wire, so both projects stringify it.
- **F10 (new)** — holding an answer spends a resource that belongs to the whole Loom.
- **F11 (new, and the strongest)** — you get one attestation per operation.

---

## Process — a harness failure mode this run discovered

**A mutation whose pattern matches nothing reports GREEN.** `perl -0pe` exits 0 and writes a
byte-identical file; the suite then passes for the most boring reason there is, and the line is
indistinguishable from *"the term is unwatched"*. Night One's version of this bug was perl failing
to **write**; this one is perl writing the **same thing**. Both wear the same costume.

Found because mutation 07 (*a cancelled transfer vanishes without the ending the client was owed*)
came back GREEN when the suite plainly asserts that ending. Hand-checking showed the pattern never
matched.

Repaired in both harnesses: `mutate()` now `cmp`s its output against the original and reports
**`NOT-APPLIED`**, which `run_one` refuses to treat as a result. Both harnesses also gained an
id filter so a repaired mutation can be re-run without redoing the matrix.

**What had to be re-run, and what did not.** A mutation that fails to apply leaves a byte-identical
tree, which can only ever produce the *baseline* result — so every **RED** verdict in both matrices
is unaffected and stands. Only non-RED lines were re-run: kitchen 14 and 15 (**confirmed genuine
GREENs**, they do apply), and downloads 05 (had been `BUILD-FAILED` on `-Werror` for an orphaned
loop variable, mutation rewritten) and 07 (rewritten with a single-line anchor).

---

## Repeated patterns

| pattern | kitchen | download | same? |
|---|---|---|---|
| acknowledgement | `OrderReceipt` | `DownloadAccepted` | ✓ both, and both had to choose which half to attest |
| responsibility identity | two namings (`order_id`/`job`) | one (`ticket`) — no inner boundary exists | ~ |
| progress | private (`passes_left`) | public, and the point | ~ materially different |
| terminal result | `Served`/`OrderLost` | `DownloadCompleted`/`DownloadFailed` | ✓ |
| cancellation | absent, deliberately | **present, and the model demanded it** | ~ |
| replacement continuity | work crosses | obligation crosses, work dies | ~ **opposite contracts, same mechanism** |
| bounded book + visible refusal | ✓ | ✓ | ✓ |
| stringified `WeaveId` | ✓ | ✓ | ✓ |
| "every entry leaves the book through a message" | ✓ | ✓ | ✓ |

---

## Verdict

```
GREEN
```

## What this project votes for

| candidate | vote | evidence |
|---|---|---|
| promise/responsibility book | **✓** | second independent consumer; the invariant *every entry leaves through a message* was re-derived, not copied |
| role authorship (provenance) | **✓** | the forged-ending case lands; third sighting |
| order/menu resolution | **×** | tested and did not appear. There is no menu here: a source either exists or it does not, and the client's choice is complete at submit time |
| activation hold/replay | **×** | no bootstrap race arose; the successor's only startup act is discharging debts, and it does that *at* activation |
| activation-sequence owner | ~ | one operator, one sequence, no contention |
| outcome observation ergonomics | **×** | `state()`/`take_outcome()` were pleasant again |
| describe-then-hand-over | **✓** | second sighting, opposite purpose |
| **which-half-to-attest** *(new)* | **✓** | F11; a decision every long operation must make and nothing helps with |
