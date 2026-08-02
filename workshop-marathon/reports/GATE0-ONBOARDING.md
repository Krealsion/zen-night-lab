# GATE 0 — Can Zen introduce itself?

Cold onboarding, 2026-08-02. Sources actually consulted, in order: `zen-vision.md`
(full) → `Loom/docs/CONTEXT.md` → `terminology.md` → `Loom/AGENTS.md` →
`docs/README.md` → all six guides → all nine reference pages → all six law files →
`Zengine/AGENTS.md` → `Zengine/README.md` → `Zengine/docs/*` (Timer corpus).
Source consulted only twice, recorded per-question below. Embargo note: CONTEXT.md
routes cold readers toward `evidence/night-lab.md`; declined here (embargoed until
postmortem), which means this experiment slightly *understates* discoverability — a
real stranger would read it.

## The twelve questions

**1. What is a Weave?** — *easy from guide.*
A bus participant: a struct of state with a shape, `Accept<...>` doors,
`Emit<...>` + a grant, `on(Shape, Mail&)` handlers. "Whole and part at once";
native or loaded from a `.so`, indistinguishable on the bus once admitted.
(mental-model.md + terminology.md, minutes.)

**2. How do I make one?** — *easy from guide.*
`ZEN_SHAPE` structs; `class X : loom::WeaveBase<X, State, Accept<...>, Emit<...>>`;
`loom::mount<X>(bus)` (or `mount_granted`). The guide's program is a compiled
example (`examples/heartbeat_woven.cpp`) — the guide cannot rot silently.

**3. How do Weaves communicate?** — *easy from guide.*
`mail.send(id)` / `send_to_role(role)` (resolved at delivery — survives
replacement) / `publish` (fan-out chosen at enqueue). Asks carry one answer
authority (`answer` / `defer_answer`, receiver checks `answers_ask()` — Loom's
word, not a payload claim). Office speech is a separate explicit act
(`as_role(...)`, MSG-07). Refusals are named observable events, never silence.

**4. How do I run one?** — *easy from guide.*
The host owns a `Switchboard`; nothing runs until `pump()` (single-threaded FIFO,
non-reentrant, MSG-01). Dynamic: `Kernel::load(name, path, role)`, then pump.
Whole-app shape: the snake host pattern — queue a boot list; `pump()` IS the game;
the Timer service starts time when loaded (TIMER-02).

**5. How do I see refusals?** — *easy from guide.*
`bus.add_observer` tap (`BusEvent` with reason, target, stamped sender, schema,
life/incarnation diagnostics); the reasons table in diagnostics.md reads as
directions to fixes; host-side journal `bus.outcome(ticket)` (1024 ring). Sender
cannot observe its own send's fate — a documented seam, not a surprise.

**6. How do roles differ from identities?** — *easy from reference + laws.*
Four facts never conflated: **sender identity** (bus-stamped, unforgeable, MSG-02)
· **role-addressed destination** (resolved at delivery, MSG-04) · **role
membership** (live `role_holder` lookup) · **office authorship** (historical
stamped delivery fact, only from explicit `as_role`, MSG-07). Holding is
necessary, never sufficient.

**7. How do I replace something live?** — *easy from guide (outstandingly so).*
Same contract: `kernel.reload_from` (validate-then-commit behind a stable id).
Different contract, verified, no gap: `loom::PreparedReplacement` — sealed
candidate → domain ask/answer → coordinator maps to Ready → commit *schedules* →
one admission dispatch moves topology and delivers activation as one event.
Continuity is authored, never automatic (PR-09). Legacy graceful swap (the
letter) preserves work but verifies nothing; ceremonies disjoint. Crash paths:
`kill` / `reload` (budgeted revival) / `swap_state`.

**8. What does Zengine already give me?** — *easy from README (one long page).*
Timer (full docs corpus: ordered `EnsureTimer` with honest receipts, `TimedWeave`
authored rhythms, remaining-duration continuity); Input (5 locked shapes, sole
producer weave, POSIX/Win32 backends); Surface (`SurfaceText{slot,text}` intent,
replaceable Skins holding `zengine.skin` — two TUI + one real SDL3 window);
snake as a five-weave composition demo; activation helpers (`ActivationCursor`).
*Caveat:* Input/Surface have no reference pages — "their sections are their
reference" (repo README as docs).

**9. What visual/input/time facilities exist?** — *easy from README + required
source for the full truth.*
Visual: slot-text vocabulary only; `SnakeVisual` is a named V1 coupling; **the
general canvas vocabulary is explicitly "a later phase."** The SDL window is
structurally output-only in V1 (not-focusable; terminal is the one ear; SDL
Reader is a named unbuilt follow-on). Time: complete. Input: keys real on both
backends; mouse shapes exist (Win32 console records; POSIX terminal keys-only).
Also in-tree but build-only: `zen-ui`, `zen-ui-pixel`, `zen-ui-sdl`,
`zen-console`, `zen-tui`, `zen-bridge` — found via CMakeLists, not docs.

**10. Which parts are intentionally not exported?** — *class documented; exact
set required source.*
Zengine's README names the class and the why (console, TUI, bridge, UI trio, SDL
skin — "each joins the export when a hosting consumer appears"). The exact
install list required reading Loom's CMakeLists: exported = `loom::core`,
`loom::switchboard`, `loom::kernel` (+warnings/sanitize). **Not exported:
isolation** — noteworthy for the safety story (Gate 8 will price it).

**11. How would a stranger consume Loom?** — *easy from README/AGENTS.*
Build + `cmake --install` → `find_package(loom)` → the three targets; gate on
`if(TARGET loom::kernel)`; weave libs need `-fno-gnu-unique`; WSL canonical;
Windows kernel is opt-in dev/demo, truth-pinned.

**12. How would a stranger consume Zengine?** — *STILL UNCLEAR; required source.*
Zengine's CMake has **no install/export**. The README teaches building Zengine
itself, not consuming it. The evident real answer (and what Night Lab's build
harness did): copy headers + prebuilt `.so` artifacts by hand. "Take it, replace
piece by piece, or ignore it" is philosophy with no packaged mechanism yet.

## Verdict

**GREEN, with named debts.** A competent cold builder can absolutely derive
enough to begin: the guide layer is short, honest, compiled-example-backed, and
the laws state what they do NOT mean — which prevented at least four wrong
assumptions during this read (commit≠committed, holding≠authoring,
replacement≠continuity, correlation≠authentication). The debts are specific:
Zengine's consumption story (Q12), Input/Surface reference debt (Q8), the
unexported-surface discovery path (Q10), a stale "v4" in KERN-04, and no CONTEXT
row for the bridge. All recorded in PRESSURE.md. Nothing blocks starting to make.
