// workshop — the bootstrap shell (SPECIAL MACHINERY, recorded as S-1).
//
// The shell is the one thing here that is not an ordinary weave: it owns the
// Switchboard (host root authority), the Kernel, and the pump loop, because
// SOMETHING must own the process. Everything it does beyond that is done the
// ordinary way — lifecycle commands are messages from a granted operator
// weave through the Weave Manager door, launch facts are published intent,
// the registry and the skin are loadable weaves any toy could replace.
// The shape is deliberately the snake host's: Zen already teaches this
// pattern; the Workshop was born into it rather than migrating later.
//
// Specialness admitted here (see reports/SPECIALNESS.md):
//   S-1  holds Switchboard& (root): pump, mount, send_as, grants
//   S-2  knows WORKSHOP_SOURCE_ROOT / WORKSHOP_BINARY_DIR at compile time
//        (an ordinary creation cannot know where the Workshop keeps toys)
//
// Commands:
//   workshop list                     what creations exist (admitted truth)
//   workshop describe <toy>           the spec as the GATE sees it
//   workshop new <name>               scaffold a creation (emits admittable JSON)
//   workshop run <toy> [--for-seconds N]

#include "bridge.hpp"
#include "bundle.hpp"
#include "explain.hpp"
#include "host_weaves.hpp"
#include "vocabulary.hpp"

#include "surface/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace surface = zengine::surface;
namespace ztimer = zengine::timer;

