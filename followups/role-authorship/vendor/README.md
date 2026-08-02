# vendor — the pinned substrate for this follow-up

Same law as the marathon's vendor: a **pinned snapshot** of the Loom, installed
under `vendor/loom`, never the live sibling tree. An experiment is evidence
about the substrate it ran on; the marathon stays pinned to `78d64ea` (ABI v4)
*because* its findings are about that world, and this follow-up pins the world
those findings changed.

## What is pinned

| thing | pin | how it got here |
|---|---|---|
| Loom source | `30eab0a` ("Carry role authorship across dynamic ABI v5") | `git -C ../../../../Loom archive 30eab0a \| tar -x -C loom-src` |
| Loom install | built from the above | `cmake --install`, commands below |

**ABI v5.** The follow-up's weaves and the pinned Loom agree on
`ZEN_ABI_VERSION 5`. The marathon's vendored artifacts would refuse to load
here — cleanly, by version — which is exactly the compatibility discipline the
seam promises.

## Re-cutting the snapshot

```sh
# from playground/night-lab/followups/role-authorship/
rm -rf vendor/loom-src vendor/loom-build vendor/loom
mkdir -p vendor/loom-src
git -C ../../../../Loom archive <commit> | tar -x -C vendor/loom-src
cmake -S vendor/loom-src -B vendor/loom-build \
      -DCMAKE_BUILD_TYPE=Debug -DZEN_BUILD_TESTS=OFF -DZEN_BUILD_EXAMPLES=OFF -DZEN_SDL=OFF
cmake --build vendor/loom-build -j"$(nproc)"
cmake --install vendor/loom-build --prefix "$PWD/vendor/loom"
rm -rf vendor/loom-src vendor/loom-build
```

Then update the table above. Nothing in `Zen/Loom` or `Zen/Zengine` is written
by any of this.
