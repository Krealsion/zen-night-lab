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

## F-10 — An answer refused for want of AUTHORITY is reported as if the grant were wrong, and the deferred door beside it says otherwise

**Experiment:** `records-committee`

**What the application was trying to do.** Nothing, at the point this happened.
A labelled control had the house circulate a ballot of its own to seat one, to
show that the tally cannot be reached from outside it. The member read the
ballot and voted, exactly as it would for a real one.

**What was surprising.** The tap said:

```text
CapabilityDenied on Vote  (from weave 3)  sender's grant does not permit this shape to this target
```

Weave 3 is seat one, whose grant is `Grant{}.allow_to_any("Vote", 1)` — and in
**this same run, under that same grant**, that same weave sent that same shape
six times and all six were delivered. The grant is not the problem and cannot be.

The real reason is that the ballot came from the host root, so there was no
requester to answer. The substrate says so itself, at `answer_as` in
`src/switchboard/switchboard.cpp`:

```text
// Three ways to have no authority, and each is a refusal of AUTHORITY —
// categorically distinct from the grant check that still runs afterwards on
// a legitimate answer:
//   - nothing is being dispatched, or the caller is not the weave being
//     dispatched [...]
//   - the request came from a root, so there is no requester to answer;
//   - this delivery's one answer is already spent.
```

and then reports all three as `RefusalReason::CapabilityDenied`.

**Why it is worth recording rather than shrugging at.** The vocabulary already
contains the right answer, and the enum says so in as many words:

```text
ForeignAuthority
  A lifecycle/answer authority [...] that is expired, ALREADY SPENT, or bound to
  a different conversation or incarnation. DISTINCT FROM CapabilityDenied on
  purpose (R2B-2): the sender's grant may be perfectly correct while the
  AUTHORITY DOMAIN is wrong, and reporting that as "you lack the grant" sends an
  operator looking in exactly the wrong place.
```

And three functions further down the same file, `spend_deferred_as` — the
*deferred* answer door — refuses the same category with `ForeignAuthority`, for
a foreign token, a missing or spent record, the wrong respondent, or the wrong
incarnation.

So the two doors disagree, and they overlap exactly: **an already-spent answer
is `ForeignAuthority` through the deferred door and `CapabilityDenied` through
the immediate one.** This is the hazard the enum's own documentation describes,
landing on the reason written to prevent it.

**What the caller is told is fine.** `answer()` returns an invalid Ticket, and
the source explains why that rather than a refusal ticket. The imprecision is on
the **tap and the journal** — the diagnostic — which is F-09's shape from
`prompt-corner`: a reason that is true of the mechanism and misleading about the
situation.

**Workaround.** None needed, and none written. This application never answers a
root delivery except deliberately, in a labelled control.

**Whose.** **Loom**, diagnostics. Not missing truth, not a defect in behaviour,
and not blocking — the refusal is correct, only its name is not.

**Sightings.** 1 (this experiment), measured. The `ForeignAuthority` half is
read from the pinned source rather than provoked: this experiment never
exercised the deferred door's refusal path.

**Blocked the experiment?** No.

---

## `ringing-chamber` opened no new entry, and that is the entry

**Experiment:** `ringing-chamber`

The fourth current-era application found **nothing new to complain about**. It is
recorded here, in the ledger's own numbering space, because an experiment that
produces no friction is a result and not a gap — and because a ledger that only
ever grows would eventually stop meaning anything.

What that is and is not:

- It is **not** that this application asked less of the substrate. It used
  publications, office authorship on both a publication and a claim, answers with
  correlation, a Sense read across six offices, a loaded artifact swapped for a
  second one mid-evening, and a forged host-root send — and every one of them
  behaved as documented on the first attempt.
- It is **not** that nothing was refused. Six kinds of thing were refused during
  the evening. Five of them are the **domain's** refusals: a ringer deciding a
  voice was not the conductor's, a ringer deciding a line answered no question it
  asked, a listener deciding a noise was not a bell, the conductor deciding a
  bell was not up, and the pricker deciding a row had already been rung. In every
  one of those the substrate delivered the message and the **recipient**
  discriminated, which is the shape ZNL-00's C-06 and ZNL-01's C-16 both found.
- It **is** that the only thing Loom itself refused all evening had to be
  deliberately forged (§ *notable non-friction* below), and that the author wrote
  no workaround for anything.

**Whose.** Nobody's. **Sightings.** Not applicable. **Blocked the experiment?**
No.

---

## Sightings, from `ringing-chamber` (ZNL-03)

**F-04 — a native weave that holds an office cannot use `mount()`. FOURTH
independent consumer.** `ringing-chamber` wrote the same local `mount_office`
helper the other three each wrote, for the same reason, without consulting any of
them — the four experiments share no file. **Four independent consumers now,
twenty-two uses** (7 + 5 + 2 + 8). This one has the most uses of any of them and
the plainest reason: every participant in a tower is a job rather than a person,
so *everything* here holds an office — six ropes, the conductor and the pricker.
The shape of the count is unchanged from ZNL-02's narrowing: the gap is in the
**native** mount helpers, and `Kernel::load(name, path, role, grant)` — which
this application also uses, for the method — has had the missing argument all
along. Still authoring ergonomics, still not missing truth, and still not a
request. The duplicate was left in place.

