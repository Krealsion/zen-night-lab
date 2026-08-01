# EVIDENCE — the architecture voting table

Six applications vote. A candidate abstraction is only as real as the number of **independent**
sightings it has. Two copies of the same fixture are one sighting.

```
✓   genuine occurrence
~   adjacent but materially different
×   tested and did not appear
B   blocked by a missing substrate fact
    (blank = project not yet run)
```

Evidence scale: 0 sightings = speculation · 1 = name the shape · 2 independent = candidate
abstraction · 3+ materially different = serious extraction candidate · repeated workaround against
the same wall = core-design candidate.

| Candidate | Kitchen | Download | Build | Import | Lobby | Scheduler | Independent | Verdict |
|---|---|---|---|---|---|---|---:|---|
| Promise / responsibility book | ✓ | ✓ | | | | | 2 | *pending* |
| Order / menu resolution | ✓ | × | | | | | 1 | *pending* |
| Role authorship (provenance) | ✓ | ✓ | | | | | 2 | *pending* |
| Activation hold / replay | ✓ | × | | | | | 1 | *pending* |
| Activation-sequence owner | ~ | ~ | | | | | 0 | *pending* |
| Outcome observation ergonomics | × | × | | | | | 0 | *pending* |
| Describe-then-hand-over *(new)* | ✓ | ✓ | | | | | 2 | *pending* |
| Which-half-to-attest *(new)* | ✓ | ✓ | | | | | 2 | *pending* |
| Stringified `WeaveId` on the wire *(new)* | ✓ | ✓ | | | | | 2 | *pending* |

### Notes on the non-obvious cells

- **Order/menu, Download = ×.** Tested and absent, not merely unused: a source either exists or it
  does not. The client's choice is complete at submit time, so there is nothing to offer back.
- **Activation hold/replay, Download = ×.** No bootstrap race arose. The successor's only startup
  act is discharging inherited debts, and it does that *at* activation rather than before it.
- **Sequence owner, both = ~.** Both projects have one operator and one sequence. Nothing
  contended for the number, so nothing wanted an owner for it.
- **Role authorship, both = ✓** and both are *defects that land*: a forged `Plated` finishes a
  dish, a forged `DownloadCompleted` ends a transfer. Neither application can refuse it.

---

## Sugar audit — per project

The expected ordinary result is **raw replacement operations in app code = 0**.

| Project | facade ops | raw ops in app code | txn ids in domain payloads | manual lifecycle wiring | manual candidate cleanup | manual outcome filtering |
|---|---:|---:|---:|---:|---:|---:|
| kitchen-replay | 47 | **0** | 0 | 0 | 0 | 0 |
| download-manager | 40 | **0** | 0 | 0 | 0 | 0 |
| build-farm | | | | | | |
| import-pipeline | | | | | | |
| lobby | | | | | | |
| scheduler | | | | | | |

---

## Cross-project replacement coverage

| arm | covered by |
|---|---|
| immediate candidate readiness | kitchen (`PrepareStation{consult=false}`), download (`verify_sources=false`) |
| deferred candidate readiness | kitchen (`consult=true` → `AskHousePassRate`), download (`verify_sources=true` → `AskCatalogueSize`) |
| authentic refusal | kitchen (fryer asked to be the grill; work it cannot cook), download (catalogue disagreement; unservable debt; over-bound debt) |
| candidate failure | kitchen + download (`StartStage::CandidateLoad`, loader's own words preserved) |
| `AdmissionPending` observed | kitchen + download (asserted between `commit` and the pump) |
| committed outcome | kitchen + download |
| aborted outcome | kitchen + download (`ExplicitAbort`, incumbent still serving) |
| exact error inspection | kitchen + download (`NoRoleHolder`, `CandidateLoad`, `IncumbentBusy`, `AlreadyStarted`, `InvalidReadiness`, `CandidateRefused`) |

---

## Cross-project answer coverage

| surface | used by | felt natural? |
|---|---|---|
| ordinary send | kitchen (outcomes, `Prep`, `Plated`), download (progress, terminals) | yes — the default for anything that must survive replacement |
| publication | kitchen (`StationOpen`) | yes, and **absent from download** — nothing there is an announcement to nobody in particular |
| ordinary reply | **nobody, in either project** | never wanted: every reply is either an *answer* (must be provable) or a *role-addressed send* (must survive replacement) |
| authenticated immediate answer | kitchen (refusals, declines, diagnostics, preparation), download (accepts, refusals, diagnostics, preparation) | yes — the workhorse now that it crosses the `.so` seam |
| authenticated deferred answer | kitchen (receipt across routing; candidate readiness), download (terminal in the holds build; candidate readiness) | yes for a *bounded* wait; **F10** for a long one |
| answer provenance (`answers_ask`) | kitchen (the wall on `RouteChoice`, receipts, bequests), download (the wall on `ObligationsDescribed`; a *measurement* on client traffic) | yes, and its absence is the loudest thing in both reports |
| role-addressed delivery | every service address in both | yes — the reason replacement is invisible to callers |
| direct delivery | outcomes to a diner/client, by id, from a stringified `WeaveId` | yes, but see **F9** |
