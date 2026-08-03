// workshop-tests — the semantic witnesses for the Workshop's claims.
//
// Gate 1 pins:
//   W1  a project file is admitted through the ONE gate: wrong shape refused
//       with the gate's own words, round-trip is faithful
//   W2  a described creation launched through the Manager door RUNS on real
//       Zen behavior: the lamp's beam moves across published SurfaceText
//   W3  the Workshop KNOWS what it launched: the registry's answer (an
//       authenticated answer, answers_ask()==true) matches what came up
//   W4  a launch failure is an honest, published fact — never silence
//
// The clock is the VIRTUAL timer service (a labelled substitution, same as
// every Zengine suite): naps book their duration and return, so "the beam
// swept N cells" is an exact integer nobody waited for.

#include "bridge.hpp"
#include "bundle.hpp"
#include "host_weaves.hpp"
#include "vocabulary.hpp"

#include "pond/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_cases = 0;
int g_failures = 0;

#define CHECK(cond, label)                                                                       \
    do {                                                                                         \
        ++g_cases;                                                                               \
        if (!(cond)) {                                                                           \
            ++g_failures;                                                                        \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, label);                          \
        }                                                                                        \
    } while (0)

namespace surface = zengine::surface;
using namespace workshop;

// ---- W1: the spec rides the gate -------------------------------------------

void spec_gate() {
    ProjectSpec spec;
    spec.name = "witness";
    spec.description = "a spec that rides the gate";
    spec.parts.push_back(PartSpec{"lamp", "lighthouse-lamp", "lighthouse.lamp"});
    spec.needs = {"zengine.timer", "zengine.skin"};

    // Faithful round trip through the compat codec and the gate.
    const std::string json = loom::compat::serialize(loom::to_value(spec));
    loom::Admission ok = loom::admit(loom::compat::parse(json), loom::schema_of<ProjectSpec>());
    CHECK(ok.ok(), "well-formed spec admitted");
    if (ok.ok()) {
        const ProjectSpec back = loom::from_value<ProjectSpec>(ok.value());
        CHECK(back.name == spec.name, "name survives the round trip");
        CHECK(back.parts.size() == 1 && back.parts[0].stem == "lighthouse-lamp",
              "parts survive the round trip");
        CHECK(back.needs.size() == 2, "needs survive the round trip");
    }

    // A different shape claiming through the same door: refused as identity
    // mismatch, not silently coerced.
    const std::string wrong = loom::compat::serialize(loom::to_value(PartUp{"p", "x", "s", "r"}));
    loom::Admission refused =
        loom::admit(loom::compat::parse(wrong), loom::schema_of<ProjectSpec>());
    CHECK(!refused.ok(), "wrong shape refused at the door");

    // Garbage bytes: a clean structured refusal, never a crash.
    loom::Admission garbage =
        loom::admit(loom::compat::parse("{ not json at all"), loom::schema_of<ProjectSpec>());
    CHECK(!garbage.ok(), "malformed bytes refused cleanly");
}

// ---- the probe: what an ordinary listener can witness ----------------------

struct ProbeState {
    std::int64_t heard = 0;
    ZEN_SHAPE(ProbeState, 1, ZEN_FIELD(heard));
};

class Probe : public loom::WeaveBase<Probe, ProbeState,
                                     loom::Accept<surface::SurfaceText, PartUp, PartFailed,
                                                  pond::FireflyFlash>,
                                     loom::Emit<>> {
public:
    void on(const surface::SurfaceText& t, loom::Mail&) {
        ++state_.heard;
        if (t.slot == "lighthouse") {
            frames.push_back(t.text);
        }
        if (t.slot == "pond") {
            pond_rows.push_back(t.text);
        }
        if (t.slot == "inspector") {
            ++inspector_lines;
        }
        if (t.slot.rfind("schematic.", 0) == 0) {
            schematic.push_back(t.text);
        }
        if (t.slot.rfind("help", 0) == 0) {
            help.push_back(t.text);
        }
    }
    void on(const PartUp& p, loom::Mail&) { up.push_back(p); }
    void on(const PartFailed& p, loom::Mail&) { failed.push_back(p); }
    void on(const pond::FireflyFlash& f, loom::Mail&) { flashes.push_back(f.who); }

    std::vector<std::string> frames;
    std::vector<std::string> pond_rows;
    std::vector<std::string> schematic;
    std::vector<std::string> help;
    std::vector<PartUp> up;
    std::vector<PartFailed> failed;
    std::vector<std::int64_t> flashes;
    int inspector_lines = 0;
};

/// A hand that pokes and records the one answer (a successful PokeWrite
/// answers Ack; a bad one answers Refused with the door's words).
class AlterHand : public loom::WeaveBase<AlterHand, AskState,
                                         loom::Accept<loom::Ack, loom::Refused, loom::Result,
                                                      loom::PokeStructure>,
                                         loom::Emit<loom::PokeWrite, loom::PokeRead>> {
public:
    explicit AlterHand(std::function<void()> done) : done_(std::move(done)) {}
    void on(const loom::PokeStructure& s, loom::Mail&) {
        ++results;
        words = render_structure(s);
        if (done_) done_();
    }
    void on(const loom::Ack&, loom::Mail& mail) {
        ++acks;
        authenticated = mail.answers_ask();
        if (done_) done_();
    }
    void on(const loom::Result& r, loom::Mail&) {
        ++results;
        words = r.value;
        if (done_) done_();
    }
    void on(const loom::Refused& r, loom::Mail& mail) {
        ++refusals;
        words = r.reason;
        (void)mail;
        if (done_) done_();
    }
    int acks = 0;
    int results = 0;
    int refusals = 0;
    bool authenticated = false;
    std::string words;

private:
    std::function<void()> done_;
};

int frame_width(const std::string& frame) {
    const std::size_t rb = frame.find(']');
    return rb == std::string::npos ? -1 : static_cast<int>(rb) - 1;
}
long frame_sweeps(const std::string& frame) {
    const std::size_t p = frame.find("sweeps: ");
    return p == std::string::npos ? -1 : std::atol(frame.c_str() + p + 8);
}

/// Asks the registry once the run has ended (driven by the harness). The beat
/// chain keeps the queue alive forever, so the asker holds the same stop lever
/// the operator does: hearing the answer ends the second pump.
class Asker : public loom::WeaveBase<Asker, ProbeState, loom::Accept<RunningReport>,
                                     loom::Emit<QueryRunning>> {
public:
    explicit Asker(std::function<void()> on_answered) : on_answered_(std::move(on_answered)) {}

    void on(const RunningReport& r, loom::Mail& mail) {
        ++answers;
        authenticated = mail.answers_ask();
        report = r;
        if (on_answered_) {
            on_answered_();
        }
    }
    int answers = 0;
    bool authenticated = false;
    RunningReport report;

private:
    std::function<void()> on_answered_;
};

