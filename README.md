# night-lab

Bounded experiments on the Loom. Nothing here is a deliverable and nothing here is a framework.
The playground is allowed to be strange; the core is not ours to change.

Two experiments live here, side by side, and they are pinned against **different** substrate
commits on purpose. An experiment is evidence about the substrate it ran on; re-pointing an old
experiment at a new Loom would destroy the record rather than extend it.

| path | what it is | pinned against |
|---|---|---|
| `original/` | **Night One** — the job kitchen (2026-07-30). Preserved byte-for-byte. | Loom `d7dd974`, Zengine `93eef58`, ABI v3 |
| `marathon/` | **Night Two** — the six-project marathon. | Loom `78d64ea`, Zengine `f6a4c69`, ABI v4 |
| `workshop-marathon/` | **Night Three** — the Serious Playground marathon (2026-08-02). | Loom `61b2915`, Zengine `0356f02`, ABI v5 |

## original/

The first night's experiment, exactly as it was written, including its own `README.md`,
`reports/latest.md`, `vendor/README.md` and mutation harness. It moved down one directory level
and **nothing inside it was edited**, so the two absolute paths it hard-codes now need one extra
component:

```sh
# tests/mutate.sh says LAB=/mnt/g/programming/cpp/Zen/playground/night-lab
# it is now  .../playground/night-lab/original
```

Its `build/` tree was regenerated at the new location (a CMake cache bakes in absolute source
paths, so the old one was stale rather than valuable). Everything else is untouched.

## marathon/

Six materially different applications, built to make the substrate justify itself. Start with
`marathon/README.md`; the running ledgers are `marathon/FRICTION.md` and `marathon/EVIDENCE.md`.

## workshop-marathon/

Night Three asks a different question from the first two. They asked what the
substrate does for applications; this one asks whether the **Workshop** — the
making-tool the vision promises — is already latent in Zen, by having a cold
builder try to grow one in a single pass.

Five toys, a stranger, and a prototype that describes, runs, inspects, alters
live, descends into code, composes, and shares. Start with
`workshop-marathon/README.md` to use it, or
`workshop-marathon/reports/FINAL-REPORT.md` for the result.

The one-line answer: **the *making* Workshop was largely latent already; the
*seeing* Workshop is not there yet, and Zen says so itself.** One core
reproducer came out of it (`repros/core/silent-seam-emission` — a loaded
weave's emission can vanish with no observable trace anywhere, which sharpens
Night Two's Blocker 2), and the release litmus is **ALMOST**, with two named
blockers.

Unlike Night Two, this experiment carries only the **four** Zengine artifacts
it actually loads rather than everything the package builds; the rest are
rebuildable from the pin (`vendor/README.md` says which and why).

## House rules all three experiments hold themselves to

- Only existing public Loom and Zengine behaviour. Nothing in this repo writes to `Zen/Loom` or
  `Zen/Zengine`.
- A local fake is allowed only when it is **labelled** and does not invalidate the question.
- If an experiment appears to need a Loom or Zengine change, it does not make one. It produces the
  smallest reproducer, describes the missing seam, and continues.
