# prompt-corner

One act of a play, called live from the prompt book, with the Deputy Stage
Manager replaced halfway through — while the act keeps running.

```text
                 the house (owns the act, and decides when the book moves)
                            │
      company stage manager ┤ relieves the corner, and does the talking
                            │
        THE PROMPT CORNER ──┼── standby ──▶ LX      SOUND      FLYS
        (a .so; replaceable)│   ◀── standing by ──┘     │        │
                            │   ── GO ──▶ (as the office)│       │
                            │                            │   the deck crew
                            └──────────────────── the fly floor asks the deck
                                                  before it will stand by
```

## What is actually being modelled

Not a clock, and not a state machine. The act of **calling** a show.

A DSM sits at the corner with the book open, gives a department a standby, waits
to hear that they are standing by, and then says GO. Two rules make that work,
and both are about people rather than about state:

```text
WHO SAID IT        a GO from anybody but the person calling the show is
                   not a cue. It is a voice in the dark.

WERE THEY WARNED   an operator does not take a cue they were not stood by
                   for. On the fly floor that rule is why nobody dies.
```

Both are causal, both are conversations, and neither is a latest-claim.
**There is not one Sense in this application** — not as restraint, but because a
running show has nothing it wants to say in the present tense. What it has is
things that were said, and by whom.

## Why this domain

Night Lab has already asked the substrate about a job kitchen, a download
manager, a build farm, an import pipeline, a lobby, a scheduler, a Workshop
prototype and a railway interlocking. A theatre asks something none of them
asked, because there is exactly one job in a theatre that can change hands
**while the work is still happening and must not pause**, and the whole building
has a procedure for it.

That is the only honest reason to reach for prepared replacement: the domain
wanted the show to continue, and it wanted the successor verified before it went
anywhere near the book. Nothing here was built to exercise an API.

## Who is in it

| participant | kind | what it is |
|---|---|---|
| the corner | **`.so`, loaded at runtime**, office `caller` | The DSM. Holds the book, gives standbys, says GO as the office. The only replaceable participant, and the only one that needs to be. |
| LX, SOUND, FLYS | native weave, one office each | Board operators. They enforce both rules above against everybody, including whoever is calling. The fly floor cannot answer a standby out of its own head. |
| the deck | native weave, office `deck` | Somebody has to walk under the bar and look, and they do it on the next beat. This is why fly standbys go early. |
| the company stage manager | native weave, office `csm` | The only person who may take the book off one DSM and give it to another, and the one who does the talking while it happens. The replacement's coordinator. |
| the house | not a weave | Owns the act, decides when the book moves, and writes the show report. |

The DSM is the thing in the shared library, and that is not an arbitrary choice
about which file to compile separately. The board operator is at the board for
the whole act. The book can be taken by somebody else at the top of a page, and
the audience must not be able to tell.

Both incarnations are **the same artifact**. The relief has to be the DSM the
production rehearsed; a relief assembled specially for the handover would prove
nothing about whether the handover works.

## Building and running

WSL/GCC only; the kernel is `dlopen`/POSIX ground. You need an installed Loom at
the SHA in `../substrate.lock` — see `../README.md` for the six commands that
produce one. **`../substrate.lock` applies unchanged**: this experiment resolved
the same remote `main`, built it with the same flags, and ran it on the same
toolchain, so it carries no lock of its own.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/path/to/loom-install
cmake --build build -j
ctest --test-dir build --output-on-failure --no-tests=error
```

or run the three by hand:

```sh
./build/theatre ./build/prompt-corner-caller.so
./build/theatre ./build/prompt-corner-caller.so --drop-the-book
./build/theatre ./build/prompt-corner-caller.so --wrong-book
```

`build/` is gitignored repository-wide. Build the substrate outside this
repository.

## What success looks like

The act runs, the book changes hands at beat 15, and the show report — written
from what the **departments** did, never from what the corner believes it called
— agrees with the book.

```text
  t13 CONTROL    a Go for LX 5, spoken by the house and not the corner
  t13 LX         IGNORED LX 5 -- not from the corner
  t14 HOUSE      relief in the wings (weave 7), sealed -- in the wings
  t15 CSM        relieving the corner
  --  CSM        briefing the relief: next is #7, standing by [LX 5, FLY 2]
  --  CSM        the relief has the book, from #7
  t15 HOUSE      handover ended Committed (None)
  t16 LX         LX 5  the salt house
  ...
  cues taken            14        called by dsm-a 7 / by dsm-b 7
  standbys accepted     16 (book is 14)
  answers to a gone DSM 1
  SHOW OK
