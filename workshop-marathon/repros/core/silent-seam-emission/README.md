# repro: a loaded weave's emission can vanish with no observable trace

**Status: CORE PRESSURE (with a DOCUMENTATION FAILURE edge). Found at Gate 8
(NL3), pinned by `tests/test_workshop.cpp` `safety_witness` S2.**

## What Workshop promise was being attempted

"Dare safely": deny a creation's declared need (`--deny zengine.timer`) and
show the consequences as VISIBLE refusals — "a refusal is a named observable
event, never silence" (MSG-05 spirit; diagnostics guide's headline).

## What truth could not be expressed

When the lighthouse lamp (a loaded TimedWeave) is activated on a bus where no
timer service was ever loaded, its binding sends `EnsureTimer` to the
`zengine.timer` role — and that emission produces **nothing anywhere**:
no BusEvent, no journal entry, no refusal of any reason. It is rejected at
the library/schema seam, because `EnsureTimer` was never registered (only
accept-sets and state schemas register; the denied service was the sole
registrar of its own vocabulary), and a dynamic send returns no ticket by
design.

The identical intent from a NATIVE weave (typed send, no registry resolution)
is a clean, observable `NoSuchTarget` — witnessed in the same test. So the
observability floor differs by tier: native reach fails loudly, loaded reach
can fail silently.

## Current mechanisms tried

- tap (`add_observer`): sees nothing for the loaded emission.
- journal: dynamic sends carry no ticket (documented, KERN/ANS-06).
- recipient-side observation: there is no delivery to observe.
- the sender being told: the shim's send is fire-and-forget; the TimedWeave
  binding does not (and could not) surface a seam rejection.

## Workaround, and why it is insufficient

The Workshop can special-case "need denied → warn that vocabulary owned by
the denied service will vanish silently" (it prints a warning on --deny).
That is a GUESS about future silence, not an observation of it — exactly the
"workaround requires guessing current state instead of carrying fact" shape
that distinguishes core pressure from ergonomics.

## How many independent toys hit this

One creation (lighthouse under --deny) plus the control arm. Every TimedWeave
toy would hit it identically under a denied/absent timer.

## The narrowest missing truth

A seam-level rejection (unresolvable schema, in either direction) should be
an observable event on the host side — one more `BusEvent` kind or refusal
reason ("SeamUnresolved"?), carrying the claimed (name, version). Not a
ticket for dynamic senders (structurally out), not a change to registration
semantics — just: the tap should see it, because today nothing does.

## Note on documentation

`guides/dynamic-weaves.md` says delivery fate "is observed at recipients and
taps" — for this case, neither exists. The surface vocabulary's comment
("an ask that goes nowhere... rejected at the library/schema seam") is the
one honest breadcrumb, found only after the fact.

## Reproduce

Run the pinned witness: `workshop-tests` S2 arm, or read
`tests/test_workshop.cpp` (`safety_witness`, second block).
