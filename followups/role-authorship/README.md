# Follow-up: role authorship (R2D-0)

The marathon (`../../marathon/`, pinned Loom `78d64ea`, ABI v4) found one core
truth applications repeatedly needed and could not express: **a weave may hold
an office and still speak personally**, and nothing on a delivery could prove
which capacity a statement came from. Five sightings; both application-level
workarounds built and priced; the seam recorded as the system's most
evidence-backed core candidate.

R2D-0 built the missing fact: `mail.as_role(R).send/send_to_role/publish`,
verified by Loom at the authorship moment, carried as immutable delivery
provenance, readable as `mail.authored_from_role(R)` — across the dynamic seam
(ABI v5). This follow-up replays the three sharpest marathon findings against
the new substrate and classifies the former seam.

**The marathon and the original are untouched.** They are evidence about the
substrates they ran on. This follow-up is pinned to its own vendored Loom —
see [vendor/README.md](vendor/README.md) for the exact commit.

## The three replays

| replay | the marathon finding it answers | what closes it |
|---|---|---|
| [lobby](lobby/) | a forged `MatchCreated` sends a player to an attacker's server; the pull workaround's wall kept out the forger AND the honest successor | the strict player checks the OFFICE, so it accepts the push, rejects the same holder's personal chatter and every rogue, and accepts the honest successor for free |
| [build-farm](build-farm/) | a forged `WorkerOpen` — an *announcement as evidence* — destroyed healthy work, and a publication was unattestable by construction | office-authored publication: every listener verifies the announcement; the rogue's same-shaped one verifies as nothing |
| [download-manager](download-manager/) | one attestation per operation forced "which half do you attest?" — acceptance or terminal truth, never both | the acceptance is the authenticated ANSWER; the terminal truth is office-authored ordinary speech; both verifiable, no deferred capability held for the duration |

Result and classification: [REPORT.md](REPORT.md).

## Building

WSL/GCC, exactly like the marathon:

```sh
cmake -S . -B build && cmake --build build -j"$(nproc)" && ctest --test-dir build
```

Mutations: `bash mutate.sh` (three application-level cuts, canary first — each
removes a deliberate `as_role` and expects the strict receivers to redden).
