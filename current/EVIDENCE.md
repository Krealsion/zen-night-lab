# EVIDENCE — current-era Night Lab

Every claim is paired with the witness that supports it, the exact world it was
measured in, and the nearby stronger claim it **does not** establish.

An experiment is evidence about the Loom it actually consumed. Nothing here
generalizes to a Loom it did not run on.

**Era scope for everything below** (see `substrate.lock`):

```text
loom        c86717b0fcf6bbaf4144e3ab91c215824a2b6eae
abi         6
toolchain   gcc 11.4.0 / cmake 3.22.1 / Ubuntu 22.04.5 LTS (WSL2) on Windows 11
config      Debug; -DZEN_BUILD_TESTS=OFF -DZEN_BUILD_EXAMPLES=OFF -DZEN_SDL=OFF
```

ZNL-01 resolved remote `main` again at the start of its own phase and got the
same SHA, built it with the same flags on the same toolchain, and measured
`ZEN_ABI_VERSION 6u` out of its own install prefix. The era lock therefore
describes both experiments and neither carries a lock of its own.

---

## C-01 — An installed Loom package alone is enough to build and run a multi-weave application with a dynamically loaded participant

**WITNESS.** `signal-box`: five native track circuits, a native signaller, a
native safety monitor, and one interlocking loaded from a `.so`, running a
26-tick shift and exiting 0. The entire consumer-side build contract is
`find_package(loom REQUIRED)`, `loom::core` / `loom::switchboard` /
`loom::kernel`, and `loom_weave_build_contract(signal-box)`. Registered as a
CTest entry: `1/1 Test #1: shift ... Passed`.

**DOES NOT PROVE.** Nothing about Loom's own suite — Loom's official verifier
(`tests/verify.cmake`) was not run in this phase and no claim here is evidence
about Loom's population, enforcement tallies or green. Nothing about Zengine,
which has no package to consume. Nothing on any other platform, compiler, build
type, or hosting mode; in particular nothing about out-of-process isolation,
which is not part of the exported surface.

---

## C-02 — The experiment does not depend on the Loom source tree or build tree

**WITNESS.** Both were deleted (`rm -rf loom-src loom-build`, verified absent),
leaving only the install prefix. A **fresh** build directory then configured,
built, passed CTest and ran green under `env -u CMAKE_PREFIX_PATH -u OLDPWD`.
Separately, every absolute path family referenced by the generated build tree
was enumerated, and there are exactly three: the experiment's own source
directory, the scratch install prefix, and `/usr`. No file under the build tree
references `Zen/Loom` or `Zen/Zengine`, and the installed package files contain
no source- or build-tree path.

**DOES NOT PROVE.** That the install prefix is relocatable after installation,
or that the package can be consumed without CMake. Neither was tested.

---

## C-03 — `loom_weave_build_contract()` from the installed package genuinely applies the KERN-05 non-unique-symbol semantics

**WITNESS.** Artifact-level, with a negative control, using the check Loom's own
guide prescribes:

```text
nm -D --defined-only signal-box.so   | grep -c ' u '   ->   0     (with the contract)
nm -D --defined-only no-contract.so  | grep -c ' u '   ->  51     (without it)
```

Identical source, same compiler, same flags otherwise; the second was compiled
by hand against the install prefix's headers with the contract omitted. The
`0` was reproduced on the artifact rebuilt after Loom's source and build trees
were deleted.

**DOES NOT PROVE.** That omitting the contract would have produced an observable
failure *in this application* — it was not run to failure, and the hazard the
guide describes needs a second library sharing the vocabulary header, which this
experiment does not have. Nor anything about PE-COFF, where the function
deliberately applies nothing.

---

## C-04 — An office Sense claim is an exact, synchronous liveness signal for a role-held service

**WITNESS.** The interlocking was unloaded mid-shift. From that instant
`latest_from_office<BoxStatus>("interlocking")` read `NoClaim`, because an
office claim is erased when the office becomes unheld. The signaller held three
movements on that reading alone, sending nothing and waiting for nothing; the
safety monitor independently recorded 4 sweeps with no box. When the box was
reloaded the reading stayed `NoClaim` until the box actually spoke — which is
the same signal telling the truth about a different situation.

**DOES NOT PROVE.** That this detects a service which still holds its office but
has stopped working. It cannot, and this is not a defect: a wedged box keeps its
last claim, and a claim is a latest observation, never a heartbeat. It also does
not prove anything about a service whose office moved by admission rather than
by unload — that path was not exercised.

