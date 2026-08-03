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

### P-008 — LOCAL PATTERN (observation republication bridge) + DISCOVERED
The bus tap is host-tier by design; the Workshop's bridge converts it into
ordinary published `BusFact` intent so the inspector (and any curious toy) is
an ordinary consumer. Zero substrate friction — the runtime's BusEvent already
carries everything an honest observer needs (stamped office, life diagnostics,
expected/actual requester on answer refusals). Classify the tap surface
DISCOVERED; the bridge LOCAL PATTERN. Sightings: 2 — the inspector, and then
the echo toy's machine-fly (a TOY consuming bus-facts, via the PLAY DETOUR).
One more materially independent consumer upgrades this to PACKAGE PRESSURE
(an observation vocabulary may be engine-shaped, not Workshop-shaped).

### P-009 — DISCOVERED (a named deferred trigger has a live claimant)
Two honest schema evolutions in one day (ProjectSpec v1→v2→v3; PartSpec v1→v2)
each meant hand-rewriting every existing project file — the strict gate
(MissingField) makes old files refuse loudly, which is correct and also
exactly the pain the **migration layer** trigger in Loom's known-seams
("first persisted value that must evolve") predicts. The Workshop is now that
first claimant. Not proposing the layer — recording that the trigger fired.
Sightings: 2 (same day).

### P-010 — LOCAL PATTERN, watching (roles as instance namespace)
The pond addresses instances as roles (`pond.fly.1..8`) because a poke needs
an address that survives and parts have no other durable address. It works,
and it also means the ROLE namespace is doing double duty as an instance
namespace by convention. If a third toy mints `<toy>.<part>.<n>` roles, this
becomes REPEATED PATTERN and the "minted identity needs a surviving
namespace" guideline gets a Workshop-shaped sighting. Sightings: 1.

### P-011 — CORE PRESSURE (the silent seam) — reproducer filed
A loaded weave's emission whose schema nobody registered vanishes with NO
observable trace: no BusEvent, no journal ticket (structurally absent), no
recipient, and the sender shim is fire-and-forget. Found when Gate 8 denied
the timer need: the lamp's EnsureTimer disappeared while a native weave's
identical reach refused loudly (NoSuchTarget). The observability floor
differs by tier, and "refusals are never silence" has a seam-shaped hole.
Narrowest missing truth: a host-side observable event for seam rejections.
Reproducer: `repros/core/silent-seam-emission/`. Pinned by safety_witness S2.
Sightings: 1 creation + control arm (every TimedWeave toy would hit it).

### P-004 — UPDATE (Gate 8 confirms): enforced containment unreachable
Confirmed at Gate 8: with `zen-isolation` absent from the install export, a
stranger-consumer Workshop has NO road to the enforced-containment tier. The
safety view says so plainly instead of painting a shield. Classification
firms up as PACKAGE/EXPORT PRESSURE (the machinery exists; the export
boundary withholds it — deliberate per the Loom's own comment, priced here:
the "move anything into safety with a single choice" promise cannot be kept
from outside).

### P-005 — VISION AMBIGUITY (pre-registered at Gate 0, to be tested)
"Wire ideas without writing code" meets a substrate whose visual vocabulary is
deliberately tiny (`SurfaceText` slots; canvas "a later phase"; SDL window
output-only; no SDL input Reader). The promise admits at least two readings:
(a) the Workshop invents its own visual/canvas vocabulary locally (Workshop-owns
-experience reading), or (b) the substrate is expected to grow one (Zengine
package reading). The toys will vote; neither is assumed.
