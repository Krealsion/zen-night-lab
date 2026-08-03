# REDISCOVERY — this Workshop vs. the six-application marathon

Written **after** the embargo lifted (all gates attempted, garden built,
cold-user pass recorded, ctest 2/2 green). The Night Two notebook was not
consulted while building; this file is the first comparison.

**Contamination declared up front.** At Gate 0 I recorded that
`marathon/README.md` — which I was told to read for METHOD — contains one
conclusion sentence: that five of six Night Two projects wanted *role-holding
provenance*. I could not unread it. Everything below about provenance is
marked **TAINTED** and given no credit.

---

## 1. Independently rediscovered

### The sender cannot see the fate of its own send — CONFIRMED, from a different door
Night Two's **Blocker 2** (3/6 projects, medium severity): a send to an unheld
role is refused and the sender is told nothing; only a host tap sees it. Their
path in was `send_to_role` to a role nobody held.

Mine was a **grant mistake**: at Gate 1 an under-granted native `TimedWeave`
was silently inert — its `EnsureTimer` was `CapabilityDenied` at every
delivery, invisible to the weave by law, and I had no tap installed. The
symptom was "nothing happens", and I diagnosed it by reasoning rather than
observation. It cost me the first real hour of the marathon and it is the
reason Gate 2's inspector exists at all (notebook §2, P-007).

Same seam, two unrelated approaches, two different first-consequences. Their
applications built **clocks** to work around it (watchdogs, sweeps, patience);
my Workshop built an **inspector** — because a making-tool's job is to show
the user what happened, not to survive it. Strong independent confirmation,
with a new consequence attached: *for a tool whose product is legibility, this
seam is not a workaround cost, it is the thing you must build first.*

### Minted identity needs a surviving namespace — ADJACENT, not identical
Night Two: 3 sightings, 2 of them defects; the rule extracted was *"the
identity is per-incarnation; the namespace must not be."*

I hit a neighbouring shape (P-010): the pond addresses eight instances of one
artifact as roles `pond.fly.1..8`, because a poke needs an address that
survives and parts have no other durable one. That makes the **role namespace
do double duty as an instance namespace by convention**. Their defect was
*two authors of one namespace*; mine is *one namespace serving two purposes*.
Related family, genuinely different fault. Two toys, so it stays LOCAL.

### Refusals-are-directions is load-bearing — CONFIRMED and extended
Night Two praised that every substrate refusal is inspectable by its own name.
I rediscovered it as a *teaching* surface: `explain.hpp` mechanizes the
diagnostics table, and the cold user then explained a `NotAccepted` correctly
using only what the Workshop showed it, without reading code. Their finding
was "the operator is sent to the right fix"; mine adds "a stranger can be
*taught the model* by the same text."

---

## 2. No longer present

### Blocker 1 — role-holding provenance — **CLOSED, and I consumed the fix without feeling the wound** (TAINTED)
Night Two's only CORE-DESIGN blocker, 5 independent sightings, worst case *"a
player leaves for an attacker's server."* In current Zen it is **law**: MSG-07,
`mail.as_role(...)` / `authored_from_role(...)`, historical and immutable,
with ABI v5 dynamic parity and fail-closed across the isolation pipe.

The honest account of my experiment: **I never independently re-derived the
need.** My Workshop has no office making trusted claims to a receiver who must
act on them. What I did instead is *rest on the fix* — the inspector's I2
witness pins that a role-holder's authenticated answer arrives as **personal
speech** with an empty `authored_role`, and that the Workshop must never fill
that field in from current membership. That witness is only meaningful because
the distinction exists; a year ago it would have been unwritable.

So: not a rediscovery. A **consumption**, and a demonstration that the closed
blocker's semantics survive contact with a naive new consumer. Tainted anyway.

### Night One's answer-seam and `TimedWeave` activation — already closed before my pin
Both fixed before `61b2915`. I built a native `TimedWeave` (the governor) and
a dynamic one (the lamp) and never met either.

---

## 3. Not encountered — and this is the most interesting section

**`describe-then-hand-over` — 6/6 in Night Two, 0/1 here.** Their unanimous
finding, the one they called *"a substrate gap wearing a pattern's clothes."*
My Workshop never wrote it. Not once.