---

## C-05 — A replaced dynamic service is distinguishable from its predecessor from the provenance on a Sense reading alone

**WITNESS.** After unload and reload, the signaller read
`SenseAuthorship::author` and `author_life` off its ordinary panel read and
logged `NEW BOX  was weave 8 life 1, now weave 9 life 1` — with no cooperation
from the host, no extra message, and no access to the Switchboard. It then
re-set the route the new box had never known about (`routes re-set 1`).

**DOES NOT PROVE.** Anything about `author_incarnation_is_current`. This was an
unload followed by a fresh load — a new weave at a new address — not a prepared
replacement, so the life-versus-incarnation distinction the field exists for was
never exercised.

---

## C-06 — Office authorship is what makes a safety message actionable; being able to send the shape is not enough

**WITNESS.** Two `SafetyFault` messages reached the signaller's office in the
same run.

```text
t20  IGNORED    unauthored SafetyFault: forged        <- host send, personal speech
t22  !! DANGER  routes UP and DOWN both set over junction -- all signals to danger
```

The second was `mail.as_role("monitor").send_to_role("signaller", ...)`; the
recipient's only test is `mail.authored_from_role("monitor")`. Final counters:
`forged faults ignored 1`, `monitor faults 1`, `at_danger true`.

**DOES NOT PROVE.** That the forged message was unsendable — it was sent, by the
host, deliberately, and any weave granted the shape could send one too. What is
established is only that the recipient can tell the two apart without consulting
the role table, and that the distinction survived being attacked from the one
side that can express the attack here.

---

## C-07 — The safety monitor detects the condition it exists to detect

**WITNESS.** A labelled host-root injection at t22 —
`Switchboard::office_claim_as(box, "interlocking", BoxStatus{open, up, down})`,
a door no ordinary participant can reach — planted a claim that both routes were
set. The monitor raised exactly **1** fault, on that sweep, having raised **0**
across the 21 sweeps before it and continuing to raise none after (it latches).
The monitor derives the conflict from the layout independently; it never asks
the box whether the box is safe.

**DOES NOT PROVE.** That the interlocking is correct. Twenty-six ticks, three
trains and one diamond crossing is a demonstration, not a proof of an
interlocking. What the injection *does* buy is that "the monitor saw no faults
during normal operation" is a measurement rather than an absence — without it,
a monitor that never worked at all would have produced the identical number.

---

## C-08 — A request into an unheld office refuses observably, but the sender is not told

**WITNESS.** A labelled control at t10, immediately after the box was unloaded:
a host send of `RouteRequest` into the empty `interlocking` office produced
`NoSuchTarget on RouteRequest` on the tap. Nothing was returned to the sender.

**DOES NOT PROVE.** Anything new about Loom — this is the documented
sender-cannot-observe-send-fate seam, confirmed from an unrelated domain. It is
recorded because it is the reason the application reads the panel instead of
inferring anything from a send.

---

## C-09 — Absence of evidence is expressible as something other than `false`

**WITNESS.** The interlocking refuses a route over a section whose occupancy it
cannot read, and says which and why:

```text
DOWN refused for 2B15: no occupancy evidence for down-departure (NoClaim)
```

`SenseRefusal` keeps `NoClaim`, `NotAuthorized`, `Undeclared`, `OfficeNotHeld`
and `GateRefused` apart, so an interlocking can distinguish "nobody has claimed"
from "I am not allowed to look" — which send an engineer to opposite places, and
neither of which is "the section is clear".

**DOES NOT PROVE.** That the application would have got this right with a
coarser API. It is a claim about what the substrate made *sayable*, not about
what a careless consumer would do.

---

## C-10 — A second, unrelated application consumes the same installed package with the same five lines and nothing else

**WITNESS.** `prompt-corner`: three native department weaves, a deck crew, a
company stage manager, and one DSM loaded from a `.so` and replaced mid-run,
across three CTest entries (`act`, `control-drop-the-book`, `control-wrong-book`)
— `3/3 Test ... Passed`. Its entire Zen-facing build declaration is
`find_package(loom REQUIRED)`, an `if(NOT TARGET loom::kernel)` gate, three
imported targets, and one `loom_weave_build_contract(...)`. It was written
without reading `signal-box`'s `CMakeLists.txt` and shares no file with it.
Zero warnings and zero errors under a fresh configure and build.

