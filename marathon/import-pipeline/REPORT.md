# Project 4 — Media / import pipeline

**Verdict: GREEN.** 26 cases / 133 assertions (25/129 at the time of the matrix, plus one case
added to close a gap the matrix found). Mutations **14 RED, 1 GREEN** — and the GREEN is the gap
that case closes.

**Pinned against:** Loom `78d64ea`, Zengine `f6a4c69`, ABI v4.

---

## Purpose

The Timer package authored a conversation shape and the kitchen used it unchanged:

```
request → available choices → selected choice → resolved choice → receipt
```

with two rules attached: **refusal is an outcome and never a menu choice**, and **an unknown
spelling is refused rather than guessed at**. Two sightings is a candidate. This project is the
**third**, from a domain chosen to be as different as possible while still having the shape:

> **The requester cannot know the menu in advance.** A diner knows the words "grill" and
> "any_station" before ordering. Somebody importing a file does not know what is in it — the
> options are *discovered by the service*, and the conversation cannot be completed in one exchange
> even in principle.

---

## Domain model

One role, `import.pipeline`. `ImportAsset{ticket, file}` is answered with **the menu** — not an
acknowledgement, because nothing has been agreed yet. `ChooseOption{ticket, menu, choice}` is
answered with a **resolution**. The receipt arrives later as an ordinary message.

A menu has an **identity** (`ImportOptions::menu`), minted by the importer. One field answers three
hostile cases: the stale choice, the duplicate choice, and the choice that races a replacement.

---

## Does the shape reproduce? **Yes, and with one part earning its name.**

| element | Timer | kitchen | import |
|---|---|---|---|
| request | ✓ | ✓ | ✓ |
| available choices | ✓ | ✓ | ✓ **discovered, not declared** |
| refusal is an outcome, not a menu entry | ✓ | ✓ | ✓ (a file that admits nothing is *refused*, not offered an empty menu) |
| unknown spelling refused, never guessed | ✓ | ✓ | ✓ |
| **resolved choice** | ✓ | ~ (collapsed into the receipt) | ✓ **a distinct step that earns itself** |
| receipt | ✓ | ✓ | ✓ |

The kitchen's *resolved choice* and its *receipt* were the same message. Here they cannot be: a
requester may name something **underspecified** (`h264` when the file admits two h264
interpretations), and the importer resolves it to exactly one, says which and why, **before the
work starts** — so the requester can disagree and abandon. That is the difference between a service
that guesses and one that decides in the open.

**Third independent sighting.** The shape is real.

---

## THE WALL THREE PREVIOUS PROJECTS COULD NOT BUILD

> **`HOSTILE: A FORGED CHOICE FROM THE WRONG PARTICIPANT IS REFUSED`**

The rogue is handed *everything* a bus-watcher could collect: the ticket, the menu identity, a
valid option label, and the correlation. None of it is a secret. It is refused — by an ordinary
equality against the bus-stamped sender.

**Why this works here and nowhere else: the counterparty is a specific weave, not a role.** The
importer offered a menu *to a particular requester* and recorded its id at the moment of the ask.
Three projects have had to write "and there is no way to ask Loom whether the sender holds the role
it claims"; this one does not have to.

That is not the hole closing. It is the same hole seen from the one angle where it does not matter:
**identity works when you are talking to somebody, and fails when you are talking to whoever
currently holds an office.** Project 5 takes the other angle deliberately.

---

## Replacement behaviour — the THIRD distinct answer

> **A menu belongs to the life that offered it.**

| | what crosses | the conversation |
|---|---|---|
| kitchen | the **work** | continues |
| download manager | the **obligation** | **ends**, in an honest failure |
| build farm | the **intent** | **restarts**, as a numbered attempt |
| **import pipeline** | the **question** | **REOPENS**, with a new menu identity |

The successor re-derives the options (they are a function of the file, so it can), mints a **new**
menu, and re-offers — as an **ordinary** message, because the answer right died with the life that
earned it. A requester can see the difference (`answers_ask()`), and the suite measures that it
does: `menus_attested == 1`, `menus_unattested == 1`.

A conversation that had already **resolved** simply finishes: a resolved choice is a *label*, which
is a word, and words cross.

And the choice that raced the replacement is refused with the current menu named in the refusal, so
the requester is never left guessing.

### A real defect the suite found

**The successor's menu counter restarts at 1**, so its first menu was called `m1` — exactly the
name the requester was still holding from the predecessor. The stale-choice check then passed on a
**name collision** and a choice was acted on against a different set of options.

The identity is per-incarnation; the **namespace must not be**. `next_menu` now crosses in the
preparation ask, and mutation 14 pins it. This is the third project to need a minted-identity
counter to cross a replacement (the kitchen carries `next_job`; the build farm's attempt number has
the same shape).

---

## Hostile cases

| case | what happened |
|---|---|
| a choice not on the menu | refused, naming how many were offered |
| a stale menu identity | refused, naming the menu that *is* open |
| a duplicate choice | refused, naming what was already decided; the first decision stands |
| **a forged choice from the wrong participant** | **refused** — the wall is real |
| a file the catalogue cannot read | refused, naming the catalogue |
| a file that admits nothing | refused; **not** an empty menu |
| a duplicate ticket | refused |
| the 13th conversation | refused against the published bound |
| abandoning a conversation nobody has | `zen.Refused` |
| a forged `ImporterReady` | `InvalidReadiness`; the transaction stays `Preparing` |
| a candidate that ships another catalogue | authentic refusal → `CandidateRefused` |
| a candidate handed a file it cannot read | authentic refusal |
| a candidate handed more conversations than the bound | authentic refusal *(case added after mutation 12 found it unwatched)* |
| an offer with no transaction in flight | counted, not crashed |

---

## Sugar audit

| | count |
|---|---|
| `PreparedReplacement` facade operations used | `start` 7, `ask` 7, `offer_current_answer` 2, `commit` 3, `abort` 1, `state` 12, `take_outcome` 8 |
| **raw prepared-replacement operations in app code** | **0** |
| manual transaction ids in domain payloads | **0** |
| manual lifecycle authority wiring | **0** |
| manual candidate cleanup | **0** |
| manual outcome filtering | **0** |

---

## Authoring friction

- **F2 (3rd sighting)** — the coordinator/handle raw pointer, written a third time, identical.
- **F9 (3rd sighting)** — `WeaveId` stringified onto the wire.
- **F12 (new)** — fifth-project fixture repetition; see FRICTION.md.
- **F13 (new)** — **a minted identity's namespace does not cross a replacement**, and nothing
  warns. Three projects have now needed a counter to cross; two of them found out from a test.

---

## Verdict

```
GREEN
```

## What this project votes for

| candidate | vote | evidence |
|---|---|---|
| order/menu resolution | **✓✓** | third independent sighting, and the *resolved choice* step finally earns its own name |
| role authorship (provenance) | **✓ (inverted)** | the one project where the check IS performable — because the counterparty is a weave, not an office. That is evidence *about the shape of the gap*, not against it |
| promise/responsibility book | ~ | a book with a bound and visible refusal, but no patience and no watchdog: nothing here can go silent, because every step is somebody's answer |
| describe-then-hand-over | **✓** | fourth sighting, fourth purpose (the question, not the work/obligation/intent) |
| activation hold/replay | **×** | the successor's startup act happens *at* activation |
| **minted-identity namespaces** *(new)* | **✓** | third sighting of a counter that must cross, first one where not crossing it was exploitable |