The reason is structural, not luck: **I never performed a verified replacement
of a service holding work in flight.** My live alterations were
reload-in-place (state rides the gate — nothing to hand over) and deliberate
hard swaps where discontinuity was the *point* (Gate 3's A3 registry swap
exists to show the successor's memory is honestly empty). A Workshop organ has
no obligations to a caller; a download manager does.

Same for **the promise/responsibility book** (3 consumers there, 0 here) and
**order/menu resolution** (3 there, 0 here).

**What that says, carefully:** an entire application class — the tool that
makes things — exercises a nearly disjoint region of the substrate from the
service-with-obligations class. Night Two's top-ranked errand #3 ("a
`describe`-side ceremony for prepared replacement") gets **no additional vote
from this marathon**, and should not be read as weakened by that: it is
evidence that the two portfolios test different halves.

**`PreparedReplacement` itself: 178 facade calls there, zero here.** The
substrate's most carefully built ceremony went entirely unused by a whole
Workshop. That is not a criticism of it — my needs were `reload_from` (same
contract) and `SwapWeave` (contract change, continuity unwanted). Worth
recording plainly: *the verified-successor ceremony is for services, and a
making-tool may never touch it.* Gate 4's remaining incompleteness (a
contract-CHANGING code edit) is exactly where a Workshop would finally need it.

**Agreements by absence:** activation-sequence owner (0 there, 0 here) and
outcome-observation ergonomics (0 there, 0 here) — though mine is weak
agreement, since I never drove a transaction at all.

---

## 4. Contradicted

**Nothing.** I looked for a contradiction and will not manufacture one. The
closest thing to tension is the `PreparedReplacement` divergence above, and
it is a scope difference rather than a disagreement: both portfolios find the
handle fits its domain, and mine adds that the domain has a boundary.

---

## 5. Newly discovered — pressure the application marathon never exercised

Six headless services never had to show a human anything, hand a creation to a
stranger, or make a safety promise. Everything here is new ground.

1. **P-011 — the silent seam (CORE PRESSURE, reproducer filed).** This
   *sharpens Night Two's Blocker 2 into something worse.* They wrote: the
   sender is not told, "only a host tap can see it." I found a case where
   **the tap sees nothing either**: a loaded weave's emission whose schema
   nobody registered vanishes with no BusEvent, no journal entry, no
   recipient — while a native weave's identical intent refuses loudly as
   `NoSuchTarget`. The observability floor differs by tier. Found by denying
   a declared capability at Gate 8; `repros/core/silent-seam-emission/`.
2. **P-004 — the enforced-containment tier is unreachable from the exported
   surface.** `zen-isolation` is not in Loom's install export, so a
   stranger-consumer Workshop cannot offer the vision's *"move anything into
   safety with a single choice."* The safety view says so instead of painting
   a badge. Never surfaced by Night Two because no application there made a
   safety promise to a person.
3. **P-003 — Zengine has no consumption story.** No install, no export, no
   documented consumer path; consuming it means hand-copying headers and
   binaries (Night Two's vendor directory did the same thing, but as a lab
   convenience rather than a finding). My Gate 7 made it structural: you
   cannot ship a creation whose services you cannot name a version of.
4. **P-005 / P-006 — the visual vocabulary ceiling.** `SurfaceText` slots are
   the whole canvas; the general canvas vocabulary is explicitly "a later
   phase"; the SDL window is structurally output-only in V1. The pond FOUGHT
   this (a pond wants a canvas, not a glyph row). The entire "wire ideas /
   see the running program" axis of the vision is untested by Night Two.
5. **P-009 — the migration trigger has a live claimant.** Loom's known-seams
   names "the first persisted value that must evolve" as a deferred trigger.
   `ProjectSpec` went v1→v2→v3 in a single day, each step hand-rewriting every
   project file, because the strict gate refuses old files loudly (correctly).
   The Workshop is that first claimant.
6. **P-012 — `content_id` never varies (found by the cold user).** The compat
   envelope carries the *schema's* content-id; five different project files
   print the same value. Correct by GATE-04's definition, actively misleading
   in a value envelope. It made a stranger distrust its own edits.
7. **P-013 — a bundle's manifest is not fingerprinted.** Artifact bytes are;
   the manifest naming them is not. Renaming a shared creation is free, and
   the import still says "fingerprints VERIFIED" (true, and adjacent enough to
   mislead). My design gap, found by the stranger.
8. **P-014 — witnesses coupled to user-editable content.** A cold user's
   legitimate edit to `lamp.cpp` broke my `heights_witness`, which pinned the
   glyph literally. When the users' surface *is* the test fixture, witnesses
   must pin behavior. New class of problem for a making-tool.
9. **Live alteration was TTY-only.** The headline promise had no scriptable
   path; piped keys were silently swallowed. Fixed in the bounded pass
   (`--poke SEC:role.field=value`). No analogue in a headless portfolio.

---

## Method note worth carrying forward

Night Two reported that its mutation harness lied in three new ways. Mine
found something gentler and useful: **building a tripwire improved the
architecture.** Canary #9 needed proof that a recursive Workshop operation
went through the public door — which required a durable record of steward-door
deliveries, which is how the inspector's `doors` ring came to exist. The
general ring was being flooded by beat traffic within one virtual second, and
nobody had noticed. The canary found that, not the tests.
