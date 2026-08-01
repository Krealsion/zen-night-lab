# FRICTION — the running ledger

Written **as it happens**, never reconstructed at report time. One entry per moment where saying
the thing was harder than meaning it.

Categories: `domain complexity` · `C++ tax` · `Loom plumbing` · `Zengine plumbing` ·
`missing vocabulary` · `diagnostic weakness` · `real unavoidable decision`

Severity: `paper cut` · `recurring friction` · `architecture pressure`

---

## F1 — "activation first" means something narrower than it sounds

**PROJECT:** kitchen-replay
**TASK:** assert that the prepared candidate's activation really is first.

**WHAT I WANTED TO SAY:** *the candidate's first delivery is its activation.*

**WHAT I HAD TO WRITE:** a tap, a MARK taken at the moment of commit, and an assertion about the
first delivery *after the mark* — because the preparation conversation is delivered to the
candidate before the activation and must be.

**WHY IT IS FRICTION, not a misunderstanding:** I wrote the naive assertion first and it failed
with `PrepareStation != zen.Activated`. The substrate is right and the phrase is the problem:
"activation first" is a claim about the candidate's first delivery **as part of the world**, and
an author who has only read the phrase writes the assertion I wrote. The two facts that make it
unambiguous — the candidate is spoken to while sealed, and the seal is outside the world — are
both documented, in two different places, neither next to the phrase.

**CATEGORY:** diagnostic weakness (prose, not code)
**SEVERITY:** paper cut
**SECOND SIGHTING:** none

---

## F2 — the handle is the host's, the conversation is a weave's, and nothing in the type system says so

**PROJECT:** kitchen-replay
**TASK:** let the coordinator offer the candidate's answer to the transaction.

**WHAT I WANTED TO SAY:** *when the candidate answers, offer that answer to this replacement.*

**WHAT I HAD TO WRITE:** a `loom::PreparedReplacement*` raw pointer on a host-owned struct, handed
to the coordinator weave at mount time, re-pointed by hand on every new transaction
(`new_upgrade()`), and null-checked in the handler because a `StationReady` can arrive when no
replacement is in flight.

**WHY:** `PreparedReplacement` needs `Switchboard&`, which a weave can never hold — correctly, and
the header says why. But `offer_current_answer` must be called *from inside the coordinator's
delivery*, which only a weave can be. So every prepared replacement has a host object and a weave
object that must be wired together by the application, and the wiring is a bare pointer whose
lifetime nothing checks. Loom's own suite does the same thing (`PreparedReplacement*& which`), so
this is the shape, not my invention.

**CATEGORY:** Loom plumbing
**SEVERITY:** recurring friction (expect it in every project that replaces a service)
**SECOND SIGHTING:** **download-manager.** Written independently, arrived at the identical shape:
`OpsDesk::upgrade` is a `loom::PreparedReplacement*`, re-pointed by `new_upgrade()`, null-checked
in the handler. Two projects, two identical raw pointers across the same boundary.

---

## F3 — the bus authenticates THAT the candidate answered, not WHAT it said

**PROJECT:** kitchen-replay
**TASK:** map the candidate's domain answer onto `PreparationAnswer::{Ready,Refused}`.

**WHAT I WANTED TO SAY:** *the candidate refused, so the transaction ends with its verdict.*

**WHAT I HAD TO WRITE:** the same thing, but the mapping from `StationNotReady` to
`PreparationAnswer::Refused` is **the coordinator's**, and a coordinator that offers `Ready` while
handling a `StationNotReady` gets a perfectly valid, fully authenticated `Ready`.

**WHY IT MATTERS:** the authoring page says "let the candidate answer for itself" and "a refusing
candidate is offered the same way". Both true. But the security fact the substrate establishes is
*an authentic answer to this exact ask arrived from this exact candidate* — the Ready/Refused
verdict rides on the coordinator's honesty. That is a defensible boundary (the coordinator is
already trusted enough to be the one party a sealed candidate may speak to) and it is not written
down anywhere I could find. Mutation 08 exists to keep this visible.

**CATEGORY:** missing vocabulary *(or: honest prose about an existing boundary)*
**SEVERITY:** paper cut — but a sharp one, because it reads as stronger than it is
**SECOND SIGHTING:** none

---

## F4 — the two replacement ceremonies are disjoint, and only the application can compose them

**PROJECT:** kitchen-replay
**TASK:** replace a live station without losing the dish it is holding.

**WHAT I WANTED TO SAY:** *replace this station with that one, verify the successor first, and
carry the work across.*

