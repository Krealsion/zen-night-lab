# workshop-marathon — the Serious Playground prototype (Night Lab III)

A cold-built prototype of Zen's **Workshop**: describe a creation in a
project file, run it on the real substrate, watch it, reach inside it while
it lives, share it. Built against pinned **Loom `61b2915` / Zengine `0356f02`
/ ABI v5** (vendored — nothing here writes to the real trees).

## What this is / is NOT

IS: a working describe→run→see→inspect→alter→share loop over real Zen; five
example creations; a witness suite (90+ cases) pinning every claim.

IS NOT: a product, a GUI node editor, a package manager, or a promise. The
visual surface is a terminal; bundles ship binaries, not source; there is no
crypto identity — provenance is labelled at exactly the trust it deserves.
Current limits live in `reports/` (PRESSURE.md, SPECIALNESS.md, PILOT.md).

## Build (WSL, GCC >= 11.4, cmake >= 3.22)

```sh
cd /mnt/g/programming/cpp/Zen/playground/night-lab/workshop-marathon
bash vendor/setup.sh      # once: vendors + builds the pinned substrate (~minutes)
bash vendor/collect.sh    # once: collects Zengine service artifacts
cmake -S . -B build && cmake --build build -j"$(nproc)"
ctest --test-dir build    # the witness suite; ~2s
```

## First contact

```sh
./build/workshop/workshop list                 # what creations exist
./build/workshop/workshop run lighthouse --for-seconds 10
./build/workshop/workshop run pond --for-seconds 20   # watch them synchronize
```

Everything you see is published intent painted by a replaceable skin; the
`[inspector]` line is the machine narrating itself.

## Make something

```sh
./build/workshop/workshop new mytoy            # scaffolds toys/mytoy/project.json
# add toys/mytoy/mytoy-part.cpp + a CMake target (copy toys/lighthouse/ as a model),
# or skip code entirely: constellation is ONLY a project.json reusing other toys' parts
./build/workshop/workshop describe mytoy       # your file as the gate admitted it
./build/workshop/workshop view mytoy           # the schematic
./build/workshop/workshop build mytoy && ./build/workshop/workshop run mytoy
```

A project file declares parts (name/stem/role), needs (timer/skin/input),
`set` (initial pokes — same artifact, many configured lives) and `knobs`
(live reach-in points).

## Inspect, reach inside, learn

- Post-run reports print automatically (what launched + what the machine did,
  refusals explained).
- `run <toy> --refuse` provokes one refusal and explains it; `--watch` prints
  raw tap lines; `--deny <need>` runs with less power, visibly.
- `run <toy> -i` (real terminal): `v` schematic, `h` "what is this?" answered
  by the running part itself, `p` cycle a knob, `1` swap the skin, `r` reload
  the part in place (state survives), `u` rebuild + reload (the code height),
  `q` quit.
- `safety <toy>`: the power view — no shields painted.

## Share

```sh
./build/workshop/workshop export <toy> <dest-dir> [author]   # author is UNVERIFIED
./build/workshop/workshop import <bundle-dir>                # fingerprints verified
```

An imported toy runs from its own artifacts (`toys/<name>/artifacts/`) with
no reference to your build tree, and confers no grants.

## Where the truth lives

`reports/NOTEBOOK.md` (the build chronicle) · `reports/PRESSURE.md` (what
pushed back, incl. one core reproducer) · `reports/VOTES.md` (which
abstractions the toys fought) · `reports/CANARIES.md` (the lies the suite
catches) · `reports/SPECIALNESS.md` (what is still privileged and why).
