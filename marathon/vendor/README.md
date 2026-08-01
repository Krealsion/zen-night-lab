# vendor — the pinned substrate for the marathon

The marathon consumes a **pinned snapshot** of the Loom and pre-built artifacts of Zengine's
Timer package. It never builds against the live sibling trees, and it never writes to them.

**Why.** `Zen/Loom` and `Zen/Zengine` are worked on in parallel with this lab. An experiment whose
green depends on whatever landed in a sibling repo an hour ago is not evidence about either one.
Pinning also means a night's result stays reproducible tomorrow. The first Night Lab experiment
proved this the same night it adopted it: the live Loom tree gained in-progress edits while the
experiment ran, and the lab neither saw them nor was disturbed.

## What is pinned

| thing | pin | how it got here |
|---|---|---|
| Loom source | `78d64ea` ("Add the replacing-a-service-safely authoring page") | `git -C ../../../Loom archive 78d64ea \| tar -x -C loom-src` |
| Loom install | built from the above | `cmake -S loom-src -B loom-build …` then `cmake --install` |
| `zengine/include/timer/{vocabulary,binding}.hpp` | Zengine `f6a4c69` | copied from `Zen/Zengine/timer/` |
| `zengine/include/activation/activation.hpp` | Zengine `f6a4c69` | copied from `Zen/Zengine/activation/` |
| `zengine/lib/zengine-timer.so` | Zengine `f6a4c69` build tree | `Zen/Zengine/build/snake/` |
| `zengine/lib/zengine-timer-virtual.so` | " | `Zen/Zengine/build/tests/` |
| `zengine/lib/zengine-timer-virtual-v2.so` | " | `Zen/Zengine/build/tests/` |
| `zengine/lib/zengine-timer-declines.so` | " | `Zen/Zengine/build/tests/` |

**ABI v4.** The `.so` files and the pinned Loom agree on `ZEN_ABI_VERSION 4`. If a future Loom
bumps the ABI these stop loading and say so — a clean refusal, which is the point of a versioned
ABI.

## The four Timer artifacts, and why four

The Timer package ships one service over swappable clocks and the marathon wants all of them:

| artifact | what it is | who uses it |
|---|---|---|
| `zengine-timer.so` | the shipped service on the **real monotonic clock** | the demos, so at least one lane feels real time |
| `zengine-timer-virtual.so` | the same service on a clock whose nap **books** the duration and returns | every suite — deadlines become exact integers nobody waited for |
| `zengine-timer-virtual-v2.so` | the same source, a **second real library** | prepared replacement of the Timer itself: a candidate that is genuinely a different artifact |
| `zengine-timer-declines.so` | loads, seals, is asked, and **refuses** the preparation | the authentic-refusal arm of the replacement matrix |

The `-v2` and `-declines` artifacts are what make "replace the clock underneath a running
application" and "a candidate that says no" testable without inventing either one locally.

## Re-cutting the snapshot

```sh
# from playground/night-lab/marathon/
rm -rf vendor/loom-src vendor/loom-build vendor/loom
mkdir -p vendor/loom-src
git -C ../../../Loom archive <commit> | tar -x -C vendor/loom-src
cmake -S vendor/loom-src -B vendor/loom-build \
      -DCMAKE_BUILD_TYPE=Debug -DZEN_BUILD_TESTS=OFF -DZEN_BUILD_EXAMPLES=OFF -DZEN_SDL=OFF
cmake --build vendor/loom-build -j"$(nproc)"
cmake --install vendor/loom-build --prefix "$PWD/vendor/loom"
```

Then update the table above. Nothing in `Zen/Loom` or `Zen/Zengine` is written by any of this.
