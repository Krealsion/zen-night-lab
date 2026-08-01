# vendor — the pinned substrate

The lab consumes a **pinned snapshot** of the Loom and pre-built artifacts of Zengine's Timer
package. It never builds against the live sibling trees, and it never writes to them.

**Why.** `Zen/Loom` and `Zen/Zengine` are worked on in parallel with this lab. An experiment whose
green depends on whatever landed in a sibling repo an hour ago is not evidence about either one.
Pinning also means a night's result stays reproducible tomorrow.

## What is pinned

| thing | pin | how it got here |
|---|---|---|
| Loom source | `d7dd974` ("Use captured requester incarnation for deferred answers") | `git -C ../../../Loom archive d7dd974 \| tar -x -C loom-src` |
| Loom install | built from the above | `cmake -S loom-src -B loom-build …` then `cmake --install` |
| `zengine/include/timer/{vocabulary,binding}.hpp` | Zengine `93eef58` | copied from `Zen/Zengine/timer/` |
| `zengine/include/activation/activation.hpp` | Zengine `93eef58` | copied from `Zen/Zengine/activation/` |
| `zengine/lib/zengine-timer.so` | Zengine `93eef58` build tree | copied from `Zen/Zengine/build/snake/` |
| `zengine/lib/zengine-timer-virtual.so` | Zengine `93eef58` build tree | copied from `Zen/Zengine/build/tests/` |

The two headers are the Timer package's public **contract** — a consumer must have them, exactly as
it would have them from an installed Zengine. The two `.so` files are **artifacts**, not a rebuild
of someone else's tree; they are tracked in this repo on purpose, because an experiment nobody can
re-run is not evidence.

The `.so` files are ABI v3 (`ZEN_ABI_VERSION 3`), matching the pinned Loom. If a future Loom bumps
the ABI, these stop loading and say so — a clean refusal, which is the point of a versioned ABI.

## Re-cutting the snapshot

```sh
# from playground/night-lab/
rm -rf vendor/loom-src vendor/loom-build vendor/loom
mkdir -p vendor/loom-src
git -C ../../Loom archive <commit> | tar -x -C vendor/loom-src
cmake -S vendor/loom-src -B vendor/loom-build \
      -DCMAKE_BUILD_TYPE=Debug -DZEN_BUILD_TESTS=OFF -DZEN_BUILD_EXAMPLES=OFF -DZEN_SDL=OFF
cmake --build vendor/loom-build -j"$(nproc)"
cmake --install vendor/loom-build --prefix "$PWD/vendor/loom"
```

Then update the table above. Nothing in `Zen/Loom` or `Zen/Zengine` is written by any of this.