```

## What crossed, and who decided

```text
WHAT CROSSED    the position in the book (one integer), and the list of
                standbys that had been given and not yet gone.

WHY             those two are the only things the departments cannot
                reconstruct. Everything else about the show is already
                in the room: the book is compiled into the DSM, and each
                department knows its own board.

WHO DECIDED     the outgoing DSM authored it, after it stopped calling.
                The company stage manager carried it. The relief checked
                the edition and could have refused. The house committed.
                Loom carried a message and verified a successor; it did
                not decide what continuity meant.
```

The boundary matters more than the payload. `StandDown` is an **ordinary
message** with no standing whatsoever in Loom — what makes it a boundary is that
the DSM's own handler stops calling *in* it and authors its position *there*,
where nothing further can change it. A book position read off a DSM who is still
working is a guess; this one is exact.

**The relief re-gives every standby it inherits**, and that is not good manners.
The departments' answers were owed to the DSM who has just left the corner, and
a conversation belongs to the life that opened it. In the default run the fly
floor's answer to the outgoing DSM is refused on the tap by name, and FLY 2 goes
anyway — off an answer the relief obtained for itself.

## Expected refusals — all five are the product working

```text
a GO not authored by the caller's office      ignored; counted
a GO for a cue nobody was stood by for        held; counted
a standby or GO for a cue already taken       "we've had that cue"
a relief handed the wrong edition             refuses preparation, for itself
a beat delivered after the boundary           declined by domain policy, counted
```

The last one is a **domain** refusal, not a Loom one: the message was delivered,
the office was still held, the shape was accepted, and the DSM declined to act.

## The two controls, and what they are for

The default run's `SHOW OK` would be worth nothing if the report could not tell a
good handover from a bad one. So:

**`--drop-the-book`** — the relief is briefed with an empty book. Every
mechanical measure of the replacement still says success: the candidate was
verified, it answered for itself, the transaction committed, the role moved. And
the show is still wrong — seven cues are never called, the departments query the
ones they had already had, and the report says so. *This is the run that makes
the default run's claim a measurement.*

**`--wrong-book`** — the relief is handed a different edition. It refuses, for
itself, spending the one answer the preparation ask earned it; the transaction
ends `CandidateRefused`; the CSM says "as you were"; and the outgoing DSM calls
the whole act. A refused handover must not leave nobody calling the show.

Both exit 0 when they behave as controls must, and non-zero if the witness
fails to notice.

## Known friction

All recorded in `../FRICTION.md`. The ones this application found:

- **F-06** — `upgrade.ask()` is the **coordinator's** gated speech. Omit the ask
  shape from the coordinator's grant and `ask()` still returns ok, the delivery
  is refused `CapabilityDenied` on the tap, and the transaction's one preparation
  conversation is spent — the candidate can never be briefed. Hit on the first
  run of this application.
- **F-07** — `PreparedReplacement::start()` loads the candidate through
  `Kernel::load_candidate()`, which takes no `Grant`, so a successor arrives with
  `allow_any()` even when the incumbent was loaded narrowly. This application
  composes `load(grant)` + `seal_weave` + `start_existing` instead, so the relief
  has exactly the incumbent's six rules.
- **F-08** — `commit(sequence)` wants a number this theatre has no lineage for.
- **F-09** — a **retired** incumbent is sealed, and the concealment written for
  not-yet-live candidates applies to it: the tap says `NoSuchTarget` for a
  participant that is alive and registered.
- **F-04** (again, from `signal-box`) — `mount()` takes no role, so every
  office-holder here needs the same six-line local binder.

None blocked anything and none is a request.

## What this deliberately does not test

- **Zengine.** Not used, not vendored, not needed, not missed.
- **Loom's own suite.** Its official verifier was not run; nothing here is
  evidence about Loom's green.
- **Senses.** Not one, anywhere. A show has nothing to say in the present tense,
  so this application is a poor place to learn anything about them.
- **Isolation.** The containment note is printed at start-up and says
  `no OS sandbox`. The enforced tier is not reachable from the exported package.
- **Reload, revival, the letter, publications, relays, pokes.** The domain never
  reached for them.
- **Concurrency and the real clock.** The house owns the act; dispatch is
  single-threaded FIFO and this application depends on it.
- **Retirement.** The outgoing DSM is left sealed and alive at curtain rather
  than unloaded, because what happens to it is one of the things being watched.
- **Being a real prompt book.** Fourteen cues and one act is a demonstration.
  Do not run a get-out with it.
