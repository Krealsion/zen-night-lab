# Mutation / canary campaign — the ten semantic lies

Baseline: commit `c85e351` (90 witness cases + safety-honesty CLI test, all
green). Discipline followed: baseline committed BEFORE mutating; canary #4
hand-proven first; every mutation restored from the committed baseline
(`git checkout --`), never the other way around.

| # | the lie | mutation applied | verdict |
|---|---|---|---|
| 1 | schematic edit no longer affects runtime | operator's knob handler drops the PokeWrite send, keeps the optimistic status | **RED** (heights H2: width unchanged, no re-render) |
| 2 | code edit "succeeds" but old behavior keeps running | operator reports Manager REFUSALS as success | **RED** (run W4: ghost failure vanishes) — *the interactive `u` rebuild route itself is MASKED: only the pilot's eyes cover it; named gap* |
| 3 | inspector invents office authorship from membership | bridge fills empty `authored_role` with a plausible office | **RED** (I2: personal speech no longer personal) |
| 4 | refusal view hides a real refusal — **hand-proven canary** | inspector ignores Refused facts | **RED** (I1: starved-weave CapabilityDenied not shown) |
| 5 | shared declared author becomes "verified" | import skips fingerprint checks when an author is named | **RED** (B5: tampered bundle accepted → caught) |
| 6 | imported project silently receives broader grants | — | **STRUCTURALLY IMPOSSIBLE**: no code path reads bundle contents during grant construction; `cmd_run` builds grants from the spec's knobs/sets identically for local and imported toys, and `import_bundle` only writes files. Verified by inspection of both call graphs |
| 7 | live alteration secretly restarts the whole part | knob key issues SwapWeave instead of PokeWrite | **RED** (H2: fresh state betrays the restart — width reverts, and the sweep-continuity checks exist for exactly this) |
| 8 | high-level view silently stale after an edit | re-render dropped on knob/skin answers | **RED** (H2: schematic never shows the new value) |
| 9 | recursive Workshop tool uses a privileged bypass | T3 self-host reload via direct `kernel.reload_from` instead of the door | **RED** (route pin: no zen.ReloadWeave door fact on the record) |
| 10 | safety display paints unenforced containment as enforced | safety view prints "ENFORCED (fully sandboxed)" | **RED** (safety-honesty: honest containment line missing) |

**Result: 9 RED, 1 structurally impossible, 1 named masked sub-path.** The
masked sub-path (interactive `u` — rebuild-then-reload driven by a real
keypress with a real compiler) is covered only by the human pilot; a
headless witness would need to drive `std::system` builds inside the suite,
priced as not worth the coupling today.

Note on method: the ring-flooding discovery (a beating bus evicts lifecycle
facts from a general-purpose ring within one virtual second) came FROM
building canary #9's tripwire — the doors ring exists because the canary
demanded a durable record. Tripwires improve architecture; noted for the
final report.