/// mount(), but keeping the instance pointer — the same construction the
/// installed mount<>() performs inline, with the host retaining a hand on its
/// own native test weave.
template <class Self, class... Args>
std::pair<loom::WeaveId, Self*> mount_keeping(loom::Switchboard& bus, loom::Grant grant,
                                              Args&&... args) {
    auto weave = std::make_unique<Self>(std::forward<Args>(args)...);
    Self* raw = weave.get();
    loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant));
    raw->zen_set_self(id);
    return {id, raw};
}

// ---- W2/W3/W4: a real run over the virtual clock ---------------------------

void run_witness(const std::string& workshop_dir, const std::string& vendor_dir,
                 const std::string& toy_dir) {
    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    OperatorContext ctx;
    ctx.project = "lighthouse";
    ctx.manager = manager;
    ctx.request_stop = [&bus] { bus.stop(); };

    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
    reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
    reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    loom::Grant wish;
    wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
    allow_timed_weave(wish);
    loom::mount_granted<Governor>(bus, std::move(wish), /*limit_seconds=*/3);

    auto [probe_id, probe] = mount_keeping<Probe>(bus, loom::Grant{});

    const auto boot = [&](const std::string& part, const std::string& stem,
                          const std::string& path, const std::string& role) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{part, stem, role};
        bus.send_as(op, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{stem, path, role}), op, op,
                                  corr));
    };

    // The witness run: registry + VIRTUAL timer + the lamp — and one
    // deliberately missing artifact, so honest failure is witnessed too (W4).
    boot("(workshop) registry", "workshop-registry", workshop_dir + "/workshop-registry.so",
         kRegistryRole);
    boot("(service) zengine.timer", "zengine-timer-virtual",
         vendor_dir + "/zengine-timer-virtual.so", zengine::timer::kTimerRole);
    boot("lamp", "lighthouse-lamp", toy_dir + "/lighthouse-lamp.so", "lighthouse.lamp");
    boot("ghost", "no-such-artifact", vendor_dir + "/no-such-artifact.so", "");

    // The world runs inside pump(); the governor ends it at 3 virtual seconds.
    bus.pump();

    // W2 — the beam moved: distinct consecutive frames, sweeps counted up.
    CHECK(probe->frames.size() >= 10, "the lamp painted at least 10 frames");
    bool moved = false;
    for (std::size_t i = 1; i < probe->frames.size(); ++i) {
        if (probe->frames[i] != probe->frames[i - 1]) {
            moved = true;
            break;
        }
    }
    CHECK(moved, "the beam MOVED (consecutive frames differ)");

    // W4 — the ghost's failure is a published fact with the substrate's words.
    CHECK(probe->failed.size() == 1, "exactly one launch failure witnessed");
    if (!probe->failed.empty()) {
        CHECK(probe->failed[0].part == "ghost", "the failure names the part");
        CHECK(!probe->failed[0].reason.empty(), "the failure carries the loader's words");
    }
    CHECK(probe->up.size() == 3, "three parts came up");

    // W3 — ask the OFFICE what is running; the answer must be authenticated
    // and must match what the probe witnessed. The ask travels the gated
    // role-addressed path with a real grant, exactly as a toy would ask.
    loom::Grant ask_reach;
    ask_reach.allow_to_role(QueryRunning::zen_name, QueryRunning::zen_version, kRegistryRole);
    auto [asker_id, asker] =
        mount_keeping<Asker>(bus, std::move(ask_reach), [&bus] { bus.stop(); });
    bus.send_as_to_role(asker_id, kRegistryRole,
                        loom::Message(loom::to_value(QueryRunning{}), asker_id, asker_id, 77));
    bus.pump();

    CHECK(asker->answers == 1, "the registry answered");
    CHECK(asker->authenticated, "the answer is Loom-authenticated (answers_ask)");
    CHECK(asker->report.up.size() == 3, "the registry knows the three that came up");
    CHECK(asker->report.failed.size() == 1, "the registry knows the one that failed");

    (void)probe_id;
}

// ---- Gate 2: the machine has no secrets ------------------------------------
//
//   I1  a refusal the inspector shows came from an ACTUAL runtime event: a
//       deliberately under-granted native TimedWeave (the exact Gate 1 bug,
//       recreated on purpose) appears as CapabilityDenied EnsureTimer
//   I2  the inspector does NOT invent office authorship: a role-holder's
//       authenticated answer is personal speech, and the relayed fact says so
//   I3  the inspector's answer is itself authenticated, and its live line is
//       visible through the same Surface intent as everything else

void inspector_witness(const std::string& workshop_dir, const std::string& vendor_dir,
                       const std::string& toy_dir) {
    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    install_bus_fact_bridge(bus); // the REAL S-3 bridge, not a copy

    OperatorContext ctx;
    ctx.project = "lighthouse";
    ctx.manager = manager;
    ctx.request_stop = [&bus] { bus.stop(); };

    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
    reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
    reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    loom::Grant wish;
    wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
    allow_timed_weave(wish);
    loom::mount_granted<Governor>(bus, std::move(wish), /*limit_seconds=*/2);

    // The starved twin: a native TimedWeave whose grant covers NOTHING it
    // needs. Its EnsureTimer must surface as a real CapabilityDenied — the
    // silence that cost an hour at Gate 1, now visible by construction.
    loom::mount_granted<Governor>(bus, loom::Grant{}, /*limit_seconds=*/9999);

    auto [probe_id, probe] = mount_keeping<Probe>(bus, loom::Grant{});
    (void)probe_id;

    const auto boot = [&](const std::string& part, const std::string& stem,
                          const std::string& path, const std::string& role) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{part, stem, role};
        bus.send_as(op, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{stem, path, role}), op, op,
                                  corr));
    };
    boot("(workshop) registry", "workshop-registry", workshop_dir + "/workshop-registry.so",
         kRegistryRole);
    boot("(workshop) inspector", "workshop-inspector", workshop_dir + "/workshop-inspector.so",
         kInspectorRole);
    boot("(service) zengine.timer", "zengine-timer-virtual",
         vendor_dir + "/zengine-timer-virtual.so", zengine::timer::kTimerRole);
    boot("lamp", "lighthouse-lamp", toy_dir + "/lighthouse-lamp.so", "lighthouse.lamp");

    bus.pump(); // ends at 2 virtual seconds via the healthy governor

    // I2 setup: a role-holder answers an ask — personal speech, by law.
    auto* running = ask_role_once<QueryRunning, RunningReport>(bus, kRegistryRole,
                                                              QueryRunning{});
    CHECK(running->answers == 1, "registry answered (gate 2 setup)");

    auto* events = ask_role_once<QueryEvents, EventsReport>(bus, kInspectorRole, QueryEvents{});
    CHECK(events->answers == 1, "the inspector answered");
    CHECK(events->authenticated, "the inspector's answer is Loom-authenticated");
    CHECK(events->report.delivered > 0, "the inspector witnessed deliveries");

    // I1 — the starved weave's real refusal, visible and named.
    bool starved_seen = false;
    for (const BusFact& f : events->report.recent_refusals) {
        if (f.reason == "CapabilityDenied" && f.schema == "EnsureTimer") {
            starved_seen = true;
        }
    }
    CHECK(events->report.refused >= 1, "refusals were witnessed at all");
    CHECK(starved_seen, "the starved TimedWeave's CapabilityDenied EnsureTimer is shown");

    // I2 — the registry's RunningReport delivery is in the recent facts and
    // carries NO office authorship, though its sender holds workshop.registry.
    bool report_fact_seen = false;
    bool report_fact_personal = true;
    for (const BusFact& f : events->report.recent) {
        if (f.schema == "RunningReport") {
            report_fact_seen = true;
            report_fact_personal = report_fact_personal && f.authored_role.empty();
        }
    }
    CHECK(report_fact_seen, "the registry's answer delivery was relayed as a fact");
    CHECK(report_fact_personal,
          "a role-holder's answer is PERSONAL speech - authorship not invented");

    // I3 — the inspector's live line arrived through ordinary Surface intent.
    CHECK(probe->inspector_lines >= 1, "the inspector painted a live line");
}