**F-01 — a freshly loaded service has claimed nothing. SECOND INDEPENDENT
CONSUMER OF THE SHAPE, AT NO COST.** The exact reading appears, and the sentence
it produces is `WE ARE NOT ALL HERE -- 4 (NoClaim)`. Whether a bell is up is a
standing fact about the bell, so it is a claim; a rope whose ringer has not yet
said anything reads `NoClaim`, and that is byte-for-byte the reading a rope with
nobody on it would give.

**What is different from ZNL-00 is that it cost nothing, and the reason is worth
recording precisely.** `signal-box` was *blocked* by this — its first run
deadlocked, because "no box" and "a box that has not spoken" needed different
responses and it could not tell them apart. Here they need the **same** response:
a conductor who cannot see that a bell is up does not go, and it makes no
difference whatsoever whether that is because the bell is down or because nobody
is standing there. The domain has one answer for both, so the ambiguity is not an
ambiguity.

So this is a second consumer of the *shape* and **not** a second consumer of the
*pressure*, and the two should not be added together. The honest summary after
four applications is: one consumer needed the distinction and worked around it in
one edit; one consumer met the same reading and did not need the distinction; two
consumers have no Senses at all and cannot see it.

**F-02 — the sender cannot observe send fate. NOT MET, for the first time.**
Three applications running, this seam had bitten every one. It has no surface
here, for two reasons that are both the domain's:

- **A bell expects no answer.** Every production message in this application is a
  publication, and a publication is not a request. There is nothing whose fate a
  ringer would want to know: it rang, and whoever was listening heard it.
- **A publication tells its sender how many heard it.** `Office::publish` returns
  `OfficePublication{authored, recipients}` — a real count, at the call, with
  "the office was refused" kept distinct from "authorized and nobody was
  listening". The directed doors return a `Ticket` that only the **host** can
  resolve (`Switchboard::outcome`), and a weave holds a `Bus`, not a
  `Switchboard`.

That asymmetry is worth naming because no previous current-era experiment
published anything and so none could have seen it: **the send-fate seam is not
uniform across the send verbs.** It is not a new complaint — a fanout count is
not delivery, and each of those deliveries is still independently gated
afterwards with nothing reported back. But a domain whose traffic is broadcast
gets meaningfully more back from Loom than one whose traffic is directed, and the
seam's own note does not distinguish them. **Recorded as a narrowing of F-02, not
as a new entry.** No consumer count changes.

**F-05 — `wsl.exe` argument handling. FIFTH sighting, and it CORRECTS the
workaround this ledger recorded.** ZNL-02 wrote: "invoke `wsl.exe` from
PowerShell rather than Git Bash". Measured this phase, from PowerShell:

```text
wsl.exe -d Ubuntu-22.04 -- bash -c 'A=hello; echo "[$A]"'   ->   []
```

Identical to the Git Bash behaviour. The shell on the Windows side is not the
variable, and the previously recorded workaround does not work. **The workaround
that does work is the other half of what ZNL-00 and ZNL-01 adopted**: write every
multi-step operation to a script *file* with fully absolute paths and no
variables that the handoff can eat, and run it as
`wsl.exe -d <distro> -- bash <absolute-path-to-script>`. Not Zen's, and now
recorded correctly.

**F-06, F-07, F-08, F-09, F-10 — no new evidence.** This application never
touches prepared replacement, never commits an activation, and never answers a
root delivery, so none of them has a surface here. **F-10 in particular remains
at one sighting**; nothing in this experiment tested it either way.

---

## Second and third sightings of ledger entries earlier experiments opened

**F-02 — the sender cannot observe send fate. THIRD independent consumer, and
this time the domain built its policy around it.** The secretary circulates a
ballot to each of five seats by role. A ballot to a vacant seat refuses
`NoSuchTarget` at delivery and the secretary is told nothing at all — measured
in `control-inquorate`, where two seats are empty and every one of the eight
ballots to them was refused on the tap while the secretary learned of none of
them. What is worth recording is what the domain did about it: **a circulation
closes on a date, not on a headcount**, and quorum is what makes a silent
vacancy survivable rather than fatal. That is real committee practice and it
cost the application nothing it would not have wanted anyway — the same shape as
F-01's `PutInService`. Three independent consumers have now met this seam and
none of them has been blocked by it; two of the three found the domain already
had an answer.

**F-04 — a native weave that holds an office cannot use `mount()`. THIRD
independent consumer.** `records-committee` wrote the same local `mount_office`
helper `signal-box` and `prompt-corner` each wrote, for the same reason, without
consulting either — the three experiments share no file. **Three independent
consumers now, fourteen uses** (7 + 5 + 2). Note the shape of the third: only
*two* uses here, because the five members of this committee are loaded weaves
and `Kernel::load(name, path, role, grant)` **does** take a role. So the gap is
specifically in the *native* mount helpers, and the dynamic door next to them
has had the missing argument all along. Still authoring ergonomics rather than
missing truth, and still not a request.

