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

## F-06 — The preparation ask is the coordinator's gated speech, and getting that wrong spends the transaction's only conversation

**Experiment:** `prompt-corner`

**What the application was trying to do.** Brief the relief DSM: hand the
sealed candidate the outgoing DSM's exact book position, through
`PreparedReplacement::ask()` from inside the coordinator's handler, exactly as
`docs/guides/replacing-a-service.md` shows.

**What was surprising.** Nothing arrived, and `ask()` said it was fine.

`upgrade.ask(brief)` returned `TxnResult{ok = true}`. The tap said
`CapabilityDenied on BriefTheRelief`. The coordinator's grant listed the two
shapes it sends to the caller and nothing else, and the ask is **the
coordinator's own speech**, stamped with its id and gated against its grant at
delivery — which the substrate says in as many words at
`src/switchboard/switchboard.cpp` in `ask_candidate_to_prepare`:

```text
THE ASK IS THE COORDINATOR'S SPEECH, and it is sent exactly as the
coordinator's own speech would be: stamped with its id, gated against its
grant at delivery [...]
```

That is correct, deliberate, and the guide does not mention it. The guide's
example (`upgrade.ask(PrepareStorage{...})`) reads as the *handle* speaking, and
the handle is host-owned, so a first-time author does not think about a grant at
all.

**Why it is worse than an ordinary missed grant.** `ask_candidate_to_prepare`
sets `conversation = Conversation::Open` when it *queues*, not when it delivers.
So after a refused delivery:

```text
the candidate was never asked anything
the transaction is still Preparing
a second ask returns PreparationAlreadyAsked
```

The transaction cannot be repaired. It can only be aborted or left to exhaust
its budget, and the candidate — which is loaded, sealed and consuming a slot —
never gets its one chance to answer. This is the send-fate seam landing on the
one conversation in the substrate that cannot be re-opened.

**Workaround.** One rule on the coordinator's grant. It has to be
`allow_to_any(shape, version)` rather than the tighter
`allow(shape, version, candidate_id)`, because the candidate's id does not exist
when the coordinator is mounted.

**Whose.** **Loom**, and specifically **documentation placement** — the
mechanism is right and is written down in the source; it is absent from the
guide a consumer follows. The unrepairable-conversation consequence is a
substrate fact worth naming rather than a defect claim.

**Sightings.** 1 (this experiment, first run). It is the first Night Lab
sighting of the preparation ask at all.

**Blocked the experiment?** No — one line, once the tap was read. It would have
been a long afternoon without the tap, because every visible return value said
success.

---

## F-07 — A prepared candidate cannot be given a grant, so a successor is wider than its incumbent

**Experiment:** `prompt-corner`

**What the application was trying to do.** Give the relief DSM exactly the
authority the outgoing DSM had. In this theatre that is a short, real list:
give standbys to three departments, say GO to those three departments as the
office, and answer the company stage manager. A DSM may not address the house
and may not publish, and neither should a DSM on their second night.

**What was surprising.** The documented path cannot express it.
`PreparedReplacement::start()` loads the candidate through
`Kernel::load_candidate(name, path, coordinator)`, which takes **no `Grant`** —
there is one overload and it has no grant parameter. It delegates to the
three-argument `Kernel::load`, whose default is `Grant{}.allow_any()`
(`src/kernel/kernel.cpp`). The ordinary `load` has a four-argument overload that
takes a grant, added deliberately at R2E-0 so a host can say how far it trusts a
loaded artifact; the candidate path does not.

So a host that narrows its incumbent, and then replaces it through the
documented handle, **silently widens the service**.

**Workaround, and it works.** The three primitives underneath are all public,
and composing them gives the successor the incumbent's exact grant:

```cpp
kernel.load("dsm-b", path, /*role=*/"", dsm_grant());   // narrow, no role
bus.seal_weave(relief.id, csm_id);                      // sealed to the coordinator
upgrade.start_existing({op, coordinator, role, relief.id, budget});
```

`start_existing` documents that it owns no artifact cleanup, which is the
trade: the caller brought the candidate, so the caller keeps it if `begin`
refuses. That is a fair price and this application pays it.

**Whose.** **Loom**, authoring ergonomics with an authority flavour. Not missing
truth — everything needed is public and the composition is four lines — but the
convenient path and the safe path disagree, and the convenient path is the one
the guide teaches.

**Sightings.** 1 (this experiment). It is the first Night Lab consumer to give
a *replaceable* service a narrow grant; `signal-box` gave its loaded weave a
narrow grant but never replaced it.

**Blocked the experiment?** No.

---

## F-08 — `commit(sequence)` still wants a number nobody in the domain owns

**Experiment:** `prompt-corner`

**What the application was trying to do.** Commit the handover.

**What was awkward.** `commit(std::int64_t sequence)` needs a monotonic number
and this theatre has no lineage that wants counting. "How many times the book
has changed hands tonight" is nearly meaningful and is not what the field is
for. A literal `1` was passed.

