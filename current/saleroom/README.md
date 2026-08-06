# saleroom

A country auction, Wednesday morning, six lots. A rostrum, a clerk, a front
office, and four people with money.

```text
              THE FRONT OFFICE                    the book
              holds the vendors'                (a commission bid,
              instructions; puts               left by somebody who
              the sheet on the board             is not in the room)
                      │                                  │
                   [board]  ◀── a claim, under the office │
                      │  read, never asked for            ▼
                      ▼                            THE ROSTRUM ──▶ THE CLERK
      paddle 3  ──┐                                  ▲    │        writes it down,
      paddle 7  ──┼──── Bid ─────────────────────────┘    │        and only what
      paddle 11 ──┤     (to the rostrum, and to           └── the rostrum said
      paddle 14 ──┘      nobody else, ever)                    published to the room
```

Six lots go up, the room bids, the hammer falls, and the clerk writes it down.
The increment ladder makes it terminate, the reserve makes it possible to fail,
and the hammer price is a number nobody in the room chose.

## What is actually being modelled

**Three of the people the sale is for are not in a position to check it.**

```text
THE VENDOR        left a reserve with the front office and went home. He
                  cannot see whether it was honoured.

THE ABSENTEE      left a commission bid — a figure the rostrum will bid on his
                  behalf. The duty is to spend as little of it as the room
                  forces. He cannot see whether it was.

THE UNDERBIDDER   is in the room, and is the person who actually SETS the
                  price, and never learns it. The hammer stops at the first
                  rung he will not pay.
```

The first two are represented by the one participant who is paid a percentage of
the number. That is not a flaw in auctions; it is what the procedures are for.
And it is the first domain in this laboratory whose participants have **adverse
interests** — `records-committee`'s five assessors disagree, but they all want
the same thing.

So there are two quite different ways for this program to print a perfectly
clean sale and be wrong, and they are not the same shape at all:

```text
THE BOARD IS STALE   the office had no sheet for the lot in hand, so the last
                     lot's sheet was still up — and the last lot was
                     unreserved. A £300 picture goes at £110, the record
                     balances, and nobody in the room could have known.
                     Wronged: the vendor.  Remedy: the sale is void.

THE BOOK IS RUN UP   the rostrum takes the absentee's commission straight to
                     its ceiling instead of one rung above the room. Nothing
                     on the floor is wrong at all. The absentee pays £340 for
                     a lot the room stopped wanting at £200.
                     Wronged: the buyer.  Remedy: refund the difference.
```

Same symptom — a clean sale in a balanced record. Different cause, different
victim, different remedy. And the reason the pair is worth having: **different
check.** The reserve audit structurally cannot see the second, and the
competitive-execution audit structurally cannot see the first. Each control
asserts that the other one stayed silent.

## Why this domain

Night Lab has already asked the substrate about a job kitchen, a download
manager, a build farm, an import pipeline, a lobby, a scheduler, a Workshop
prototype, a railway interlocking, a theatre, a records committee, a bell tower
and a shutter telegraph. What none of them had is a number that **emerges from
information everybody is deliberately hiding**.

`ringing-chamber`'s truth is invisible because it exists nowhere — no ringer
could describe a row. A saleroom's truth is invisible for the opposite reason:
every figure exists, precisely, in somebody's head, and the whole institution is
built to keep them apart until the hammer has fallen. The price is the
second-highest private limit, one rung up. Nobody in the room can compute it and
nobody afterwards can either — until they go round and ask.

And a little auction that actually runs, with a ladder and a book and a reserve
and fair warning, is a good thing to have built.

## Who is in it

| participant | kind | what it is |
|---|---|---|
| the rostrum | native weave, office `rostrum` | The auctioneer. Reads the board, opens the lot, takes the room's bids one rung at a time, executes the book against the room, and brings the hammer down. **The only participant that may sell anything to anybody.** |
| the front office | native weave, office `front-office` | Took the entries in, so it holds every vendor's instructions. **The only participant that may put a figure on the board.** Its grant is empty: a reserve is not told, it is up. |
| the clerk | native weave, office `clerk` | Sits at the rostrum's elbow and writes the sale down. Its grant is empty too, and it writes down nothing that was not the rostrum's. |
| Mrs Ledbury, Kestrel Fine Art, Mr Selwood, Hallam & Rooke | **four `.so`s, loaded at runtime**, offices `paddle.3` `paddle.7` `paddle.11` `paddle.14` | Four people with a private figure and a private manner. Each may say one shape to one office. |
| the house | not a weave | Owns the catalogue, the clock, the reserves before they reach the office, and the audit afterwards. |

