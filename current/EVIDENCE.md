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
genuinely standing by at that moment and its real GO is three beats away, so the
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

---

# ZNL-02 — `records-committee`

**This experiment ran on a different Loom** — see
`records-committee/substrate.lock`. Everything from C-18 down is evidence about
`d0d8257287908991449a7a2906ee29400abf1d5b`, not about the era lock's
`c86717b0`, and the two sets do not transfer to each other. The toolchain,
configuration and measured ABI are identical; the commit is not.

---

## C-18 — A third unrelated application consumes the same installed package unchanged, and five loadable weaves cost no more build declaration than one

**WITNESS.** `records-committee`: two native weaves, **five separately built
`.so` assessors loaded concurrently from five different artifacts**, across
seven CTest entries — `7/7 Test ... Passed`. Its entire Zen-facing build
declaration is `find_package(loom REQUIRED)`, an `if(NOT TARGET loom::kernel)`
gate, three imported targets, and `loom_weave_build_contract(...)` applied in a
`foreach` over the five. It was written without reading either earlier
experiment's `CMakeLists.txt` and shares no file with them. Zero warnings and
zero errors on a fresh configure and build.

**DOES NOT PROVE.** Anything Loom's own suite proves; the verifier was not run.
Anything about a Loom other than the one in `records-committee/substrate.lock`.
Nothing about loading many more than five, and nothing about unloading them —
this application loads its members and never removes one.

---

## C-19 — `loom_weave_build_contract()` is load-bearing here, not prophylactic, and the collision it prevents is exact

**WITNESS.** Artifact-level, with a negative control. C-03 could only measure
the symbol count because `signal-box` had a single library and said so. This
application has **five libraries compiling the same `committee.hpp`**, which is
precisely the hazard `docs/guides/dynamic-weaves.md` describes:

```text
with the contract        assessor-{aldis,brant,corve,denny,elsdon}.so   0 each
without it (hand-built)  no-contract-aldis.so                          45
                         no-contract-brant.so                          45
symbols defined STB_GNU_UNIQUE by BOTH                                 45
  e.g. guard variable for loom::manifest_schema()::s
       guard variable for loom::field_desc_schema()::s
```

Every one of the 45 is defined by both libraries — the intersection is total,
not incidental. The `0`s were reproduced on the artifacts rebuilt after Loom's
source and build trees were deleted.

**DOES NOT PROVE.** That omitting the contract would have produced an observable
failure in this application. It was not run to failure: the guide's hazard needs
one image's statics to be *destroyed* while another still reads them, and this
application never unloads a member. What is established is that the collision
surface the contract exists to remove is genuinely present here, which is one
step past where C-03 could get.

---

## C-20 — Durable state written by one process changes what a later, separate process decides — and changes how a member votes, not merely what is minuted

**WITNESS.** `two-sittings`. The 1979 process runs, writes one text file, and
**exits**. A second process then runs the 1980 sitting with nothing between them
but that file. Three separate durable facts do work:

```text
  1979-044  Greenish Warbler        <- came back onto the agenda BY ITSELF, out
        NOT ACCEPTED in round 2 (1-4)   of the file, submission intact

  1980-006  Little Bunting
        ACCEPTED in round 1 (5-0)
        (not a first; the county list already had it)

  1980-012  Pallid Harrier
        ACCEPTED in round 1 (5-0)
        *** FIRST COUNTY RECORD ***
        a resubmission of 1979-011, which this committee recorded NOT ACCEPTED

  1980-022  Rustic Bunting
        NOT ACCEPTED in round 2 (0-5); round one was 1-4
```

The species list, the determination history and the held-over agenda are three
different readings, and the Pallid Harrier needs two of them at once: it *is* a
first (the 1979 record was not accepted, so it never joined the list) *and* it
is a resubmission of that record.

**The sharpest part is 1980-022.** Seat three consults the recorder before
voting and applies a higher standard to a first county record than to a second.
The county has had a Rustic Bunting since 1979-004, so seat three accepted it in
round one — a vote that would have been a rejection without the file. The
durable state is not decorating the minutes; it is inside a member's judgement.

**DOES NOT PROVE.** Anything about Loom. Loom has no persistence, was not asked
for any, and carried none of this: the recorder writes a text file with
`std::ofstream` and reads it with `std::ifstream`. What crossed the restart
crossed because the application wrote it. Nor does it prove anything about
concurrent writers, partial-write atomicity at the filesystem level (the
application detects a truncated file; it does not write atomically), or a file
edited by anything other than this program.

---

## C-21 — The recovery witness can tell a correct reconstruction from a false green, and the false green is reachable

**WITNESS.** Four controls, three refusing and one deliberately succeeding
wrongly.

```text
control-no-list          no file                -> CANNOT SIT, exit 2, nothing written
control-another-county   county field altered   -> CANNOT SIT, exit 2, file unchanged
control-half-a-list      final line removed     -> CANNOT SIT, exit 2, file unchanged
control-lost-list        file deleted, refounded-> SITS, exit 0, and is WRONG
```

The three refusals each assert both halves: the committee did not sit, **and**
the file it refused to read is byte-identical afterwards (SHA-256 before and
after, with the county field restored for the comparison in the second case).

