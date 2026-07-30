# night-lab

Bounded experiments on the Loom. One experiment at a time; each leaves behind runnable code,
focused tests, a usage example, and a written-up morning report in `reports/`.

Nothing here is a deliverable and nothing here is a framework. The playground is allowed to be
strange; the core is not ours to change.

## What is in the tree

| path | what it is |
|---|---|
| `kitchen/` | **the job kitchen** — the current experiment. Three roles, six loadable weaves, one vocabulary, one scripted demo host. |
| `repro/` | the smallest concrete reproducer for the one core seam the experiment found. |
| `tests/` | the suite (`test_kitchen.cpp`), its host harness, and the mutation harness that checks the suite can go red. |
| `vendor/` | the **pinned** Loom snapshot and Zengine Timer artifacts. See `vendor/README.md`. |
| `reports/latest.md` | the morning report. |

## Building and running

Everything is WSL/GCC. The lab consumes a pinned Loom snapshot from `vendor/loom`, never the
live sibling tree — see `vendor/README.md` for why and for how to re-cut it.

```sh
cd /mnt/g/programming/cpp/Zen/playground/night-lab
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"

ctest --test-dir build --output-on-failure   # the suite + the reproducer
./build/kitchen/kitchen-demo                 # the usage example, on the real clock
./build/kitchen/repro-answer-seam            # the core-seam reproducer, on its own

bash tests/mutate.sh                         # does the suite actually catch anything?
NIGHT_LAB_TRACE=1 ./build/kitchen/night-lab-tests   # every delivery and refusal, from the host's tap
```

## House rules this lab holds itself to

- Only existing public Loom and Zengine behaviour. No root sends after boot, no wildcard grants,
  no direct Switchboard access from a weave, no undeclared messages, no threads, no hidden global
  state, no silent failure.
- A local fake is allowed only when it is **labelled** and does not invalidate the question. There
  is exactly one in this lab — the Timer's clock — and `tests/harness.hpp` says so at the top.
- If an experiment appears to need a Loom or Zengine change, it does not make one. It produces the
  smallest reproducer, describes the missing seam, and continues only if a local workaround can
  still test the original question honestly.
