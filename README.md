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

## House rules both experiments hold themselves to

- Only existing public Loom and Zengine behaviour. Nothing in this repo writes to `Zen/Loom` or
  `Zen/Zengine`.
- A local fake is allowed only when it is **labelled** and does not invalidate the question.
- If an experiment appears to need a Loom or Zengine change, it does not make one. It produces the
  smallest reproducer, describes the missing seam, and continues.
