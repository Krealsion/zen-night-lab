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

## What this era has NOT established

- Nothing about **Zengine**. No experiment here has consumed it, hand-vendored
  it, or needed it. The five-sighting P-003 boundary from ZNL-R is unchanged and
  uncounted by this era so far.
- Nothing about the **four historical areas**, which were not built, not run,
  and not read during ZNL-00.
- Nothing about **Loom's own verification**. Its official oracle was not run
  here, and package-consumer evidence is not a substitute for it.
- Nothing about **any platform but Linux/WSL with GCC 11.4**.
