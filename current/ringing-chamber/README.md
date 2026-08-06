# ringing-chamber

A practice night at St Cuthbert's, Wenbourne. Six ringers, a tower captain who
calls, and somebody in the corner writing down what was actually rung.

```text
        the book (a .so)                    the tower captain
              ▲                                    │ "Bob!"
   "what is my line?"                              ▼
              │                    ┌──────── published, to nobody ────────┐
        ┌─────┴─────┬───────┬──────┴┬────────┬────────┐                   │
      bell.1     bell.2  bell.3  bell.4   bell.5   bell.6                 │
        └─────┬─────┴───────┴───────┴────────┴────────┘                   │
              │  each blow published, as its own rope's office            │
              ▼                                                           ▼
        the conductor  (hears rounds, calls, stands the band)      the pricker
                                                              (writes it down,
                                                               and proves it)
```

The band rings a **120 of Plain Bob Doubles** — every one of the hundred and
twenty possible orders of five bells, once each — then stands, gets **Plain Bob
Minor** up, and rings a plain course of that.

## What is actually being modelled

**Nobody rings the row.**

That sentence is the whole application. A ringer knows one thing: their own
*line* — the positions their own bell occupies, blow by blow, through a lead,
and what a call does to them at the lead end. They learn it out of the book
beforehand and then ring it from memory. They are never told where anybody else
is, they never see a row, and they could not describe one if asked.

A **row** is therefore not a message and not a calculation. It is the order in
which six bells happened to strike, and it exists only in the ears of whoever
was listening. What makes a touch worth anything — that no row was rung twice —
is a property of a sequence that no participant can see.

```text
each ringer          knows its own line, and its own place, and nothing else
the conductor        hears every bell; calls; knows the composition
the pricker          hears every bell; writes it down; proves it
the book             answers one question, for one place bell, before the touch
```

## Why this domain

Night Lab has already asked the substrate about a job kitchen, a download
manager, a build farm, an import pipeline, a lobby, a scheduler, a Workshop
prototype, a railway interlocking, a theatre and a records committee. Three of
those had a participant that *held the answer* — a signal box, a prompt book, a
county list — and the application's job was to get at it safely.

A tower has no such participant. The thing everybody cares about is not owned by
anyone, is not stored anywhere, and only exists as the sum of what six people
independently did. A band is not a service and its consumers; it is six peers
who must be in sequence, each blind.

And a working miniature tower that rings a true extent is a satisfying object on
its own, which is most of what a playground is for.

## Who is in it

| participant | kind | what it is |
|---|---|---|
| Nell, Bram, Ivo, Peg, Alma, Ossie | native weave, one office each (`bell.1`…`bell.6`) | Six ringers. Each holds a line and a place, and may do exactly two things: ask the book for a line, and sound its own bell. |
| the tower captain | native weave, office `conductor` | Stands out and calls. Holds the composition, hears every bell, calls a bob one whole pull before the lead end, says "That's all" when it **hears** rounds, and stands the band when what it heard was not a row. |
| the pricker | native weave, office `pricker` | Sits in the corner writing the touch down as it is struck, and proves it afterwards. **Its grant is empty** — it may say nothing to anybody, ever. |
| the book | **a `.so`, loaded at runtime**, office `method` | Plain Bob Doubles, and later Plain Bob Minor. Answers one question — "I am the *n*th place bell; what is my line?" — and is asked nothing at all once the bells are going. |
| the tower | not a weave | Owns the ropes, the evening's programme, and the clock on the wall. |

The **method** is the thing in the shared library, and that is not an arbitrary
choice about which file to compile separately. A method is a published, named,
separately-authored thing that a band learns out of a collection somebody else
maintains. It has never heard of a tower. Getting a different one up is a real
operational event — you stand, you open a different book, and everybody learns
it again — which is what makes load and unload mean something here rather than
being a mechanism on display.

## Where the truth comes from, and where it cannot come from

