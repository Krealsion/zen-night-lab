# Gate 11 — a stranger enters

## Method (what was real, what was simulated)

**Genuinely fresh context.** A subagent with no memory of this marathon, no
access to the builder's notebook, and no hints. It was told it was a
competent C++ developer meeting Zen for the first time, and that its
confusion was the valuable output.

**Given only:** `workshop-marathon/README.md`, the experiment tree (code,
toys, build), and Zen's public docs (`Loom/docs/`, `zen-vision.md`,
`Zengine/README.md` + `Zengine/docs/`).

**Withheld:** everything under `reports/` (this notebook, the pressure
ledger, the canary list, the specialness ledger, the pilot record), the
prior Night Lab experiments (`original/`, `marathon/`, `followups/`), the
Night Lab III prompt itself, and every hint about where known answers live.

**Frozen.** The prototype was committed at `a5463bc` before the attempt and
NOT repaired during it. Failures below are first-attempt failures.

**Honest limitations of this simulation:**
- The cold user is an AI agent, not a human. It reads faster and complains
  less than a person; treat its successes as an UPPER bound on
  discoverability and its failures as very solid evidence.
- It could not press keys in a live run (non-interactive environment), so
  the interactive keymap (`-i`) — the whole live-alteration surface — was
  outside its reach. It was asked to do the closest non-interactive thing
  and record the gap.
- Its first attempt was interrupted by an environment token limit (not by
  the software) and resumed from the same context; that interruption is an
  artifact of the lab, not a finding about Zen.

## Result: 8 SUCCESS, 1 SUCCESS-with-caveat, 1 effective FAILURE

**Documentation actually used:** `workshop-marathon/README.md` (~95%), the
built-in usage text, the existing `toys/*/project.json` as de-facto schema
reference, comments in `lamp.cpp`, and — when the README ran out —
`workshop/shell.cpp` itself. **It never opened `Loom/docs/`, `zen-vision.md`,
or `Zengine/docs/` at all.** The README plus the toys were self-sufficient
for all ten tasks. A compliment to the README; also means this pass says
nothing about whether Zen's own docs would have resolved its mysteries.

| # | task | verdict | the short of it |
|---|---|---|---|
| 1 | launch | SUCCESS | "there is nothing to launch" — it's `git`-shaped, not `emacs`-shaped. The README's framing implies an app |
| 2 | run a toy | SUCCESS | beam swept, inspector narrated, post-run report distinguished registry-says from inspector-counted |
| 3 | create from the high surface | SUCCESS | built `duet` (2 fireflies + canvas) with **no C++** — but `workshop new` steered it the other way |
| 4 | inspect a live run | SUCCESS | `safety` called "the standout"; `--watch` "looks broken" |
| 5 | explain the refusal | SUCCESS | got it exactly right from the Workshop's own words alone |
| 6 | code view | SUCCESS | "there is no code view" — found source via the filesystem |
| 7 | code-level change | SUCCESS | glyph changed, rebuilt, live "in about ten seconds, no ceremony" |
| 8 | alter while it runs | **PARTIAL / effectively FAILURE** | TTY-only. Piped keys silently swallowed — even `q` didn't quit |
| 9 | export | SUCCESS | "the relentless honesty of the labels" |
| 10 | import fresh + run | SUCCESS (2 routes) | proved artifact independence better than my own witness did |

### The failure that matters (task 8)

> *"reach inside it while it lives"* is one of the four pillars in the
> README's own one-line pitch, and it is reachable **only** through a TTY key
> loop. There is no `--poke role.field=value`, no `--at 3s poke …`, no
> `--script file`, no stdin protocol. That means live alteration is
> untestable in CI, undemonstrable over a pipe, and unusable by an agent.
> Every other pillar (describe/run/see/share) has a clean non-interactive
> path; this one has none. If I could request one feature, it is this.

Piped keystrokes were accepted by the process and had **no effect at all** —
not even `q`. Silent, not refused. The Input weave needs a real terminal.

### The three biggest obstacles (its words, condensed)

1. **Live alteration is TTY-only, with no scripted equivalent.** Piped keys
   are silently swallowed, so "you don't get an error, you get nothing."
2. **The project-file schema is undocumented, and `workshop new` steers you
   away from it.** The most delightful surface — compose existing parts in
   JSON, no C++ — is mentioned in exactly one subordinate clause of the
   README, while the scaffolder tells you to go write C++ instead. No field
   reference, no way to ask which fields a part exposes without reading its
   source.
3. **Output is hostile to capture, and `--watch` looks broken.** Bare `\r` +
   `[K` with no TTY detection and no `--plain`; every piped run needed
   `tr '\r' '\n'`. `--watch` prints *nothing* on a healthy run (it observes
   only refusals) — "a newcomer will conclude the flag is broken, as I did."

### What most impressed it

> **The refusal chain, and the honesty vocabulary around it.** One `--refuse`
> flag produces four independent accounts of the same event … that all agree
> and all explain it in the machine's own terms. … Around that sits a
> consistent, disciplined labelling of trust: `UNVERIFIED` / `DECLARED,
> unverifiable` / `VERIFIED against shipped bytes` / `authenticated answer:
> yes` / `fnv64 — verifiable, not cryptographic` / `importing confers NO
> grants`. And `safety` volunteers the tool's own worst news … without being
> asked. Software almost never tells you exactly how much to trust it. This
> does, everywhere, in the same voice.

### Would it keep playing? "Honestly — yes."

> …because the loop is short and the machine explains itself. … What would
> make me stop is Task 8. … Give me `--poke role.field=value --at 3s` and
> I'd cheerfully lose a weekend to this.

## Findings the builder did not know, discovered by the stranger

- **`content_id` never varies** across five different project files. Its
  guess was right: the compat envelope carries the *schema's* content-id
  (GATE-04 identity), not the value's. Correct by Loom's definition,
  actively misleading in a value envelope — *"a field called `content_id`
  that never varies with content cost me real time and made me distrust my
  own edits."* Classified DOCUMENTATION FAILURE (P-012).
- **Renaming a bundle is free.** `sed`-renaming the project in both files
  still imports reporting `fingerprints VERIFIED` — only artifact bytes are
  hashed; the manifest is not. Honest under the stated labels (the artifacts
  ARE verified; the manifest is DECLARED), but the two facts sit close
  enough to mislead. Recorded as a real gap in MY bundle design (P-013).
- **`import` takes no destination.** "Being a *different person* means having
  a different checkout, not passing a flag."
- **A newcomer's legitimate toy edit breaks the builder's witness suite.**
  Its `'#'`→`'%'` change collides with `heights_witness`, which pins the
  glyph. Test coupling to toy *content* — the toys are the user's surface,
  so witnesses should pin behavior, not the user's chosen characters (P-014).
- **`/tmp` is tmpfs and evaporates between WSL invocations** — killed its
  first export→import attempt. Environment, not Zen; worth a README line.
- **`workshop build <toy>` on a composed toy rebuilds the donor toy's
  parts** (stems are shared). True and unstated.
