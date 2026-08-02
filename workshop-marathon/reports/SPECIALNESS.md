# SPECIALNESS — the bootstrap-shell ledger

Every time the Workshop gains something ordinary creations cannot themselves
inspect / message / replace / reuse / compose / represent, it is recorded here
when it is written — not discovered at the end.

Final form (built at postmortem):

| SPECIAL THING | WHY SPECIAL TODAY | ORDINARY TOY CAN INSPECT? | ORDINARY TOY CAN REPLACE? | EVIDENCE IT MUST REMAIN SPECIAL | NEXT PRESSURE THAT WOULD REMOVE IT |
|---|---|---|---|---|---|

---

## Running entries

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
