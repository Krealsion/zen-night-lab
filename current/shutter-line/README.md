# shutter-line

The Admiralty telegraph from Whitehall to Portsmouth: six hilltops, two signal
offices, and a message that nobody in between can read.

```text
   WHITEHALL                                                     PORTSMOUTH
       │                                                              │
    [ADM]──CHELSEA──PUTNEY──COOMBE──NETLEY──HASCOMBE──BLACKDOWN──[PMH]
       │      each hill shows its frame to the two hills it can see    │
       │      and to nobody else -- that is a grant, not a manner      │
       ▼                                                              ▼
   the 1805 vocabulary                                    the 1805 vocabulary
   (a .so on the desk)                                    (a .so on the desk)
```

Six shutters in a frame, open or shut, give sixty-four numbers. A clerk turns a
message into numbers out of the book on the desk; the numbers crawl down the
line one hill a minute; a clerk at the other end turns them back into words out
of the book on *that* desk. Everything in between is repetition.

## What is actually being modelled

**Fidelity, and the two quite different ways it fails.**

A station's whole job is to show, faithfully, what it just saw. It is never told
what the message says and could not tell you if you asked — it holds no book,
and it has no reason to want one. So the thing this application is about is not
owned by any participant and is not computed by anybody: it is the *relation*
between what went in at one end and what came out at the other, across sixteen
pairs of hands.

Two things can break that relation, and they are not the same shape at all:

```text
A SHUTTER IS MISREAD     one number changes on one hill in bad light.
                         The line's own answer is the END-TO-END REPEAT:
                         Portsmouth sends the numbers back up, London
                         compares them against its file, and calls for a
                         repeat if they differ.

THE BOOKS DISAGREE       no number changes at all. The repeat agrees
                         perfectly, every journal on the line holds the same
                         numbers, and the words at the far end are different
                         words. The repeat cannot see this and never could.
                         The answer is a different one: the message says
                         WHICH BOOK IT WAS WRITTEN WITH, and an office
                         holding another refuses to write it out.
```

Both of those end with a perfectly good order arriving at Portsmouth, and this
application exists to be able to tell them apart afterwards.

## Why this domain

Night Lab has already asked the substrate about a job kitchen, a download
manager, a build farm, an import pipeline, a lobby, a scheduler, a Workshop
prototype, a railway interlocking, a theatre, a records committee and a bell
tower. Three of those had a participant that **held the answer**; the fourth had
a truth **nobody could see**. A telegraph line is a third thing: the answer
exists, exactly one participant put it there and exactly one other needs it, and
the entire middle of the application is people carrying something they do not
understand.

That is a nice little world to build for its own sake. It also has the most
famous failure in the history of signalling — a message that arrives, is
well-formed, reads as good English, and is not what was sent — and a real
procedure that catches one cause of it and structurally cannot catch the other.

## Who is in it

| participant | kind | what it is |
|---|---|---|
| the Admiralty office | native weave, office `admiralty` | Codes a message out of the book on its desk, hoists it one setting a minute, and compares Portsmouth's repeat against its own file of the **numbers**. |
| six hilltop stations | native weave, one office each (`station.1`…`station.6`) | Chelsea, Putney Heath, Coombe Warren, Netley Heath, Hascombe, Blackdown. Each holds one frame at a time and shows it to the next hill along on the minute. Each keeps a journal. |
| the Portsmouth office | native weave, office `portsmouth` | Takes the numbers down, repeats them back, and writes the message out of the book on **its** desk once London says CORRECT. |
| the two vocabularies | **`.so`s, loaded at runtime**, offices `book.admiralty` and `book.portsmouth` | The printed codebook. Answers three questions and asks none. Its grant lets it speak to the one desk it sits on. |
| the day | not a weave | Owns the clock on the wall, the weather on the hills, and the traffic. |

The **vocabulary** is the thing in the shared library, and that is not an
arbitrary choice about which file to compile separately. A codebook is written
by somebody else, printed, issued in editions, and **each office holds its own
copy**. Two offices holding two different editions, neither able to tell by
looking at the other, is the whole of the second failure above — and you cannot
model it at all with one object that both ends share. Changing the book is a
real operational event: at midday the 1806 vocabulary comes into force, the old
book comes off both desks and a different one goes on.

## What each participant may say, and to whom

The grants are narrow and every one of them is different:

```text
station.k        Frame -> the hill above it, Frame -> the hill below it
                 and nothing else, ever

admiralty        Frame -> station.1
                 Coding, WhichBook -> book.admiralty
                 may observe Visibility

portsmouth       Frame -> station.6
                 Decoding, WhichBook -> book.portsmouth

book.admiralty   ThisBook, Coded, Decoded -> the Admiralty desk, and nobody else
book.portsmouth  ThisBook, Coded, Decoded -> the Portsmouth desk
```