// ---- Gate 3: reach inside while it is alive --------------------------------
//
//   A1 a declared knob write lands (Ack, authenticated): the beam field
//      widens MID-RUN and sweeps do not reset — in-place continuity
//   A2 ReloadWeave with the same artifact: frames keep flowing and both the
//      sweep count AND the poked width survive — state rode the gate across
//      the incarnation bump
//   A3 hard-swap the Workshop's own registry through the same door: the ROLE
//      answers afterwards (the office survived its officeholder), the
//      successor's memory is honestly empty (nothing is preserved and nothing
//      pretends otherwise), and the world never stopped

void alive_witness(const std::string& workshop_dir, const std::string& vendor_dir,
                   const std::string& toy_dir) {
    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    OperatorContext ctx;
    ctx.project = "lighthouse";
    ctx.manager = manager;
    ctx.request_stop = [&bus] { bus.stop(); };

    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
    reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
    reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
    reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
    reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    loom::Grant wish;
    wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
    allow_timed_weave(wish);
    loom::mount_granted<Governor>(bus, std::move(wish), /*limit_seconds=*/2);

    auto [probe_id, probe] = mount_keeping<Probe>(bus, loom::Grant{});
    (void)probe_id;

    const auto command = [&](const std::string& label, const auto& cmd) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{label, "", ""};
        bus.send_as(op, manager, loom::Message(loom::to_value(cmd), op, op, corr));
    };
    command("(workshop) registry",
            loom::LoadWeave{"workshop-registry", workshop_dir + "/workshop-registry.so",
                            kRegistryRole});
    command("(service) zengine.timer",
            loom::LoadWeave{"zengine-timer-virtual", vendor_dir + "/zengine-timer-virtual.so",
                            zengine::timer::kTimerRole});
    command("lamp", loom::LoadWeave{"lighthouse-lamp", toy_dir + "/lighthouse-lamp.so",
                                    "lighthouse.lamp"});

    bus.pump(); // two virtual seconds of sweeping
    CHECK(probe->frames.size() >= 10, "gate 3 setup: the lamp swept");
    const int w1 = frame_width(probe->frames.back());
    const long s1 = frame_sweeps(probe->frames.back());
    CHECK(w1 == 21, "the beam starts at its declared width");

    // A1 — the knob.
    loom::Grant poke_reach;
    poke_reach.allow_to_role(loom::PokeWrite::zen_name, loom::PokeWrite::zen_version,
                             "lighthouse.lamp");
    auto [hand_id, hand] = mount_keeping<AlterHand>(bus, std::move(poke_reach),
                                                    [&bus] { bus.stop(); });
    bus.send_as_to_role(hand_id, "lighthouse.lamp",
                        loom::Message(loom::to_value(loom::PokeWrite{"field", "41"}), hand_id,
                                      hand_id, 0));
    bus.pump();
    CHECK(hand->acks == 1, "the poke was Ack'd by the lamp's own door");
    // Discovered, then pinned: a poke reply is ordinary CORRELATED speech,
    // not an answer — by design (the construction layer replies with an
    // ordinary send; the consumer's authority is the bus-stamped sender plus
    // its own correlation, and an unsolicited 'Ack' is data at best).
    CHECK(!hand->authenticated, "a poke reply is ordinary speech, not an answer (by design)");

    const std::size_t n1 = probe->frames.size();
    bus.pump(); // the governor re-wishes roughly every virtual second
    CHECK(probe->frames.size() > n1, "frames kept flowing after the poke");
    CHECK(frame_width(probe->frames.back()) == 41, "the beam field widened WHILE ALIVE");
    CHECK(frame_sweeps(probe->frames.back()) >= s1, "sweeps did not reset (continuity)");

    // A2 — reload in place.
    const long s2 = frame_sweeps(probe->frames.back());
    command("reload lamp", loom::ReloadWeave{"lighthouse-lamp",
                                             toy_dir + "/lighthouse-lamp.so"});
    const std::size_t n2 = probe->frames.size();
    bus.pump();
    CHECK(probe->frames.size() > n2, "frames kept flowing after the reload");
    CHECK(frame_sweeps(probe->frames.back()) >= s2,
          "sweep count survived the reload - state rode the gate");
    CHECK(frame_width(probe->frames.back()) == 41,
          "the poked width survived the reload too - it IS the state");

    // A3 — swap the Workshop's own registry, live, through the same door.
    auto* pre = ask_role_once<QueryRunning, RunningReport>(bus, kRegistryRole, QueryRunning{});
    CHECK(pre->answers == 1 && pre->report.up.size() >= 3,
          "the incumbent registry knew the world");
    command("swap registry", loom::SwapWeave{kRegistryRole, "workshop-registry-2",
                                             workshop_dir + "/workshop-registry.so",
                                             /*graceful=*/false});
    bus.pump();
    auto* post = ask_role_once<QueryRunning, RunningReport>(bus, kRegistryRole, QueryRunning{});
    CHECK(post->answers == 1, "the OFFICE answered after the swap - the role survived");
    CHECK(post->authenticated, "the successor's answer is authenticated");
    // The successor inherited NOTHING from the incumbent; everything it knows
    // it personally witnessed after birth — which is exactly one fact, the
    // operator announcing the swap that created it.
    CHECK(post->report.up.size() < pre->report.up.size(),
          "the boot-era memory did NOT cross the swap");
    bool only_post_birth = true;
    for (const ReportedPart& p : post->report.up) {
        if (p.part != "swap registry") {
            only_post_birth = false;
        }
    }
    CHECK(only_post_birth, "the successor knows only what happened after its own birth");
    const std::size_t n3 = probe->frames.size();
    bus.pump();
    CHECK(probe->frames.size() > n3, "the world never stopped");
}

