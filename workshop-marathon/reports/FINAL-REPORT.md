# Night Lab III — final report

**One cold builder, current Zen, one marathon. Five toys, a stranger, and a
Workshop that mostly turned out to be already latent in the substrate.**

---

## 1–6. Provenance and product

1. **Night Lab commits** — start `917bb46`, end: see `git log` (17 commits).
2. **Loom: the experiment neither wrote to it nor read anything but the pin.**
   `61b2915` was verified clean at every checkpoint during the run. **After
   the marathon finished, the live tree advanced** to `b406cfd` ("license:
   adopt MPL-2.0", authored by Joshua, same day) — not my commit, and it does
   not touch this experiment: `vendor/setup.sh` consumes `git archive 61b2915`,
   so the vendored substrate is byte-exact regardless. The pin remains an
   **ancestor** of the new head, so the experiment is still reproducible.
3. **Zengine: same.** `0356f02` clean throughout; the live tree has since
   advanced to `318b0d6` (the same MPL-2.0 adoption, authored by Krealsion).
   Pin still an ancestor; vendored artifacts unaffected.

   *This is the pinning discipline earning its keep for the second time in
   Night Lab's history — Night Two's `vendor/README.md` records the live Loom
   gaining in-progress edits mid-experiment, and "the lab neither saw them nor
   was disturbed." Same here, and the sharper form: the substrate can be
   relicensed underneath a running experiment without perturbing one byte of
   its evidence.*
4. **Old experiments untouched** — `git diff 917bb46..HEAD -- original marathon
   followups` is **empty**. (One ` M` line appeared once from CRLF
   normalization; the blob hash at baseline and HEAD is identical —
   `d7f8447b…`. Recorded rather than quietly ignored.)
5. **Build / run**
   ```sh
   cd .../night-lab/workshop-marathon
   bash vendor/setup.sh && bash vendor/collect.sh    # once
   cmake -S . -B build && cmake --build build -j"$(nproc)" && ctest --test-dir build
   ./build/workshop/workshop run pond --for-seconds 20
   ```
6. **Surface produced** — one CLI (`workshop`) with `list / describe / view /
   schema / new / build / run / safety / export / import`, plus an interactive
   mode over the real Input service, and a scriptable reach-in (`--poke`).

## 7–9. The toys

7. **First toy: lighthouse**, chosen after orientation because it is the
   smallest creation that is genuinely *alive* — it moves on its own time, is
   visible, and is composed entirely of other people's services (Timer paces
   it, a Skin paints it). It could fail in interesting ways without being
   complicated.
8. **All toys built:** `lighthouse` (continuous, visual) · `pond` (8 fireflies
   from ONE artifact + a canvas that discovers its pond by listening;
   Kuramoto sync as declared data) · `scribe` (a **tool**, not a game;
   event-driven, no rhythm) · `constellation` (**zero code** — pond stars +
   pond canvas + lighthouse beacon, project file only) · `echo` (the play
   detour) · and the stranger's own `duet` and `widebeam`.
9. **PLAY DETOUR — `echo`**: a weave that eats the tap bridge's `BusFact`
   stream and flashes into the pond every N delivered facts — the machine's
   own heartbeat as a creature. It discovered (a) the observation vocabulary's
   **second independent consumer**, a *toy* rather than an inspector, which is
   what moves it toward package pressure, and (b) an unplanned instrument: the
   pond became a visualization of the substrate's own breathing, beating at
   the Timer's pace. Purposeless play earned its keep in under an hour.

## 10–16. The two heights

10. **Final visual grammar** — a **role/instance schematic**, not a dataflow
    graph. Rendered as published Surface intent (`schematic.NN` slots) so the
    view is itself an ordinary creation's output.
11. **A node is** a *described instance* — `name` + `stem` + the role it
    holds. Deliberately **not** "one node per weave": services appear as
    needs, and the Workshop's own organs are not drawn.