The **bidders** are the things in the shared libraries, and that is not an
arbitrary choice about which files to compile separately. A bidder's limit was
formed somewhere else, is not the saleroom's to inspect, and is not the
saleroom's to correct — and people arrive and leave. Mrs Ledbury buys the
spaniels, has a go at the watercolour, surrenders paddle 3 and goes home while
the sale is still running, which is what `unload` means here.

They are **four different programs**, not one program with a limit parameter:

```text
paddle 3   Mrs Ledbury       will not open. Somebody else has to want it
                             first. Then one rung at a time, and she stops
                             on her figure without a flicker.
paddle 7   Kestrel Fine Art  trade. Quick at the bottom, out early, because a
                             dealer's limit is what they can sell it for less
                             what they need to make.
paddle 11  Mr Selwood        a jump bidder — two rungs at once to frighten the
                             room — and the vendor of lot 12.
paddle 14  Hallam & Rooke    trade, and local. Sits on his hands until fair
                             warning, then bids like anybody else.
```

The figures are in those four files and in no other. `sale.cpp` includes
`saleroom.hpp` and nothing else, and the tables live only in the bidders:

```sh
grep -l 'kBook' *.cpp *.hpp
#   bidder_hallam.cpp  bidder_kestrel.cpp  bidder_ledbury.cpp  bidder_selwood.cpp
```

That is a property of the build, not a promise about restraint. The house cannot
work out what the hammer price ought to be; it has to ask everybody afterwards.

## What each participant may say, and to whom

```text
paddle.N          Bid -> the rostrum
                  and nothing else, to nobody, ever

rostrum           Step, Determination -> the clerk
                  LotUp, Asking, Knocked, BoughtIn, Withdrawn -> the room
                  may observe Reserve

front-office      nothing at all. It claims; it does not speak.
clerk             nothing at all. It writes; it does not speak.
```

The first of those is **the sentence this saleroom's honesty rests on**. A ring —
two dealers agreeing not to bid against each other and settling up afterwards —
is the oldest crime in the auction world, and the structural defence against it
is not vigilance. It is that a bidder addresses the rostrum and has no way of
addressing another bidder. So it is challenged rather than asserted; see the
controls below.

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

or take the sale by hand:

```sh
P="./build/paddle-3.so ./build/paddle-7.so ./build/paddle-11.so ./build/paddle-14.so"
./build/sale $P
./build/sale $P --take-the-board-as-read
./build/sale $P --run-the-book-up
```

`build/` is gitignored repository-wide. Build the substrate outside this
repository.

## What success looks like

```text
   11   ROSTRUM   lot 11, a pair of Staffordshire spaniels. £90 anywhere?
   11   ROSTRUM   £90, paddle 7
   11   ROSTRUM   £95, paddle 3
   ...
   11   ROSTRUM   fair warning at £150...
   11   ROSTRUM   £160, paddle 14
   11   ROSTRUM   £170, paddle 3
   11   ROSTRUM   fair warning at £170...
   11   ROSTRUM   **  £170 -- paddle 3  **
   ...
   16   ROSTRUM   £190, paddle 7
   16   ROSTRUM   £200, on the book
   16   ROSTRUM   **  £200 -- on the book  **
```

and then the part nobody in the room could have done — going round afterwards
and asking everybody what they were good for:

```text
  the sale record, as the clerk has it

    lot 11  £170  to paddle 3
    lot 12  £500  to paddle 14
    lot 13  did not reach the reserve
    lot 14  £50  to paddle 14
    lot 15  withdrawn: the sheet on the board is lot 14
    lot 16  £200  to the book

  what the room was good for, asked afterwards, one by one

    Mrs Ledbury         11:£190  13:£150  15:£130(never bid)
    Kestrel Fine Art    11:£140  12:£520  13:£190  14:£45  15:£95(never bid)  16:£200
    Mr Selwood          13:£210  16:£160
    Hallam & Rooke      11:£160  12:£620  14:£55  15:£110(never bid)

  ok    lot 11: it went to the one who was good for the most (paddle 3 at £190)
  ok    lot 11: the underbidder would not have gone one more (£160 good for, £180 asked)
  ok    lot 13: bought in, and nobody here could reach £220 (best was £210)
  ok    lot 16: the book was run against the room one rung at a time
```

