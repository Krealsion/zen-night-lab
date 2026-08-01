# Project 5 — Lobby / matchmaking

**Verdict: GREEN** — and the green is the point, because what it proves is a *cost*, not a
capability. 16 cases / 95 assertions. Mutation results in FINAL-REPORT.md.

**Pinned against:** Loom `78d64ea`, Zengine `f6a4c69`, ABI v4.

---

## Purpose

Four projects had written some version of *"there is no way to ask Loom whether the sender of this
message holds the role it claims"*, and every one of them treated it as bookkeeping. This project
makes it a security question with teeth:

> **Are speaker identity and office authorship observably different security facts in a real
> application?**

In a lobby the statement that matters is `MatchCreated`. A player that believes one **leaves the
lobby and connects somewhere**. So a weave that can forge it pulls players out of a real lobby into
a fake match. That is not a misreported counter.

---

## The case that makes the distinction unavoidable

The matchmaker is an ordinary weave. It **holds an office** (`lobby.matchmaker`) and it also has a
**personal capacity** — it can say things as itself. Those are the same `WeaveId`.

So *"which exact weave sent this?"* cannot separate:

```
the matchmaker, speaking as the matchmaker   ->  act on it
the matchmaker, speaking personally          ->  it is chatter
```

And role-addressing cannot help, because **a role is a property of the RECEIVER of a message, never
of its sender.** `send_to_role` says where a message went; nothing on the wire says what capacity it
came from.

---

## The answer is not "impossible". It is a trade nobody can make.

Two matchmakers from one source, and the pair *is* the finding.

|  | `lobby-matchmaker-push` | `lobby-matchmaker-pull` |
|---|---|---|
| trigger | a **fact about the world** ("enough are ready") | a **question** (`SeekMatch`) |
| `MatchCreated` is | an ordinary directed message | **Loom's authenticated answer to that player's own request** |
| forgeable by an unrelated weave | **yes** — measured | **no** — measured |
| a strict player | refuses **everything, including real matches** | works |
| cost | none | see below |

> **A push is triggered by a fact about the world, and Loom attests ANSWERS.** There is nothing to
> attach an attestation to. To get one you must re-found the whole service on each player's own
> question — which is not a refactor, it is a different service.

### What the pull style costs, measured

1. **Its capacity is one Loom's.** `kMaxDeferredAnswers` is 64 and belongs to the process, not the
   weave. A lobby of sixty-five waiting players exhausts every other weave's ability to hold a
   conversation. *(This is the download manager's F10, found independently, now with a security
   motive rather than a bookkeeping one.)*
2. **A replaced office strands every waiting player.** The answer right belongs to the life that
   earned it. The successor inherits the *fact* that a player is waiting — words cross — and can
   only speak to them as an ordinary message.
3. **…so a strict player refuses the HONEST SUCCESSOR for exactly the reason it refuses a forger.**
   That is case *"THE COST"*, and it is the sharpest result here: the wall the pull style builds
   cannot distinguish the office's legitimate heir from an attacker, because from the receiver's
   side there is nothing to distinguish.

---

## The five speakers (the brief's comparison)

| # | speaker | push lobby | pull lobby |
|---|---|---|---|
| 1 | the **current role holder**, as the office | unattested | **attested** |
| 2 | **the same weave, personally** | *indistinguishable from 1* | **refused** — the only separation available anywhere |
| 3 | the **predecessor**, after replacement | **cannot speak at all** — the admission SEALS it | same |
| 4 | an **unrelated weave** | *indistinguishable from 1* | refused |
| 5 | the **successor**, after replacement | fine (nothing was ever attested) | **refused** — it is honest and it looks exactly like 4 |

Row 3 is the substrate's, and it is complete: a retired incumbent is sealed by the admission and may
speak only to the coordinator that sealed it. Rows 2, 4 and 5 are the application's problem, and the
pull style trades row 5 for rows 2 and 4.

---

## And the observers have no move at all

`MatchCreated` is addressed to the players. Everybody *else* who needs to know a match happened —
the **registry**, most obviously, since those players are no longer in the lobby — is not a party to
any of those conversations. What reaches them is `MatchStarted`, a **publication**.

**A publication can never be attested to anybody, in either style.** Case *"THE REGISTRY IS A
RECEIVER TOO"* measures it: one forged publication and the two halves of the system disagree about
who is in the room — the strict players correctly refuse and stay put, while the registry empties.

So the pull workaround protects the *parties* and does exactly nothing for an *observer*. Any
system where a third party must act on an office's statements has no option at all today.

---

## Zen surface used

Everything the other projects used, plus: **publication as a first-class weakness** rather than a
convenience, and the seal as a *security* property (row 3 above) rather than a lifecycle detail.

`loom::PreparedReplacement`: both styles replaced, committed and aborted; authentic refusal on a
house-rule mismatch; `NoRoleHolder` and `CandidateLoad` inspected.

---

## Sugar audit

| | count |
|---|---|
| facade operations used | `start` 6, `ask` 5, `offer_current_answer` 2, `commit` 3, `abort` 1, `state` 8, `take_outcome` 5 |
| **raw prepared-replacement operations in app code** | **0** |
| manual transaction ids in domain payloads | **0** |
| manual lifecycle authority wiring | **0** |
| manual candidate cleanup | **0** |
| manual outcome filtering | **0** |

---

## Verdict

```
GREEN
```

Not PROVEN BLOCKED — the distinction *is* expressible, at a price. Recording it as blocked would
have been the easier and less useful answer; recording the price is what makes it an errand somebody
can size.

## What this project votes for

| candidate | vote | evidence |
|---|---|---|
| **role authorship (provenance)** | **✓✓✓** | fifth sighting, and the first where the consequence is a player leaving for a fake server rather than a wrong number in a log. Plus the first measurement of what the workaround costs |
| which-half-to-attest | **✓** | third sighting; here the choice is *push or pull*, i.e. whether the service can be attested at all |
| promise/responsibility book | **×** | tested and did not appear: a lobby holds membership, not promises. Nothing here can go silent |
| order/menu resolution | **×** | no menu |
| activation hold/replay | **×** | the successor's startup act happens at activation |
| describe-then-hand-over | **✓** | fifth sighting — and the first where the description includes *what cannot be handed over* (`held_answer_rights`) |
| **attestation is for parties, not observers** *(new)* | **✓** | the registry case: a publication is unattestable by construction |