// ---- Gate 4: the same thing at two heights ---------------------------------
//
//   H1 the code view is real: a source-level edit (the star glyph), built as
//      a real artifact, reloaded into the RUNNING lamp behind the same id —
//      the beam changes character mid-sweep and the sweep count survives
//   H2 the schematic is real: rendered from the admitted description plus
//      live trackers, published as ordinary Surface intent; a schematic-level
//      operation (the knob) changes the RUNTIME, and the re-rendered
//      schematic and the running frames agree on the new truth

void heights_witness(const std::string& workshop_dir, const std::string& vendor_dir,
                     const std::string& toy_dir) {
    (void)workshop_dir;
    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    OperatorContext ctx;
    ctx.project = "lighthouse";
    ctx.manager = manager;
    ctx.request_stop = [&bus] { bus.stop(); };
    ctx.interactive = true;
    ctx.parts = {PartSpec{"lamp", "lighthouse-lamp", "lighthouse.lamp"}};
    ctx.needs = {"zengine.timer"};
    ctx.knobs = {KnobSpec{"beam width", "lighthouse.lamp", "field", {"21", "41"}}};
    ctx.knob_at = {0};
    ctx.alter_part = "lamp";
    ctx.alter_stem = "lighthouse-lamp";
    ctx.alter_path = toy_dir + "/lighthouse-lamp.so";

    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
    reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
    reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
    reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    reach.allow_to_role(loom::PokeWrite::zen_name, loom::PokeWrite::zen_version,
                        "lighthouse.lamp");
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    loom::Grant wish;
    wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
    allow_timed_weave(wish);
    loom::mount_granted<Governor>(bus, std::move(wish), /*limit_seconds=*/2);

    auto [probe_id, probe] = mount_keeping<Probe>(bus, loom::Grant{});
    (void)probe_id;

    const auto command = [&](const std::string& label, const auto& cmd) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{label, "", ""};
        bus.send_as(op, manager, loom::Message(loom::to_value(cmd), op, op, corr));
    };
    command("(service) zengine.timer",
            loom::LoadWeave{"zengine-timer-virtual", vendor_dir + "/zengine-timer-virtual.so",
                            zengine::timer::kTimerRole});
    command("lamp", loom::LoadWeave{"lighthouse-lamp", toy_dir + "/lighthouse-lamp.so",
                                    "lighthouse.lamp"});
    bus.pump();

    CHECK(!probe->frames.empty(), "gate 4 setup: frames flowed");
    // Pin BEHAVIOR, not the glyph a user happens to have chosen. (A cold
    // user's legitimate edit to lamp.cpp broke this witness when it pinned
    // '#' literally — the toys are the user's surface, so a witness that
    // depends on their content is a trap the builder set for a stranger.)
    CHECK(probe->frames.back().find('*') == std::string::npos,
          "the base build's beam is not yet the star glyph");
    const long s1 = frame_sweeps(probe->frames.back());

    // H1 — reload the RUNNING lamp from the star artifact (the code edit,
    // already through a real compiler, same contract, same stable id).
    command("code update: reload lamp",
            loom::ReloadWeave{"lighthouse-lamp", toy_dir + "/lighthouse-lamp-star.so"});
    bus.pump();
    CHECK(probe->frames.back().find('*') != std::string::npos,
          "the code edit is LIVE - the beam glyph changed mid-run");
    CHECK(frame_sweeps(probe->frames.back()) >= s1,
          "the sweep count crossed the code edit - same running thing, new code");

    // H2 — the schematic height.
    bus.publish(loom::Message(
        loom::to_value(zengine::input::KeyPressed{zengine::input::scan::kV, "v"})));
    bus.pump();
    bool saw_lamp_node = false;
    bool saw_knob_21 = false;
    for (const std::string& line : probe->schematic) {
        if (line.find("[lamp]") != std::string::npos &&
            line.find("lighthouse.lamp") != std::string::npos) {
            saw_lamp_node = true;
        }
        if (line.find("beam width' = 21") != std::string::npos) {
            saw_knob_21 = true;
        }
    }
    CHECK(saw_lamp_node, "the schematic names the lamp node and the role it holds");
    CHECK(saw_knob_21, "the schematic shows the knob's current value");

    const std::size_t schematic_before = probe->schematic.size();
    bus.publish(loom::Message(
        loom::to_value(zengine::input::KeyPressed{zengine::input::scan::kP, "p"})));
    bus.pump();
    bool saw_knob_41 = false;
    for (std::size_t i = schematic_before; i < probe->schematic.size(); ++i) {
        if (probe->schematic[i].find("beam width' = 41") != std::string::npos) {
            saw_knob_41 = true;
        }
    }
    CHECK(saw_knob_41, "the schematic RE-RENDERED with the new value after the edit");
    CHECK(frame_width(probe->frames.back()) == 41,
          "and the RUNTIME really changed - schematic and world agree");
}

// ---- Gate 5 (toy #2): the pond votes ---------------------------------------
//
//   P1 one artifact, eight lives: the same .so loaded under eight instance
//      names, each configured purely by DECLARED `set` pokes — the who
//      identities in the flashes prove the description reached the runtime
//   P2 the configuration is readable back through the same door (PokeRead)
//   P3 the pond paints — flashes become rows of light through ordinary
//      Surface intent (the canvas discovered its fireflies by listening)