**Whose.** **Loom**, the documented `activation-sequence ownership` watch item
in `docs/reference/known-seams.md`, which records two sightings (the Codex Rule
Garden and R2E-0's Handoff Garden) whose only meaning was *"the API needs a
number"*.

**Sightings.** This is a **third, from an unrelated domain and an independent
consumer**, and it has the same meaning as the first two. The seam's own stated
trigger — *"a consumer for which the sequence carries domain meaning"* — has
still not fired, and this experiment does not fire it. What the third sighting
adds is only that the shape reproduces outside the two applications that found
it.

**Blocked the experiment?** No.

---

## F-09 — A retired incumbent is concealed by the rule written to conceal candidates

**Experiment:** `prompt-corner`

**What the application was trying to do.** Nothing, at the point this happened.
The fly floor had taken a standby away with it (`defer_answer`) to go and check
the deck. Between the ask and the answer, the DSM who asked was replaced. When
the deck reported, the fly floor answered everyone who had asked it — including
the DSM who had left.

**What was surprising.** The tap said:

```text
NoSuchTarget on StandingBy
```

and the host, in the same breath, could see:

```text
outgoing DSM after it   alive=yes   artifact=Sealed
```

Both are true and both are deliberate. On commit the substrate seals the
incumbent *for retirement* (`deliver_admission`: *"The incumbent is sealed FOR
RETIREMENT, to the same coordinator: it stops receiving production entirely"*),
and inbound delivery to any sealed addressee is refused with a reason chosen on
purpose (`deliver_one`: *"INBOUND, and deliberately indistinguishable from an
unregistered id: the world must not be able to discover that a candidate exists"*).

The concealment is exactly right for a **candidate**. A retired **incumbent** is
the opposite situation — the world already knows it exists, because it was the
service two beats ago — and it inherits the same disguise. An operator reading
`NoSuchTarget` on the tap goes looking for a weave that is not there; the weave
is there, alive, registered, and sealed.

`spend_deferred` itself returned a **valid ticket**: the answer was legitimately
queued (the requester's incarnation was unchanged — it was replaced, not
reloaded) and refused later, at delivery, by the seal. So the two halves of the
story are in different places, which is the ordinary send-fate seam and not new.

**Workaround.** None needed. The relief **re-gives every standby it inherits**,
which a DSM taking the book does out loud anyway, and which is the only way this
incarnation could ever hear "standing by" for a conversation the previous one
opened. The re-give is load-bearing rather than good manners, and the run proves
it: the refusal happens, and the fly cue goes anyway.

**Whose.** **Loom**, diagnostics — the behaviour is correct and the *reason* is
imprecise for one of the two situations it covers. Recorded, not requested: the
substrate cannot say "retired" without giving the world a way to distinguish a
sealed candidate from an unregistered id, which is the property the rule exists
to protect. Whether a retirement seal deserves its own inbound reason is a
question this experiment asks and does not answer.

**Sightings.** 1. Nominates nothing. Adjacent to, but distinct from, the
documented `continuity is authored` position: what did not cross here was not
*state*, it was a **conversation**.

**Blocked the experiment?** No.

---

## Second sightings of ledger entries ZNL-00 already opened

**F-04 — a native weave that holds an office cannot use `mount()`.** Second
independent experiment, five uses within it (three departments, the deck, the
company stage manager). `prompt-corner` wrote the same six-line `mount_office`
helper `signal-box` wrote, for the same reason, without consulting it — the two
experiments share no file. **Two independent consumers now, twelve uses.** Still
authoring ergonomics rather than missing truth, and still not a request.

**F-02 — the sender cannot observe send fate.** Confirmed again, and this time
it *cost* something rather than being routed around: F-06 is that seam landing
on the preparation ask, where the conversation cannot be re-opened afterwards.
Second independent consumer.

**F-05 — `wsl.exe -- bash -c '<script>'` loses variable expansion.** Reproduced
exactly at the start of this phase, in both quoting styles
(`bash -lc "A=hello; echo [\$A]"` and `bash -lc 'A=hello; echo "[$A]"'` both
print `[]`). Third sighting of this boundary across three phases. Same
workaround: script files, absolute paths, never `$PWD`. Not Zen's.

---

## Not friction, recorded so the ledger is not mistaken for a list of complaints

### From `signal-box` (ZNL-00)

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

### From `prompt-corner` (ZNL-01)

**The consumer path produced none a second time**, from an unrelated domain and
with no reference to the first experiment's build file: the same five lines of
CMake, the same one flag, nothing to abstract. Two independent consumers have
now found nothing to complain about in the installed package, which is worth
recording precisely because it is the part of the substrate that is easiest to
get quietly wrong.

**The replacement ceremony was authorable without reading a report.** Everything
this application needed to drive a prepared replacement came out of the pinned
source and `docs/guides/replacing-a-service.md`, and the guide's division of
labour — the candidate authentically supplies the domain answer, the coordinator
maps it to Ready or Refused, nothing commits by itself — is what the application
actually wanted. The friction above is around the edges of that path, not in it.

**Every refusal this application met named the right thing but one.** The
candidate's own `CandidateRefused`, the department-level domain refusals, and
the `CapabilityDenied` behind F-06 all sent the author to the right place
first time. The exception is F-09, where the reason is true of the mechanism and
misleading about the situation.
