# The Night Lab Marathon

Six materially different applications, built to make the substrate justify itself.

Not to prove Zen is good. Not to make every experiment green. **Put different lives inside the
machine and see which parts of the machine they all reach for.**

**Pinned against:** Loom `78d64ea`, Zengine `f6a4c69`, **ABI v4**. See `vendor/README.md`.
Nothing in this tree writes to `Zen/Loom` or `Zen/Zengine`.

## Status: six GREEN, none blocked

| # | project | the architectural question it exists to ask | cases / assertions |
|---|---|---|---:|
| 1 | `kitchen-replay/` | What did a year of substrate work actually change for an application that already existed? | 39 / 196 |
| 2 | `download-manager/` | Is the original answer capability the right thing to hold for the whole operation? | 30 / 153 |
| 3 | `build-farm/` | Same shape, different domain — do two independent implementations grow the same bookkeeping? | 29 / 153 |
| 4 | `import-pipeline/` | Does *request → menu → resolved choice → receipt* reproduce outside the kitchen? | 26 / 134 |
| 5 | `lobby/` | Are *speaker identity* and *office authorship* observably different security facts? | 17 / 99 |
| 6 | `scheduler/` | Do two individually pleasant APIs stay pleasant together? | 18 / 113 |
| | | **total** | **159 / 848** |

**Start with `FINAL-REPORT.md`.** The one-line answer: the substrate's authoring surface is in very
good shape (178 semantic facade operations, **zero** raw prepared-replacement calls in application code), and
the one thing five of six applications reached for and could not have is **role-holding provenance**.

## The ledgers

- `FINAL-REPORT.md` — portfolio status, the ranked candidates, the blockers, the next five errands,
  and what **not** to build.
- `FRICTION.md` — 17 entries, written as they happened.
- `EVIDENCE.md` — the voting table, the sugar audit, and the replacement/answer coverage matrices.
- `<project>/REPORT.md` — one compact report per project.

## Building and running

WSL/GCC only (the kernel is `dlopen`/POSIX ground).

```sh
cd /mnt/g/programming/cpp/Zen/playground/night-lab/marathon
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

# every project has one, and each ends with a residue check
bash kitchen-replay/mutate.sh          # add ids to re-run a subset: `... 14 15`
bash download-manager/mutate.sh
bash build-farm/mutate.sh
bash import-pipeline/mutate.sh
bash lobby/mutate.sh
bash scheduler/mutate.sh

MARATHON_TRACE=1 ./build/kitchen-replay/kitchen-tests   # every delivery and refusal
./build/kitchen-replay/kitchen-demo                     # the one demo, on the REAL clock
```

## The laws this marathon held itself to

- **Night Lab discovers abstractions. It does not pre-authorize them.** No feature was added to Loom
  or Zengine because one project wanted it. **Neither repository was modified.**
- **The shared-helper law.** Each project began with only Loom, Zengine, the prepared-replacement
  handle and the standard library. Extraction was *permitted* after project 3 and **deliberately
  declined** — see FINAL-REPORT §4 for why.
- **`loom::PreparedReplacement` is the path.** Every replacement went through the handle; the one
  nonzero in the sugar audit is explained and classified.
- **No core patches.** Two blockers were found; both became reproducers and written laws, and both
  projects continued around them.
- **A labelled fake is allowed; an unlabelled one is not.** The Timer's virtual clock is the only
  substitution, and every suite that uses it says so at the top of its harness.
- **A GREEN mutation is a reported gap, never a shrug.** Three were found; two were closed with new
  cases, and the third is named with its masking mechanism.