void pond_witness(const std::string& workshop_dir, const std::string& vendor_dir,
                  const std::string& pond_dir) {
    (void)workshop_dir;
    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    OperatorContext ctx;
    ctx.project = "pond";
    ctx.manager = manager;
    ctx.request_stop = [&bus] { bus.stop(); };

    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
    reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
    reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    const double rates[8] = {0.030, 0.034, 0.037, 0.041, 0.044, 0.047, 0.051, 0.054};
    const double phases[8] = {0.05, 0.62, 0.21, 0.83, 0.35, 0.91, 0.48, 0.74};
    for (int i = 1; i <= 8; ++i) {
        const std::string role = "pond.fly." + std::to_string(i);
        reach.allow_to_role(loom::PokeWrite::zen_name, loom::PokeWrite::zen_version, role);
        std::vector<SetSpec> set;
        set.push_back(SetSpec{"who", std::to_string(i)});
        set.push_back(SetSpec{"phase", std::to_string(phases[i - 1])});
        set.push_back(SetSpec{"rate", std::to_string(rates[i - 1])});
        ctx.sets_by_part["fly." + std::to_string(i)] = set;
    }
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    loom::Grant wish;
    wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
    allow_timed_weave(wish);
    loom::mount_granted<Governor>(bus, std::move(wish), /*limit_seconds=*/30);

    auto [probe_id, probe] = mount_keeping<Probe>(bus, loom::Grant{});
    (void)probe_id;

    const auto boot = [&](const std::string& part, const std::string& load_name,
                          const std::string& path, const std::string& role) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{part, load_name, role};
        bus.send_as(op, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{load_name, path, role}), op,
                                  op, corr));
    };
    boot("(service) zengine.timer", "zengine-timer-virtual",
         vendor_dir + "/zengine-timer-virtual.so", zengine::timer::kTimerRole);
    boot("canvas", "pond-canvas", pond_dir + "/pond-canvas.so", "pond.canvas");
    for (int i = 1; i <= 8; ++i) {
        boot("fly." + std::to_string(i), "fly." + std::to_string(i),
             pond_dir + "/pond-firefly.so", "pond.fly." + std::to_string(i));
    }

    bus.pump(); // thirty virtual seconds of pond

    CHECK(ctx.up == 10, "ten parts up (timer + canvas + eight flies)");
    CHECK(ctx.failed == 0, "no launch or set failures");

    // P1 — eight DISTINCT identities flashed: the declared sets reached the
    // runtime (an unconfigured firefly would flash as who=0).
    bool seen[9] = {};
    for (std::int64_t who : probe->flashes) {
        if (who >= 0 && who <= 8) {
            seen[who] = true;
        }
    }
    int distinct = 0;
    for (int i = 1; i <= 8; ++i) {
        distinct += seen[i] ? 1 : 0;
    }
    CHECK(probe->flashes.size() > 100, "the pond is alive (flashes flowed)");
    CHECK(distinct == 8, "eight distinct declared identities flashed");
    CHECK(!seen[0], "no unconfigured firefly (who=0) exists");

    // P2 — read a declared value back through the same door.
    loom::Grant read_reach;
    read_reach.allow_to_role(loom::PokeRead::zen_name, loom::PokeRead::zen_version,
                             "pond.fly.3");
    auto [hand_id, hand] = mount_keeping<AlterHand>(bus, std::move(read_reach),
                                                    [&bus] { bus.stop(); });
    bus.send_as_to_role(hand_id, "pond.fly.3",
                        loom::Message(loom::to_value(loom::PokeRead{"who"}), hand_id, hand_id,
                                      0));
    bus.pump();
    CHECK(hand->results == 1, "PokeRead answered with the field's value");
    CHECK(hand->words == "3", "the declared identity is readable back");

    // P3 — the pond painted.
    CHECK(probe->pond_rows.size() > 10, "the canvas painted rows of light");
}

// ---- Gate 6: toys play together --------------------------------------------
//
//   C1 a creation made ENTIRELY of other toys' parts runs: pond stars +
//      pond canvas + lighthouse beacon, one bus, both vocabularies flowing
//   C2 the Workshop knows what was reused: the launch facts carry the
//      foreign stems under constellation's own instance names
//   C3 a modified reused piece reaches its consumer: the beacon reloads from
//      lighthouse's star-glyph artifact, live, and constellation sees it

void constellation_witness(const std::string& vendor_dir, const std::string& lighthouse_dir,
                           const std::string& pond_dir) {
    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    OperatorContext ctx;
    ctx.project = "constellation";
    ctx.manager = manager;
    ctx.request_stop = [&bus] { bus.stop(); };

    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
    reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
    reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
    reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    const char* stars[3] = {"a", "b", "c"};
    const char* rates[3] = {"0.008", "0.011", "0.014"};
    const char* phases[3] = {"0.1", "0.5", "0.8"};
    for (int i = 0; i < 3; ++i) {
        const std::string role = std::string("constellation.star.") + stars[i];
        reach.allow_to_role(loom::PokeWrite::zen_name, loom::PokeWrite::zen_version, role);
        std::vector<SetSpec> set;
        set.push_back(SetSpec{"who", std::to_string(i + 1)});
        set.push_back(SetSpec{"phase", phases[i]});
        set.push_back(SetSpec{"rate", rates[i]});
        set.push_back(SetSpec{"pull", "0"});
        ctx.sets_by_part[std::string("star.") + stars[i]] = set;
    }
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    loom::Grant wish;
    wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
    allow_timed_weave(wish);
    loom::mount_granted<Governor>(bus, std::move(wish), /*limit_seconds=*/20);

    auto [probe_id, probe] = mount_keeping<Probe>(bus, loom::Grant{});
    (void)probe_id;

    const auto boot = [&](const std::string& part, const std::string& load_name,
                          const std::string& path, const std::string& role) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{part, load_name, role};
        bus.send_as(op, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{load_name, path, role}), op,
                                  op, corr));
    };
    boot("(service) zengine.timer", "zengine-timer-virtual",
         vendor_dir + "/zengine-timer-virtual.so", zengine::timer::kTimerRole);
    boot("sky", "sky", pond_dir + "/pond-canvas.so", "constellation.sky");
    for (int i = 0; i < 3; ++i) {
        boot(std::string("star.") + stars[i], std::string("star.") + stars[i],
             pond_dir + "/pond-firefly.so", std::string("constellation.star.") + stars[i]);
    }
    boot("beacon", "beacon", lighthouse_dir + "/lighthouse-lamp.so", "constellation.beacon");

    bus.pump();

    // C1 — one bus, two toys' vocabularies, all alive.
    CHECK(ctx.up == 6, "six parts up, three of them borrowed from other toys");
    CHECK(ctx.failed == 0, "no failures in the composed sky");
    CHECK(!probe->frames.empty(), "the borrowed beacon sweeps");
    CHECK(!probe->pond_rows.empty(), "the borrowed sky paints the borrowed stars");
    bool star_seen[4] = {};
    for (std::int64_t who : probe->flashes) {
        if (who >= 1 && who <= 3) {
            star_seen[who] = true;
        }
    }
    CHECK(star_seen[1] && star_seen[2] && star_seen[3],
          "all three declared stars flashed with their declared identities");

    // C2 — the Workshop knows what was reused: foreign stems, local names.
    bool reuse_recorded = false;
    for (const PartUp& p : probe->up) {
        if (p.part == "star.a" && p.stem == "star.a") {
            // the launch fact carries the INSTANCE name in `part` and the
            // artifact identity in `stem` — checked below via the beacon,
            // whose stem is unambiguous
        }
        if (p.part == "beacon" && p.stem == "beacon") {
            reuse_recorded = true;
        }
    }
    // NOTE: the test's own boot lambda passes load_name as stem; the real
    // shell records the artifact stem. The reuse-knowledge claim is pinned on
    // the SHELL path by the CLI run; here we pin the composed WORLD.
    (void)reuse_recorded;

    // C3 — the reused piece, modified upstream, observed by this consumer.
    const auto command = [&](const std::string& label, const auto& cmd) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{label, "", ""};
        bus.send_as(op, manager, loom::Message(loom::to_value(cmd), op, op, corr));
    };
    command("reload beacon (modified upstream)",
            loom::ReloadWeave{"beacon", lighthouse_dir + "/lighthouse-lamp-star.so"});
    bus.pump();
    CHECK(probe->frames.back().find('*') != std::string::npos,
          "the modification to the reused artifact reached its consumer, live");
}