**WHAT I HAD TO WRITE:** two different conversations with two different parties, invented for this
application:

| | graceful swap (Weave Manager) | prepared replacement |
|---|---|---|
| talks to | the OUTGOING holder | the INCOMING holder |
| preserves work | yes (the letter) | **no** — the incumbent is never told |
| verifies the successor | **no** | yes |
| window with an unasked holder | yes | none |

Neither gives both. The application's answer — and it works — is that the **preparation window is
the one interval in which the incumbent is alive and the successor is reachable**, so the owner
asks the incumbent to describe its work (`DescribeWork` → `WorkDescribed`, an ordinary question
that changes nothing) and hands that description to the candidate inside the preparation ask.

**WHY IT IS FRICTION AND NOT JUST DOMAIN WORK:** the composition is sound but it is *load-bearing
and invisible*. Nothing in either ceremony hints that the other exists, and an author who reaches
for prepared replacement because it is the newer, safer-sounding one silently gets hard-swap
semantics for work in flight. The kitchen only noticed because it already had a watchdog that
turns silence into a word.

**CATEGORY:** architecture pressure
**SEVERITY:** architecture pressure
**SECOND SIGHTING:** **download-manager**, and it needed the composition for a DIFFERENT REASON,
which is what makes it independent evidence rather than a copy. The kitchen used the preparation
window to carry *work* across. The download manager cannot carry its work across at all — a
half-downloaded file IS its bytes — so it uses the same window to carry the **obligation**: who is
owed a terminal message, about what, and how far it had got. Same mechanism, opposite purpose. The
thing that must cross a replacement is not the work; it is the promise.

---

## F5 — a weave still cannot see the fate of its own send

**PROJECT:** kitchen-replay
**TASK:** notice that a station is gone.

**WHAT I WANTED TO SAY:** *did that reach anyone?*

**WHAT I HAD TO WRITE:** the entire watchdog — a role-addressed sweep, a patience counter per
ticket, and the discipline that every ticket leaves the book through a message.

**STATUS:** Night One's finding, **re-tested and unchanged**. `send_to_role` to an unheld role is
refused, and the sender is told nothing: no ticket outcome, no event. The host can see it from a
tap, which is why `MARATHON_TRACE=1` exists.

**CATEGORY:** Loom plumbing / diagnostic weakness
**SEVERITY:** architecture pressure
**SECOND SIGHTING:** Night One (this is the second sighting; the first was the original kitchen)

---

## F6 — reading a loaded weave's own state needs a hand-built schema

**PROJECT:** kitchen-replay
**TASK:** find out what the answering weave BELIEVED happened, from the host.

**WHAT I WANTED TO SAY:** *what does that weave's `answered_ok` counter say?*

**WHAT I HAD TO WRITE:** either (a) `snapshot_bytes` → `parse` → build a `Schema` by hand that
mirrors the weave's private state shape → `admit` → `get(field)->as_int()`, which is what Loom's
own suite does with a `versioned_state_schema()` helper; or (b) route the number back as an
ordinary message. I chose (b), and the reproducer is better for it — but only because the weave
happened to be one I wrote.

**CATEGORY:** missing vocabulary
**SEVERITY:** paper cut (test-only, so far)
**SECOND SIGHTING:** none

---

## F7 — with a beat chain running, the beat is the queue's clock

**PROJECT:** kitchen-replay
**TASK:** pump "enough" for a multi-hop conversation.

**WHAT I HAD TO WRITE:** hand-tuned beat budgets at every call site, and two test failures that
looked like logic bugs and were arithmetic (`pump(2)` was not enough for the rogue's forged
message to be *dispatched*, so the assertion read as "the forgery never arrived" when it had
simply not been reached yet).

**STATUS:** Night One's finding, re-tested and unchanged. It is inherent to a single-threaded
pump-with-a-budget and is the price of a virtual clock that makes deadlines exact.

**CATEGORY:** real unavoidable decision *(the budget must come from somewhere)*
**SEVERITY:** recurring friction
**SECOND SIGHTING:** Night One

---

## F8 — the Emit-derived grant is the wrong shape for a test-only nudge, and the failure is invisible

**PROJECT:** download-manager
**TASK:** poke a bystander weave into trying to hold a conversation.

**WHAT I WANTED TO SAY:** *bystander, try to defer an answer.*

**WHAT I HAD TO WRITE:** the same thing — plus a hand-written `loom::Grant` with
`allow_to_any(AskBystander)` and `mount_granted` instead of `mount`, because `mount()` derives the
grant from the weave's declared `Emit<...>` and `AskBystander` is not something the weave emits.
It is the harness poking it.