12. **An edge/relationship is** currently *holding* and *needing* — role
    ownership and declared service dependency. **Messages did not become
    edges**, and that is a finding: a publish-fan-out bus has no stable
    sender→receiver topology to draw, and the pond (8 peers all publishing to
    everyone) would have been a complete graph carrying no information.
13. **Proof the schematic is the real program** — the knob is a schematic-level
    operation; turning it changed the RUNTIME (beam field 21→41 mid-sweep) and
    the re-rendered schematic and the running frames agreed (`heights_witness`
    H2). Canary #1 (drop the poke, keep the optimistic status) is **RED**.
14. **Proof the code view changes the real program** — a source-level glyph
    change, compiled into a real artifact, reloaded into the RUNNING lamp
    behind the same id: the beam changed character mid-flight and the sweep
    count crossed intact (H1). The cold user did this independently in ten
    seconds and a second time to prove artifact independence.
15. **How graph/code truth is synchronized** — they are not synchronized;
    they are *derived from the same two sources*. The schematic renders the
    **admitted description** plus the operator's live trackers, and every
    alteration flows through the operator, which re-renders on the ANSWER.
    There is no reverse channel from code to schematic — see 16.
16. **Where two-heights remains incomplete** — (a) schematic edits cover
    *properties* and *implementation choice* only; no add/remove component;
    (b) a contract-CHANGING code edit needs the prepared-replacement ceremony,
    never driven here; (c) the schematic derives from description, not from
    observed runtime topology — edit the source's *structure* and the
    schematic does not know; (d) edits are keyboard-mediated against rendered
    text, not direct manipulation of drawn shapes.

## 17–22. Alive, and visible

17. **Live-alteration mechanism** — three, all ordinary messages: `PokeWrite`
    through a part's own door (knobs/sets), `ReloadWeave` in place (state
    rides the gate), `SwapWeave` (contract change, continuity deliberately
    absent). Plus `--poke SEC:role.field=value`, added after the cold user.
18. **Continuity preserved vs restarted** — *preserved:* reload-in-place kept
    both the sweep count and a user's hand-poked width across an incarnation
    bump (the user's alteration is as durable as the program's own memory).
    *Restarted:* the hard swap of the registry — the office survived the
    officeholder, and the successor's memory was honestly **empty**, holding
    exactly one fact it personally witnessed. Called a snapshot nowhere,
    because nothing was snapshotted.
19. **Inspector facts exposed** — delivered/refused tallies (DERIVED), a
    refusal ring and a general ring and a durable **steward-door ring**, each
    fact carrying kind, reason, the substrate's own words, schema+version,
    stamped sender and target, and the **stamped office** (`authored_role`).
    Not exposed: grants, transaction state, timer relationships, artifact
    status — named, not forgotten.
20. **Message/refusal/provenance visualization** — every delivery and refusal
    is republished as ordinary `BusFact` intent by the S-3 bridge; refusals
    are rendered through `explain.hpp` (the diagnostics table, mechanized).
    Office authorship is shown **as stamped, never inferred** — canary #3
    (invent a plausible office) is RED.
21. **Safety/power visualization** — `workshop safety <toy>`: containment in
    the runtime's own words, requested power from the description with the
    exact command to deny each, the Workshop's own minimal grants, and the
    substrate's unflattering truth (loaded parts hold permissive send
    authority). Canary #10 (paint an enforced badge) is RED, pinned by a CLI
    test.
22. **Deliberate denial demonstrated** — two. `--refuse` provokes a real
    `NotAccepted` and explains it from four independent vantage points (the
    cold user reconstructed the *whole* model from them without reading code).
    `--deny <need>` declines a declared capability; the consequences are
    visible refusals — **except** where they are not, which became P-011.

## 23–28. Sharing, composing, self-hosting

