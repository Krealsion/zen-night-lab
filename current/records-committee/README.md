# records-committee

A county rarities committee. Five assessors read the same submission, disagree,
and the county list outlives all of them.

```text
                    the submissions                the county list
                          │                     (a file, decades old)
                          ▼                              ▲
   the house ─────▶ THE SECRETARY ──ballot──▶ seat.1 …   │
   (agenda, calendar)     │        ◀──vote────  seat.5   │
                          │                        │     │
                          │                     seat.3 asks
                          └────── determination ──▶ THE RECORDER
                                  (as the secretary office)
```

Somebody claims a bird that should not have been here. A committee of five
reads the claim, votes, and either the county list gains a species or it does
not. Then everybody goes home for a year.

## What is actually being modelled

Not a workflow, and not a database. **A body of people who disagree, and a
record that has to be right for longer than any of them will serve.**

Two things make that work, and both are about judgement rather than state:

```text
FIVE HEADS        the whole reason a committee exists is that competent
                  people reach different verdicts from the same evidence.
                  Collapse them into one and you have not simplified the
                  domain, you have deleted it.

ONE LIST          "first county record" is not a property of the bird or of
                  the submission. It is a property of the file. A committee
                  that cannot read its own list will announce a first for a
                  species the county has had for years, print a completely
                  happy report, and be wrong for as long as anybody keeps the
                  minutes.
```

**There is not one Sense in this application.** A committee has nothing to say
in the present tense. What it has is things it decided, on dates, by tallies —
and an accumulated judgement whose only honest home is a file.

## Why this domain

Night Lab has already asked the substrate about a job kitchen, a download
manager, a build farm, an import pipeline, a lobby, a scheduler, a Workshop
prototype, a railway interlocking and a theatre. None of them had two
participants who could **contradict each other about the same fact**, and none
of them had anything that needed to survive the process exiting.

A rarities committee has both, and it did not have to be arranged: it is
constituted *because* people disagree, and its output is a list kept across
decades by people who hand it on.

## Who is in it

| participant | kind | what it is |
|---|---|---|
| the five assessors | **five separate `.so`s**, one office each (`seat.1`…`seat.5`) | Members. Each one's standards are its own code. The committee gets a verdict and never the reasoning. |
| the secretary | native weave, office `secretary` | Issues the ballots, keeps the ballot book, applies the published rules, minutes the result. Has no opinion about birds. |
| the recorder | native weave, office `archive` | The county list. **The only participant that touches the file**, and the only one that can answer "has this county had one before". |
| the house | not a weave | Owns the calendar and the agenda, and writes nothing down itself. |

Each member is a separately built artifact, and that is not an arbitrary choice
about which source to compile apart. A member's reasoning was formed elsewhere,
is not the committee's to inspect, and is not the committee's to correct. Five
`.so`s is what a committee *is*.

They are **five different programs**, not one program with a threshold
parameter. Seat one accepts on a photograph; seat two accepts only when every
confusion species is eliminated; seat three raises its standard for a first
county record and lowers it for a second; seat four thinks about how likely any
of it was; seat five looks for a reason to say no rather than a reason to say
yes. The splits below fall out of that.

## Seat three, and why it is the interesting one

Seat three cannot vote out of its own head. Whether the county has had this
species before is not in the submission — it is in the recorder's file, which is
older than seat three's membership. So seat three **takes the ballot away**
(`defer_answer`), asks the recorder, and votes when it has the answer.

That is the coupling this whole experiment turns on: **the durable state does
not merely change a line in the minutes, it changes how a member votes.** The
same Rustic Bunting record gets an accept from seat three in 1980 and would have
got a reject in 1979, because in between, a file said the county had had one.

## The published rules

```text
Round one    unanimous accept        -> ACCEPTED
             unanimous reject        -> NOT ACCEPTED
             anything else           -> recirculate, with the comments attached
Round two    four or more accepts    -> ACCEPTED
             otherwise               -> NOT ACCEPTED
Quorum       four votes. Below it the record is HELD OVER, undecided.
```

`NOT ACCEPTED` is "not proven", never "the observer is lying". A second round
exists because members read each other's comments and change their minds — and
in the 1980 sitting one of them does.

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

or run a sitting by hand:

```sh
./build/sitting --archive /tmp/m.list --year 1979 --found ./build/assessor-*.so
./build/sitting --archive /tmp/m.list --year 1980         ./build/assessor-*.so
```

The second command **refuses to run** unless the first has already happened.
That is the application, not a safety rail.

`build/` is gitignored repository-wide. Build the substrate outside this
repository, and note that no county list is tracked here: every scenario makes
its own and destroys it first.

## What success looks like

```text
  1979-017  Little Bunting
        R. Pyke, Nov, 2 observers
        ACCEPTED in round 2 (4-1); round one was 4-1
        *** FIRST COUNTY RECORD ***
          seat 3: a first for the county needs media and a complete elimination
```

and then, a year and one process later:

```text
  1980-006  Little Bunting
        J. Stannard, Oct, photograph, 2 observers
        ACCEPTED in round 1 (5-0)
        (not a first; the county list already had it)

  1980-012  Pallid Harrier
        T. Bewick, May, photograph
        ACCEPTED in round 1 (5-0)
        *** FIRST COUNTY RECORD ***
        a resubmission of 1979-011, which this committee recorded NOT ACCEPTED
```