The first of those is **the sentence the whole application rests on**. Every
claim the day makes about where a message has been comes from laying the six
journals side by side; if a station could reach past its neighbours, the
journals would prove nothing at all. So it is challenged rather than asserted —
see the controls below.

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

or work the line by hand:

```sh
./build/telegraph ./build/vocabulary-1805.so ./build/vocabulary-1806.so
./build/telegraph ./build/vocabulary-1805.so ./build/vocabulary-1806.so --in-a-hurry
./build/telegraph ./build/vocabulary-1805.so ./build/vocabulary-1806.so --half-the-line
./build/telegraph ./build/vocabulary-1805.so ./build/vocabulary-1806.so --fog
```

`build/` is gitignored repository-wide. Build the substrate outside this
repository.

## What success looks like

```text
     3  ADM      to the Port Admiral, Portsmouth: "FRENCH FLEET AT SEA SAIL"
     3  ADM      the line is working through to Portsmouth
     3  ADM      coded in 8 hoists; up she goes
  --    a squall crosses Netley Heath
    13  NETLEY   read .#.#.# (21) through the squall; it was .#.### (23)
    17  PMH      taken down in 8 hoists -- repeating back
    31  ADM      the repeat DOES NOT agree at hoist 7: sent .#.###, back .#.#.# -- call for a repeat
    46  PMH      taken down in 8 hoists -- repeating back
    60  ADM      the repeat agrees -- CORRECT
    67  PMH      delivered to the Port Admiral: "FRENCH FLEET AT SEA SAIL"
```

and the whole line, minute by minute, as the day's own tap saw it — the message
is the diagonal:

```text
   min   ADM  CHEL PUTN COOM NETL HASC BLAC  PMH
    10    .    23   22   20   18   17    9    1
    11    .     4   23   22   20   18   17    9
    12    .    .     4   23   22   20   18   17
    13    .    .    .     4   23   22   20   18
    14    .    .    .    .     4   21   22   20      <- the 23 became a 21
```

and then the six journals, read back through the ordinary gate, saying where:

```text
    message 1, down the line
      CHELSEA     1 9 17 18 20 22 23 4 ...
      PUTNEY      1 9 17 18 20 22 23 4 ...
      COOMBE      1 9 17 18 20 22 23 4 ...
      NETLEY      1 9 17 18 20 22 21 4 ...
      HASCOMBE    1 9 17 18 20 22 21 4 ...
      BLACKDOWN   1 9 17 18 20 22 21 4 ...
```

Nobody in the two offices could have worked that out. It comes from the hills'
own accounts of themselves, laid side by side.

## Expected refusals — every one of them is the product working

```text
a frame that is not my neighbour's     not repeated (the STATION decides)
a frame I cannot see through fog       not repeated, and the hill says so
a word that is not in this book        the message does not leave the desk
a message written with another book    not written out (the OFFICE decides)
a repeat that does not agree           call for a repeat
signalling past my neighbours          CapabilityDenied -- Loom decides, and it
                                       is the only thing Loom refused all day
```

Five owners: the station, the office, the book, the domain, and the bus.

## The controls, and what they are for

`A GOOD DAY ON THE LINE` would be worth nothing if the day could not tell a good
message from a bad one.

**`--in-a-hurry` — the most dangerous thing this application can do.** The same
squall crosses the same hill at the same minute; the only difference is that the
admiral marks the message *no repeat required*, which is a real operational
choice for urgent traffic and is carried on the line as its own signal. Then:

```text
  every hill repeated faithfully        could not read 0, clashes 0
  the message arrived complete          8 hoists, ends with ENDS
  nothing was refused anywhere          bus refusals 0
  the Port Admiral was told             "FRENCH FLEET AT SEA ANCHOR"
```

Every mechanical measure inside that run reports success, and the fleet has been
told to do the opposite of what London ordered. *The scenario asserts that this
goes wrong*, so "the message arrived as sent" in the default run is a
measurement rather than a constant.

**`--half-the-line` — the failure the repeat cannot see.** Portsmouth alone
takes the 1806 vocabulary. The same message goes down twice: once carrying the
vocabulary signal, and once in the old form without it.

```text
  with the signal      NOT WRITTEN OUT -- written with vocabulary 1;
                       this office holds vocabulary 2
  without it           "SPANISH SQUADRON OFF USHANT RETURN"
  and                  the repeat AGREED, and every journal on the line
                       holds exactly the same numbers
```

