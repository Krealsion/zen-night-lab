# Pressure / Evidence Ledger

Classified while it happens, not retrofitted. Categories:

- **DISCOVERED** — Zen already expressed the need cleanly; found and used.
- **LOCAL PATTERN** — Workshop needed an abstraction; evidence says it belongs in Workshop.
- **REPEATED PATTERN** — 2+ materially independent toys wanted the same local shape.
- **PACKAGE PRESSURE** — several applications want an engine-shaped service/vocabulary
  that may belong in Zengine. No Zengine patches.
- **CORE PRESSURE** — current Loom semantics prevent a system truth from being expressed
  honestly. No Loom patches; reproducer required.
- **ERGONOMIC PRESSURE** — the truth is expressible but needlessly painful.
- **DOCUMENTATION FAILURE** — capability existed; a cold builder could not reasonably
  discover it from the current path.
- **VISION AMBIGUITY** — the Workshop promise admits materially different interpretations.
- **SPECIAL MACHINERY** — privileged machinery ordinary creations cannot
  inspect/use/replace (also tracked in SPECIALNESS.md).
- **DEAD END** — a reasonable design tried and discarded. Kept.

---

## Entries

### P-001 — DOCUMENTATION FAILURE (minor, drift)
`Loom/docs/laws/kernel-laws.md` KERN-04 says "Current: **v4**"; `abi.h` pins
`ZEN_ABI_VERSION 5u` and `reference/dynamic-abi.md` says v5. Stale law text from
the v5 bump. Per AGENTS.md's own rule: source+tests win; recorded, not fixed.
Sightings: 1 (Gate 0 read).

### P-002 — DOCUMENTATION FAILURE (routing gap)
The Bridge (remote operator; `zen-bridge`, `kMaxOperatorConnections`,
`kMaxPendingDelivered`, a `bridge` test suite) is real enough to have published
bounds, but `CONTEXT.md` has no topic row for it and no reference page exists. A
cold builder discovers it via the bounds page or CMakeLists only.
Sightings: 1 (Gate 0 read).

### P-003 — DOCUMENTATION FAILURE + ERGONOMIC PRESSURE (consumption story)
Zengine has no install/export and no documented consumer path (Gate 0 Q12 —
"still unclear"). Consuming it means hand-copying headers + built artifacts.
Input/Surface additionally have no reference pages (README sections as docs).
Sightings: 1 read + 1 structural (this experiment must now hand-vendor Zengine
artifacts to build anything on it — each vendoring act is a repeat sighting).

### P-004 — OBSERVATION (unclassified until Gate 8)
`zen-isolation` is not in Loom's install export. The OS-containment story
(capabilities reference: enforced namespaces/cgroups, `containment()`
confirmation) may be unreachable for a stranger-consumer. If Gate 8 confirms a
stranger cannot choose an enforced host path through the public surface, this
becomes CORE PRESSURE (a safety promise inexpressible from outside); if there is
a public road, downgrade to DOCUMENTATION.

### P-006 — LOCAL PATTERN (generic slot surface)
The snake-era TUI skins paint exactly two slots ("status"/"score") and drop the
rest; the first toy's own slot would have been invisible. The Workshop ships
`workshop-skin-log` — an ordinary replaceable Skin painting every slot as
scrollback. First concrete sighting of the missing general canvas/slot
vocabulary (ties to P-005). Owner if it ever earns extraction: Zengine
(Surface package). One sighting; stays local.

### P-007 — ERGONOMIC PRESSURE + BORING FRICTION (native TimedWeave reach)
A native `TimedWeave` needs three role-scoped grant rules (`EnsureTimer`,
`EnsureRoleTimer`, `CancelTimer` → `zengine.timer`) that its binding layer
speaks on its behalf. Under-grant it and the weave is silently inert: the
CapabilityDenied refusals are visible only to a tap (send-fate seam, by law),
so the failure reads as "nothing happens". The binding's header documents the
cost ("WHAT IT COSTS, said up front") — the friction is that no *code* spells
it: every host re-derives the grant list by hand. Workshop fix:
`allow_timed_weave(grant)` helper. Probable owner if repeated: Zengine
(binding could export its own required-grant recipe). Sightings: 1.

### P-005 — VISION AMBIGUITY (pre-registered at Gate 0, to be tested)
"Wire ideas without writing code" meets a substrate whose visual vocabulary is
deliberately tiny (`SurfaceText` slots; canvas "a later phase"; SDL window
output-only; no SDL input Reader). The promise admits at least two readings:
(a) the Workshop invents its own visual/canvas vocabulary locally (Workshop-owns
-experience reading), or (b) the substrate is expected to grow one (Zengine
package reading). The toys will vote; neither is assumed.
