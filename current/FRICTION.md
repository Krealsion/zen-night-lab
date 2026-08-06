# FRICTION — current-era Night Lab

Written in the order things happened, from first contact.

**Sightings nominate. They do not authorize.** Nothing here is a request for a
Loom change, and the count matters more than the complaint: one-off confusion
is not repeated ceremony, and repeated ceremony is not missing truth.

Categories used below: **application** (we got it wrong or the domain is just
like that) · **Night Lab** · **Loom** · **package boundary** · **documentation**
· **environment**.

---

## F-01 — A freshly loaded service has claimed nothing, so its liveness signal is absent for a window

**Experiment:** `signal-box`

**What the application was trying to do.** Use the interlocking's office Sense
claim as the signal box's panel lamp. An office claim is erased when the office
becomes unheld, so `latest_from_office<BoxStatus>("interlocking")` reading
`NoClaim` is exactly "there is no signal box" — an exact, synchronous liveness
signal that costs no bus traffic and needs no timeout. That is a genuinely nice
property and the application leaned on it hard.

**What was surprising.** A weave that has just been loaded has claimed nothing,
so the panel is dark while the equipment is standing right there in service.
"No box" and "a box that has not spoken yet" are the same reading. The
application deadlocked on the very first tick of its very first run: the
signaller would not move a train without a panel, and nothing was ever going to
light the panel.

**The same thing happened again one level down, independently.** A track circuit
that has never had a train on it has never claimed either — so the interlocking,
applying its own (correct) rule that absence of evidence is not evidence of
clearance, refused every route over every section for the whole shift. The first
green run of this application was preceded by two separate instances of exactly
this shape.

**Workaround.** A domain commissioning message, `PutInService`. The host proves
the track circuits before the first train; the signaller rings the box whenever
the panel is dark, and if anybody is home the panel lights on the next tick.
Both are real railway practice, so the workaround cost the application nothing
it would not have wanted anyway — which is part of why this is a sighting and
not a complaint.

**Whose.** **Application**, mostly. There is a **documentation / package
boundary** edge worth recording precisely, without asking for anything:
`docs/guides/dynamic-weaves.md` teaches `kernel.load(name, path, role)` as the
consumer path, and that path delivers nothing to the loaded weave. `zen.Activated`
exists and is exactly "a new code incarnation committed at this address", but it
is emitted by the kernel's *control door* (`kernel/control.hpp`) and by prepared
replacement — not by the direct `Kernel::load` call the guide shows. A consumer
following the guide gets no first moment.

**Sightings.** 2, independent, within one application (the box and the plant).

**Blocked the experiment?** No. It stopped the first run and was fixed in the
application in one edit.

**Classification.** Repeated ceremony, with a possible missing-truth question
behind it (*may a weave claim at mount?*) that this experiment does not answer
and does not ask for.

---

## F-02 — The sender cannot observe send fate — and a Sense removed the workaround this normally forces

**Experiment:** `signal-box`

**What the application was trying to do.** Know whether a route request reached
anybody, so a train is never left standing on a request that went nowhere.

**What was surprising.** Nothing — this is a documented, named Loom seam, and
the application hit it exactly where the documentation says it will. Measured
directly as a labelled control: a request into the emptied `interlocking` office
produced `NoSuchTarget on RouteRequest` on the tap, and the sender was told
nothing at all.

**What is worth recording is the opposite direction.** The standard authored
answer to this seam is a watchdog or a timeout. This application did not need
one, because reading the office claim *before* sending answers the same question
earlier and exactly. The signaller keeps a `lost` counter for a request that
never came back; across every run of this experiment it has stayed **0**, and it
survives only as an invariant check rather than as a mechanism.

**Whose.** **Loom** — known seam, already documented in
`docs/reference/known-seams.md`, already priced by six historical Night Lab
applications.

**Sightings.** 1 (this experiment), confirming an existing seam from an
unrelated domain. It nominates nothing; if anything it slightly *reduces* the
pressure, because Senses give one class of consumer a cheaper answer than the
watchdog.

**Blocked the experiment?** No.

---

## F-03 — A replaced dynamic service keeps nothing, and only the application can notice

**Experiment:** `signal-box`