`FRENCH FLEET AT SEA SAIL` and `SPANISH SQUADRON OFF USHANT RETURN` are the same
five numbers. There is nothing wrong on the line and nothing to find in the
journals; the only thing standing between the two is one hoist near the front of
the message saying which book it was written with.

**`--fog` — the line is interrupted.** Hascombe cannot see Netley Heath.
Nothing reaches Portsmouth, the message is in four journals and not in the other
two, and the Admiralty — which reads the hills' own claims about what they can
see, without asking any of them anything — names Hascombe before it even sends.

**And two frames have to be forged.** Everything above leaves the *grants*
entirely unexercised: every refusal in the list is somebody in the domain
deciding something. A station's own code only ever addresses its two
neighbours, so the honest API cannot put either of these frames on the wire and
the day forges them with `Switchboard::office_send_to_role_as` — the verified
host door, which stamps the sender from the caller's own authority and then
applies every ordinary delivery law.

```text
Coombe Warren, SPEAKING AS ITSELF, hands its frame straight to Portsmouth
    authorship SUCCEEDS -- station.3 really is its office, so the domain's
    own "is this my neighbour's frame?" rule would have been satisfied
    and the grant is the ONLY thing left
        -> CapabilityDenied on Frame

Coombe Warren shows Netley Heath a frame CLAIMING TO BE HASCOMBE
    the destination is one its grant permits, so the grant is not what
    stops it
        -> RoleAuthorshipDenied on Frame, and nothing is queued at all
```

```text
  bus refusals seen        2  [CapabilityDenied on Frame,
                               RoleAuthorshipDenied on Frame]
```

Two, across three hundred and forty-four frames, and between them they pin the
two facts the journals depend on: **a hill cannot reach past its neighbours**,
and **a hill cannot pretend to be a different hill**. Portsmouth's
`not my neighbour` counter stays at **0**, which is the interesting half of the
first: the grant refused the delivery outright, so the domain rule behind it was
never reached.

## Known friction

All recorded in `../FRICTION.md`. This application opened **no new entry**, and
confirmed two old ones:

- **F-04** (fifth independent consumer) — `mount()` takes no role, so the eight
  office-holding native weaves here need the same local binder the other four
  experiments each wrote from scratch. Eight uses; thirty across the era.
- **F-05** — the `wsl.exe` argument handoff, again, in exactly the shape ZNL-03
  corrected the workaround for. Not Zen's.

**F-02 was not met**, for the second time in five. Nothing on this line expects
an answer from the direction it sent: a hoist is shown and either the next hill
repeats it or it does not, and the two offices learn each other's fate from the
traffic itself. The end-to-end repeat is what a domain builds when it wants to
know a message arrived, and it is the domain's, not a substitute for anything.

The notable **non**-friction is the one that made the section above possible:
the host-side forging surface is complete. `send_as`, `send_as_to_role`,
`publish_as`, `office_send_as`, `office_send_to_role_as`, `office_publish_as`,
`claim_as` and `office_claim_as` are all public, so a day can put on the wire
any frame a participant could have produced — *including a verified
office-authored one* — with the sender stamped from its own root authority
rather than from a payload. Challenging a boundary the honest API cannot express
therefore costs two lines and needs no machinery.

**F-02 was not met**, for the second time in five: nothing on this line expects
an answer from the direction it sent, and the two offices learn each other's
fate from the traffic itself rather than from a ticket.

None blocked anything and none is a request.

## What this deliberately does not test

- **Zengine.** Not used, not vendored, not needed, not missed.
- **Loom's own suite.** Its official verifier was not run; nothing here is
  evidence about Loom's green.
- **Prepared replacement, reload, revival, the letter, graceful swap,
  publications, relays, pokes, deferred answers.** A signal station that cannot
  see is not a station that has died, and the line's answer to an interruption
  is to wait for the weather. The domain never reached for any of them.
- **Persistence.** The line closes at dusk and the journals are paper. Nothing
  here is written down between processes.
- **Isolation.** The containment note is printed at start-up and says
  `no OS sandbox`. The enforced tier is not reachable from the exported package.
- **Concurrency and the real clock.** The day owns the minute; dispatch is
  single-threaded FIFO and this application depends on it precisely — the wave
  moves one hill per minute *because* every station's minute is delivered before
  any frame that minute produces.
- **Being a real telegraph.** Six hills, two editions and three messages is a
  demonstration. The line signals, the vocabulary numbering and the repeat
  procedure here are this application's own arrangement in the spirit of the
  thing, not a transcription of Admiralty practice.
