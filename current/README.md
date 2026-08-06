# current/ — the current-era Night Lab

Live downstream experimentation against a **current** Zen, standing beside the
frozen historical laboratory rather than replacing it.

The four directories above this one (`original/`, `marathon/`,
`workshop-marathon/`, `followups/`) are preserved evidence about the substrate
they actually ran on, pinned to four different Looms across four ABI
generations. They are read-only forever. Nothing here updates them, and nothing
here reads from them.

## What a current-era experiment is

One small application, in a domain that has nothing to do with Zen, built
against an **installed Loom package** — the same surface a stranger would get.

It is not a package probe. ZNL-R already proved the package can load a weave and
exchange a message. What these experiments are for is putting a life inside the
machine and seeing which parts of the machine it reaches for, and where it has
to invent something the substrate did not offer.

## The house rules

- **Only the installed package.** No `add_subdirectory(../../Loom)`, no
  source-tree include path, no build-tree archive, no copied Loom CMake or
  compiler flag. `find_package(loom REQUIRED)` against a prefix, and that is
  all.
- **Nothing here writes to `Zen/Loom` or `Zen/Zengine`.** If an experiment
  appears to need a substrate change, it does not make one. It produces the
  smallest reproducer, describes the missing seam, and continues.
- **No shared framework.** *No file may be shared between two experiments
  except a test framework and the substrate itself.* Each experiment owns its
  own vocabulary and its own mistakes. This is Night Two's rule
  (`marathon/CMakeLists.txt`), kept because it is what lets each experiment be
  genuinely different — and genuinely wrong in its own way.
- **A labelled fake is allowed; an unlabelled one is not.**
- **Sightings nominate. They do not authorize.** `FRICTION.md` counts what was
  awkward; it is not a queue, and nothing in it is a request.

The warning sign to watch for is the first `current/common/` directory. If one
appears, this has become a framework and should be split back apart.

## What is here

| path | what it is |
|---|---|
| `substrate.lock` | the exact Zen ZNL-00 and ZNL-01 experienced |
| `FRICTION.md` | the running friction ledger, in the order things happened |
| `EVIDENCE.md` | the running claim/witness ledger, with the non-claims |
| `signal-box/` | **ZNL-00** — a miniature railway interlocking |
| `prompt-corner/` | **ZNL-01** — one act of a play, called live, with the DSM replaced halfway through |
| `records-committee/` | **ZNL-02** — five assessors who disagree about the same bird, and a county list that outlives them (**own `substrate.lock`**) |
| `ringing-chamber/` | **ZNL-03** — six ringers who each know only their own line, and a touch whose truth nobody in the tower can see (**own `substrate.lock`**) |
| `shutter-line/` | **ZNL-04** — the Admiralty telegraph over six hilltops, and a message nobody in between can read (**own `substrate.lock`**) |
| `saleroom/` | **ZNL-05** — a country auction; four private figures nobody may see, and two clean sales that are wrong (**own `substrate.lock`**) |

`substrate.lock` is a **record, never an input**. No build file reads it, and
nothing parses it. It exists because an installed Loom cannot tell you which
Loom it is: `find_package(loom 0.1)` is satisfied by every pin ZNL-R measured
across four ABI generations, and nothing in the install prefix records a commit.
The ABI version is carried and enforced, but that is a compatibility class, not
an identity.

An experiment may carry its own `substrate.lock` if it pins differently.
Absence means the era's lock applies. ZNL-00 and ZNL-01 both resolved the same
remote `main` at the start of their own phase, built it with the same flags on
the same toolchain, and measured the same installed ABI — so neither carries a
lock of its own, and the era lock is not a shorthand for "whatever the first one
used".

**ZNL-02, ZNL-03, ZNL-04 and ZNL-05 each carry their own lock.** The era lock is
not rewritten and must not be: an era lock that quietly absorbed a newer pin
would make an earlier experiment's evidence claim a substrate it never ran on.
The locks differ in exactly one field — `loom` — and agree on toolchain,
configuration and measured ABI, which is itself the reason the ABI cannot stand
in for the commit: four different Looms measured the same compatibility class.

```text
current/substrate.lock                      c86717b0   ZNL-00, ZNL-01
current/records-committee/substrate.lock    d0d8257    ZNL-02
current/ringing-chamber/substrate.lock      b084b1c9   ZNL-03
current/shutter-line/substrate.lock         55fbb15f   ZNL-04
current/saleroom/substrate.lock             55fbb15f   ZNL-05
```

The last two are the same pin, and ZNL-05 still writes its own lock rather than
leaning on ZNL-04's. Absence means *the era lock applies*, and the era lock is
`c86717b0` — so a `saleroom` with no lock would inherit the wrong Loom by this
page's own rule. A lock records what an experiment ran on; it is not a pointer
to somebody else's.

## Getting a substrate to run against

Six commands, no network beyond the clone, about twelve seconds:

```sh
git clone https://github.com/Krealsion/Loom.git /tmp/loom-src
git -C /tmp/loom-src checkout --detach <the SHA in substrate.lock>
cmake -S /tmp/loom-src -B /tmp/loom-build -DCMAKE_BUILD_TYPE=Debug \
      -DZEN_BUILD_TESTS=OFF -DZEN_BUILD_EXAMPLES=OFF -DZEN_SDL=OFF
cmake --build /tmp/loom-build -j
cmake --install /tmp/loom-build --prefix /tmp/loom-install
# then point any experiment at it with -DCMAKE_PREFIX_PATH=/tmp/loom-install
```

Build the substrate **outside** this repository and outside the Zen workspace.
The install prefix is not tracked here and must never be committed: an
experiment is reproducible from its tracked source plus `substrate.lock` plus
the named Loom SHA, not from a vendored dump.

WSL/GCC only — the kernel is `dlopen`/POSIX ground.