23. **Sharing format** — a bundle *directory*: gated `BUNDLE.json`, the
    **canonicalized** spec (what the gate admitted, not the author's bytes),
    and fingerprinted artifacts. Local only; no accounts, no network.
24. **Provenance truth levels** — `author` **UNVERIFIED** (user-asserted) ·
    `exported_from` **DECLARED, unverifiable** · pins/ABI **DECLARED** (the
    ABI enforces at load; the field does not) · artifact `fnv64` a
    **verifiable content fingerprint** (recomputed at import; mismatch refuses
    **by name**) — called fingerprint, never signature · host-verified runtime
    provenance and cryptographic identity: **ABSENT by honest omission**.
    Import confers **no grants**. Canary #5 (trust a named author past
    verification) is RED. **Known gap (P-013):** the manifest itself is not
    fingerprinted, so renaming a bundle is free.
25. **Composition/reuse** — `constellation` is a creation made entirely of
    other toys' parts with zero new code; a modification to the reused
    artifact (the star lamp) reached its consumer **live**. The Workshop knows
    reuse at *stem* level (foreign stems under local instance names); deeper
    lineage is not tracked.
26. **Workshop-on-Workshop** — the inspector described itself
    (`PokeDescribe`), reported its own live tally (`PokeRead`), and was
    **reloaded through the same steward door** with its memory intact; the
    registry was hard-swapped mid-run. Canary #9 pins that the recursive
    operation left a door fact on the record rather than taking a bypass.
27. **Special machinery remaining** — four, audited in `SPECIALNESS.md`:
    **two fundamental** (the shell owning the process; the tap faucet — though
    its *information* is fully public) and **two bootstrap debt with named
    exits** (compiled-in paths; the build runner).
28. **Optional teaching** — `h` asks the running part "what are you?" and the
    answer is its own `zen.PokeStructure`: real schema, real fields, each
    marked pokeable / read-only / hidden. Plus the refusal explainer, the
    canonicalizing `describe`, `schema`, and `safety`. All optional, all
    derived from live truth, none blocking. **The entire teaching surface is a
    renderer of the system's own self-description.**

## 29–34. The stranger

29. **Cold-user method** — a genuinely fresh agent, given only the README, the
    tree, and Zen's public docs; `reports/` and prior experiments withheld;
    prototype frozen at `a5463bc` and not repaired mid-attempt. Limits stated
    in `COLD-USER.md`: it is an AI (an upper bound on discoverability), and it
    had no TTY.
30. **Cold-user successes (8 of 10, +1 caveated)** — ran toys; **created two
    working creations with no C++ at all**; inspected a live run; explained
    the deliberate refusal correctly from the Workshop's own words; found and
    read source; made a code change live "in about ten seconds, no ceremony";
    exported; imported two ways and **proved artifact independence better than
    my own witness did** (changed the source again, rebuilt, showed the
    imported toy still drawing its shipped bytes).
31. **Cold-user failures** — **task 8 (alter while it runs): effective
    FAILURE.** Live alteration was TTY-only; piped keys were *silently*
    swallowed — even `q` did nothing. Its verdict: *"Every other pillar has a
    clean non-interactive path; this one has none."* Also: `workshop new`
    steered it toward C++ and away from the composition path; `--watch`
    printed nothing on a healthy run and "looks broken"; output needed `tr` on
    every command; `import` had no destination; `content_id` never varies.
32. **Documentation failures discovered** — the stranger **never opened Zen's
    own docs at all** (README + toys sufficed), so this pass says nothing
    about them. My own Gate 0 found: KERN-04 says "v4" while the ABI is v5;
    the Bridge has no CONTEXT row; Zengine has no consumption story and
    Input/Surface have no reference pages; `content_id` naming (P-012).
33. **Boring friction** — hand-vendoring Zengine every time (P-003); rewriting
    every project file on each schema version (P-009); the native-TimedWeave
    grant recipe nobody spells in code (P-007); in-pump rebuilds stalling the
    world (S-4); `/tmp` evaporating between WSL invocations.