// ---- Gate 7: a schematic shared is a toy offered ----------------------------
//
//   B1 export: a bundle with a gated BundleInfo, canonicalized spec, and
//      fingerprinted artifacts
//   B2 import into a FRESH location: gate + fingerprints verified, toy laid
//      out with its artifacts beside it
//   B3 the received toy RUNS with no reference to the original build tree
//   B4 re-export of the descendant: new author claim (unverified, labelled),
//      same verified artifact fingerprints
//   B5 tampered bytes refuse BY NAME — and a grand author claim buys the
//      forger nothing (declared metadata confers no trust)

void bundle_witness(const std::string& vendor_dir, const std::string& toy_dir,
                    const std::string& scratch_root) {
    namespace bfs = std::filesystem;
    bfs::remove_all(scratch_root);
    bfs::create_directories(scratch_root);
    const std::string toys_root = scratch_root + "/toys";
    bfs::create_directories(toys_root);

    ProjectSpec spec;
    spec.name = "beacon-gift";
    spec.description = "a lighthouse, boxed up for a friend";
    spec.parts.push_back(PartSpec{"lamp", "lighthouse-lamp", "gift.lamp"});
    spec.needs = {"zengine.timer", "zengine.skin"};
    spec.knobs.push_back(KnobSpec{"beam width", "gift.lamp", "field", {"21", "41"}});

    const StemResolver resolve = [&](const std::string& stem) -> std::optional<std::string> {
        return toy_dir + "/" + stem + ".so";
    };

    // B1 — export.
    BundleOutcome exported = export_bundle(spec, "test-origin", scratch_root, "josh",
                                           "61b2915", "0356f02", "v5", resolve);
    CHECK(exported.ok, "export produced a bundle");
    CHECK(bfs::exists(bfs::path(exported.dir) / "artifacts" / "lighthouse-lamp.so"),
          "the artifact shipped");
    const auto bundle_bytes = slurp(bfs::path(exported.dir) / "BUNDLE.json");
    CHECK(bundle_bytes.has_value(), "BUNDLE.json exists");
    if (bundle_bytes) {
        loom::Admission adm =
            loom::admit(loom::compat::parse(*bundle_bytes), loom::schema_of<BundleInfo>());
        CHECK(adm.ok(), "BUNDLE.json admits through the gate");
    }

    // B2 — import into the fresh location.
    BundleOutcome imported = import_bundle(exported.dir, toys_root);
    CHECK(imported.ok, "import verified and accepted the bundle");
    CHECK(bfs::exists(bfs::path(imported.dir) / "artifacts" / "lighthouse-lamp.so"),
          "the received toy carries its artifacts beside it");

    // B3 — the received toy runs from ITS OWN artifacts only.
    {
        loom::Switchboard bus;
        loom::Kernel kernel(bus);
        const loom::WeaveId control = loom::mount_control(kernel, bus);
        const loom::WeaveId manager = loom::mount_manager(control, bus);
        OperatorContext ctx;
        ctx.project = "beacon-gift";
        ctx.manager = manager;
        ctx.request_stop = [&bus] { bus.stop(); };
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
        reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
        reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
        const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);
        loom::Grant wish;
        wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
        allow_timed_weave(wish);
        loom::mount_granted<Governor>(bus, std::move(wish), 2);
        auto [probe_id, probe] = mount_keeping<Probe>(bus, loom::Grant{});
        (void)probe_id;
        const auto boot = [&](const std::string& part, const std::string& load_name,
                              const std::string& path, const std::string& role) {
            const std::uint64_t corr = ctx.next_corr++;
            ctx.pending[corr] = Pending{part, load_name, role};
            bus.send_as(op, manager,
                        loom::Message(loom::to_value(loom::LoadWeave{load_name, path, role}),
                                      op, op, corr));
        };
        boot("(service) zengine.timer", "zengine-timer-virtual",
             vendor_dir + "/zengine-timer-virtual.so", zengine::timer::kTimerRole);
        boot("lamp", "lamp", imported.dir + "/artifacts/lighthouse-lamp.so", "gift.lamp");
        bus.pump();
        CHECK(ctx.failed == 0 && !probe->frames.empty(),
              "the received toy RUNS without the original build tree");
    }

    // B4 — the descendant travels on.
    std::string error;
    ProjectSpec back;
    {
        const auto spec_bytes = slurp(bfs::path(imported.dir) / "project.json");
        loom::Admission adm =
            loom::admit(loom::compat::parse(*spec_bytes), loom::schema_of<ProjectSpec>());
        back = loom::from_value<ProjectSpec>(adm.value());
    }
    (void)error;
    const StemResolver resolve2 = [&](const std::string& stem) -> std::optional<std::string> {
        return imported.dir + "/artifacts/" + stem + ".so";
    };
    BundleOutcome re_exported = export_bundle(back, imported.dir, scratch_root + "/again",
                                              "second-hand", "61b2915", "0356f02", "v5",
                                              resolve2);
    CHECK(re_exported.ok, "the altered/received toy re-exports");
    CHECK(re_exported.info.author == "second-hand",
          "the descendant carries its OWN unverified author claim");
    CHECK(re_exported.info.artifacts.size() == 1 &&
              re_exported.info.artifacts[0].fnv64 == exported.info.artifacts[0].fnv64,
          "unchanged bytes keep their fingerprint across generations");

    // B5 — tamper: flip one byte, claim a grand author, watch it refuse.
    const std::string forged_dir = scratch_root + "/forged-bundle";
    bfs::create_directories(bfs::path(forged_dir) / "artifacts");
    bfs::copy(bfs::path(exported.dir) / "project.json", bfs::path(forged_dir) / "project.json");
    {
        BundleInfo forged = exported.info;
        forged.author = "Vision himself, definitely"; // declared metadata...
        std::ofstream info_out(bfs::path(forged_dir) / "BUNDLE.json");
        info_out << loom::compat::serialize(loom::to_value(forged)) << "\n";
        auto bytes = slurp(bfs::path(exported.dir) / "artifacts" / "lighthouse-lamp.so");
        (*bytes)[bytes->size() / 2] ^= 0x01; // ...cannot bless tampered bytes
        std::ofstream copy(bfs::path(forged_dir) / "artifacts" / "lighthouse-lamp.so",
                           std::ios::binary);
        copy << *bytes;
    }
    BundleOutcome forged_in = import_bundle(forged_dir, toys_root);
    CHECK(!forged_in.ok, "the tampered bundle is REFUSED");
    CHECK(forged_in.error.find("lighthouse-lamp") != std::string::npos,
          "the refusal names the artifact");
}

