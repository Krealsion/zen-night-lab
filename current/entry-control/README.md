# entry-control

A working fire in a warehouse at ten past three in the morning. Two ways in, two
entry control points, six firefighters in breathing apparatus, and one sentence
that has to be true at the end of it.

```text
              BA MAIN CONTROL                       COMMAND
              reads both boards                     commits crews, and is the
              and nobody else may                   only voice that may say
                   ▲         ▲                      "everybody out"
        a claim ───┘         └─── a claim                  │
                                                           │
   ┌── ENTRY CONTROL ALPHA ──┐              ┌── ENTRY CONTROL BRAVO ──┐
   │  the board at the front │              │ the board at the rear   │
   │  tallies · turn-arounds │              │ tallies · turn-arounds  │
   │  times due out          │              │ times due out           │
   └────────┬────────────────┘              └────────┬────────────────┘
            │ its own channel                        │ its own channel
       RED-1 · RED-2                       BLUE-1 · BLUE-2 · GREEN-1 · GREEN-2
       (six .so's, six sets of lungs the board cannot inspect)
```

**EVERYBODY WHO WENT IN CAME OUT.** Everything here — the brass tallies, the
turn-around pressures, the times due out, the pressure checks, the emergency
crew, the two independent boards — exists to make that one sentence checkable by
somebody standing in a car park in the dark.

## What is actually being modelled

**A board that can only ever predict, watching people who are actually
spending.**

An entry control officer has three numbers and no eyes: the pressure they read
off your gauge as you handed over your tally, a nominal consumption rate printed
on the board, and whatever the last pressure check told them. From those they
work out when you must turn round and when you are due out.

And you are inside a building breathing at your own rate, which is a property of
your body and your work and how frightened you are, and which nobody outside can
see or ever could:

```text
the board plans on        8 bar a minute
Farrow actually does      6
Aish                      7
Teague                    9
Okonkwo                   8
Braddock                 10, and 14 for the first three minutes because
                             somebody has just told them a colleague is missing
Ndlovu                    8 walking in and 12 once they are on the branch, which
                             is the one a single pressure check cannot catch
```

That gap is the whole domain. The pressure check exists because of it, the
safety margin exists because of it, and the whistle exists because sometimes
both of those are not enough.

## Why this domain

Night Lab has already asked the substrate about a job kitchen, a download
manager, a build farm, an import pipeline, a lobby, a scheduler, a Workshop
prototype, a railway interlocking, a theatre, a records committee, a bell tower,
a shutter telegraph and a country auction. Three of those had a participant that
**held the answer**; one had a truth **nobody could see**; one had an answer
**carried by people who could not read it**; one had a number that **emerged
from what everybody was hiding**.

An entry control board is a seventh relation, and two things in it are new to
this laboratory:

- **A consumable that runs out.** Every previous experiment's clock was a beat
  counter. Here the clock is air in a cylinder, and the entire procedure is
  arranged around the fact that it does not stop.
- **A participant that stops answering, where the domain itself cannot say
  why.** Not a planned departure and not a refusal. Somebody was asked a
  question, twice, and did not reply, and the three explanations look identical
  from a board.

It is also a safety-critical operations domain, which is the second one in this
laboratory, and it was chosen anyway. ZNL-04 named this exact application as its
strongest runner-up and declined it purely because the portfolio already had a
railway. That filter had a side effect worth stating: the domains that naturally
contain participant failure are overwhelmingly safety-critical operations
domains, so declining them for variety was quietly filtering out the one thing
six experiments had never reached.

A railway interlocking and an entry control board are not the same application
problem. An interlocking grants and refuses routes so that two things are never
in one place. A board accounts for people it cannot see, against something that
is running out. Different verbs, different failure, different invariant.

## Who is in it

