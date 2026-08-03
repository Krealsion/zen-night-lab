# The pilot — what was really run, and what was not

**Honest scope first:** this environment has NO display (no WSLg in the
session) and NO human at the keys. Therefore:

- The **real visible window** was NOT achieved. The SDL skin exists (vendored,
  built, suite-proven under SDL's dummy driver) but no OS window opened here.
- The **interactive keymap** (v/h/1/p/r/u/q) was exercised by PUBLISHED
  KeyPressed messages in the witness suite, not by human fingers on a raw
  terminal. `workshop run <toy> -i` in a real WSL terminal is the standing
  invitation; the machinery is in place and witness-covered, the experience
  is not human-verified.
- Headless rendering is NOT being called a human pilot. This file is the
  record of exactly what was simulated.

**What WAS run, on the real clock, through the real CLI** (2026-08-02):

- `list` — five creations, honest one-liners.
- `view constellation` — the described schematic of a toy composed entirely
  from other toys' parts.
- `safety lighthouse` — containment in the runtime's own words; needs with
  their own deny instructions.
- `run lighthouse --for-seconds 2 --refuse` — live world, deliberate
  NotAccepted, inspector explains it from the reason vocabulary.
- `run lighthouse --deny zengine.timer` — a declared need declined; the world
  honestly runs dry and says so.
- `export lighthouse /tmp/pilot-share josh` → bundle with unverified-author
  labelling and fnv64 fingerprints; `import` of it refused (toy already
  exists — not-overwriting demonstrated; fresh-location import is
  witness-covered in B2/B3).
- `run lighthouse` and `run scribe` — the beam sweeps; the input service
  claims the terminal; zero refusals; post-run registry + inspector reports
  authenticated.

Earlier the same day, live runs also covered: pond (sync visible in the
canvas rows), echo (the machine-fly's column beating with the fireflies),
constellation (9 up, borrowed parts named by stem).

**Verdict for the report-back:** a scripted application pilot is REAL and
recorded; the human-facing visible pilot is NOT DONE here and is one
`workshop run lighthouse -i` away on any WSL terminal with a human attached.
