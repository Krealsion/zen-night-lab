# The Night Lab Marathon

Six materially different applications, built to make the substrate justify itself.

Not to prove Zen is good. Not to make every experiment green. **Put different lives inside the
machine and see which parts of the machine they all reach for.**

**Pinned against:** Loom `78d64ea`, Zengine `f6a4c69`, **ABI v4**. See `vendor/README.md`.
Nothing in this tree writes to `Zen/Loom` or `Zen/Zengine`.

## The projects

| # | project | the architectural question it exists to ask |
|---|---|---|
| 1 | `kitchen-replay/` | What did a year of substrate work actually change for an application that already existed? |
| 2 | `download-manager/` | Is "acknowledged responsibility → progress → terminal fulfilment" a reusable shape or was it the kitchen's? |
| 3 | `build-farm/` | Same shape, different domain — do two independent implementations grow the same non-domain bookkeeping? |
| 4 | `import-pipeline/` | Does *request → menu → resolved choice → receipt* reproduce outside the kitchen? |
| 5 | `lobby/` | Are *speaker identity* and *office/role authorship* observably different security facts? |
| 6 | `scheduler/` | Do two individually pleasant APIs (Timer, PreparedReplacement) stay pleasant together? |

## The ledgers

- `FRICTION.md` — every moment authoring was harder than meaning it, written as it happened.
- `EVIDENCE.md` — the voting table, the sugar audit, and the replacement/answer coverage matrices.
- `<project>/REPORT.md` — one compact report per project.
- `FINAL-REPORT.md` — written only after all six have spoken.

## Building and running

WSL/GCC only (the kernel is `dlopen`/POSIX ground).

```sh
cd /mnt/g/programming/cpp/Zen/playground/night-lab/marathon
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## The laws this marathon holds itself to

- **Night Lab discovers abstractions. It does not pre-authorize them.** No feature is added to
  Loom or Zengine because one project wanted it.
- **The shared-helper law.** Each project begins with only Loom, Zengine, the prepared-replacement
  handle, and the standard library. A pattern seen once is *named*, not extracted. Extraction needs
  two genuinely independent consumers, and it stays inside Night Lab.
- **`loom::PreparedReplacement` is the path.** Every replacement in application orchestration goes
  through the handle. Every fall-through to raw machinery is recorded and classified.
- **No core patches.** A core bug becomes the smallest reproducer plus a written desired law, and
  the project continues around it or ends PROVEN BLOCKED.
- **A labelled fake is allowed; an unlabelled one is not.** The Timer's virtual clock is the only
  substitution, and every suite that uses it says so.