| participant | kind | what it is |
|---|---|---|
| six wearers | **six `.so`s, loaded at runtime**, offices `wearer.red-1` … `wearer.green-2` | Aish, Ndlovu, Farrow, Teague, Okonkwo and Braddock. Each has a cylinder, a tally, and a way of breathing that is theirs. Each speaks to their own entry control point and to nobody else. |
| two entry control officers | native weave, offices `entry-control.alpha`, `entry-control.bravo` | Each holds a board at one way in. Takes tallies, does the arithmetic, asks for pressures, projects, withdraws crews, and gives the tallies back. Cannot see the other board. |
| BA main control | native weave, office `ba-main` | Established because there is more than one entry control point. Holds the incident's overall account, and is **the only participant that may read a board**. |
| command | native weave, office `command` | Decides what is committed and where, and is the only voice on the fireground that may order a withdrawal. Does not read the boards; is told. |
| the incident | not a weave | Owns the clock, the fire, the building and the debrief. |

The **wearers** are the things in the shared libraries, and that is not an
arbitrary choice about which files to compile separately. How fast somebody
breathes is formed somewhere else, is not the board's to inspect, and is not the
board's to correct. That is a property of the build rather than a promise about
restraint — the rates live only in the wearers:

```sh
grep -l 'state_.rate' *.cpp *.hpp
#   wearer_aish.cpp  wearer_braddock.cpp  wearer_farrow.cpp
#   wearer_ndlovu.cpp  wearer_okonkwo.cpp  wearer_teague.cpp
```

`incident.cpp` includes `fireground.hpp` and nothing else. The board's
arithmetic is in the header where everybody can see it, because it is printed on
the board; the lungs are not.

## The arithmetic, as it is printed on the board

```text
  safety margin           55 bar    the whistle actuates here
  nominal rate             8 bar/min

  usable                  entry - 55
  turn-around pressure    55 + usable/2      come out when you reach this,
                                             whatever you are in the middle of
  time due out            entry_minute + usable/8
```

At 300 bar that is a turn-around of **177 bar** and a time due out **thirty
minutes** later. Every wearer knows their own turn-around figure — half your
usable air plus the margin is the rule, not the board's opinion — and the board
writes it down and reads it back as a confirmation.

## Building and running

WSL/GCC only; the kernel is `dlopen`/POSIX ground. You need an installed Loom at
the SHA in `./substrate.lock` — **not** `../substrate.lock`, which records the
older Loom that ZNL-00 and ZNL-01 ran on. See `../README.md` for the six
commands that produce one.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/path/to/loom-install
cmake --build build -j
ctest --test-dir build --output-on-failure --no-tests=error
```

or work the incident by hand:

```sh
W="./build/aish.so ./build/ndlovu.so ./build/farrow.so ./build/teague.so ./build/okonkwo.so ./build/braddock.so"
./build/incident $W
./build/incident $W --out-on-the-radio
./build/incident $W --straight-in
```

`build/` is gitignored repository-wide. Build the substrate outside this
repository.

## What success looks like

```text
    0   ALPHA    tally RED-1 on the board: in at 300 bar, turn round at 177 bar, due out minute 30
    6   ALPHA    RED-2 260 bar (board expected 252 bar)
   12   ALPHA    RED-2 188 bar (board expected 204 bar)
   12   BRAVO    !! BLUE-2 has missed 2 checks -- I cannot say whether that is the radio or the wearer
   12   BRAVO    tried to read the entry-control.alpha board: NotAuthorized -- asking BA main control instead
   12   COMMAND  bravo cannot account for BLUE-2 -- committing the emergency crew
   12   BA MAIN  GREEN crew committed -- THERE IS NO EMERGENCY CREW
   13   ALPHA    the board makes RED-2 turn-around at minute 13 -- RED crew out now
   19   BRAVO    BLUE-2 reports out of the building, 129 bar
   22   ALPHA    NOT COMMITTING RED crew: no emergency crew available
   24   BRAVO    GREEN-2 -- tally back in their hand
  --    both boards read zero committed
