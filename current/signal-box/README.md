# signal-box

A miniature railway interlocking for a diamond crossing, built entirely on an
installed Loom package.

```text
                   up-departure
                        │
  down-approach ──── junction ──── down-departure
                        │
                   up-approach
```

Two lines cross at grade. A train on route **UP** runs
`up-approach → junction → up-departure`; a train on route **DOWN** runs
`down-approach → junction → down-departure`. Both need the junction, and they
must never have it at the same time. That is the smallest piece of railway that
needs an interlocking at all, and everything here exists to keep that one
sentence true.

## Who is in it

| participant | kind | what it is |
|---|---|---|
| 5 × track circuit | native weave, one office each | Knows one bit — occupied or clear — and claims it. Sends nothing to anybody, and is registered with an entirely empty grant. |
| the signal box | **`.so`, loaded at runtime**, office `interlocking` | The safety logic. Grants or refuses routes, and knows the layout. |
| the safety monitor | native weave, office `monitor` | An independent auditor that is *not* in the request path. Pulls what everyone claims each sweep and checks the one invariant from the layout, never by asking the box. |
| the signaller | native weave, office `signaller` | The person at the panel. Holds the shift plan and moves trains only when it has been told it may. |
| the host | not a weave | Owns the clock and the trains, commissions the plant, and takes the box in and out of service. |

The interlocking is the thing in the shared library, and that is not an
arbitrary choice about which file to compile separately. A real interlocking is
separately-built, separately-certified equipment plugged into a plant it did not
build. It is also the one part that can be *taken out* while the railway stands
still — which is what makes load and unload mean something here instead of
being a mechanism on display.

## Why this domain

Night Lab has already asked the substrate about a job kitchen, a download
manager, a build farm, an import pipeline, a lobby, a scheduler and a Workshop
prototype. A railway asks a different question, because a railway keeps two
kinds of fact apart on purpose and always has:

```text
"a train entered section C"        something that happened   -> a message
"section C is occupied"            something that is so      -> a latest claim
```

An interlocking that had to subscribe to a stream of occupancy events, and then
reason about which ones it might have missed, would be a worse interlocking than
one that looks at the panel. That split is the domain's, not the API's — which
is the only honest reason to build an application on top of Senses.

## Building and running

WSL/GCC only; the kernel is `dlopen`/POSIX ground. You need an installed Loom at
the SHA in `../substrate.lock` — see `../README.md` for the six commands that
produce one.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/path/to/loom-install
cmake --build build -j
./build/railway ./build/signal-box.so     # or: ctest --test-dir build
```

`build/` is gitignored repository-wide. Build the substrate outside this
repository.

## What success looks like

The program prints a shift log and then checks what the shift is allowed to
claim afterwards. It exits non-zero if any of them is untrue.

```text
  t02  REQUEST    UP for 1A22
         CLEAR    UP set for 1A22
  t03  REQUEST    DOWN for 2B15
         ON       DOWN refused for 2B15: conflicts with route UP over junction
  ...
  --     TAKING THE SIGNAL BOX OUT OF SERVICE
  --     CONTROL: request into the empty office -> NoSuchTarget on RouteRequest
  t11  HELD       panel dark (NoClaim) - 3C07 stands; ringing the box
  --     SIGNAL BOX BACK IN SERVICE
  t13  NEW BOX    was weave 8 life 1, now weave 9 life 1
  t13  RE-SET     DOWN for 2B15 (the new box never knew about it)
  ...
  --     CONTROL: forged (unauthored) SafetyFault
  t20  IGNORED    unauthored SafetyFault: forged
  --     INJECT: a BoxStatus claiming both routes set
  t22  !! DANGER  routes UP and DOWN both set over junction -- all signals to danger

SHIFT OK
```

Four things in that log are worth naming, because they are the point:

**The panel is a Sense, and its absence is the signal.** An office claim is
erased when the office becomes unheld, so `NoClaim` under the `interlocking`
office is exactly "there is no signal box". The signaller holds trains on that
reading alone — no timeout, no watchdog, no traffic.

**A replaced box is visibly a different box.** The reading carries who claimed
it, so the signaller can tell that the panel came back lit *by somebody else* —
and re-set the route the new box never knew about. The plant remembered where
the trains were; the box remembered nothing.

**Two controls and one injection, all labelled.** A request into the emptied
office (to show the refusal is observable but not to the sender); a forged,
unauthored `SafetyFault` (to show the signaller ignores it); and a host-root
`office_claim_as` planting an impossible `BoxStatus` (so that "the monitor found
no faults" is a measurement rather than an absence — without it, a monitor that
never worked would print the same zero).

**Nothing treats "I don't know" as "clear".** A section whose occupancy cannot
be read refuses the route by name. This is why the plant has to be *proved*
before the first train: a track circuit that has never had a train on it has
never claimed anything, and to a reader that looks exactly like a circuit that
is not there.

## Known friction

All recorded in `../FRICTION.md`. The two that came from this application:

- **F-01** — a freshly loaded weave has claimed nothing, so a Sense-based
  liveness signal has a blind window at commissioning. Hit twice, independently
  (the box and the plant). Answered in the application with a `PutInService`
  message, which is real railway practice anyway.
- **F-04** — `mount()` / `mount_granted()` take no role, so an office-holding
  native weave needs a five-line local re-implementation. Seven uses here.

Neither blocked anything, and neither is a request.

## What this deliberately does not test

- **Zengine.** Not used, not vendored, not needed. This is a Loom-only
  experiment on purpose.
- **Loom's own suite.** Its official verifier was not run; nothing here is
  evidence about Loom's green.
- **Prepared replacement.** The box is unloaded and freshly loaded, not
  replaced through `PreparedReplacement`, so nothing here exercises candidate
  preparation, commit, or the life-versus-incarnation distinction.
- **Isolation.** The kernel's own containment note is printed at start-up and it
  says `no OS sandbox`. The enforced tier is not reachable from the exported
  package, and this experiment does not pretend otherwise.
- **Concurrency, persistence, and the real clock.** The host owns time and
  nothing here invents one; dispatch is single-threaded FIFO and the experiment
  depends on that.
- **Being a correct interlocking.** Twenty-six ticks, three trains and one
  crossing is a demonstration. Do not signal a railway with it.