**F-05 — `wsl.exe` argument handling.** Reproduced once more at the start of
this phase, in a new form: a Git Bash → `wsl.exe` invocation mangled an absolute
`/mnt/c/...` script path into `C:/Program Files/Git/mnt/c/...`. Fourth sighting
of this boundary across four phases; the workaround is unchanged (script files,
absolute paths, and invoke `wsl.exe` from PowerShell rather than Git Bash). Not
Zen's.

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

### From `records-committee` (ZNL-02)

**The consumer path produced none a third time.** Same five lines of CMake, same
one `if(NOT TARGET loom::kernel)` gate, same one `loom_weave_build_contract` —
applied five times instead of once, which was the only difference and cost
nothing. Three independent consumers have now found nothing to complain about
in the installed package.

**Several participants of the same kind needed no group mechanism, and the
domain is why.** Loom has singleton offices and publications and no concept of a
peer group. An application with five members might have been expected to want
one. It did not: a committee has **seats**, a seat is a singleton that outlives
its occupant, and `send_to_role("seat.3", ...)` is exactly the sentence the
domain wanted to say. The membership list is the committee's own constitution
and belongs in the application. Recorded because the absence of friction here is
the interesting result — the substrate's shape and the domain's shape agreed
without either bending.

**The `answer_as_role` seam has now been looked at by a consumer that did not
need it.** `known-seams.md` records that no public door produces a combined
answer+office fact and that `answer_as_role` "waits for a consumer". A committee
vote is a plausible one: it is both the answer to a ballot and an act of a seat.
This application looked and **did not need it** — the secretary mints the ballot
number, so the ask's correlation already binds the vote to the record, the seat
and the round, and the documented obligation to "match the correlation against
your own outstanding ask" is the whole of what was required. The seat name never
travels in a payload and never needs to. One consumer declining to fire a
trigger is weaker evidence than one firing it, and it is still evidence.

**`commit(sequence)` was not reached.** This application never replaced
anything, so F-08 receives nothing from it either way.

**A loaded weave's declared state IS readable from the host, through the
ordinary gate.** This experiment initially assumed otherwise and was wrong.
`Switchboard::snapshot_bytes(id)` → `loom::parse` → `loom::admit` against the
state schema is a fully exported path and it works on a `.so` weave: five of
them were read back this way as a third independent witness. Recorded here
because "you cannot see inside a loaded weave" is an easy and wrong thing to
believe, and nothing in the guides sends a consumer to that path.

### From `ringing-chamber` (ZNL-03)

**The consumer path produced none a fourth time.** Same five lines of CMake, same
`if(NOT TARGET loom::kernel)` gate, same `loom_weave_build_contract` in a
`foreach`. Four independent consumers have now found nothing to complain about in
the installed package, and the fourth one swapped one loaded artifact for a
different one halfway through a run without that costing a line of anything.

**Publications were wanted, and were the first thing this era's domains have
wanted them for.** After three applications `EVIDENCE.md` recorded that nothing
had reached for publish, relay, poke or the bequest letter. A bell is heard by
everybody in the room and is addressed to nobody, so this domain reached for
`publish` immediately and for nothing else — and the fanout count it returns is
directly useful (see the F-02 note above). Recorded because "three applications
did not want it" was becoming a fact about Loom rather than a fact about those
three domains, and it was the second.

**Office authorship carried a claim and a publication, not just a send, and both
were load-bearing.** `as_role(R).claim(...)` is what makes "the 4 is up" a fact
about the *rope* rather than about a weave, so the conductor can read the ropes
without knowing who is standing at them. `as_role(R).publish(...)` is what makes
a hand slapped on the wall not a bell. Neither needed anything the guides do not
already show, and the two verbs sitting in the same grammar as `send` is what let
one application use all three without three different mental models. This is the
fourth application to use office authorship and the first to use its claim and
publish forms; no new pressure, and worth recording as a surface that held.

**Loom refused exactly one thing all evening, and it had to be forged.** Every
other refusal in this application belongs to the domain — a ringer deciding a
voice was not the conductor's, a listener deciding a noise was not a bell. That
left the grants themselves entirely unexercised, which would have made "the
pricker may say nothing to anybody" a claim resting on nothing. The pricker
cannot express the attack (it has no verb that sends), so the host forged it with
`send_as`, which stamps the pricker as author and authorises against the
pricker's own empty grant: `CapabilityDenied on Call`, once, on the tap. This is
`zen-orientation.md`'s "when the honest API can't express an attack, the test
must forge the hostile wire frame" arriving from a fourth domain and needing no
special machinery — `send_as` is a public, documented host verb.

**A method that is `unload`ed and replaced by a different artifact left nothing
behind.** The tower rings Plain Bob Doubles, stands, unloads it, loads Plain Bob
Minor into the same office, and every ringer learns the new method from scratch.
Nothing carried over and nothing needed to: the band's own state is reset by the
domain message that tells them to learn, and the method weave holds no state
worth keeping. Recorded as a non-finding because `signal-box` used unload/load
for a *replacement of the same equipment* and this is the other case — a
different thing entirely, in the same office, with no continuity wanted at all.