```

Four things in that log are the point.

**The board catches Ndlovu, and it takes two checks to do it.** At minute six
Ndlovu reads 260 against an expected 252 and there is nothing to see. At minute
twelve they read 188 against an expected 204, because they have been on the
branch since minute five at twelve bar a minute. The board reprojects from what
it just measured, decides Ndlovu will be at turn-around at minute thirteen, and
brings **the whole crew** out — because a crew comes out on the first person's
air. Everybody comes out with air in hand. That is the procedure working, and
it is only visible because the rate lives somewhere the board cannot read.

**Teague stops answering and the officer will not guess.** Behind steel racking
a fireground radio does not get out. Two checks go unanswered, and the three
explanations — the radio failed, the wearer is working, the wearer is in
difficulty — are indistinguishable from a board. The procedure does not pick
one. It escalates on a clock and commits the emergency crew. The honest ending
of this incident is that it was the radio and Teague was fine and came out on
their own figure exactly as briefed. **The escalation was still correct**, and
that is what a working fireground looks like.

**Committing the emergency crew means there is no emergency crew.** The moment
GREEN go in, BA main control's claim flips, and at minute twenty-two command
wants another crew at Alpha and cannot have one. Nobody goes in without a crew
standing by to come and get them.

**"The board reads zero" is not "everybody is out."** The board is paperwork.
The tallies are people. The whole of the first control below is that sentence.

## The three accounts, and the two that are blind to each other

`DEBRIEF OK` from a board's own arithmetic would be the officer marking their
own homework, so the incident is counted three ways that share no counter:

```text
the boards    6 tallies taken, 6 returned, 0 in hand
the watch     6 rigged, 6 went into the building, by their own account
              (read back out of six shared libraries through the ordinary gate)
the tap       9 Gauge deliveries, 15 checks, 6 tallies taken
the checks    15 asked by the boards = 9 answered + 6 unanswered,
              by the wearers' own count
```

That last line is the one the incident asserts rather than prints. Every
pressure check either got an answer or did not, the wearers' own accounts say
which, the boards say how many they asked for, and the tap saw exactly fifteen
cross the bus and exactly nine come back. Three counters, three owners, and if
they do not close there is something to find out rather than something to
soften.

Two of the accounts are checks, and the interesting thing about them is what
each one **cannot** see:

```text
THE BOARD'S OWN ARITHMETIC       entries opened = closed + still in
                                 tallies taken  = returned + in hand
                                 and  still in  = in hand      <- the law
                                 Cannot see somebody it was never told about.

THE ROLL                         everybody who went into the building
                                 is on somebody's board
                                 Cannot see a board closed on a voice: those
                                 wearers were booked in perfectly.
```

## The controls, and what they are for

There are two ways for this program to print a completely clean incident and be
wrong. They are not the same shape, and **each control asserts that the other
check stayed silent.**

**`--out-on-the-radio` — the board is closed on a voice.** The officer at Alpha
books Red crew out when the radio report comes in rather than when the tallies
come back, which is quick and is what a busy officer does. Everything else is
green: both crews were booked in properly, the arithmetic on every line is
right, they came out with air to spare, and the board reads zero committed.

```text
  WRONG alpha: 2 opened, 2 closed, 0 still in, 2 tallies in hand
        tally RED-1 is still on this board
        tally RED-2 is still on this board

  ok    and the roll stayed silent -- they were both booked in
  ok    and the air account stayed silent -- they came out with air
```

**`--straight-in` — somebody went in unbooked.** The emergency crew is committed
at a run and one tally is not taken as they pass the board. Braddock goes on the
shout, which is correct of them, and is in a burning building that nobody knows
they are in. **Bravo's board balances perfectly** — one entry opened, one
closed, one tally taken, one returned — because it never heard of them.

```text
  WRONG Braddock (GREEN-2) entered the building and is on nobody's board

  ok    and the board's own arithmetic stayed silent -- it balances perfectly
  ok    and the air account stayed silent