**DOES NOT PROVE.** Anything Loom's own suite proves; the verifier was not run.
Anything about a Loom other than the one in `substrate.lock`, or a platform,
compiler or build type other than the one recorded there.

---

## C-11 — Prepared replacement carried an application's authored continuity across a live service change, and the application decided every part of what crossed

**WITNESS.** The DSM holding office `caller` was replaced mid-act. What crossed
was two things and nothing else: the index of the next cue in the book, and the
list of standbys given whose GO had not been called. The outgoing DSM authored
them **after** it stopped calling, in its handler for an ordinary `StandDown`
message that Loom gives no standing to; the company stage manager (the
transaction's coordinator) carried them into the preparation ask; the candidate
checked the edition, adopted them, and answered for itself; the house committed.

```text
  t15 CSM        relieving the corner
  --  CSM        briefing the relief: next is #7, standing by [LX 5, FLY 2]
  --  CSM        the relief has the book, from #7
  t15 HOUSE      handover ended Committed (None)
  t16 LX         LX 5  the salt house       <- called by the successor
  ...
  cues in the book 14 | cues taken 14 | by dsm-a 7 | by dsm-b 7
  cues never called 0 | departments out of order 0 | queried 0
```

The witness is the three **departments'** own records, not the corner's: each
department's list of cues it actually took, read against its own page of the
book.

**DOES NOT PROVE.** That prepared replacement preserved anything by itself — it
preserved nothing, exactly as [PR-09] says, and the whole of the continuity
above is application code. Nor that the boundary is atomic in any sense Loom
provides: what makes the authored position exact is that the DSM's own handler
stopped calling before authoring it, and that is a domain policy this
application chose. Nor anything about reload, revival, the bequest letter, or
graceful swap, none of which was used.

---

## C-12 — The witness that says the handover worked can tell when it did not

**WITNESS.** `--drop-the-book` runs the identical act and briefs the relief with
an empty book. **Every mechanical measure of the replacement still reports
success** — the candidate was verified, it answered authentically, the
transaction reached `Committed`, and the role moved to the successor:

```text
  handover ended Committed (None)      book now held by weave 7 (dsm-b)
```

and the show is wrong, by name:

```text
  cues taken 7 of 14
  cues never called 7  [LX 5, LX 6, LX 7, LX 8, SQ 3, SQ 4, FLY 2]
  queried (already had it) 7
  called by dsm-b 0
```

The control asserts the *detection* and exits non-zero if the report fails to
notice.

**DOES NOT PROVE.** That the application would detect every possible broken
transfer — it detects the one it was built to detect, which is a successor that
does not know where the show is. A successor briefed with a *wrong but plausible*
position was not tried.

---

## C-13 — A candidate that refuses preparation leaves the incumbent serving, and the application can put it back to work

**WITNESS.** `--wrong-book` briefs the relief with a different edition of the
prompt book. The candidate answers `CannotTakeTheBook` for itself, spending the
one answer authority the preparation ask earned it; the coordinator offers that
as `PreparationAnswer::Refused`; the transaction terminalizes:

```text
  handover ended Aborted (CandidateRefused)
  book now held by weave 6 (dsm-a)
  relief after it   alive=no  artifact=NotLoaded
  cues taken 14 | called by dsm-a 14 | cues never called 0
```

Because the incumbent had already stood down at the boundary, the domain also
has to put it back — the coordinator sends `CarryOn` and the outgoing DSM calls
the rest of the act. A refused handover that left nobody calling the show would
be a worse outcome than no handover at all.

**DOES NOT PROVE.** Anything about a candidate that refuses *after* the
transaction is Ready, or about an admission refused at commit. Neither path was
exercised.

---

## C-14 — A retired incumbent is alive, registered and sealed; a delivery addressed to it is refused as if it did not exist

**WITNESS.** The fly floor deferred an answer, went to check the deck, and the
DSM that asked was replaced before the deck reported. When the fly floor
answered everyone who had asked it, `spend_deferred` returned a **valid ticket**
for the retired DSM (the requester's incarnation was unchanged — it was
replaced, not reloaded), and the delivery was then refused:

```text
  bus refusals seen 1  [NoSuchTarget on StandingBy]
  outgoing DSM after it   alive=yes   artifact=Sealed
  relief after it         alive=yes   artifact=Live
```

Both facts are the substrate's own and both are deliberate: `deliver_admission`
seals the incumbent for retirement, and inbound delivery to any sealed addressee
is refused `NoSuchTarget` so that the world cannot discover a candidate exists.
The host can tell the difference (`alive()`, `ArtifactStatus::Sealed`); the tap
event cannot.

**DOES NOT PROVE.** That this is a defect. The concealment is correct for the
case it was written for, and no reason that distinguished a *retired* seal from
a *candidate* seal could preserve it. What is established is only that the two
situations are currently indistinguishable **on the tap**, and that an
application which reads the tap will be sent to the wrong place.

---

## C-15 — Re-giving the inherited standbys is load-bearing, not ceremony

**WITNESS.** The successor re-gives every standby the brief carried, from its
`zen.Activated` handler. In the default run the departments accept **16**
standbys for a **14**-cue book — the two inherited ones, given twice. The
refusal in C-14 is the reason: the answer to the first ask was owed to a DSM
that no longer receives anything, so without the re-give the successor could
never hear `standing by` for `FLY 2`, and the fly floor would never take a cue
it had not been stood by for. FLY 2 was taken:

```text
  standbys accepted 16 (book is 14)
  answers to a gone DSM 1
  t18 FLYS  FLY 2  the wall in
```

**DOES NOT PROVE.** That an application must re-give. A domain could equally
carry the *confirmations* across the boundary and trust them — this one does not,
because a department that has been re-warned is a department that knows a
different person is calling.

---

## C-16 — Office authorship is what makes a cue a cue, tested where nothing else would have stopped it

**WITNESS.** A labelled control at beat 13 sends `LX 5 GO` to the lighting board
**from the house**, personally, not authored as the `caller` office. LX 5 is
genuinely standing by at that moment and its real GO is four beats away, so the
department's own standby rule would have let it through and the lighting state
would have landed early. The only thing between the message and the stage is
`mail.authored_from_role("caller")`:

```text
  t13 CONTROL    a Go for LX 5, spoken by the house and not the corner
  t13 LX         IGNORED LX 5 -- not from the corner
  t16 LX         LX 5  the salt house       <- at its proper place, once
  ignored (not the corner) 1
```

**DOES NOT PROVE.** That the message was unsendable. It was sent, deliberately,
by the host root. What is established is that the recipient can tell office
speech from personal speech without consulting the role table, and that the
control was placed where **no other rule in the application** would have caught
it.

---

## C-17 — The successor holds exactly the incumbent's grant, because the application composed the ceremony itself

**WITNESS.** Both DSMs are loaded through
`Kernel::load(name, path, role, grant)` with the same six-rule grant, and the
relief is sealed by hand (`Switchboard::seal_weave`) before
`PreparedReplacement::start_existing`. The documented `start()` path could not
have done this: it loads through `Kernel::load_candidate(name, path, coordinator)`,
which has no `Grant` parameter and delegates to the default
`Grant{}.allow_any()` (`src/kernel/kernel.cpp`). The composed path ran green
across all three modes.

**DOES NOT PROVE — and this is the important half.** No runtime denial was
provoked against the DSM's grant, so this claim rests on **reading the source**
plus the fact that the composed path worked, not on a measured refusal. This
experiment did **not** demonstrate that the relief would be refused something
outside its six rules. It did separately demonstrate that grants are enforced on
the replacement path at all: F-06's `CapabilityDenied on BriefTheRelief` is a
real, measured, tapped refusal of a coordinator that lacked one rule.

---

## What this era has NOT established

- Nothing about **Zengine**. No experiment here has consumed it, hand-vendored
  it, or needed it. The five-sighting P-003 boundary from ZNL-R is unchanged and
  uncounted by this era after two experiments.
- Nothing about the **four historical areas**, which were not built, not run,
  and not read during ZNL-00 or ZNL-01.
- Nothing about **Loom's own verification**. Its official oracle was not run
  here, and package-consumer evidence is not a substitute for it.
- Nothing about **any platform but Linux/WSL with GCC 11.4**.
- Nothing about **out-of-process isolation**, which is not part of the exported
  surface and which neither experiment has been able to reach.
- After ZNL-01, still nothing about **reload**, **revival**, the **bequest
  letter**, **graceful swap**, **relays**, **pokes**, or **publications**. Two
  applications have now not wanted any of them.