Three separate durable facts are doing work in those nine lines: the species
list (Little Bunting is not a first), the determination history (the Pallid
Harrier is a resubmission of a record that was *not* accepted, so it still is
one), and the held-over agenda — the Greenish Warbler that arrived too late in
1979 came back onto the 1980 agenda **by itself**, out of the file, with its
whole submission intact.

## What crossed the restart, and who decided

```text
WHAT CROSSED    the species list, every determination ever made, and the
                submissions of records held over undecided.

WHERE           one text file, written by the recorder at adjournment.

WHO AUTHORED    the secretary, as the secretary office. The recorder writes
                what the office determined and refuses anything else,
                however well-formed.

WHO TRUSTED     the recorder, and only after checking that the file is
                Marchfield's and is whole.

WHAT DID NOT    the assessors, the ballot book, the votes, the comments, and
                every seat's occupant. A committee reconvenes; it does not
                resume.
```

Loom carried the messages, attested that the secretary was speaking as the
secretary, and decided which votes were votes. **It has no opinion about
persistence and was not asked for one.** The file is ordinary application state
and the application writes it — which is the honest answer here, not a gap.

## Three accounts of the same afternoon

`SHOW OK` from the secretary's own ballot book would be the secretary marking its
own homework, so the sitting checks its arithmetic against two other places:

```text
the secretary   its ballot book                      30 votes counted
the tap         every Vote that crossed the bus       31 delivered
the members     each seat's own declared state,       31 ballots read,
                read back through the gate            31 votes cast
```

The difference of one is the house's own vote, in both directions — it crossed
the bus and it answered no ballot. The third account is the interesting one: the
meeting cannot hold a typed pointer into a shared library, so it asks the bus for
each seat's snapshot bytes and puts them through the **ordinary gate**
(`parse` → `Unverified` → `admit` against the state schema this side compiled).
Five separately-built artifacts' own accounts of what they did, admitted the same
way a message would be.

## Expected refusals — all of them are the product working

```text
a ballot to an empty seat            refused on the bus; the secretary is not told
a vote answering no ballot            not counted
a determination not spoken as the
  secretary office                    not written down
a county list from another county     the committee does not sit
a county list that stops mid-file     the committee does not sit
no county list at all                 the committee does not sit
three members                         quorum fails; everything is held over
```

The last four are the point. **A committee that quietly starts from nothing is
the failure this application exists to be about**, so "there is no list" is
never allowed to become "the list is empty".

## The controls, and what they are for

`SHOW OK` on the default run would be worth nothing if the report could not tell
a good sitting from a bad one.

**`control-lost-list`** is the one that matters. The 1979 file is deleted and
somebody founds a fresh list; the 1980 sitting then runs perfectly happily and
announces `*** FIRST COUNTY RECORD ***` for a Little Bunting the county has had
since 1979. Every mechanical measure inside the sitting still reports success —
the votes were genuine, the tally was right, the rules were applied, the minutes
were written. **The scenario asserts that this goes wrong**, because a control
that cannot fail proves nothing about the run that can.

**`control-no-list`**, **`control-another-county`** and **`control-half-a-list`**
are the three ways the file can be untrustworthy. Each asserts both that the
committee refused *and* that the file it refused to read is byte-for-byte
unchanged afterwards.

**`control-inquorate`** seats three members. Nothing is decided, however obvious
the records look, and the county list does not move.

Three more controls run inside the founding sitting itself, labelled in the log:
the house casts a vote of its own (answers no ballot, not counted); the house
circulates a ballot of its own to seat one (the member votes, and the vote goes
to the house — the tally cannot be reached from outside it); and the house
minutes a determination of its own (not spoken as the secretary, not written).

## Known friction

All recorded in `../FRICTION.md`. The ones this application found:

- **F-10** — the immediate answer path reports an **authority** refusal as
  `CapabilityDenied`, the grant reason, while the deferred path three functions
  away reports the same category — including the same *already spent* case — as
  `ForeignAuthority`, which the enum's own documentation says exists precisely to
  avoid this. Measured here on a weave whose grant demonstrably permits the
  shape: the same weave sent the same shape six times in the same run under the
  same grant.
- **F-02** (again, third independent consumer) — the secretary is not told that a
  ballot to an empty seat went nowhere. It learns of a vacancy only by counting
  what came back, which is why the circulation closes on a **date**.
- **F-04** (again, third independent consumer) — `mount()` takes no role, so the
  two native office-holders here need the same local binder the other two
  experiments wrote.

None blocked anything and none is a request.

## What this deliberately does not test

- **Zengine.** Not used, not vendored, not needed, not missed.
- **Loom's own suite.** Its official verifier was not run; nothing here is
  evidence about Loom's green.
- **Senses.** Not one, anywhere — the domain has nothing to say in the present
  tense. This is a poor place to learn anything about them.
- **Prepared replacement, reload, revival, the letter, graceful swap,
  publications, relays, pokes.** A seat changing hands between sittings is a
  fresh load into a role, not a continuity ceremony, and the domain never
  reached for one.
- **Isolation.** The containment note is printed at start-up and says
  `no OS sandbox`. The enforced tier is not reachable from the exported package.
- **Concurrency and the real clock.** The house owns the calendar; dispatch is
  single-threaded FIFO and this application depends on it.
- **Persistence as a substrate concern.** The recorder writes a text file with
  `std::ofstream`. Nothing here is evidence for or against a Zen persistence
  layer, and the experiment deliberately did not invent one.
- **Being a real records committee.** Five members, nine records and two
  sittings is a demonstration. Do not publish a county avifauna with it.