```

Both scenarios exit 0 when they behave as controls must, and non-zero if the
debrief fails to notice.

## Expected refusals — every one of them is the product working

```text
a pressure reading from a previous wear   not a reading about now (the BOARD decides)
a committal with no emergency crew        refused (the BOARD decides)
a "get out" nobody authored as command    a voice on the fireground (the WEARER decides)
a wearer reaching the other entry point   CapabilityDenied      -- the BUS decides
a withdrawal in command's name            RoleAuthorshipDenied  -- the BUS decides
an officer reading the other board        NotAuthorized         -- the BUS decides
```

Three owners: the board, the wearer, and Loom. The last three are the Zen
edges, and two of the three had to be forged.

## The three Zen edges

Every refusal above the line is somebody in the domain deciding something, which
leaves the grants and the offices entirely unexercised. So three sentences this
incident actually depends on are attacked rather than asserted. All three are
things a fireground says about itself; none was arranged.

```text
EDGE A -- WHO YOU MAY REACH, in its strongest form
    Teague, behind the racking and unable to raise Bravo, comes up on Alpha's
    channel. SPEAKING AS THEMSELVES -- wearer.blue-2 is an office they
    genuinely hold, so authorship succeeds.
        bus.office_send_to_role_as(teague, "wearer.blue-2",
                                   "entry-control.alpha", Gauge{...})
            -> CapabilityDenied on Gauge
    THERE IS NO DOMAIN RULE BEHIND THIS ONE. The shape is one Alpha accepts,
    the sender is who they say they are, the content is exactly what a wearer
    in trouble transmits, and ALPHA WOULD ACT ON IT -- its handler writes down
    a reading from anybody not on its board and says so more loudly than it
    says anything else. Alpha's "not on my board" counter stays at 0. The
    grant is the only thing keeping the two boards independent accounts.

EDGE B -- WHO YOU MAY SPEAK AS
    Bravo has a wearer it cannot account for and the roof is starting to go,
    and orders the withdrawal in command's name, to the crew in the most
    danger.
        bus.office_send_to_role_as(bravo, "command", "wearer.blue-1",
                                   Evacuate{"the roof is going"})
            -> RoleAuthorshipDenied on Evacuate, nothing queued
    The destination is right, the shape is right, the situation is real. Only
    the office is false -- and authorship is decided BEFORE the grant is
    consulted, so Bravo's own grant (which has no Evacuate rule and would also
    have refused it) is never reached.

EDGE C -- WHO MAY READ WHAT, and this is a door the era had never opened
    Bravo, with BLUE-2 missing, looks for them on the other board. Perfectly
    well meant, and the honest weave API can say it -- no forging needed.
        mail.latest_from_office<Board>("entry-control.alpha")
            -> SenseRefusal::NotAuthorized
    Alpha's board is live and current at that instant: BA main control reads
    it in the same minute. So this is NOT "nobody has claimed anything", and
    the distinction is the whole reason NotAuthorized exists as its own
    answer. The officer's next line is "asking BA main control instead",
    which is the right door -- the refusal sent them where the question
    belongs.
```

```text
  bus refusals seen  2  [CapabilityDenied on Gauge,
                         RoleAuthorshipDenied on Evacuate]
```

Two on the bus and one returned to its caller, across three different kinds of
Zen authority — who you may reach, who you may speak as, and **who may read** —
in an application that wanted all three for its own reasons. A fourth was
available (only an entry control officer may put a figure on its own board,
which is an office *claim*) and was deliberately left alone: `saleroom` already
measured that exact refusal, and repeating a denial the laboratory has is worth
less than the one kind it did not.

**Every one of the three was mutation-tested**, in a copy outside the
repository, and every mutation turned its control red:

```text
widen a wearer's grant to reach Alpha    -> delivered; Alpha writes it down;
                                            "not on my board" becomes 1