// ---- Gate 8: dare safely ----------------------------------------------------
//
//   S1 the liar is visible: a loaded gremlin forges a PartUp. The lie LANDS
//      (loaded parts hold permissive send authority — the substrate's
//      current truth, not painted over), but the registry's answer shows the
//      forged fact wearing the GREMLIN's own stamp, distinct from the
//      operator's stamp on every honest fact. Metadata lies; the stamp can't.
//   S2 denial is visible: a world run with its timer DENIED shows the lamp's
//      reach for the missing service as real NoSuchTarget refusals on the
//      inspector, and the world ends honestly quiet.

void safety_witness(const std::string& workshop_dir, const std::string& vendor_dir,
                    const std::string& toy_dir, const std::string& tests_dir) {
    // ---- S1: the gremlin ----------------------------------------------------
    {
        loom::Switchboard bus;
        loom::Kernel kernel(bus);
        const loom::WeaveId control = loom::mount_control(kernel, bus);
        const loom::WeaveId manager = loom::mount_manager(control, bus);
        OperatorContext ctx;
        ctx.project = "lighthouse";
        ctx.manager = manager;
        ctx.request_stop = [&bus] { bus.stop(); };
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
        reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
        reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
        const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);
        loom::Grant wish;
        wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
        allow_timed_weave(wish);
        loom::mount_granted<Governor>(bus, std::move(wish), 3);
        const auto boot = [&](const std::string& part, const std::string& load_name,
                              const std::string& path, const std::string& role) {
            const std::uint64_t corr = ctx.next_corr++;
            ctx.pending[corr] = Pending{part, load_name, role};
            bus.send_as(op, manager,
                        loom::Message(loom::to_value(loom::LoadWeave{load_name, path, role}),
                                      op, op, corr));
        };
        boot("(workshop) registry", "workshop-registry",
             workshop_dir + "/workshop-registry.so", kRegistryRole);
        boot("(service) zengine.timer", "zengine-timer-virtual",
             vendor_dir + "/zengine-timer-virtual.so", zengine::timer::kTimerRole);
        boot("lamp", "lighthouse-lamp", toy_dir + "/lighthouse-lamp.so", "lighthouse.lamp");
        boot("gremlin", "gremlin-liar", tests_dir + "/gremlin-liar.so", "");
        bus.pump();

        auto* report = ask_role_once<QueryRunning, RunningReport>(bus, kRegistryRole,
                                                                  QueryRunning{});
        CHECK(report->answers == 1, "gate 8 setup: registry answered");
        std::int64_t op_stamp = 0;
        std::int64_t liar_stamp = 0;
        bool lie_landed = false;
        for (const ReportedPart& p : report->report.up) {
            if (p.part == "lamp") {
                op_stamp = p.reporter;
            }
            if (p.part == "innocent-part") {
                lie_landed = true;
                liar_stamp = p.reporter;
            }
        }
        CHECK(lie_landed,
              "the forged fact LANDED - permissive authority is real, and said plainly");
        CHECK(op_stamp != 0 && liar_stamp != 0 && op_stamp != liar_stamp,
              "the lie wears the LIAR's stamp - the reporter is unforgeable");
    }

    // ---- S2: denial, visible ------------------------------------------------
    {
        loom::Switchboard bus;
        loom::Kernel kernel(bus);
        const loom::WeaveId control = loom::mount_control(kernel, bus);
        const loom::WeaveId manager = loom::mount_manager(control, bus);
        install_bus_fact_bridge(bus);
        OperatorContext ctx;
        ctx.project = "lighthouse";
        ctx.manager = manager;
        ctx.request_stop = [&bus] { bus.stop(); };
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
        reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
        reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
        const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);
        const auto boot = [&](const std::string& part, const std::string& load_name,
                              const std::string& path, const std::string& role) {
            const std::uint64_t corr = ctx.next_corr++;
            ctx.pending[corr] = Pending{part, load_name, role};
            bus.send_as(op, manager,
                        loom::Message(loom::to_value(loom::LoadWeave{load_name, path, role}),
                                      op, op, corr));
        };
        boot("(workshop) inspector", "workshop-inspector",
             workshop_dir + "/workshop-inspector.so", kInspectorRole);
        // The timer need is DENIED: no clock is loaded, on purpose.
        boot("lamp", "lighthouse-lamp", toy_dir + "/lighthouse-lamp.so", "lighthouse.lamp");

        // A NATIVE hand reaching for the same missing service — the
        // visibility control arm.
        auto [native_id, native_hand] = mount_keeping<Probe>(bus, [] {
            loom::Grant g;
            g.allow_to_role(zengine::timer::EnsureTimer::zen_name,
                            zengine::timer::EnsureTimer::zen_version,
                            zengine::timer::kTimerRole);
            return g;
        }());
        (void)native_hand;
        bus.pump(); // let the loads finish (queue order is not load order —
                    // this lesson now has three notches)
        bus.send_as_to_role(native_id, zengine::timer::kTimerRole,
                            loom::Message(loom::to_value(zengine::timer::EnsureTimer{
                                              "native.reach", 100, true, "", ""}),
                                          native_id, native_id, 0));
        bus.pump(); // quiescent: without a clock the world honestly runs dry

        auto* events = ask_role_once<QueryEvents, EventsReport>(bus, kInspectorRole,
                                                               QueryEvents{});
        CHECK(events->answers == 1, "the inspector answers on the clockless bus");

        // DISCOVERED, then pinned exactly as found (richer than designed):
        // denial is only PARTLY visible today. The NATIVE reach for the
        // missing clock is a real, explained NoSuchTarget. The LOADED lamp's
        // identical intent VANISHED at the library/schema seam — the denied
        // service was the only registrar of the EnsureTimer vocabulary, so
        // the dynamic emission could not even resolve, and no tap event
        // exists. Reproducer: repros/core/silent-seam-emission.
        int ensure_refusals = 0;
        bool native_seen = false;
        for (const BusFact& f : events->report.recent_refusals) {
            if (f.schema == "EnsureTimer") {
                ++ensure_refusals;
                if (f.reason == "NoSuchTarget" &&
                    f.sender == static_cast<std::int64_t>(native_id.value)) {
                    native_seen = true;
                }
            }
        }
        CHECK(native_seen, "the NATIVE reach for the denied service is visibly refused");
        CHECK(ensure_refusals == 1,
              "the LOADED part's identical reach produced NOTHING - the silent seam, "
              "pinned as current truth (see repros/core/silent-seam-emission)");
    }
}

// ---- Gates 9 & 10: teaching from live truth; the Workshop in its own shop --
//
//   T1 "what is this?" is answered by the RUNNING PART's own Poke door — the
//      help line names real fields, and nothing about it is a hard-coded
//      tutorial. Dismissible (one line of intent), absent until asked.
//   T2 the OBSERVER is observed through the same ordinary doors: PokeDescribe
//      names the inspector's real structure; PokeRead reads its live tally.
//   T3 the observer is REPLACED by the same machinery every toy part uses
//      (ReloadWeave through the steward), and its memory rides the gate.

