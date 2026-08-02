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
                                     loom::Accept<surface::SurfaceText, PartUp, PartFailed>,
                                     loom::Emit<>> {
public:
    void on(const surface::SurfaceText& t, loom::Mail&) {
        ++state_.heard;
        if (t.slot == "lighthouse") {
            frames.push_back(t.text);
        }
        if (t.slot == "inspector") {
            ++inspector_lines;
        }
    }
    void on(const PartUp& p, loom::Mail&) { up.push_back(p); }
    void on(const PartFailed& p, loom::Mail&) { failed.push_back(p); }

    std::vector<std::string> frames;
    std::vector<PartUp> up;
    std::vector<PartFailed> failed;
    int inspector_lines = 0;
};

/// A hand that pokes and records the one answer (a successful PokeWrite
/// answers Ack; a bad one answers Refused with the door's words).
class AlterHand : public loom::WeaveBase<AlterHand, AskState,
                                         loom::Accept<loom::Ack, loom::Refused>,
                                         loom::Emit<loom::PokeWrite>> {
public:
    explicit AlterHand(std::function<void()> done) : done_(std::move(done)) {}
    void on(const loom::Ack&, loom::Mail& mail) {
        ++acks;
        authenticated = mail.answers_ask();
        if (done_) done_();
    }
    void on(const loom::Refused& r, loom::Mail& mail) {
        ++refusals;
        words = r.reason;
        (void)mail;
        if (done_) done_();
    }
    int acks = 0;
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
    for (const PartUp& p : post->report.up) {
        if (p.part != "swap registry") {
            only_post_birth = false;
        }
    }
    CHECK(only_post_birth, "the successor knows only what happened after its own birth");
    const std::size_t n3 = probe->frames.size();
    bus.pump();
    CHECK(probe->frames.size() > n3, "the world never stopped");
}

} // namespace

int main(int argc, char** argv) {
    const std::string workshop_dir = argc > 1 ? argv[1] : "";
    const std::string vendor_dir = argc > 2 ? argv[2] : "";
    const std::string toy_dir = argc > 3 ? argv[3] : "";

    spec_gate();
    if (!workshop_dir.empty() && !vendor_dir.empty() && !toy_dir.empty()) {
        run_witness(workshop_dir, vendor_dir, toy_dir);
        inspector_witness(workshop_dir, vendor_dir, toy_dir);
        alive_witness(workshop_dir, vendor_dir, toy_dir);
    } else {
        std::printf("SKIP run_witness (no artifact dirs given)\n");
    }

    std::printf("workshop-tests: %d cases, %d failures\n", g_cases, g_failures);
    return g_failures == 0 ? 0 : 1;
}