namespace workshop {
namespace {

// ---- paths (S-2: the shell's compiled-in knowledge) ------------------------

std::string source_root() { return WORKSHOP_SOURCE_ROOT; }
std::string binary_dir() { return WORKSHOP_BINARY_DIR; }

/// Where a stem may resolve to an artifact, in order: the toy's own build
/// output, EVERY other toy's build output (composition: one toy's parts are
/// another's material — constellation forced this), the workshop's build
/// output, the vendored Zengine services.
std::vector<fs::path> artifact_dirs(const std::string& toy) {
    std::vector<fs::path> dirs;
    dirs.push_back(fs::path(binary_dir()) / "toys" / toy);
    // An IMPORTED toy carries its artifacts beside its description.
    dirs.push_back(fs::path(source_root()) / "toys" / toy / "artifacts");
    const fs::path toys = fs::path(binary_dir()) / "toys";
    if (fs::exists(toys)) {
        for (const auto& entry : fs::directory_iterator(toys)) {
            if (entry.is_directory() && entry.path().filename() != toy) {
                dirs.push_back(entry.path());
            }
        }
    }
    dirs.push_back(fs::path(binary_dir()) / "workshop");
    dirs.push_back(fs::path(source_root()) / "vendor" / "zengine" / "lib");
    return dirs;
}

std::optional<fs::path> resolve_artifact(const std::string& toy, const std::string& stem) {
    for (const fs::path& dir : artifact_dirs(toy)) {
        fs::path candidate = dir / (stem + ".so");
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

// ---- reading a project file through the gate -------------------------------

struct LoadedSpec {
    ProjectSpec spec;
    std::string dir; ///< the project's directory (its identity on disk, v1)
};

/// The whole road: bytes -> compat::parse -> the one gate -> struct. A
/// malformed project is a refusal with the gate's own words, never a parser
/// crash and never a half-read spec.
std::optional<LoadedSpec> read_spec(const fs::path& file, std::string& error) {
    std::ifstream in(file);
    if (!in) {
        error = "cannot open " + file.string();
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();

    loom::Unverified claimed = loom::compat::parse(buffer.str());
    loom::Admission admitted = loom::admit(claimed, loom::schema_of<ProjectSpec>());
    if (!admitted.ok()) {
        const loom::Error& first = admitted.first_error();
        error = "the gate refused " + file.string() + ": " + first.message();
        return std::nullopt;
    }
    return LoadedSpec{loom::from_value<ProjectSpec>(admitted.value()), file.parent_path().string()};
}

fs::path toy_file(const std::string& name) {
    fs::path direct(name);
    if (direct.filename() == "project.json" && fs::exists(direct)) {
        return direct;
    }
    return fs::path(source_root()) / "toys" / name / "project.json";
}

// ---- the service map -------------------------------------------------------

/// The services the Workshop can supply for a declared need. The skin need is
/// answered with the Workshop's own log skin (an ordinary Skin painting every
/// slot); a toy that wants the snake-era TUI look can swap it later.
struct Service {
    const char* need;
    const char* stem;
    const char* role;
};
constexpr Service kServices[] = {
    {"zengine.timer", "zengine-timer", ztimer::kTimerRole},
    {"zengine.skin", "workshop-skin-log", surface::kSkinRole},
    {"zengine.input", "zengine-input", "zengine.input"},
};

const Service* find_service(const std::string& need) {
    for (const Service& s : kServices) {
        if (need == s.need) {
            return &s;
        }
    }
    return nullptr;
}

// ---- commands --------------------------------------------------------------

int cmd_list() {
    const fs::path toys = fs::path(source_root()) / "toys";
    if (!fs::exists(toys)) {
        std::printf("no toys directory at %s\n", toys.string().c_str());
        return 1;
    }
    for (const auto& entry : fs::directory_iterator(toys)) {
        if (!entry.is_directory()) {
            continue;
        }
        const fs::path file = entry.path() / "project.json";
        if (!fs::exists(file)) {
            continue;
        }
        std::string error;
        if (auto loaded = read_spec(file, error)) {
            std::printf("  %-14s %s  (%zu part%s)\n", loaded->spec.name.c_str(),
                        loaded->spec.description.c_str(), loaded->spec.parts.size(),
                        loaded->spec.parts.size() == 1 ? "" : "s");
        } else {
            std::printf("  %-14s UNREADABLE: %s\n", entry.path().filename().string().c_str(),
                        error.c_str());
        }
    }
    return 0;
}

int cmd_view(const std::string& name) {
    std::string error;
    auto loaded = read_spec(toy_file(name), error);
    if (!loaded) {
        std::printf("%s\n", error.c_str());
        return 1;
    }
    OperatorContext ctx;
    ctx.project = loaded->spec.name;
    ctx.parts = loaded->spec.parts;
    ctx.needs = loaded->spec.needs;
    ctx.knobs = loaded->spec.knobs;
    ctx.knob_at.assign(ctx.knobs.size(), 0);
    for (const std::string& line : schematic_lines(ctx, /*live=*/false)) {
        std::printf("%s\n", line.c_str());
    }
    return 0;
}

int cmd_build(const std::string& name) {
    std::string error;
    auto loaded = read_spec(toy_file(name), error);
    if (!loaded) {
        std::printf("%s\n", error.c_str());
        return 1;
    }
    std::string targets;
    for (const PartSpec& part : loaded->spec.parts) {
        targets += " --target " + part.stem;
    }
    if (targets.empty()) {
        std::printf("'%s' declares no parts to build\n", loaded->spec.name.c_str());
        return 0;
    }
    const std::string cmd = "cmake --build " + binary_dir() + targets;
    std::printf("workshop - %s\n", cmd.c_str());
    return std::system(cmd.c_str());
}

/// The power view: what a creation asks for, what it would be given, and what
/// actually protects anything — with the unflattering parts said plainly.
int cmd_safety(const std::string& name) {
    std::string error;
    auto loaded = read_spec(toy_file(name), error);
    if (!loaded) {
        std::printf("%s\n", error.c_str());
        return 1;
    }
    const ProjectSpec& spec = loaded->spec;
    std::printf("=== safety: %s ===\n", spec.name.c_str());
    std::printf("containment (the runtime's own words, no shield painted over them):\n");
    std::printf("   %s\n", loom::Kernel::containment_note());
    std::printf("requested power (DECLARED by the project file):\n");
    for (const std::string& need : spec.needs) {
        std::printf("   need %s   (deny it: run --deny %s)\n", need.c_str(), need.c_str());
    }
    std::printf("what the Workshop's own hands hold (host-assigned, minimal):\n");
    std::printf("   operator: LoadWeave->manager, publishes (facts/intent), per-knob and "
                "per-set pokes to declared roles only\n");
    std::printf("   governor: StopWish + the Timer protocol\n");
    std::printf("what LOADED PARTS hold (the substrate's current truth, said plainly):\n");
    std::printf("   permissive send authority - a loaded part can send any shape anywhere.\n");
    std::printf("   Its speech is limited by audience, not license. The unforgeable fact is\n");
    std::printf("   the bus's sender stamp: the registry records the reporter of every\n");
    std::printf("   launch fact, so a lying part is VISIBLE even though it is not stopped.\n");
    std::printf("enforced OS containment (namespaces/cgroups, kernel-confirmed):\n");
    std::printf("   exists in the substrate; NOT part of Loom's exported surface - this\n");
    std::printf("   Workshop cannot reach it (recorded as P-004). No enforced path = no\n");
    std::printf("   enforced badge; nothing here claims otherwise.\n");
    std::printf("knobs/sets (declared reach-in points): %zu knob(s); pokes are refused by\n",
                spec.knobs.size());
    std::printf("   the target's own door unless the field is ZEN_EXPOSEd.\n");
    return 0;
}

int cmd_export(const std::string& name, const std::string& dest, const std::string& author) {
    std::string error;
    auto loaded = read_spec(toy_file(name), error);
    if (!loaded) {
        std::printf("%s\n", error.c_str());
        return 1;
    }
    const auto resolve = [&](const std::string& stem) -> std::optional<std::string> {
        auto p = resolve_artifact(loaded->spec.name, stem);
        return p ? std::optional<std::string>(p->string()) : std::nullopt;
    };
    BundleOutcome out = export_bundle(loaded->spec, loaded->dir, dest, author,
                                      WORKSHOP_LOOM_PIN, WORKSHOP_ZENGINE_PIN, WORKSHOP_ABI,
                                      resolve);
    if (!out.ok) {
        std::printf("export failed: %s\n", out.error.c_str());
        return 1;
    }
    std::printf("exported %s\n", out.dir.c_str());
    std::printf("  author (UNVERIFIED, user-asserted): %s\n",
                out.info.author.empty() ? "(none given)" : out.info.author.c_str());
    for (const ArtifactInfo& a : out.info.artifacts) {
        std::printf("  artifact %-20s %lld bytes  fingerprint %s (fnv64 - verifiable, "
                    "not cryptographic)\n",
                    a.stem.c_str(), static_cast<long long>(a.bytes), a.fnv64.c_str());
    }
    return 0;
}

int cmd_import(const std::string& bundle_dir, const std::string& into, const std::string& as) {
    const std::string dest = into.empty() ? (fs::path(source_root()) / "toys").string() : into;
    BundleOutcome out = import_bundle(bundle_dir, dest, as);
    if (!out.ok) {
        std::printf("import REFUSED: %s\n", out.error.c_str());
        return 1;
    }
    std::printf("imported into %s\n", out.dir.c_str());
    std::printf("  project: %s\n", out.info.project.c_str());
    std::printf("  author claims to be (UNVERIFIED): %s\n",
                out.info.author.empty() ? "(none)" : out.info.author.c_str());
    std::printf("  exported from (DECLARED, unverifiable): %s\n",
                out.info.exported_from.c_str());
    std::printf("  declared substrate: Loom %s, Zengine %s, ABI %s (the ABI enforces at "
                "load; these fields do not)\n",
                out.info.loom_pin.c_str(), out.info.zengine_pin.c_str(),
                out.info.abi.c_str());
    std::printf("  %zu artifact(s), fingerprints VERIFIED against shipped bytes\n",
                out.info.artifacts.size());
    std::printf("  capability needs (DECLARED): ");
    for (const std::string& need : out.info.needs) {
        std::printf("%s ", need.c_str());
    }
    std::printf("\n  importing confers NO grants - it runs with whatever the Workshop "
                "gives any toy\n");
    return 0;
}

int cmd_describe(const std::string& name) {
    std::string error;
    auto loaded = read_spec(toy_file(name), error);
    if (!loaded) {
        std::printf("%s\n", error.c_str());
        return 1;
    }
    // What the GATE accepted — round-tripped, not the file's own bytes.
    std::printf("%s\n", loom::compat::serialize(loom::to_value(loaded->spec)).c_str());
    return 0;
}

int cmd_new(const std::string& name) {
    const fs::path dir = fs::path(source_root()) / "toys" / name;
    const fs::path file = dir / "project.json";
    if (fs::exists(file)) {
        std::printf("%s already exists\n", file.string().c_str());
        return 1;
    }
    fs::create_directories(dir);
    ProjectSpec spec;
    spec.name = name;
    spec.description = "describe me";
    spec.parts.push_back(PartSpec{name + "-part", name + "-part", ""});
    spec.needs = {"zengine.timer", "zengine.skin"};
    std::ofstream out(file);
    out << loom::compat::serialize(loom::to_value(spec)) << "\n";
    std::printf("scaffolded %s\n", file.string().c_str());
    std::printf("\nTWO ways forward — the first needs no C++ at all:\n");
    std::printf("  1. COMPOSE (no code): edit that file and point `stem` at parts that\n");
    std::printf("     already exist — e.g. \"pond-firefly\", \"pond-canvas\",\n");
    std::printf("     \"lighthouse-lamp\". Give each part its own `name` and `role`, and\n");
    std::printf("     use `set` to configure it at birth. `workshop schema` lists every\n");
    std::printf("     field; `workshop list` shows what exists to borrow.\n");
    std::printf("  2. WRITE A PART: add toys/%s/%s-part.cpp and a CMake target named\n",
                name.c_str(), name.c_str());
    std::printf("     %s-part (copy toys/lighthouse/ as a model), then rebuild.\n",
                name.c_str());
    return 0;
}

/// The project-file field reference. Exists because a cold user had to
/// reverse-engineer the vocabulary from example files (finding #2).
int cmd_schema() {
    std::printf("project.json — the shape a creation is described in\n");
    std::printf("  admitted through Zen's one gate as ProjectSpec v3; a malformed file is\n");
    std::printf("  a refusal naming the field, never a half-read project.\n\n");
    std::printf("  name          text     the creation's name (matches its directory)\n");
    std::printf("  description   text     one honest line; shown by `list`\n");
    std::printf("  needs         [text]   services to supply: \"zengine.timer\",\n");
    std::printf("                         \"zengine.skin\", \"zengine.input\"\n");
    std::printf("  parts         [part]   the loadable pieces (below)\n");
    std::printf("  knobs         [knob]   live reach-in points (below)\n\n");
    std::printf("  part.name     text     THIS instance's name — one artifact may be\n");
    std::printf("                         loaded many times under different names\n");
    std::printf("  part.stem     text     the artifact to load (<stem>.so). Borrow another\n");
    std::printf("                         toy's stem freely — that is composition\n");
    std::printf("  part.role     text     the address it holds; needed if you `set` or\n");
    std::printf("                         `knob` it. Convention: <toy>.<part>\n");
    std::printf("  part.set      [{field,value}]  pokes applied at birth, through the same\n");
    std::printf("                         door a live poke uses\n\n");
    std::printf("  knob.name     text     what to call it in the schematic\n");
    std::printf("  knob.role     text     whose field to turn\n");
    std::printf("  knob.field    text     which field\n");
    std::printf("  knob.values   [text]   the cycle, as text literals\n\n");
    std::printf("WHICH FIELDS CAN I set/knob? Only those a part opened with ZEN_EXPOSE.\n");
    std::printf("  Ask the running part itself — `run <toy> -i` then press h — or read its\n");
    std::printf("  source. A poke at a field that is not open is refused BY THE PART, and\n");
    std::printf("  the refusal names the field.\n");
    return 0;
}

struct RunFlags {
    std::int64_t for_seconds = 0;
    bool watch = false;       ///< also print raw tap lines (host stdout diagnostics)
    bool refuse = false;      ///< deliberately provoke one refusal, then explain it
    bool interactive = false; ///< load the Input service; keys reach inside the live world
    std::vector<std::string> deny; ///< declared needs the Workshop refuses to supply
    std::vector<ScheduledPoke> pokes; ///< --poke SEC:role.field=value
};

/// Parse `SEC:role.field=value`. The role may contain dots (they usually do),
/// so the FIELD is the last dot-segment before '='.
bool parse_poke(const std::string& text, ScheduledPoke& out, std::string& why) {
    const std::size_t colon = text.find(':');
    const std::size_t equals = text.find('=', colon == std::string::npos ? 0 : colon);
    if (colon == std::string::npos || equals == std::string::npos || equals < colon) {
        why = "expected SEC:role.field=value";
        return false;
    }
    const std::string when = text.substr(0, colon);
    const std::string target = text.substr(colon + 1, equals - colon - 1);
    const std::size_t dot = target.rfind('.');
    if (dot == std::string::npos) {
        why = "expected role.field before '=' (the role usually contains dots too)";
        return false;
    }
    out.second = std::atoll(when.c_str());
    out.role = target.substr(0, dot);
    out.field = target.substr(dot + 1);
    out.value = text.substr(equals + 1);
    if (out.second <= 0 || out.role.empty() || out.field.empty()) {
        why = "second must be >= 1 and role/field must be non-empty";
        return false;
    }
    return true;
}

int cmd_run(const std::string& name, const RunFlags& flags) {
    std::string error;
    auto loaded = read_spec(toy_file(name), error);
    if (!loaded) {
        std::printf("%s\n", error.c_str());
        return 1;
    }
    const ProjectSpec& spec = loaded->spec;

    // Resolve everything BEFORE the world exists — honest failures name what
    // was searched.
    struct Boot {
        std::string part;
        std::string load_name; ///< the INSTANCE name (parts: part.name — one
                               ///< artifact may be loaded many times)
        std::string stem;
        std::string path;
        std::string role;
    };
    std::vector<Boot> boots;
    boots.push_back({"(workshop) registry", "workshop-registry", "workshop-registry", "",
                     kRegistryRole});
    boots.push_back({"(workshop) inspector", "workshop-inspector", "workshop-inspector", "",
                     kInspectorRole});
    if (flags.interactive) {
        boots.push_back({"(service) zengine.input", "zengine-input", "zengine-input", "",
                         "zengine.input"});
    }
    for (const std::string& need : spec.needs) {
        bool denied = false;
        for (const std::string& d : flags.deny) {
            denied = denied || d == need;
        }
        if (denied) {
            // The truthful move toward safety available today: DECLINE a
            // declared capability. The creation runs with less power, and the
            // consequences are visible refusals, never silence.
            std::printf("workshop - DENYING declared need '%s': the creation runs without "
                        "it; expect visible refusals where it reaches for the missing "
                        "service\n",
                        need.c_str());
            continue;
        }
        const Service* s = find_service(need);
        if (s == nullptr) {
            std::printf("cannot supply need '%s' (known: timer/skin/input)\n", need.c_str());
            return 1;
        }
        boots.push_back({std::string("(service) ") + s->need, s->stem, s->stem, "", s->role});
    }
    for (const PartSpec& part : spec.parts) {
        if (!part.set.empty() && part.role.empty()) {
            std::printf("part '%s' declares `set` but no role - a poke needs an address\n",
                        part.name.c_str());
            return 1;
        }
        boots.push_back({part.name, part.name, part.stem, "", part.role});
    }
    for (Boot& b : boots) {
        auto path = resolve_artifact(spec.name, b.stem);
        if (!path) {
            std::printf("artifact '%s.so' not found for %s (searched toy build, workshop "
                        "build, vendored services)\n",
                        b.stem.c_str(), b.part.c_str());
            return 1;
        }
        b.path = path->string();
    }

    std::printf("workshop - containment: %s\n", loom::Kernel::containment_note());
    std::printf("workshop - running '%s': %s\n", spec.name.c_str(), spec.description.c_str());

    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    // S-3: the tap bridge — runtime truth becomes ordinary published intent.
    install_bus_fact_bridge(bus);
    if (flags.watch) {
        // Says what it will and will not show, because a flag that prints
        // nothing on a healthy run reads as broken (cold-user finding #3).
        std::printf("~ tap: watching refusals and lifecycle events. A healthy run prints "
                    "NOTHING here; that is success, not breakage. (Deliveries are not "
                    "shown - a beating bus makes thousands per second; the inspector's "
                    "tallies count them.)\n");
        bus.add_observer([](const loom::BusEvent& e) {
            if (e.kind == loom::EventKind::Refused) {
                std::printf("~ tap: REFUSED %s (%s) sender=%llu target=%llu\n",
                            e.schema_name.c_str(), loom::name_of(e.refusal.reason),
                            static_cast<unsigned long long>(e.sender.value),
                            static_cast<unsigned long long>(e.target.value));
            } else if (e.kind == loom::EventKind::Died) {
                std::printf("~ tap: DIED weave %llu\n",
                            static_cast<unsigned long long>(e.target.value));
            } else if (e.kind == loom::EventKind::Revived) {
                std::printf("~ tap: REVIVED weave %llu%s\n",
                            static_cast<unsigned long long>(e.target.value),
                            e.from_last_known_good ? " (from last-known-good)" : "");
            }
        });
    }

    OperatorContext ctx;
    ctx.project = spec.name;
    ctx.manager = manager;
    ctx.request_stop = [&bus] { bus.stop(); };

    // The refusal demo aims a QueryRunning at the first role-holding PART —
    // a door that part never declared, so the world refuses it (NotAccepted)
    // and the inspector gets something real to explain. The operator fires it
    // when the last boot ANSWER arrives (queue order is not load order — the
    // steward's door is message-composed).
    if (flags.refuse) {
        for (const PartSpec& part : spec.parts) {
            if (!part.role.empty()) {
                ctx.refusal_role = part.role;
                break;
            }
        }
        if (ctx.refusal_role.empty()) {
            std::printf("--refuse: no role-holding part to aim at; skipping the demo\n");
        }
    }

    // Interactive alteration targets: the swap cycle, the declared knobs, the
    // first role-holding part as the reload target.
    if (flags.interactive) {
        ctx.interactive = true;
        for (const char* stem : {"workshop-skin-log", "zengine-skin-tui-classic"}) {
            if (auto path = resolve_artifact(spec.name, stem)) {
                ctx.skins.push_back(SkinChoice{stem, path->string(), stem});
            }
        }
        ctx.knobs = spec.knobs;
        ctx.knob_at.assign(ctx.knobs.size(), 0);
        for (const PartSpec& part : spec.parts) {
            if (!part.role.empty()) {
                ctx.alter_part = part.name;
                ctx.alter_stem = part.stem;
                if (auto path = resolve_artifact(spec.name, part.stem)) {
                    ctx.alter_path = path->string();
                }
                break;
            }
        }
        ctx.parts = spec.parts;
        ctx.needs = spec.needs;
        if (!ctx.alter_stem.empty()) {
            ctx.update_command =
                "cmake --build " + binary_dir() + " --target " + ctx.alter_stem;
        }
    }

    // Declared initial configuration rides the Poke door once each part's
    // up-answer arrives.
    for (const PartSpec& part : spec.parts) {
        if (!part.set.empty()) {
            ctx.sets_by_part[part.name] = part.set;
        }
    }
    ctx.scheduled = flags.pokes;
    for (const ScheduledPoke& p : ctx.scheduled) {
        std::printf("workshop - scheduled: at %llds poke %s.%s = %s\n",
                    static_cast<long long>(p.second), p.role.c_str(), p.field.c_str(),
                    p.value.c_str());
    }

    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
    reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
    reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    for (const PartSpec& part : spec.parts) {
        if (!part.set.empty()) {
            reach.allow_to_role(loom::PokeWrite::zen_name, loom::PokeWrite::zen_version,
                                part.role);
        }
    }
    for (const ScheduledPoke& p : ctx.scheduled) {
        reach.allow_to_role(loom::PokeWrite::zen_name, loom::PokeWrite::zen_version, p.role);
    }
    if (!ctx.refusal_role.empty()) {
        reach.allow_to_role(QueryRunning::zen_name, QueryRunning::zen_version, ctx.refusal_role);
    }
    if (flags.interactive) {
        reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
        reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
        for (const KnobSpec& knob : spec.knobs) {
            reach.allow_to_role(loom::PokeWrite::zen_name, loom::PokeWrite::zen_version,
                                knob.role);
        }
        if (!ctx.alter_part.empty()) {
            for (const PartSpec& part : spec.parts) {
                if (!part.role.empty()) {
                    reach.allow_to_role(loom::PokeDescribe::zen_name,
                                        loom::PokeDescribe::zen_version, part.role);
                    break;
                }
            }
        }
    }
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    // The governor counts world time out loud; scheduled pokes ride its
    // ClockTick. It is mounted whenever anything needs time named — a bounded
    // run, or a scheduled reach-in.
    if (flags.for_seconds > 0 || !ctx.scheduled.empty()) {
        loom::Grant wish;
        wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
        wish.allow_to_any(ClockTick::zen_name, ClockTick::zen_version);
        allow_timed_weave(wish);
        loom::mount_granted<Governor>(bus, std::move(wish), flags.for_seconds);
    }

    // Birth, the same gesture as everything else: ask the steward.
    for (const Boot& b : boots) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{b.part, b.stem, b.role};
        bus.send_as(op, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{b.load_name, b.path, b.role}),
                                  op, op, corr));
    }
    // The world runs inside pump(); the governor's wish or Ctrl-C ends it. A
    // quiescent bus means nothing will ever speak again — say so and leave.
    while (!ctx.quit) {
        bus.pump();
        if (!ctx.quit && bus.pending() == 0) {
            std::printf("\nworkshop - the bus went quiet (no timer service?): exiting.\n");
            break;
        }
    }
    std::printf("\nworkshop - '%s' ended: %lld up, %lld failed\n", spec.name.c_str(),
                static_cast<long long>(ctx.up), static_cast<long long>(ctx.failed));

    // ---- what the Workshop knows, asked the ordinary way -------------------
    // Truth labels are printed with the data: the registry/inspector answers
    // are authenticated (Loom's word it IS the answer); tallies are DERIVED;
    // relayed events are FACT.
    auto* running = ask_role_once<QueryRunning, RunningReport>(bus, kRegistryRole,
                                                              QueryRunning{});
    if (running->answers == 1) {
        std::printf("\n-- what launched (registry, authenticated answer: %s) --\n",
                    running->authenticated ? "yes" : "NO");
        for (const ReportedPart& p : running->report.up) {
            std::printf("   up      %-24s %s%s%s  [reported by weave %lld]\n", p.part.c_str(),
                        p.stem.c_str(), p.role.empty() ? "" : "  as ", p.role.c_str(),
                        static_cast<long long>(p.reporter));
        }
        for (const ReportedPart& p : running->report.failed) {
            std::printf("   FAILED  %-24s %s  [reported by weave %lld]\n", p.part.c_str(),
                        p.reason.c_str(), static_cast<long long>(p.reporter));
        }
    } else {
        std::printf("\n-- registry gave no answer (loaded? replaced?) --\n");
    }

    auto* events = ask_role_once<QueryEvents, EventsReport>(bus, kInspectorRole, QueryEvents{});
    if (events->answers == 1) {
        std::printf("-- what the machine did (inspector; tallies DERIVED from relayed FACTs; "
                    "authenticated: %s) --\n",
                    events->authenticated ? "yes" : "NO");
        std::printf("   delivered %lld | refused %lld\n",
                    static_cast<long long>(events->report.delivered),
                    static_cast<long long>(events->report.refused));
        for (const BusFact& f : events->report.recent_refusals) {
            std::printf("   REFUSED %s %s -> weave %lld\n", f.reason.c_str(), f.schema.c_str(),
                        static_cast<long long>(f.target));
            std::printf("           %s\n", explain_refusal(f.reason));
        }
    } else {
        std::printf("-- inspector gave no answer (loaded? replaced?) --\n");
    }

    return ctx.failed == 0 ? 0 : 1;
}

} // namespace
} // namespace workshop

