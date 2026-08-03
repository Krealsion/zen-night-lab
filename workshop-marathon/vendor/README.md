# vendor — the pinned substrate for workshop-marathon

Same discipline as `marathon/vendor` (Night Two): the experiment consumes a
**pinned snapshot**, never the live sibling trees, and never writes to them.

| thing | pin | how |
|---|---|---|
| Loom source | `61b2915` | `git -C ../../../Loom archive \| tar -x` → `loom-src/` (ignored) |
| Loom install | built from the above | `setup.sh` → `loom-install/` (ignored), consumed via `find_package(loom)` |
| Zengine source | `0356f02` | `git archive` → `zengine-src/` (ignored; vocabulary/binding headers consumed from here) |
| Zengine artifacts | built from the above against the vendored Loom install | `collect.sh` → `zengine/lib/*.so` (tracked) |

## The four artifacts, and why four

Zengine builds fifteen `.so` files. This experiment tracks **four** — the ones
it actually loads. The other eleven (the SDL skin, the snake package, the old
marathon's replacement fixtures, the probes) are rebuildable from the pin by
`setup.sh` and are not carried in git, because ~57 MB of binaries nothing here
reads is not evidence, it is luggage.

| artifact | what it is | who uses it |
|---|---|---|
| `zengine-timer.so` | the shipped service on the **real monotonic clock** | every `workshop run` — so at least one lane feels real time |
| `zengine-timer-virtual.so` | the same service on a clock whose nap **books** the duration and returns | every witness in `tests/` — "the beam swept N cells" becomes an exact integer nobody waited for |
| `zengine-input.so` | the sole producer of key events | `run -i` (the interactive keymap) and the `scribe` toy |
| `zengine-skin-tui-classic.so` | a second Skin | so the live skin **swap** replaces real painting code, not a stub |

**ABI v5.** The `.so` artifacts and the vendored Loom agree on
`ZEN_ABI_VERSION 5`. A future mismatch refuses at load, loudly — the point of
a versioned ABI.

**The vendored green:** Zengine's own ctest ran 10/10 against this exact
install (`zengine-ctest.log` retained locally, ignored by git) — the
stranger-path proof that the vendored substrate works end to end.

**Zengine consumption note (P-003 evidence):** Zengine exports nothing, so this
directory IS the consumption story: headers from the source archive, binaries
hand-collected from its build tree. Every time `collect.sh` runs, that is
another sighting of the missing packaged path.

Rebuild from scratch: `bash setup.sh && bash collect.sh` (WSL).
