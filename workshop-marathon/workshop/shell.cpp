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
/// output, the workshop's build output, the vendored Zengine services.
std::vector<fs::path> artifact_dirs(const std::string& toy) {
    return {
        fs::path(binary_dir()) / "toys" / toy,
        fs::path(binary_dir()) / "workshop",
        fs::path(source_root()) / "vendor" / "zengine" / "lib",
    };
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
    std::printf("next: add toys/%s/%s-part.cpp and a CMake target named %s-part\n",
                name.c_str(), name.c_str(), name.c_str());
    return 0;
}

struct RunFlags {
    std::int64_t for_seconds = 0;
    bool watch = false;  ///< also print raw tap lines (host stdout diagnostics)
    bool refuse = false; ///< deliberately provoke one refusal, then explain it
};

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
        std::string stem;
        std::string path;
        std::string role;
    };
    std::vector<Boot> boots;
    boots.push_back({"(workshop) registry", "workshop-registry", "", kRegistryRole});
    boots.push_back({"(workshop) inspector", "workshop-inspector", "", kInspectorRole});
    for (const std::string& need : spec.needs) {
        const Service* s = find_service(need);
        if (s == nullptr) {
            std::printf("cannot supply need '%s' (known: timer/skin/input)\n", need.c_str());
            return 1;
        }
        boots.push_back({std::string("(service) ") + s->need, s->stem, "", s->role});
    }
    for (const PartSpec& part : spec.parts) {
        boots.push_back({part.name, part.stem, "", part.role});
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
        bus.add_observer([](const loom::BusEvent& e) {
            if (e.kind == loom::EventKind::Refused) {
                std::printf("~ tap: REFUSED %s (%s) sender=%llu target=%llu\n",
                            e.schema_name.c_str(), loom::name_of(e.refusal.reason),
                            static_cast<unsigned long long>(e.sender.value),
                            static_cast<unsigned long long>(e.target.value));
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

    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
    reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
    reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    if (!ctx.refusal_role.empty()) {
        reach.allow_to_role(QueryRunning::zen_name, QueryRunning::zen_version, ctx.refusal_role);
    }
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    if (flags.for_seconds > 0) {
        loom::Grant wish;
        wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
        allow_timed_weave(wish);
        loom::mount_granted<Governor>(bus, std::move(wish), flags.for_seconds);
    }

    // Birth, the same gesture as everything else: ask the steward.
    for (const Boot& b : boots) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{b.part, b.stem, b.role};
        bus.send_as(op, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{b.stem, b.path, b.role}), op,
                                  op, corr));
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
        for (const PartUp& p : running->report.up) {
            std::printf("   up      %-24s %s%s%s\n", p.part.c_str(), p.stem.c_str(),
                        p.role.empty() ? "" : "  as ", p.role.c_str());
        }
        for (const PartFailed& p : running->report.failed) {
            std::printf("   FAILED  %-24s %s\n", p.part.c_str(), p.reason.c_str());
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
            }
        }
        return workshop::cmd_run(argv[2], flags);
    }
    std::printf("workshop — the Serious Playground prototype (Night Lab III)\n"
                "  workshop list\n"
                "  workshop describe <toy>\n"
                "  workshop new <name>\n"
                "  workshop run <toy> [--for-seconds N] [--watch] [--refuse]\n");
    return cmd.empty() ? 0 : 1;
}