**What the application was trying to do.** Take the signal box out of service
and put it back — which is the whole reason the interlocking is a `.so` in this
application rather than a class in the host.

**What was surprising.** Not the fact — "continuity is authored" is written
down — but how cleanly the domain split. The **plant remembered** where every
train was, because the track circuits are separate weaves nobody touched. The
**box remembered nothing**, so a route that was legitimately set before the
box came out was simply gone afterwards, with a train standing on it.

**What made it tractable.** A Sense reading carries who made the claim, so
"the panel is lit, but by a different box" is directly readable off
`SenseAuthorship::author` / `author_life` with no cooperation from the host and
no extra message:

```text
t13  NEW BOX    was weave 8 life 1, now weave 9 life 1
t13  RE-SET     DOWN for 2B15 (the new box never knew about it)
```

**Whose.** **Application.** This is the documented position — what crosses a
discontinuity is a domain decision — and the domain filled the hole itself, in
about fifteen lines. Night Two's six applications carried six different things
across a replacement; this is a seventh, and it is different again (a route
authorisation, re-requested rather than transferred).

**Sightings.** 1. Nominates nothing.

**Blocked the experiment?** No.

---

## F-04 — A native weave that holds an office cannot use `mount()`

**Experiment:** `signal-box`

**What the application was trying to do.** Register seven native weaves, each
holding an office, so that everything in the application is addressed by role
rather than by id.

**What was awkward.** `loom::mount<T>()` and `loom::mount_granted<T>()` do not
take a role. `Switchboard::register_weave(weave, grant, role)` is the only
binder — and it is the raw door, so it does not do the `zen_set_self()` wiring
that `mount()` does. Every office-holding native weave therefore needs a local
re-implementation of `mount_granted` that adds one argument:

```cpp
template <class W, class... Args>
loom::WeaveId mount_office(loom::Switchboard& bus, loom::Grant grant,
                           const std::string& office, Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant), office);
    raw->zen_set_self(id);   // the line it is easy to forget, and silent to omit
    return id;
}
```

Forgetting `zen_set_self` is the hazard: the weave still registers and still
receives, and its sends carry an unset self id.

**Whose.** **Loom**, authoring ergonomics — not missing truth. Everything
needed is public and the raw door is honest about being raw.

**Sightings.** 1 experiment, 7 uses within it. One instance of a known-shaped
convenience gap is not repeated independent friction.

**Blocked the experiment?** No.

---

## F-05 — `wsl.exe -- bash -c '<script>'` from Git Bash silently loses variable expansion

**Experiment:** none — the lab's own Windows environment.

**What was being attempted.** Running a multi-step build script in WSL from the
Windows side.

**What happened.** `wsl.exe -d Ubuntu-22.04 -- bash -c 'A=hello; echo "[$A]"'`
prints `[]`. The single quotes do not survive the Git Bash → `wsl.exe` argument
handoff, so every shell variable expands to nothing before the Linux side sees
it. A script that assigns a scratch path and then `rm -rf`s it is one keystroke
away from being genuinely dangerous.

**Workaround.** The same one ZNL-R adopted after its own accident in this
territory: write every multi-step operation to a script *file* with fully
absolute paths, strip CR, and run it as `wsl.exe -d <distro> -- bash <file>`.
Never `$PWD`.

**Whose.** **Environment.** Nothing to do with Zen.

**Sightings.** 1 here; adjacent to ZNL-R's `$PWD` accident, which is the second
time this boundary has produced a hazard.

**Blocked the experiment?** No.

---

## Not friction, recorded so the ledger is not mistaken for a list of complaints

The consumer path itself produced **none**. From `git clone` to a running
multi-weave application with a dynamically loaded participant:
`find_package(loom REQUIRED)`, three imported targets, one
`loom_weave_build_contract(...)`, one `if(NOT TARGET loom::kernel)` gate. No
runtime library discovery, no asset copying, no version skew, no
`LD_LIBRARY_PATH`, and nothing to abstract. Both refusals the application
actually met — `NoClaim` on a Sense read and `NoSuchTarget` on a send into an
emptied office — named the thing that was wrong precisely enough that each F-01
instance took one edit rather than a debugging session. The application was
never at any point unsure *why* something had not happened.