The most dangerous thing this program could do is tell you the touch was true
because the *composition* is true. A conductor who has proved their composition
on paper has proved nothing whatsoever about what the band did.

So the verdict comes from one place only — the pricker's paper — and the
pricker is arranged so that it has nothing else to go on:

- it is **not given the composition**;
- it **does not accept `Call`**, so it cannot hear one even though every call is
  published to the whole chamber;
- it has **never heard of a method**;
- its **grant is empty**, so it cannot ask anybody anything.

And the host cannot cheat on its behalf either. Place notation lives in
`method.hpp`, which **only the two method libraries include**; `practice.cpp`
includes `tower.hpp` and nothing else. There is no code anywhere in the tower's
translation unit that could reconstruct a row from the method and the
composition. That is a property of the build, not a promise about restraint:

```sh
grep -l '^#include "method.hpp"' *.cpp *.hpp
#   method_plain_bob_doubles.cpp
#   method_plain_bob_minor.cpp
```

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

or ring by hand:

```sh
./build/practice ./build/plain-bob-doubles.so ./build/plain-bob-minor.so
./build/practice ./build/plain-bob-doubles.so ./build/plain-bob-minor.so --fumble
./build/practice ./build/plain-bob-doubles.so ./build/plain-bob-minor.so --false-touch
./build/practice ./build/plain-bob-doubles.so ./build/plain-bob-minor.so --bell-down
```

`build/` is gitignored repository-wide. Build the substrate outside this
repository.

## What success looks like

```text
  ..     Plain Bob Doubles: 5 bells changing, a lead of 10, 37 lines given out
  ..     Ossie on the 6: covering
  ..     look to. Treble's going. She's gone.
  ..     "Bob before row 40"
  ..     "Bob before row 80"
  ..     "Bob before row 120"
  ..     "That's all."  (rounds at row 120)

    1  214356 241536 425136 452316 543216 534126 351426 315246 132546 135246
   ...
  111  215346 251436 524136 542316 453216 435126 341526 314256 132456 123456

  rows                  120
  distinct rows         120
  TRUE -- no row was rung twice
```

Four leads of Plain Bob Doubles make a plain course and it comes round, so a bob
is called at the end of every fourth lead to send the band into the next course.
Three courses and you are back where you started — and, because there are only a
hundred and twenty orders of five bells, a hundred and twenty distinct rows is
*all of them*. That is the whole point of a 120, and it is why the check reads:

```text
  ok    no row was rung twice -- the touch is TRUE
  ok    and that is every order of the five bells
```

Then the band stands, the book is closed, a different one is opened, and Ossie
gets a line instead of a cover.

## Expected refusals — every one of them is the product working

```text
a bell that is down                    the ringer will not pull, and the
                                       conductor will not go
a call from anyone but the conductor   not a call; every ringer ignores it
a line that answers no question I
  asked                                not a line; the book is the book
a place I cannot reach in one blow     a bell moves at most one place a blow
two bells in the same place            that was not a row -- STAND
a row that has already been rung       false, however well it was struck
```

Four different owners: the ringer, the conductor, the pricker, and the domain
itself.

## The controls, and what they are for

`A GOOD NIGHT'S RINGING` would be worth nothing if the paper could not tell a
good touch from a bad one. There are two kinds of bad, and one control for
each — and between them they pin the verdict to exactly the two things that can
ruin a touch, and to nothing else.

**`--fumble` — the band goes wrong.** Ivo, ringing the third, is the fifth place
bell at the first bob and is the one who should make it. He does not hear it
called. He goes where Bram is already going, there are two bells in thirds at
row 40, and the conductor stands the band. *The method artifact and the
composition are byte-identical to the good run.* Only the band was different,
and the paper knew.

There is a real piece of ringing buried in that control. Two of the five are
**unaffected** by a bob — the treble, and whoever is the fourth place bell doing
long fifths — and a ringer who mishears a call that was never going to touch
them gets away with it completely. The first draft of this control picked one of
those, rang a faultless true 120, and taught the author some ringing.