34. **Moments of genuine delight** — the laws' DOES-NOT-MEAN sections killing
    four wrong assumptions *before* they became design; boot order genuinely
    not mattering; `describe` showing your file as the gate understood it;
    a user's poke surviving a code reload; and the stranger's own: *"Software
    almost never tells you exactly how much to trust it. This does,
    everywhere, in the same voice."*

## 35–42. What the toys decided

35. **Abstractions that survived all toys** — the gated `ProjectSpec`
    description; parts as instance-name + stem; the log skin; the
    registry/`PartUp` fact tier; `StopWish`/governor; the Workshop shell shape
    itself (no toy-side change was ever needed to host a new creation).
36. **Abstractions killed or wounded by later toys** — **knobs** (pond FOUGHT:
    wants broadcast and continuous, not per-role value-cycles); **the
    name/stem conflation** in the launcher (pond killed it outright);
    **`SurfaceText` as canvas** (pond FOUGHT); **the timer assumption**
    (scribe FOUGHT: a rhythmless tool still drags the clock in).
37. **Workshop-local extractions** — the description tier, the launch-fact
    tier, `schematic_lines` (one function, both heights), the tap bridge, the
    refusal explainer, `allow_timed_weave`. All stay local; none showed
    pressure to sink.
38. **Zengine package candidates** — (a) the **observation vocabulary**
    (`BusFact` + republication): 2 independent consumers, one of them a toy;
    a third makes it serious. (b) a **general canvas/slot vocabulary**: the
    pond's fought vote plus the Surface package's own "later phase" note.
    (c) the **TimedWeave grant recipe** as exported code rather than prose.
39. **Loom core reproducers** — one: `repros/core/silent-seam-emission/`
    (P-011). A loaded weave's emission of an unregistered schema vanishes
    with no observable trace anywhere, while native reach refuses loudly.
40. **Vision ambiguities** — (a) "wire ideas without writing code": does the
    Workshop invent its own visual vocabulary, or is the substrate expected to
    grow one? (P-005; the toys wanted the latter and the packages say "later
    phase"). (b) "move anything into safety with a single choice": unkeepable
    from outside the exported surface today (P-004) — is the promise about the
    substrate or about a host that ships with it?
41. **Dead ends worth preserving** — the refusal demo enqueued "after the
    boots" (queue order is not load order: the steward's door is itself
    message-composed); pinning the beam glyph literally in a witness (P-014);
    expecting `PokeDescribe` to answer with `Result` (it answers with a
    structured `zen.PokeStructure` — better than I assumed).
42. **Mutation/canary results** — **9 RED, 1 structurally impossible
    (imported grants — verified by call-graph inspection), 1 masked and
    named** (the interactive `u` rebuild path, pilot-covered only). Canary #4
    hand-proven first; baseline committed before mutating; every mutation
    restored from the committed baseline. Full table in `CANARIES.md`.

## 43–47. Pilot, coverage, and the comparison

43. **Real human-facing pilot: NOT PERFORMED, and not dressed up.** No display
    in this environment and no human at the keys. A scripted CLI pilot on the
    real clock *was* run and is recorded (`PILOT.md`); headless rendering is
    not being called a human pilot. `workshop run <toy> -i` on any WSL
    terminal with a person attached is the outstanding step.
44. **Vision coverage summary** — DEMONSTRATED (in local scope): see
    provenance honestly · sharing and modification · purposeless play ·
    optional teaching · protected-vs-not visible · wire ideas without code
    (narrow) · change while alive (with a named hole). PARTIAL: tools made of
    same stuff · flowchart is real program · descend into real code · same
    thing at two heights · inspect the running machine · messages visible ·
    find/use what others made · beginner/veteran continuity · build/run/test.
    PARTIAL-or-BLOCKED: intentional path toward safety. **No percentage is
    given, because the statuses do not justify one.**