The English auction's own arithmetic, and the reason the audit can only be done
afterwards: **the price stops at the first rung the underbidder will not pay.**
Every sold lot is checked against it, and against the reserve the room never saw.

## Expected refusals — every one of them is the product working

```text
a bid for a lot that has gone        disregarded (the ROSTRUM decides)
a bid from somebody with no paddle   not taken (the ROSTRUM decides)
a bid below the asking price         disregarded
two hands on the same breath         he takes the higher; the other is behind
a hammer below the reserve           bought in (the ROSTRUM decides)
a lot with no sheet he can trust     not offered at all (the ROSTRUM decides)
a bidder reaching another bidder     CapabilityDenied -- the BUS decides
a hammer that is not the rostrum's   RoleAuthorshipDenied -- the BUS decides
a vendor putting up his own figure   OfficeNotHeld -- the BUS decides
```

Two owners: the rostrum, and Loom. And the last three had to be forged, because
the honest API cannot put any of them on the wire.

## The controls, and what they are for

`A GOOD MORNING'S SELLING` would be worth nothing if the audit could not tell a
good sale from a bad one. The audit is the same audit in all three runs; what
differs is what the scenario expects it to find.

**`--take-the-board-as-read`.** The sheet for the watercolour never reached the
office — the vendor rang it in the evening before and it was never made up — so
the office says so out loud and **leaves the board as it stands**, which is
honest of it. What is standing there is lot 14's sheet, and lot 14 was
unreserved. The auctioneer glances at the board instead of reading the lot
number on it, which is what an auctioneer who has run four hundred lots before
lunch does.

```text
   15   OFFICE    there is no sheet for lot 15 -- the board is left as it stands
   15   ROSTRUM   (the board says lot 14; he takes it as read)
   15   ROSTRUM   lot 15, a watercolour, the Norfolk coast. £90 anywhere?
   ...
   15   ROSTRUM   **  £110 -- paddle 3  **

  ok    lot 15: it went to the one who was good for the most (paddle 3 at £130)
  ok    lot 15: the underbidder would not have gone one more (£110 good for, £120 asked)
  WRONG lot 15: sold at £110, against a reserve of £300
```

Everything inside the room is green. The bidding was genuine, the ladder was
right, the record balances, the buyer agrees the lot is hers, and the
underbidder would not have gone one more. **The scenario asserts that this goes
wrong**, and asserts that the audit found *exactly one* thing wrong — so "nothing
went under its reserve" in the default run is a measurement rather than a
constant.

**`--run-the-book-up`.** The rostrum executes lot 16's commission bid to its
ceiling at the first opportunity instead of one rung above the room. The reserve
is cleared, the hammer goes to the highest figure in the building, the record
balances, and the absentee pays £340 for something the room stopped wanting at
£200. Again exactly one thing wrong, and it is a different thing.

**And the two controls each assert that the other check stayed silent.** That is
the point of having both: an audit that found the stale board would tell you
nothing about the book, and an audit that found the run-up would tell you
nothing about the board. Neither check is complete, and there is no reason to
think a third failure would be caught by either.

**Three things have to be forged**, because every refusal above this line is
somebody in the domain deciding something, which leaves the grants and the
offices entirely unexercised. None of the three can be said through the honest
API — a bidder's only verb is one shape to one office — so the day says them
with the public host doors, which stamp the speaker from the day's own root
authority and then apply every ordinary law.

```text
EDGE A -- the room's topology, in its strongest form
    Hallam & Rooke, SPEAKING AS THEMSELVES (paddle.14 is an office they
    genuinely hold, so authorship succeeds), offer Kestrel a knock-out on
    the bureau.
        bus.office_send_to_role_as(hallam, "paddle.14", "paddle.7", KnockOut{...})
            -> CapabilityDenied on KnockOut
    THERE IS NO DOMAIN RULE BEHIND THIS ONE. The shape is one Kestrel
    accepts, the sender is who it says it is, the content is exactly what a
    ring approach looks like, and KESTREL WOULD HONOUR IT -- its handler
    stands the lot off and says so in its own account. The grant is the only
    thing between this saleroom and a ring, and Kestrel's own
    "approaches received" counter stays at 0.

EDGE B -- authorship
    Mr Selwood, at fair warning on a lot that is about to be bought in and
    with his own paddle standing, tells the clerk the clock is his, in the
    words the rostrum would have used.
        bus.office_send_to_role_as(selwood, "rostrum", "clerk", Determination{...})
            -> RoleAuthorshipDenied on Determination, nothing queued
    The destination is where a real determination goes, the shape is what the
    clerk writes, the lot IS in hand at that money with that paddle standing.
    Only the office is false, and authorship is decided BEFORE the grant is
    consulted -- so his grant, which would also have refused it, is never
    reached.

EDGE C -- the claim side, which is a different door again
    Mr Selwood is selling the bureau and has decided overnight that the £400
    he left with the office is too little. A vendor who could put his own
    figure up could move it after the catalogue is printed.
        bus.office_claim_as(selwood, "front-office", Reserve{12, 700, false})
            -> OfficeNotHeld, and nothing is stored
    Refused at the CLAIM moment, and never downgraded to a personal claim.
    This one is not on the tap: a Sense refusal is returned to the caller.
```

