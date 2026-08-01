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
| Promise / responsibility book | | | | | | | | |
| Order / menu resolution | | | | | | | | |
| Role authorship (provenance) | | | | | | | | |
| Activation hold / replay | | | | | | | | |
| Activation-sequence owner | | | | | | | | |
| Outcome observation ergonomics | | | | | | | | |

---

## Sugar audit — per project

The expected ordinary result is **raw replacement operations in app code = 0**. Any nonzero count
requires an explanation, classified as: `advanced capability genuinely outside the handle` /
`missing sugar` / `incorrect abstraction boundary` / `test-only inspection`.

| Project | facade ops | raw ops in app code | txn ids in domain payloads | manual lifecycle wiring | manual candidate cleanup | manual outcome filtering |
|---|---:|---:|---:|---:|---:|---:|
| kitchen-replay | | | | | | |
| download-manager | | | | | | |
| build-farm | | | | | | |
| import-pipeline | | | | | | |
| lobby | | | | | | |
| scheduler | | | | | | |

---

## Cross-project replacement coverage

At least four projects must perform a real dynamic replacement through `loom::PreparedReplacement`.
Across the portfolio these arms must all be covered.

| arm | covered by |
|---|---|
| immediate candidate readiness | |
| deferred candidate readiness | |
| authentic refusal | |
| candidate failure | |
| `AdmissionPending` observed | |
| committed outcome | |
| aborted outcome | |
| exact error inspection | |

---

## Cross-project answer coverage

| surface | used by | felt natural? |
|---|---|---|
| ordinary send | | |
| publication | | |
| ordinary reply | | |
| authenticated immediate answer | | |
| authenticated deferred answer | | |
| answer provenance check (`answers_ask`) | | |
| role-addressed delivery | | |
| direct delivery | | |