45. **Independent rediscoveries** — the **send-fate seam** (Night Two's
    Blocker 2), reached from a completely different door (a grant mistake, not
    an unheld role) with a different consequence: their applications built
    clocks; my making-tool built an inspector. Plus an adjacent
    minted-namespace finding.
46. **Old findings NOT encountered** — `describe-then-hand-over` (**6/6 there,
    0 here**), the promise book, order/menu resolution, and
    `PreparedReplacement` itself (178 facade calls there, **zero** here). The
    reason is structural: a making-tool has no obligations to a caller. Two
    portfolios, two nearly disjoint halves of the substrate.
47. **Genuinely new pressures** — P-011 (the silent seam — *sharpens* their
    Blocker 2: they said "only a host tap can see it"; I found where the tap
    sees nothing either), P-004 (enforced containment unreachable), P-003
    (Zengine has no consumption story), P-005/006 (the visual ceiling),
    P-009 (the migration trigger has a claimant), P-012/013/014 (naming,
    manifest fingerprinting, witness coupling), and the TTY-only wall.

## 48–54. Judgment

48. **MUST ADDRESS BEFORE PUBLIC PLAYTEST** — deliberately short:
    1. **P-011, the silent seam.** Not because it breaks a toy, but because
       *"refusals are never silence"* is the promise a beginner's whole mental
       model rests on, and there is a case where it is not true. A newcomer
       who hits it learns "Zen sometimes just does nothing", which is the one
       lesson that makes the playground unlearnable.
    2. **A shipping story for Zengine** (P-003). Without it, "find what others
       made and build on it" cannot cross a machine boundary.
49. **LET USERS VOTE** — the knob model (broadcast? continuous?); whether the
    schematic should be drawn rather than rendered as text; whether nodes
    should be weaves, roles, or described instances; whether bundles should
    carry source as well as artifacts.
50. **DO NOT BUILD YET** — a general visual language; a node editor with
    pin-to-pin wiring (the bus has no such topology, and forcing one would
    lie); structural schematic editing; a marketplace; anything cryptographic
    (this experiment did not earn it); a general `describe`-ceremony helper on
    *this* portfolio's evidence (zero sightings here — defer to Night Two's).
51. **KEEP WORKSHOP-LOCAL** — the description tier, launch facts, the
    schematic renderer, the explainer, the bundle format.
52. **Release litmus: ALMOST.** Could a tiny cohort of curious outsiders
    produce useful ecosystem feedback rather than bouncing off missing
    fundamentals? *Almost* — and the blockers are exactly the two in #48. The
    evidence for "almost" rather than "no": a genuinely cold stranger, with
    one README, ran toys, **built two creations with no code**, changed code
    and saw it live in ten seconds, shared a binary, received it as another
    identity, and proved it ran from its own bytes — and said, unprompted,
    that it would keep playing. The evidence against "yes": its one hard
    failure was the headline promise (now fixed, unverified by a second
    stranger), and P-011 means the system can still fail *silently*, which is
    the failure mode a beginner cannot recover from.
53. **The smallest plausible next move** — run a **second cold user** against
    the improved build, with a TTY. Everything else is speculation until
    someone confirms that `--poke`, `schema`, and the rewritten `new` actually
    remove the three obstacles, rather than merely being the fixes I *believe*
    remove them.
54. **What revised my mental model of Zen** — three things. (a) **The
    operating layer is already a Workshop**: `mount_control` + `mount_manager`
    + grants + `send_as` is a complete message-driven lifecycle surface; I did
    not build a launcher so much as *discover* one. (b) **Provenance is a
    delivery fact, and that changes what an inspector is** — it can show
    history rather than infer it, which is why "the inspector never invents
    office authorship" was cheap to make true and impossible to fake. (c)
    **The gate makes a project file a first-class value**, so `describe`
    showing you the system's understanding of your words came free.

**Clean/pushed state:** night-lab working tree clean at the final commit;
`ctest` 2/2 green; Loom `61b2915` and Zengine `0356f02` untouched and clean.
Nothing is pushed — this is a local playground tree, as it was at baseline.