```text
  bus refusals seen  2  [CapabilityDenied on KnockOut,
                         RoleAuthorshipDenied on Determination]
```

Two on the bus and one off it, across three different kinds of Zen authority —
who you may reach, who you may speak as, and who may put a figure on a board —
in one small application that wanted all three for its own reasons.

## Four accounts of the same morning

`A GOOD MORNING'S SELLING` from the clerk's own record would be the saleroom
marking its own homework, so the morning is counted four ways that share no
counter:

```text
the bidders   48 hands up, by their own account (read back through the gate)
the clerk     42 rungs to the room + 5 on the book
the tap       50 Bid deliveries
the day        1 bid forged (the late one) + 1 from the back with no paddle
```

The tap is **two** higher than the room's own count, and those two are the day's
own frames. The difference is the measurement. And separately: every rung, every
disregarded bid, every bid beaten on the same breath and every paddleless shout
adds up to exactly the fifty the tap saw.

The fourth account is the cheapest and the one an auctioneer would actually take:
**every buyer named in the record heard the hammer and agrees the lot is theirs.**

## Known friction

All recorded in `../FRICTION.md`. This application opened **no new entry**, which
is the third phase running.

- **F-04** (sixth independent consumer) — `mount()` takes no role, so the three
  office-holding native weaves here need the same local binder the other five
  experiments each wrote from scratch. Three uses; thirty-three across the era.
  Recorded; urgency unchanged; the helper was left local.
- **F-01** — **not met, and the reason is the interesting part.** Every previous
  Sense consumer in this era met the same hazard: *absence reads like a fact*, a
  `NoClaim` that cannot be told from a real answer. This application met the
  mirror image. Nothing here is ever absent — the office claims a sheet for the
  first lot before the first lot is offered, and it never clears the board — so
  the branch that would say `nothing on the board (NoClaim)` is written and
  never taken. What the rostrum meets instead is **a claim that is present,
  valid, honestly stamped and about a different question**, and that is a
  strictly harder thing to notice, because every guard a `SenseReading` carries
  says it is fine. It is fine. It is the last lot's.
- **F-02** — **not met, for the third time in six**, and for a third distinct
  reason. `ringing-chamber` did not meet it because a publication returns a
  fanout count; `shutter-line` did not meet it because the thing it wanted to
  know was end-to-end. Here a bidder never wants a ticket because **the
  acknowledgement is a broadcast the whole room hears**: you put your hand up,
  and the answer is the auctioneer saying "ninety-five, paddle three" out loud.
  The domain's own confirmation channel is louder than the send it confirms.

## What this deliberately does not test

- **Zengine.** Not used, not vendored, not needed, not missed.
- **Loom's own suite.** Its official verifier was not run; nothing here is
  evidence about Loom's green.
- **Prepared replacement, reload, revival, the letter, graceful swap, relays,
  pokes, deferred answers.** A bidder who goes home has gone home; there is no
  successor and nothing crosses. The domain never reached for any of them.
- **Persistence.** A sale is a day, and the account of sale is somebody else's
  Thursday. Nothing here is written down between processes.
- **Isolation.** The containment note is printed at start-up and says
  `no OS sandbox`. The enforced tier is not reachable from the exported package.
- **Concurrency and the real clock.** The house owns the beat; dispatch is
  single-threaded FIFO and this application depends on it precisely — a beat is
  delivered, the rostrum's `Asking` reaches every bidder, and every `Bid` it
  causes is back before the day says anything else. That is what makes "the bids
  of one beat" a set rather than a race.
- **Being a real saleroom.** No buyer's premium, no VAT, no lotting fees, no
  telephone lines, no online bidding, and the auctioneer never takes a bid off
  the wall below the reserve — which is legal and which this room does not do.
  Six lots and four bidders is a demonstration. Do not sell a house with it.
