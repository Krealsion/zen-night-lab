# Role authorship — the focused replay's verdict

**Pinned substrate:** Loom `30eab0a` (ABI v5), vendored under `vendor/loom`.
**Result:** 3 suites, 4 cases / 59 assertions, all green. 3 application-level
mutations, 3 RED (canary hand-proven at its exact expected assertions before
the matrix was believed). `original/` and `marathon/` untouched.

## Classification: CLOSED

The seam the marathon recorded — *applications need to distinguish personal
speech from deliberate office speech when trust follows a replaceable role* —
is closed by `mail.as_role(...)` / `mail.authored_from_role(...)`. Each replay
below states what the marathon could not do, and what the same application
does now. No replay needed a distinct missing truth; nothing was force-fit.

## Replay A — lobby (the primary acceptance case)

The strict player acts on `MatchCreated` iff
`mail.authored_from_role("lobby.matchmaker")`. Measured:

- the office-authored push is **joined**;
- the SAME holder's personal push of the SAME shape is **rejected** — the
  statement no identity check could ever separate;
- the rogue (same artifact, no office): its office attempt refuses at the
  seam with `RoleAuthorshipDenied` and NOTHING arrives; its personal
  same-shaped forgery arrives and is **rejected**;
- after a real admission (sealed candidate, `admit_candidate`, role moved),
  the honest successor's push is **joined** — different sender id, same
  authored office — and the retired predecessor's NEW attempt refuses.

**What disappeared with the pull workaround.** The marathon's attestable
lobby required inverting the push into `SeekMatch` + a deferred answer per
waiting player. Removed wholesale: no `SeekMatch` shape exists in this
vocabulary, `grep defer lobby/` finds nothing, 100 strictly-verified pushes
consumed **0** of the Loom-wide 64 deferral slots — and the strict player
accepted the honest successor that the pull wall structurally refused
(under pull, a replaced office strands every waiting receiver, and the
successor's only possible statement was one a strict receiver must treat
as a forgery).

## Replay B — build farm (both shapes, or incomplete)

- `JobDone` rides `as_role("farm.worker.a").send_to_role("farm.dispatcher",…)`
  — authored as the worker office, delivered to the dispatcher office, the
  two facts never conflated. The strict dispatcher accepts it.
- `WorkerOpen` — the announcement that is evidence — is an office-authored
  **publication**: every listener (the dispatcher and an unrelated observer)
  verified it. This is the case the pull workaround could not touch: an
  observer of a publication had nothing to attest, by construction.
- The rogue's office attempts refuse at the seam (2 × `RoleAuthorshipDenied`,
  nothing fans out); its personal same-shaped statements arrive and verify as
  nothing. The strict farm acted only on office truth: no fabricated success
  entered the books, no forged announcement destroyed work.

## Replay C — download manager (the architecture consequence)

The marathon's sharpest pressure: one attestation per operation, so a
long-lived operation chose which half to defend. Now:

- the **acceptance** is the authenticated ANSWER, spent immediately
  (`answers_ask() == true`, authored office empty — it does not need one);
- the **terminal truth** arrives minutes later as office-authored ordinary
  speech (`authored_from_role("download.service") == true`,
  `answers_ask() == false` — it does not need to be an answer);
- the two facts prove DIFFERENT things and the client verifies both;
- deferred-answer usage across the whole ceremony: **0**. No capability was
  held for the duration; nothing strands across a replacement.

**"Which half do you attest?" is no longer a question.** Attest both, with
the proof each half actually means. That is the pressure relieved — not by a
second answer, but by a second *kind* of fact.

## Mutations (application-level: the feature doing its job in app code)

| id | cut | verdict |
|---|---|---|
| m1 (canary) | lobby matchmaker forgets `as_role` on its push | RED — hand-proven first: the strict player refuses every push, at the exact expected assertions |
| m2 | farm worker speaks/publishes personally | RED — dispatcher and observer refuse the evidence |
| m3 | download service's terminal truth goes personal | RED — the client cannot verify the second half |

Final clean rebuild after the matrix: 3/3 suites green.

## What remains, honestly (seams, not gaps)

- Out-of-process weaves fail closed in both directions (the pipe carries no
  attestation in V1) — pinned in the Loom's isolation suite, not replayed
  here.
- No public door produces a combined answer+office fact; no replay wanted
  one. The representation admits it if a consumer ever does.
