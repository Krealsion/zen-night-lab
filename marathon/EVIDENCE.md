# EVIDENCE — the architecture voting table

Six applications voted. A candidate abstraction is only as real as the number of **independent**
sightings it has; two copies of the same fixture are one sighting.

```
✓   genuine occurrence        ~   adjacent but materially different
×   tested and did not appear B   blocked by a missing substrate fact
```

Evidence scale: 0 = speculation · 1 = name the shape · 2 independent = candidate · 3+ materially
different = serious extraction candidate · repeated workaround against the same wall = core-design
candidate.

| Candidate | Kitchen | Download | Build | Import | Lobby | Scheduler | Indep. | Verdict |
|---|---|---|---|---|---|---|---:|---|
| **Role authorship (provenance)** | ✓ | ✓ | ✓✓ | ✓* | ✓✓✓ | ~ | **5** | **CORE-DESIGN CANDIDATE** |
| Promise / responsibility book | ✓ | ✓ | ✓ | ~ | × | ~ | **3** | EARNED (as a *named shape*, not a helper) |
| Describe-then-hand-over | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | **6** | EARNED — and it is a **substrate gap**, not a helper |
| Which-half-to-attest | ✓ | ✓ | ~ | ~ | ✓ | ~ | **3** | EARNED (a decision, not code) |
| Order / menu resolution | ✓ | × | × | ✓✓ | × | × | **2** | PROMISING (+Timer = 3, but see the note) |
| Minted-identity namespaces | ✓ | × | ✓ | ✓ | × | × | **3** | PROMISING |
| Stringified `WeaveId` on the wire | ✓ | ✓ | ✓ | ~ | ✓ | ~ | **4** | PROMISING |
| Activation hold / replay | ✓ | × | × | × | × | × | **1** | NAME ONLY |
| Activation-sequence owner | ~ | ~ | ~ | ~ | ~ | × | **0** | **REJECTED** |
| Outcome observation ergonomics | × | × | × | × | × | × | **0** | **REJECTED** |
| Binding table should be dynamic | — | — | — | — | — | ✓ | **1** | NAME ONLY |
| Attestation for observers | — | — | — | — | ✓ | — | **1** | NAME ONLY (but see role authorship) |

\* import-pipeline is a ✓ **inverted**: it is the one project where the check IS performable, and
the reason is exact — its counterparty is a specific weave rather than an office. That is evidence
*about the shape of the gap*, and it is why the gap can be stated so precisely.

### Notes on the non-obvious cells

- **Order/menu, Download/Build/Lobby/Scheduler = ×.** All four are *tested and absent*, not merely
  unused. A download source either exists or it does not; a build target either has a recipe or it
  does not; a lobby has no menu; a schedule has no menu. The shape needs a service that must
  **offer** something it discovered, and only two of six domains had one.
- **Sequence owner = 0.** Five projects marked `~` and one `×`. Every one has a single operator and
  a single counter; nothing ever contended for the number, *including* the scheduler, which
  performs two replacements of two different services in one program.
- **Outcome ergonomics = 0 across six.** `state()` / `take_outcome()` were pleasant every time and
  nobody wrote a line of glue around them.
- **Promise book = 3, but the three disagree** about continuity policy, progress shape and identity
  naming — see the final report's ranking.

---

## Sugar audit — per project

| Project | semantic facade ops | accessors | **raw ops in app code** | txn ids in payloads | manual lifecycle wiring | manual candidate cleanup | manual outcome filtering |
|---|---:|---:|---:|---:|---:|---:|---:|
| kitchen-replay | 46 | 12 | **0** | 0 | 0 | 0 | 0 |
| download-manager | 30 | 2 | **0** | 0 | 0 | 0 | 0 |
| build-farm | 30 | 1 | **0** | 0 | 0 | 0 | 0 |
| import-pipeline | 28 | 1 | **0** | 0 | 0 | 0 | 0 |
| lobby | 16 | 2 | **0** | 0 | 0 | 0 | 0 |
| scheduler | 28 | 6 | **0** | **2** ‡ | 0 | 0 | 0 |
| **total** | **178** | **24** | **0** | **2** | **0** | **0** | **0** |

*Semantic operations are `start` / `start_existing` / `ask` / `offer_current_answer` / `commit` /
`abort` / `tick` / `state` / `take_outcome`, counted by grep. Accessors (`id`, `candidate`,
`incumbent`, `started`, `role`, `candidate_name`) are counted separately because the handle
documents them as diagnostics rather than operations, and almost all of them are in test
assertions.*

‡ `timer::PrepareTimerHandover` carries a `transaction` field for wire legibility — its own header
says it is *not* authority. Classified as **third-party vocabulary that predates the handle**; see
FRICTION.md F15. It is the only place in six projects an application touched a transaction id.

---

## Cross-project replacement coverage

| arm | covered by |
|---|---|
| immediate candidate readiness | kitchen, download, build, import, lobby, scheduler |
| deferred candidate readiness | kitchen (`AskHousePassRate`), download (`AskCatalogueSize`), build (`AskToolchain`), import (`AskCatalogueName`) |
| authentic refusal | all six — **14 distinct domain reasons**: kitchen (wrong station; work it cannot cook), download (catalogue size; unservable debt; over-bound debt), farm (wrong worker slot; wrong toolchain; attempts exhausted), import (wrong catalogue; unreadable file; over-bound adoption), lobby (house match size), scheduler (fleet not serviced; **a third-party clock declining**) |
| candidate failure | all six (`CandidateLoad`, loader's own words preserved) |
| `AdmissionPending` observed | kitchen, download, build, import, scheduler |
| committed outcome | all six |
| aborted outcome | kitchen, download, build, import, lobby, scheduler |
| exact error inspection | `NoRoleHolder`, `CandidateLoad`, `BeginTransaction`/`IncumbentBusy`, `AlreadyStarted`, `InvalidReadiness`, `CandidateRefused`, `ExplicitAbort` |
| **a third-party service replaced** | scheduler — the Zengine **Timer**, through the Timer package's own preparation vocabulary |

---

## Cross-project answer coverage

| surface | used by | felt natural? |
|---|---|---|
| ordinary send | all six | yes — the default for anything that must survive replacement |
| publication | kitchen, farm, lobby, scheduler | yes, and **the lobby found its cost**: a publication can never be attested to anybody |
| **ordinary `reply`** | **nobody, in any of six projects** | never wanted. Every reply is either an *answer* (must be provable) or a *role-addressed send* (must survive replacement); between those two, `reply` had no job |
| authenticated immediate answer | all six | the workhorse, now that it crosses the `.so` seam |
| authenticated deferred answer | kitchen, download, build, import, lobby | yes for a *bounded* wait; **F10/F11** for a long one |
| answer provenance (`answers_ask`) | all six | yes — and its absence is the loudest thing in five of six reports |
| role-addressed delivery | all six | yes — the reason replacement is invisible to callers |
| direct delivery | all six | yes, but see **F9** |

**The one surface nobody wanted, across six applications, is `reply`.** That is the brief's "if one
surface never feels appropriate, that is interesting too" — and the reason is structural rather than
stylistic: in a world where services are replaceable, *"whoever sent this"* is either too weak (you
need proof) or too strong (you need the office, not the incarnation).