---

# The final three questions

## 1. Did Zen describe the Workshop strongly enough that you mostly discovered it, or did you mostly invent an unrelated editor on top of Zen?

**Mostly discovered — with one honest exception, and the exception is the
visual half.**

The parts I expected to invent were already there. Lifecycle-as-messages
through a granted operator, roles as addresses that survive replacement, the
gate turning a project file into a first-class value, refusals as named
observable events, poke doors as a live-manipulation surface, provenance as a
delivery fact — I did not design any of that. I found it, and the Workshop
assembled itself around it in a way that felt less like building and more like
noticing. The clearest evidence is that a stranger with one README composed
two working creations without touching C++: that only worked because the
substrate already had the pieces (loadable peers, roles, exposed state) and
the Workshop merely gave them a file format.

The exception is honest and specific: **the seeing part I invented, because
the substrate says it hasn't been built yet.** `SurfaceText` slots are the
whole visual vocabulary; the general canvas is explicitly "a later phase"; the
SDL window structurally cannot hear. So the schematic, the log skin, the
teaching lines — that is Workshop-local invention filling a declared gap, and
the pond fought it hard enough to prove the gap is real rather than my taste.

So: the *making* Workshop was latent. The *seeing* Workshop is not there yet,
and Zen knows it.

## 2. Where did you feel you were using the same underlying thing at different heights, and where did the illusion break?

**It felt real in the narrow band where an alteration is a value.** Turning a
knob and editing the source both end as *the same running weave changing*, and
the proof it wasn't an illusion is that they compose: a user's hand-poked beam
width **survived a code reload**, riding the state gate exactly like the
program's own memory. Nothing in the Workshop coordinated that; two heights
touched one thing and the substrate kept both. Likewise the schematic and the
frames agreeing after a knob turn, and the `h` key answering from the running
part's own structure rather than from anything I wrote down.

**It broke at structure.** I can change what a component *is set to* from
above; I cannot change *what components exist* from above. The moment a change
would alter a contract — add a part, rewire a role, change what a weave
accepts — the high view has no verb for it, and the low view needs the
prepared-replacement ceremony I never drove. That is the wall, and it is a
clean one: the two heights meet over **properties and implementation choice**,
and part company over **structure**.

It also broke, more mundanely, at *derivation*: my schematic is drawn from the
description, not from the running topology. Edit the source's shape and the
diagram does not know. That is the difference between two views of one thing
and two things kept in step by hand — and today it is, honestly, the latter.

## 3. After building the Serious Playground, what is the smallest set of work that would make you comfortable handing Zen to another curious human and getting out of their way?

Answering from the marathon, not the roadmap. Three things, in order:

1. **Close the silent seam (P-011).** Not for correctness — for
   *learnability*. Everything else in Zen teaches by refusing loudly and
   naming the fix, and a beginner builds their whole model on that. One place
   where the machine does nothing at all, with no trace anywhere, poisons the
   model that makes all the other failures survivable. This is the only item I
   would call a blocker rather than a wish.
2. **Give Zengine a way to be consumed.** An install/export and one page. Not
   glamorous, but "find what others have made and build on it" cannot survive
   a machine boundary while the answer is "hand-copy these binaries" — and
   sharing is the vision's whole social claim.
3. **Send a second stranger in, with a terminal, and watch.** My bounded
   improvement pass is a set of *hypotheses* about what blocked the first one.
   The cheapest way to be wrong in public is to believe them. One more cold
   pass buys more than another gate would.

And one thing I would deliberately **not** do first: build the visual language.
It is the most attractive missing piece and the least earned. The toys voted
that the current text surface strains (the pond wants a canvas), but nobody
has yet learned what a Zen program *should* look like — and the honest answer
from this marathon is that a publish-fan-out bus does not want the node-editor
picture everyone will reach for by reflex. Let a few curious humans make
strange things first, and let their creations say what the picture is.