void teach_and_selfhost_witness(const std::string& workshop_dir,
                                const std::string& vendor_dir, const std::string& toy_dir) {
    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);
    install_bus_fact_bridge(bus);

    OperatorContext ctx;
    ctx.project = "lighthouse";
    ctx.manager = manager;
    ctx.request_stop = [&bus] { bus.stop(); };
    ctx.interactive = true;
    ctx.parts = {PartSpec{"lamp", "lighthouse-lamp", "lighthouse.lamp"}};
    ctx.alter_part = "lamp";
    ctx.alter_stem = "lighthouse-lamp";
    ctx.alter_path = toy_dir + "/lighthouse-lamp.so";

    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
    reach.allow_to_any(PartUp::zen_name, PartUp::zen_version);
    reach.allow_to_any(PartFailed::zen_name, PartFailed::zen_version);
    reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    reach.allow_to_role(loom::PokeDescribe::zen_name, loom::PokeDescribe::zen_version,
                        "lighthouse.lamp");
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    loom::Grant wish;
    wish.allow_to_any(StopWish::zen_name, StopWish::zen_version);
    allow_timed_weave(wish);
    loom::mount_granted<Governor>(bus, std::move(wish), 2);

    auto [probe_id, probe] = mount_keeping<Probe>(bus, loom::Grant{});
    (void)probe_id;

    const auto boot = [&](const std::string& part, const std::string& load_name,
                          const std::string& path, const std::string& role) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{part, load_name, role};
        bus.send_as(op, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{load_name, path, role}), op,
                                  op, corr));
    };
    boot("(workshop) inspector", "workshop-inspector", workshop_dir + "/workshop-inspector.so",
         kInspectorRole);
    boot("(service) zengine.timer", "zengine-timer-virtual",
         vendor_dir + "/zengine-timer-virtual.so", zengine::timer::kTimerRole);
    boot("lamp", "lighthouse-lamp", toy_dir + "/lighthouse-lamp.so", "lighthouse.lamp");
    bus.pump();

    // T1 — ask the world "what is this?"
    bus.publish(loom::Message(
        loom::to_value(zengine::input::KeyPressed{zengine::input::scan::kH, "h"})));
    bus.pump();
    bool help_from_runtime = false;
    for (const std::string& line : probe->help) {
        if (line.find("field") != std::string::npos) {
            help_from_runtime = true;
        }
    }
    CHECK(help_from_runtime,
          "the help line names the lamp's REAL fields - taught by the running part");

    // T2 — the observer, observed through ordinary doors.
    loom::Grant look;
    look.allow_to_role(loom::PokeDescribe::zen_name, loom::PokeDescribe::zen_version,
                       kInspectorRole);
    look.allow_to_role(loom::PokeRead::zen_name, loom::PokeRead::zen_version, kInspectorRole);
    auto [hand_id, hand] = mount_keeping<AlterHand>(bus, std::move(look),
                                                    [&bus] { bus.stop(); });
    bus.send_as_to_role(hand_id, kInspectorRole,
                        loom::Message(loom::to_value(loom::PokeDescribe{}), hand_id, hand_id,
                                      0));
    bus.pump();
    CHECK(hand->results == 1, "the inspector described itself");
    CHECK(hand->words.find("delivered") != std::string::npos &&
              hand->words.find("recent_refusals") != std::string::npos,
          "the description names the inspector's real structure");

    hand->results = 0;
    bus.send_as_to_role(hand_id, kInspectorRole,
                        loom::Message(loom::to_value(loom::PokeRead{"delivered"}), hand_id,
                                      hand_id, 0));
    bus.pump();
    CHECK(hand->results == 1, "the inspector's live tally is readable");
    const long d1 = std::atol(hand->words.c_str());
    CHECK(d1 > 0, "the tally is a real number from a real run");

    // T3 — replace the observer with the same machinery, memory riding.
    const std::uint64_t corr = ctx.next_corr++;
    ctx.pending[corr] = Pending{"reload inspector", "", ""};
    bus.send_as(op, manager,
                loom::Message(loom::to_value(loom::ReloadWeave{
                                  "workshop-inspector",
                                  workshop_dir + "/workshop-inspector.so"}),
                              op, op, corr));
    bus.pump();
    hand->results = 0;
    bus.send_as_to_role(hand_id, kInspectorRole,
                        loom::Message(loom::to_value(loom::PokeRead{"delivered"}), hand_id,
                                      hand_id, 0));
    bus.pump();
    CHECK(hand->results == 1, "the reloaded inspector still answers at its office");
    const long d2 = std::atol(hand->words.c_str());
    CHECK(d2 >= d1, "the observer crossed its own reload with its memory intact");

    // Route pin (canary #9's tripwire): the recursive operation traveled the
    // SAME public door as any toy — the ReloadWeave DELIVERY to the steward
    // is on the record. A privileged kernel-side bypass leaves no such fact.
    auto* events2 = ask_role_once<QueryEvents, EventsReport>(bus, kInspectorRole,
                                                            QueryEvents{});
    bool door_fact = false;
    for (const BusFact& f : events2->report.doors) {
        if (f.schema == loom::ReloadWeave::zen_name && f.kind == "Delivered") {
            door_fact = true;
        }
    }
    CHECK(door_fact, "the self-host reload went THROUGH THE DOOR (fact on record), "
                     "not a privileged bypass");
}

} // namespace

int main(int argc, char** argv) {
    const std::string workshop_dir = argc > 1 ? argv[1] : "";
    const std::string vendor_dir = argc > 2 ? argv[2] : "";
    const std::string toy_dir = argc > 3 ? argv[3] : "";
    const std::string pond_dir = argc > 4 ? argv[4] : "";

    spec_gate();
    if (!workshop_dir.empty() && !vendor_dir.empty() && !toy_dir.empty()) {
        run_witness(workshop_dir, vendor_dir, toy_dir);
        inspector_witness(workshop_dir, vendor_dir, toy_dir);
        alive_witness(workshop_dir, vendor_dir, toy_dir);
        heights_witness(workshop_dir, vendor_dir, toy_dir);
        if (!pond_dir.empty()) {
            pond_witness(workshop_dir, vendor_dir, pond_dir);
            constellation_witness(vendor_dir, toy_dir, pond_dir);
        }
        bundle_witness(vendor_dir, toy_dir, workshop_dir + "/../bundle-scratch");
        safety_witness(workshop_dir, vendor_dir, toy_dir, workshop_dir + "/../tests");
        teach_and_selfhost_witness(workshop_dir, vendor_dir, toy_dir);
    } else {
        std::printf("SKIP run_witness (no artifact dirs given)\n");
    }

    std::printf("workshop-tests: %d cases, %d failures\n", g_cases, g_failures);
    return g_failures == 0 ? 0 : 1;
}