remove the wearer's own authorship check -> the unauthored shout is obeyed
add allow_observe("Board", 1)            -> the other board becomes readable
```

and one isolation control that is not a mutation: forging the same withdrawal
as `entry-control.bravo`, an office Bravo **genuinely holds**, makes authorship
succeed and the refusal change kind —

```text
  bus refusals seen  2  [CapabilityDenied on Gauge,
                         CapabilityDenied on Evacuate]
```

— which is how you know the first refusal was about the office and not about
anything standing behind it.

## Known friction

All recorded in `../FRICTION.md`. This application opened **no new entry**,
which is the fourth phase running.

- **F-04** (seventh independent consumer) — `mount()` takes no role, so the four
  native office-holders here need the same local `mount_office` helper the other
  six experiments each wrote from scratch. Four uses; thirty-seven across the
  era. Recorded; urgency unchanged; the helper was left local.
- **F-01** — **not met.** The one Sense-shaped hazard this laboratory keeps
  meeting is *absence reading like a fact*. Nothing here is ever absent for
  long, and where a reading could be about the wrong thing the domain hands you
  the answer for free: a `Gauge` carries its own wear number, so a reading from
  a previous cylinder can never be read as a reading about now. That is exactly
  what a `SenseReading` cannot tell you and exactly what the application is
  supposed to own.
- **F-02** — **not met, for the fourth time in seven, and for a fourth distinct
  reason.** A pressure check that goes unanswered is not a send whose fate is
  unknown: it was delivered, and this application can prove it was, because
  Teague's own account records `checks_unanswered`. What the officer does not
  know is *why nobody replied*, and no substrate could tell them. The domain's
  answer is a clock, which is what a real fireground uses, and it is the
  procedure rather than a workaround.
- **F-05** — **not met, because the known workaround was applied from the first
  command.** Every `wsl.exe` invocation this phase was driven from PowerShell
  and every multi-step operation lived in a script file with absolute paths, so
  the argument handoff never got the chance to go wrong. Recorded as an avoided
  hazard rather than a sighting, because a ledger entry that counts workarounds
  as evidence would only ever go up. Not Zen's either way.

None blocked anything and none is a request.

## What this deliberately does not test

- **Zengine.** Not used, not vendored, not needed, not missed.
- **Loom's own suite.** Its official verifier was not run; nothing here is
  evidence about Loom's green.
- **Participant failure and revival as Loom mechanisms.** The domain has a
  participant that stops answering, and it is emphatically *not* a dead one:
  Teague is alive, still receiving, and comes out under their own steam. Nothing
  here dies, is revived, is reloaded, or is replaced. `kill`, `reload`,
  `swap_state`, `reload_from`, prepared replacement, the letter and graceful
  swap were all read at the pin and none of them was what this domain wanted.
- **Unload.** Six artifacts are loaded and none is unloaded — nobody leaves this
  incident early. No unload means no post-unload residency observation, and one
  was not manufactured.
- **Persistence.** An incident is a night. The debrief is somebody else's
  morning. Nothing here is written down between processes.
- **Isolation.** The containment note is printed at start-up and says
  `no OS sandbox`. The enforced tier is not reachable from the exported package.
- **Concurrency and the real clock.** The incident owns the minute; dispatch is
  single-threaded FIFO and this application depends on it precisely — an
  officer's minute is delivered, its pressure checks reach the wearers, and
  every answer is back before the incident says anything else.
- **Being a real entry control procedure.** Two entry control points, six
  wearers and one emergency crew is a demonstration. The stage system, the
  entry control point radio discipline, the tally board layout, the reliefs and
  most of the paperwork are absent, and the arithmetic here is this
  application's own arrangement in the spirit of the thing rather than any
  service's actual policy. **Do not run an incident with it.**
