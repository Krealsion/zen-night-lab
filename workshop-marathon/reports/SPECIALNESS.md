# SPECIALNESS — the bootstrap-shell ledger

Every time the Workshop gains something ordinary creations cannot themselves
inspect / message / replace / reuse / compose / represent, it is recorded here
when it is written — not discovered at the end.

## The specialness budget (final)

| SPECIAL THING | WHY SPECIAL TODAY | ORDINARY TOY CAN INSPECT? | ORDINARY TOY CAN REPLACE? | EVIDENCE IT MUST REMAIN SPECIAL | NEXT PRESSURE THAT WOULD REMOVE IT |
|---|---|---|---|---|---|
| **S-1 the shell** (holds `Switchboard&`: pump, mount, `send_as`, grants) | some process must own the loop; Loom's own trust boundary says holding the Switchboard *is* being the host | no | no | **fundamental today.** Not a Workshop choice — `capabilities.md`: "Holding a `Switchboard&` *is* being a host for that Loom" | none found. A Workshop-inside-a-Workshop would need a nested-board story Loom does not have (and LIFE-04 says authorities are board-relative) |
| **S-2 compiled-in paths** (`WORKSHOP_SOURCE_ROOT` / `_BINARY_DIR`) | v1 needed a place to stand; stem→artifact resolution searches paths no toy can name | no | no | **none — bootstrap debt** | already half-removed: Gate 7 bundles carry artifacts beside their description, and imported toys resolve from `toys/<name>/artifacts/`. A described search path finishes it |
| **S-3 the tap faucet** (`add_observer`) | observation is host authority; a weave that could tap the bus could read every private conversation | **yes** — the bridge republishes every event as ordinary gated `BusFact` intent | no (cannot install its own tap) | **fundamental for the faucet, NOT for the information.** Two independent consumers (inspector; the echo toy's machine-fly) read bus facts as ordinary weaves | none for the faucet. If a third consumer appears the *vocabulary* becomes a Zengine package candidate — the privilege stays, the data does not |
| **S-4 the build runner** (`std::system("cmake --build …")` on the `u` key) | a real compiler invocation from inside a native weave; **the whole world pauses** while it runs | no | no | **none — bootstrap debt**, and it is ugly in the open (the pump stalls; recorded rather than hidden behind a spinner) | a build-watcher **weave** that owns compilation as a service and answers with a result. Nothing in the substrate prevents it; I ran out of marathon |

**Not special, and proven so:** the registry, the inspector, the log skin and
every toy part are ordinary loadable weaves. Each was inspected via
`PokeDescribe`, read via `PokeRead`, and replaced via the same steward door a
toy part uses — the inspector crossed its own reload with its memory intact
(Gate 10 T3), the registry was hard-swapped mid-run (Gate 3 A3), and canary #9
pins that the recursive operation left a `zen.ReloadWeave` **door fact** on the
record rather than taking a kernel-side bypass.

**The honest score:** of four privileges, **two are fundamental** (owning the
process; the observation faucet) and **two are bootstrap debt with a named
exit** (paths, the build runner). The vision says *"nothing is special;
nothing is above you."* The distance to that, measured here, is: one nested-
board question nobody has asked yet, and two errands.

---

## Running entries (as written, during the build)

---

## Running entries

### S-4 — the build runner (2026-08-02, Gate 4)
The `u` key runs a real `cmake --build` from inside the operator weave before
reloading. Host-tier OS act; the pump stalls for its duration, so the world
visibly freezes. Recorded in the open as boring friction, not hidden.

### S-1 — the shell holds root (2026-08-02, Gate 1)
`workshop/shell.cpp` owns the `Switchboard&` (pump, mount, `send_as`, grant
assignment) and the `Kernel`. Ordinary creations cannot inspect or replace the
shell, and cannot mount native weaves. Why special today: some process must
own the loop; Loom's own trust boundary says holding the Switchboard IS being
the host. Mitigation already in place: everything above the loop is messages —
lifecycle through the Manager door, launch facts as publishes, stopping as a
StopWish any granted weave may speak. Next pressure that would shrink it:
Gate 10 (can the operator itself be a loadable, replaceable weave?).

### S-2 — the shell knows the build tree (2026-08-02, Gate 1)
`WORKSHOP_SOURCE_ROOT`/`WORKSHOP_BINARY_DIR` are compiled in; stem→artifact
resolution searches paths no ordinary creation could name. Why special today:
v1 needed a place to stand. This is bootstrap debt, not fundamental: a project
identity/bundle story (Gate 7) should replace compiled-in paths with described
ones. Next pressure: sharing — an exported toy cannot reference my build tree.

### S-3 — the tap bridge (2026-08-02, Gate 2)
`workshop/bridge.hpp` holds the one `add_observer` hand (host-tier: only a
Switchboard holder can watch the bus) and re-publishes every event as ordinary
gated `BusFact` intent. Ordinary toys CANNOT install their own tap — but they
CAN consume the bridge's facts exactly as the inspector does, so the
*information* is not privileged, only the *faucet*. Evidence it must remain
special: observation is host authority by Loom's own trust boundary (a weave
that could tap the bus could read every private conversation). Next pressure
that would shrink it: none expected — but Gate 10 must show the inspector
itself being inspected/replaced through ordinary doors, proving the faucet is
the ONLY privilege left in the observation story.

### NOT special, recorded deliberately: the registry, the log skin, and the
governor are ordinary weaves (loadable/replaceable/inspectable through the
same doors as any toy part); the operator is native but speaks only granted
messages — its only privilege is existing before the first pump.