`control-lost-list` is the one that gives the others meaning. The 1979 file is
deleted and a fresh list founded; the 1980 sitting then runs perfectly happily
and prints:

```text
  1980-006  Little Bunting
        ACCEPTED in round 1 (5-0)
        *** FIRST COUNTY RECORD ***
```

for a species the county has had since 1979. Every mechanical measure inside
that sitting still reports success — the votes were genuine, the tally correct,
the rules correctly applied, the minutes written, exit 0. **The scenario asserts
that this goes wrong**, so "not a first" in `two-sittings` is a measurement
rather than a constant: the same binary, the same agenda, the same five members,
and the answer changes with the file.

**DOES NOT PROVE.** That the application detects every possible corruption. It
detects four: absent, wrong identity, and two whole-file failures (a missing or
disagreeing length line). A file that is well formed, Marchfield's, correctly
counted, and *wrong* would be sat on without complaint — there is no signature
and no checksum over the body, and the application does not claim one.

---

## C-22 — Several live participants of the same kind genuinely disagree, and the published rule — not the substrate — decides

**WITNESS.** Five assessors, five separate artifacts, five different bodies of
reasoning, reading identical submissions. Across the two sittings every outcome
shape occurs:

```text
5-0 in round one          1979-004  ACCEPTED
0-5 in round one          1979-011  NOT ACCEPTED
4-1 -> round two -> 4-1   1979-017  ACCEPTED
3-2 -> round two -> 3-2   1979-023  NOT ACCEPTED
1-4 -> round two -> 0-5   1980-022  NOT ACCEPTED  (a member changed its vote)
```

Disagreement is not simulated by a threshold: seat two rejected 1979-023 naming
`Sykes's Warbler is not eliminated` while seat one accepted it on the recording,
and the two are different programs. 1980-022 proves the second round is not
ceremony — seat three accepted in round one and changed to reject in round two,
having read a colleague's comment naming a confusion species the description
never addressed.

**Loom decided none of it.** What Loom decided is narrower and is the part worth
naming: that a `Vote` arriving at the secretary is the authorized answer to a
ballot the secretary issued (`answers_ask`), and which ballot (the correlation
the secretary minted). The rules, the quorum, the recirculation and the tie are
all application policy.

**DOES NOT PROVE.** That the rules are good rules, or that five is the right
number. Nor anything about peers that must *agree* — this domain resolves
disagreement by counting, and never needed two participants to reach a common
value.

---

## C-23 — A vote is only a vote if it answers a ballot, and the tally cannot be reached from outside the committee

**WITNESS.** Three labelled host-root controls inside the founding sitting.

```text
the house casts a vote of its own       answers_ask() false  -> unsolicited 1, not counted
the house circulates a ballot to seat 1 the member votes; the vote answers the
                                        HOUSE, so votes counted did not move
the house minutes a determination       not authored from the secretary office
                                        -> unauthored 1, and Marmora's Warbler
                                           is absent from the county list
```

The forged ballot is the interesting one: it is **self-defeating without any
rule in the application**. A member answers whoever asked it, and the house is
not the secretary, so a ballot injected from outside can never produce a vote
the secretary will count — and a root delivery grants no answer authority at
all, so the answer is refused outright.

**DOES NOT PROVE.** That any of the three was unsendable. All three were sent,
deliberately, by the host root, which is the only place in this application that
can express them. What is established is that the recipient can tell in each
case, and that two of the three needed no application rule to be safe.

---

## C-24 — Three independent accounts of the same sitting agree, and one of them is read out of five shared libraries through the ordinary gate

**WITNESS.** The founding sitting is checked against three sources that do not
share a counter:

```text
  ballots issued   30      the secretary's ballot book
  votes counted    30
  votes on the tap 31      every Vote delivery an observer saw
  the members say  31 ballots read, 31 votes cast (5 seats' own snapshots)
```

The difference of one, in both directions, is the house's own vote: it crossed
the bus and it answered no ballot. The third account is obtained by asking the
bus for each seat's `snapshot_bytes` and putting them through the **ordinary
gate** — `loom::parse` to an `Unverified`, which by construction exposes only
what it *claims* to be, then `loom::admit` against the `AssessorState` schema
this side compiled. Five separately built artifacts' own accounts of what they
did, admitted the way a message would be.

**DOES NOT PROVE.** That the members' state is trustworthy — it is the members'
own account of themselves, which is exactly why it is one of three and not the
only one. Nor that the gate refused anything here: every snapshot was admitted,
so this exercised the accepting path only. No malformed or foreign snapshot was
offered to it.

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
  letter**, **graceful swap**, **relays**, **pokes**, or **publications**. Three
  applications have now not wanted any of them.
- Nothing about **persistence as a Zen concern**. ZNL-02 needed information to
  survive a process restart and the application wrote a text file. No Loom
  surface was involved, none was missing, and none was invented. That is a
  result, not a gap — but it is emphatically *not* evidence that a Zen
  persistence layer is unnecessary in general, only that this domain did not
  reach for one.
- Nothing about **concurrent or interleaved writers** of a durable file, about
  **atomic** writing, or about a durable file edited by anything but the program
  that wrote it.
- Nothing about **peers that must agree**. ZNL-02's participants disagree and
  the application *counts*; no two participants ever had to converge on a shared
  value, and no consensus machinery was written, needed, or tested.