int main(int argc, char** argv) {
    const std::string cmd = argc > 1 ? argv[1] : "";
    if (cmd == "list") {
        return workshop::cmd_list();
    }
    if (cmd == "describe" && argc > 2) {
        return workshop::cmd_describe(argv[2]);
    }
    if (cmd == "view" && argc > 2) {
        return workshop::cmd_view(argv[2]);
    }
    if (cmd == "export" && argc > 3) {
        const std::string author = argc > 4 ? argv[4] : "";
        return workshop::cmd_export(argv[2], argv[3], author);
    }
    if (cmd == "import" && argc > 2) {
        std::string into;
        std::string as;
        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--into" && i + 1 < argc) {
                into = argv[++i];
            } else if (arg == "--as" && i + 1 < argc) {
                as = argv[++i];
            }
        }
        return workshop::cmd_import(argv[2], into, as);
    }
    if (cmd == "build" && argc > 2) {
        return workshop::cmd_build(argv[2]);
    }
    if (cmd == "new" && argc > 2) {
        return workshop::cmd_new(argv[2]);
    }
    if (cmd == "run" && argc > 2) {
        workshop::RunFlags flags;
        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--for-seconds" && i + 1 < argc) {
                flags.for_seconds = std::atoll(argv[++i]);
            } else if (arg == "--watch") {
                flags.watch = true;
            } else if (arg == "--refuse") {
                flags.refuse = true;
            } else if (arg == "--interactive" || arg == "-i") {
                flags.interactive = true;
            } else if (arg == "--deny" && i + 1 < argc) {
                flags.deny.push_back(argv[++i]);
            } else if (arg == "--poke" && i + 1 < argc) {
                workshop::ScheduledPoke p;
                std::string why;
                if (!workshop::parse_poke(argv[++i], p, why)) {
                    std::printf("--poke '%s': %s\n", argv[i], why.c_str());
                    return 1;
                }
                flags.pokes.push_back(p);
            }
        }
        return workshop::cmd_run(argv[2], flags);
    }
    if (cmd == "schema") {
        return workshop::cmd_schema();
    }
    if (cmd == "safety" && argc > 2) {
        return workshop::cmd_safety(argv[2]);
    }
    std::printf("workshop — the Serious Playground prototype (Night Lab III)\n"
                "  workshop list\n"
                "  workshop describe <toy>      the spec as the gate admits it\n"
                "  workshop view <toy>          the schematic (described shape)\n"
                "  workshop build <toy>         build the toy's parts\n"
                "  workshop new <name>          scaffold a creation\n"
                "  workshop schema              the project.json field reference\n"
                "  workshop export <toy> <dest> [author]   share (author is UNVERIFIED)\n"
                "  workshop import <bundle> [--into <dir>] [--as <name>]\n"
                "  workshop safety <toy>        the power view, unflattering parts included\n"
                "  workshop run <toy> [-i] [--for-seconds N] [--watch] [--refuse]\n"
                "                     [--deny <need>]              run with less power\n"
                "                     [--poke SEC:role.field=value]  reach in while it\n"
                "                        lives, without a terminal (repeatable)\n"
                "\n"
                "  -i needs a real TTY. --poke is the scriptable twin: it reaches into a\n"
                "  running creation from a pipe, a script, or CI.\n");
    return cmd.empty() ? 0 : 1;
}