**`--false-touch` — the composition is wrong.** Bobs at 4, 5, 6 and 7 instead.
The band is flawless: no clash, no short row, every bell heard on every row, and
it comes round *exactly* at the length the conductor expected, so the conductor
says "That's all" and is perfectly happy. And thirty of the eighty rows were rung
twice and thirty were never rung at all. The paper names the repeat — row 50 was
already rung at row 30 — and you can see it on the page, because rows 31–40,
51–60 and 71–80 are visibly the same three lines. *Everything that could be
heard was perfect.*

**`--bell-down`** leaves the 4 down. The conductor reads the ropes, names the
one that is not ready, and will not go. Nothing is rung.

Four more controls run inside the practice night itself, labelled in the log.
Three of them are refusals the *domain* makes: somebody on the stairs shouts
"Bob!" at a lead the composition leaves plain (all six ringers ignore it); a hand
is slapped on the wall claiming to be the third (both listeners hear it, neither
writes it down); and somebody tells Alma her line is different (it answers no
question she asked, so it is not her line). In every one of those the substrate
delivered the message and the *recipient* discriminated — which is the interesting
half.

The fourth has to be forged, and it is the only one. Everything above leaves the
**grants** unexercised, so "the pricker may say nothing to anybody" would be, on
that evidence, a line of code nobody had ever run. The pricker cannot express the
attack — it has no verb that sends — so the host does it on the pricker's behalf
with `send_as`, which stamps the pricker as author and then authorises against
the *pricker's* grant. That is the one bus refusal of the whole evening:

```text
  bus refusals seen         1  [CapabilityDenied on Call]
```

## Three accounts of the same ringing

`SHOW OK` from the conductor alone would be the conductor marking its own
homework, so the evening is counted three ways that share no counter:

```text
the bells    2160   each blow's own count of who heard it
the paper    1080   blows written down (720 + 360, six to a row)
the tap      2162   Struck deliveries the host's own observer saw
```

The tap is **two** higher than the bells, and those two are the hand slapped on
the wall — delivered to both listeners, and written down by neither. The
difference is the measurement.

## Known friction

All recorded in `../FRICTION.md`. This application found very little, and that
is the honest result of a fourth independent consumer:

- **F-04** (again, fourth independent consumer) — `mount()` takes no role, so the
  eight office-holding native weaves here need the same local binder the other
  three experiments each wrote from scratch.
- **F-01** (second independent consumer of the *shape*, at no cost) — a rope
  whose ringer has not yet said whether the bell is up reads `NoClaim`, which is
  the same reading as a rope with nobody on it. The conductor's answer is the
  same either way — we are not going — so the domain never needed the
  distinction, and nothing was worked around.
- **F-02** — **not met**. A bell expects no answer, and a publication tells its
  sender how many heard it, so the seam that has bitten the other three
  applications has no surface here.

None blocked anything and none is a request.

## What this deliberately does not test

- **Zengine.** Not used, not vendored, not needed, not missed.
- **Loom's own suite.** Its official verifier was not run; nothing here is
  evidence about Loom's green.
- **Persistence.** A practice night does not remember last week. Nothing here is
  written down between processes, and nothing wanted to be.
- **Prepared replacement, reload, revival, the letter, graceful swap, relays,
  pokes, deferred answers.** You cannot change a ringer in the middle of a
  touch; you stand, and start again. The domain never reached for any of them.
- **Isolation.** The containment note is printed at start-up and says
  `no OS sandbox`. The enforced tier is not reachable from the exported package.
- **Concurrency and the real clock.** The tower owns the pull; dispatch is
  single-threaded FIFO and this application depends on it. Real striking is a
  matter of milliseconds and rhythm, and there is none of that here — a row is
  an order, not a set of times.
- **Being a real band.** Six bells, two methods and a hundred and eighty rows is
  a demonstration. Do not ring a peal with it.