**HOW IT PRESENTED:** the case failed as *"the bystander was never denied a slot"* — a plausible
architectural result — when the truth was *"the bystander never ran"*. The `send_as` was
`CapabilityDenied` at delivery, where no participant could see it. I only found it by adding a
counter for "were you asked at all", which is a thing I had to think to add.

**STATUS:** Night One recorded this exact failure as its friction 5 ("three separate *why is
nothing happening?* debugging rounds"). **SECOND SIGHTING**, in a different project, by a different
route, with the same invisibility. It is the same family as F5: a send whose fate the sender
cannot see.

**CATEGORY:** diagnostic weakness
**SEVERITY:** recurring friction
**SECOND SIGHTING:** Night One's kitchen (first), download-manager (this one)

---

## F9 — a WeaveId does not fit on the wire, so every project spells it as decimal text

**PROJECT:** download-manager
**TASK:** write down which client is owed a terminal message, in a message.

**WHAT I WANTED TO SAY:** `ZEN_FIELD(client)` where `client` is a `loom::WeaveId`.

**WHAT I HAD TO WRITE:** `std::string client;  ///< canonical decimal of the client's WeaveId`,
plus a `parse_u64` helper, plus a comment explaining that a `WeaveId` is unsigned 64-bit and the
wire's `Int` is signed, so an `Int` field would silently narrow the top half of the range.

**STATUS:** the kitchen has the identical field (`ExpediterTicket::diner`) with the identical
comment and the identical `parse_u64`. **SECOND SIGHTING.** Two projects have now independently
decided that the honest way to put an address in a payload is to stringify it.

**CATEGORY:** missing vocabulary
**SEVERITY:** paper cut, twice — which is how a paper cut becomes a candidate
**SECOND SIGHTING:** kitchen-replay (first), download-manager (this one)

---

## F10 — holding an answer for a long operation spends a resource that belongs to the whole Loom

**PROJECT:** download-manager
**TASK:** decide whether the service should hold its one answer right until the operation ends.

**WHAT I WANTED TO KNOW:** *is the original answer capability the right thing to hold for the
entire operation?*

**WHAT THE MEASUREMENT SAID:** no, and not for a taste reason.

`Switchboard::kMaxDeferredAnswers` is 64 and it is **one Loom's**, not one weave's. The suite runs
seventy concurrent transfers through the holds-the-answer build and then asks a weave with nothing
whatever to do with downloads to defer an answer of its own. **It cannot.** The download service
did not exceed its own bound (deliberately set to 80 so it would not) — it exhausted the
substrate's, on behalf of everybody.

This is not a defect: the bound is documented, published, and there for a good reason (an unbounded
deferred-answer table is a memory hole any weave could dig). It is a **consequence that an
application author has no reason to expect**, because nothing at the call site says the capability
being taken is global. `defer_answer()` reads like a local decision.

**WHAT AN APPLICATION CAN DO ABOUT IT:** answer immediately and speak in ordinary messages
afterwards — which is exactly what the kitchen independently chose, and what this project's
default build does. The cost is stated in F11.

**CATEGORY:** Loom plumbing / real unavoidable decision
**SEVERITY:** architecture pressure
**SECOND SIGHTING:** none — but see F11, which is the same coin's other face

---

## F11 — you get one attestation per operation, and the choice is which half to leave undefended

**PROJECT:** download-manager
**TASK:** make both the acknowledgment and the ending trustworthy.

**WHAT I WANTED TO SAY:** *the client should be able to tell that the service really took the job,
and that the service really finished it.*

**WHAT I HAD TO WRITE:** two services, and a case named `YOU GET ONE` that asserts the sum of
attested messages is exactly 1 in both.

- `download-service` attests the **promise**. The ending is an ordinary message a rogue can forge
  with a guessed correlation, and the suite measures that it does.
- `download-service-holds` attests the **ending**. The promise is ordinary — and worse, the
  attested ending is the one thing that cannot survive a replacement, because the answer right
  belongs to the life that earned it.

**STATUS:** this is Night One's finding — *"authenticity and continuity are mutually exclusive per
message; Loom authenticates the acknowledgment, never the fulfilment"* — reached independently by
a second package, from the opposite direction, and now with a number attached. **SECOND
SIGHTING**, and the strongest single result of this project.

**CATEGORY:** architecture pressure
**SEVERITY:** architecture pressure
**SECOND SIGHTING:** Night One's kitchen (first, as prose), download-manager (this one, as a
measurement)
