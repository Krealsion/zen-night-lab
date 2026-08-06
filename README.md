# night-lab

Bounded experiments on the Loom. Nothing here is a deliverable and nothing here is a framework.
The playground is allowed to be strange; the core is not ours to change.

The repository has two halves, and they are governed differently.

```text
historical Night Lab          frozen evidence, read-only forever
    original/
    marathon/
    workshop-marathon/
    followups/

current-era Night Lab         live downstream experimentation
    current/
```

## The historical laboratory — frozen

Four areas, pinned against **different** substrate commits on purpose. An experiment is evidence
about the substrate it ran on; re-pointing an old experiment at a new Loom would destroy the record
rather than extend it.

| path | what it is | pinned against |
|---|---|---|
| `original/` | **Night One** — the job kitchen (2026-07-30). Preserved byte-for-byte. | Loom `d7dd974`, Zengine `93eef58`, ABI v3 |
| `marathon/` | **Night Two** — the six-project marathon. | Loom `78d64ea`, Zengine `f6a4c69`, ABI v4 |
| `workshop-marathon/` | **Night Three** — the Serious Playground marathon (2026-08-02). | Loom `61b2915`, Zengine `0356f02`, ABI v5 |
| `followups/role-authorship/` | The R2D-0 role-authorship replay — Loom-only, no Zengine. | Loom `30eab0a`, ABI v5 |

**These four directories are not to be updated to current Zen.** Not their pins, not their tracked
binaries, not their reports. Each one's findings are claims about the world it ran on — Night Three's
central finding is about a seam that has since been *closed*, so re-pointing it at current Loom would
make the finding fail to reproduce and a reader would conclude the report was wrong rather than that
the substrate was fixed. The follow-up exists precisely to pin the world that Night Two's findings
changed; collapsing both onto one Loom would erase a deliberate before/after pair.

The substrate enforces the separation mechanically: a current-HEAD host refuses these tracked
artifacts by name (`unsupported abi_version 5 (host supports 6)`). The ABI version is the wall, and
it already holds.

All four reconstruct and pass from their preserved pins. `workshop-marathon/repros/core/silent-seam-emission/`
is cited by path in Loom's own `docs/reference/known-seams.md`.

### original/

The first night's experiment, exactly as it was written, including its own `README.md`,
`reports/latest.md`, `vendor/README.md` and mutation harness. It moved down one directory level
and **nothing inside it was edited**, so the two absolute paths it hard-codes now need one extra
component. That restraint is itself part of the evidence.

### marathon/

Six materially different applications, built to make the substrate justify itself. Start with
`marathon/README.md`; the running ledgers are `marathon/FRICTION.md` and `marathon/EVIDENCE.md`.

### workshop-marathon/

Night Three asks a different question from the first two. They asked what the substrate does for
applications; this one asks whether the **Workshop** — the making-tool the vision promises — is
already latent in Zen, by having a cold builder try to grow one in a single pass.

The one-line answer: **the *making* Workshop was largely latent already; the *seeing* Workshop is not
there yet, and Zen says so itself.** Start with `workshop-marathon/README.md` to use it, or
`workshop-marathon/reports/FINAL-REPORT.md` for the result.

### followups/role-authorship/

The focused replay of the finding that became a Loom law. Loom-only — it has no `vendor/zengine`
directory at all, which is why it is also the proof that a Loom-only Night Lab experiment is clean.

## The current-era laboratory — live

`current/` is where new experiments happen, against a **current** Loom consumed through its installed
package. It stands beside the frozen areas and never touches them.

See `current/README.md` for the house rules and `current/substrate.lock` for the Zen the first two
experiments experienced — `current/records-committee/` resolved a later Loom and carries its own
lock. The running ledgers are `current/FRICTION.md` and `current/EVIDENCE.md`.

| path | what it is |
|---|---|
| `current/signal-box/` | a miniature railway interlocking — routes, occupancy, an independent safety monitor, and equipment that can be taken out of service |
| `current/prompt-corner/` | one act of a play, called live from the prompt book, with the Deputy Stage Manager replaced halfway through while the act keeps running |
| `current/records-committee/` | a county rarities committee — five separately built assessors who disagree about the same bird, and a county list that survives the process exiting |

## House rules every experiment holds itself to

- Only existing public Loom and Zengine behaviour. Nothing in this repo writes to `Zen/Loom` or
  `Zen/Zengine`.
- A local fake is allowed only when it is **labelled** and does not invalidate the question.
- If an experiment appears to need a Loom or Zengine change, it does not make one. It produces the
  smallest reproducer, describes the missing seam, and continues.
- **No file may be shared between two experiments** except a test framework and the substrate itself.
- Sightings nominate. They do not authorize.
