// The railway: the plant, the signaller, the safety monitor, and one shift.
//
// The host owns the clock and the trains. Everything else is a weave.
//
// Run it:  ./railway ./signal-box.so
//
// It prints a shift log and then checks the things a railway must be able to
// say afterwards. Non-zero exit means one of them was not true.

#include "vocabulary.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace signalbox;

namespace {

// ===========================================================================
// The plant
// ===========================================================================

// A track circuit. It knows one bit and it does not narrate; anybody who wants
// to know pulls it. It sends nothing to anybody, which is why it is registered
// with no authority at all — the emptiest possible grant is the honest one.
class TrackSection : public loom::WeaveBase<TrackSection, SectionState,
                                            loom::Accept<PutInService, TrainEnters, TrainLeaves>,
                                            loom::Emit<>, loom::Claims<Occupancy>> {
public:
    explicit TrackSection(std::string name) { state_.name = std::move(name); }

    // Proving the track circuits. A section that has never had a train on it
    // has never claimed anything, and to a reader that is indistinguishable
    // from a section that is not there — which the interlocking treats, quite
    // correctly, as a reason to refuse every route over it. So the plant is
    // commissioned: each circuit says "clear" once, on purpose, before the
    // first train.
    void on(const PutInService&, loom::Mail& mail) { publish(mail); }

    void on(const TrainEnters& e, loom::Mail& mail) {
        state_.occupied = true;
        ++state_.entries;
        train_ = e.train;
        publish(mail);
    }

    void on(const TrainLeaves&, loom::Mail& mail) {
        state_.occupied = false;
        train_.clear();
        publish(mail);
    }

private:
    // AS THE OFFICE. A reader wants "whatever is currently the junction track
    // circuit", not "weave 4" — so the claim has to live under the office key,
    // and a personal claim would be invisible to it.
    void publish(loom::Mail& mail) {
        mail.as_role(section_office(state_.name))
            .claim(Occupancy{state_.name, state_.occupied, train_});
    }

    // Which train, specifically, is not part of the declared state: a track
    // circuit physically cannot tell you that. It is carried here so the log
    // and the refusal reasons can be useful, and it is deliberately outside
    // the snapshot for the same reason.
    std::string train_;
};

// ===========================================================================
// The safety monitor
// ===========================================================================

// An independent auditor that is NOT in the request path. It cannot grant a
// route, cannot refuse one, and nothing has to consult it for the railway to
// work. It only looks, every sweep, at what everyone currently claims — which
// is the only way to check an interlocking without becoming part of it.
class ConflictMonitor : public loom::WeaveBase<ConflictMonitor, MonitorState, loom::Accept<Sweep>,
                                               loom::Emit<SafetyFault>> {
public:
    void on(const Sweep& s, loom::Mail& mail) {
        ++state_.sweeps;

        const loom::SenseReading panel = mail.latest_from_office<BoxStatus>(kOfficeInterlocking);
        if (!panel) {
            // No box. Not a fault by itself — a railway with no interlocking is
            // a railway where nothing is allowed to move, which is safe.
            ++box_absent_;
            return;
        }

        const BoxStatus box = loom::from_value<BoxStatus>(*panel.value);
        const std::string conflict = conflicting_lock(box);
        if (conflict.empty() || latched_) {
            return;
        }

        latched_ = true;
        ++state_.faults;
        // AS THE MONITOR OFFICE. The signaller is about to stop a railway on
        // the strength of this sentence, so it must be able to tell that the
        // safety monitor said it — not merely that something able to send it
        // this shape said it.
        mail.as_role(kOfficeMonitor).send_to_role(kOfficeSignaller, SafetyFault{conflict, s.at});
    }

    std::int64_t box_absent() const { return box_absent_; }

private:
    // Derived from the layout, independently of the box's own reasoning. If the
    // monitor asked the box "are you safe?" it would be auditing nothing.
    static std::string conflicting_lock(const BoxStatus& box) {
        std::vector<std::string> held;
        if (box.up_locked) {
            held.emplace_back(kRouteUp);
        }
        if (box.down_locked) {
            held.emplace_back(kRouteDown);
        }
        for (std::size_t i = 0; i < held.size(); ++i) {
            for (std::size_t j = i + 1; j < held.size(); ++j) {
                for (const std::string& a : route_sections(held[i])) {
                    for (const std::string& b : route_sections(held[j])) {
                        if (a == b) {
                            return "routes " + held[i] + " and " + held[j] + " both set over " + a;
                        }
                    }
                }
            }
        }
        return "";
    }

    std::int64_t box_absent_ = 0;
    bool latched_ = false;
};

// ===========================================================================
// The signaller
// ===========================================================================

enum class Act { Request, Release, Enter, Step, Leave };

struct Order {
    Act act;
    std::string a; // route, or the section being entered/left/stepped from
    std::string b; // the section being stepped into
    std::string train;
};

// The person at the panel. Holds the shift plan, asks the box for routes, and
// moves trains only when it has been told it may.
class Signaller : public loom::WeaveBase<Signaller, SignallerState,
                                         loom::Accept<Tick, RouteVerdict, SafetyFault>,
                                         loom::Emit<PutInService, RouteRequest, RouteRelease,
                                                    TrainEnters, TrainLeaves>> {
public:
    explicit Signaller(std::vector<Order> plan) : plan_(std::move(plan)) {}

    void on(const Tick& t, loom::Mail& mail) {
        ++state_.step;

        // A request that never came back. It has not happened in any run of
        // this application — see the note in the README about why the panel
        // check below made a timeout unnecessary — but a signaller who leaves a
        // request outstanding and forgets about it is not a signaller.
        if (outstanding_) {
            ++state_.lost;
            outstanding_ = false;
        }

        if (state_.at_danger) {
            log(t.at, "AT DANGER  nothing moves");
            return;
        }
        if (next_ >= plan_.size()) {
            return;
        }

        const Order& order = plan_[next_];
        const bool needs_box = order.act == Act::Request || order.act == Act::Release;

        if (needs_box) {
            // LOOK AT THE PANEL FIRST. An office claim is erased when the office
            // becomes unheld, so "no claim under the interlocking office" is
            // exactly "there is no signal box" — a fact this weave can read
            // synchronously, without sending anything to anybody and without
            // waiting to find out that nobody answered.
            const loom::SenseReading panel =
                mail.latest_from_office<BoxStatus>(kOfficeInterlocking);
            if (!panel) {
                // Dark. That is either "no box" or "a box that has just been
                // installed and has not spoken yet", and from here those look
                // identical. Ring it: if anybody is home the panel lights, and
                // if nobody is, nothing happens and the train keeps standing.
                ++state_.held;
                log(t.at, "HELD       panel dark (" + std::string(loom::name_of(panel.refusal)) +
                              ") - " + order.train + " stands; ringing the box");
                mail.send_to_role(kOfficeInterlocking, PutInService{t.at});
                return;
            }

            // The panel is lit, but is it the same box? The claim carries who
            // made it, so a box that has been taken out and put back is a
            // visibly different author — and a new box remembers none of the
            // routes the old one had set. The plant still knows where the
            // trains are; the box does not know what it had allowed.
            const std::uint64_t author = panel.by.author.value;
            const std::uint64_t life = panel.by.author_life;
            if (seen_box_ && (author != last_author_ || life != last_life_)) {
                log(t.at, "NEW BOX    was weave " + std::to_string(last_author_) + " life " +
                              std::to_string(last_life_) + ", now weave " +
                              std::to_string(author) + " life " + std::to_string(life));
                last_author_ = author;
                last_life_ = life;
                if (!held_route_.empty()) {
                    ++state_.re_set;
                    log(t.at, "RE-SET     " + held_route_ + " for " + held_train_ +
                                  " (the new box never knew about it)");
                    request(mail, held_route_, held_train_);
                    return; // the shift plan has not moved on; this was repair work
                }
            }
            last_author_ = author;
            last_life_ = life;
            seen_box_ = true;
        }

        ++next_;
        switch (order.act) {
        case Act::Request:
            log(t.at, "REQUEST    " + order.a + " for " + order.train);
            request(mail, order.a, order.train);
            break;
        case Act::Release:
            log(t.at, "RELEASE    " + order.a + " for " + order.train);
            mail.send_to_role(kOfficeInterlocking, RouteRelease{order.a, order.train});
            if (held_route_ == order.a) {
                held_route_.clear();
                held_train_.clear();
            }
            break;
        case Act::Enter:
            log(t.at, "MOVE       " + order.train + " into " + order.a);
            mail.send_to_role(section_office(order.a), TrainEnters{order.train});
            break;
        case Act::Step:
            log(t.at, "MOVE       " + order.train + " " + order.a + " -> " + order.b);
            mail.send_to_role(section_office(order.a), TrainLeaves{order.train});
            mail.send_to_role(section_office(order.b), TrainEnters{order.train});
            break;
        case Act::Leave:
            log(t.at, "MOVE       " + order.train + " clear of " + order.a);
            mail.send_to_role(section_office(order.a), TrainLeaves{order.train});
            break;
        }
    }

    void on(const RouteVerdict& v, loom::Mail& mail) {
        // Loom's own word that this is the answer to the request this weave
        // sent. A lookalike RouteVerdict from anywhere else is not this.
        if (!mail.answers_ask()) {
            log(-1, "IGNORED    a RouteVerdict that answers nothing");
            return;
        }
        outstanding_ = false;
        if (v.granted) {
            ++state_.granted;
            held_route_ = v.route;
            held_train_ = v.train;
            log(-1, "  CLEAR    " + v.route + " set for " + v.train);
        } else {
            ++state_.refused;
            log(-1, "  ON       " + v.route + " refused for " + v.train + ": " + v.reason);
        }
    }

    void on(const SafetyFault& f, loom::Mail& mail) {
        // A safety monitor's word stops a railway. Anyone can construct this
        // struct; only the monitor office can have authored it, and holding the
        // office is not the same as having spoken as it.
        if (!mail.authored_from_role(kOfficeMonitor)) {
            ++ignored_faults_;
            log(f.at, "IGNORED    unauthored SafetyFault: " + f.what);
            return;
        }
        state_.at_danger = true;
        log(f.at, "!! DANGER  " + f.what + " -- all signals to danger");
    }

    std::int64_t ignored_faults() const { return ignored_faults_; }
    std::size_t remaining() const { return plan_.size() - next_; }

private:
    void request(loom::Mail& mail, const std::string& route, const std::string& train) {
        outstanding_ = true;
        mail.send_to_role(kOfficeInterlocking, RouteRequest{route, train});
    }

    static void log(std::int64_t at, const std::string& what) {
        if (at >= 0) {
            std::cout << "  t" << (at < 10 ? "0" : "") << at << "  " << what << "\n";
        } else {
            std::cout << "       " << what << "\n";
        }
    }

    std::vector<Order> plan_;
    std::size_t next_ = 0;
    bool outstanding_ = false;
    std::string held_route_;
    std::string held_train_;
    std::uint64_t last_author_ = 0;
    std::uint64_t last_life_ = 0;
    bool seen_box_ = false;
    std::int64_t ignored_faults_ = 0;
};

// ===========================================================================
// Wiring
// ===========================================================================

template <class W, class... Args>
loom::WeaveId mount_office(loom::Switchboard& bus, loom::Grant grant, const std::string& office,
                           Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant), office);
    raw->zen_set_self(id);
    return id;
}

std::vector<Order> shift_plan() {
    return {
        {Act::Request, kRouteUp, "", "1A22"},
        {Act::Request, kRouteDown, "", "2B15"}, // refused: 1A22 has the junction
        {Act::Enter, "up-approach", "", "1A22"},
        {Act::Step, "up-approach", "junction", "1A22"},
        {Act::Step, "junction", "up-departure", "1A22"},
        {Act::Leave, "up-departure", "", "1A22"},
        {Act::Release, kRouteUp, "", "1A22"},
        {Act::Request, kRouteDown, "", "2B15"}, // now it is free
        {Act::Enter, "down-approach", "", "2B15"},
        {Act::Request, kRouteUp, "", "3C07"}, // held while the box is out
        {Act::Step, "down-approach", "junction", "2B15"},
        {Act::Step, "junction", "down-departure", "2B15"},
        {Act::Leave, "down-departure", "", "2B15"},
        {Act::Release, kRouteDown, "", "2B15"},
        {Act::Request, kRouteUp, "", "3C07"},
        {Act::Enter, "up-approach", "", "3C07"},
        {Act::Step, "up-approach", "junction", "3C07"},
    };
}

bool check(const char* what, bool ok) {
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: railway <path-to-signal-box.so>\n";
        return 2;
    }
    const std::string box_path = argv[1];

    loom::Switchboard bus;
    loom::Kernel kernel(bus);

    std::cout << "signal-box -- a diamond crossing, one shift\n";
    std::cout << "contain: " << loom::Kernel::containment_note() << "\n\n";

    // The plant. No authority whatsoever: a track circuit may say nothing to
    // anyone. It can still claim, because claiming is not sending.
    for (const std::string& name : all_sections()) {
        mount_office<TrackSection>(bus, loom::Grant{}, section_office(name), name);
    }

    // The auditor. It may look at everything and say exactly one thing, to
    // exactly one office.
    loom::Grant monitor_grant;
    monitor_grant.allow_observe_any().allow_to_role("SafetyFault", 1, kOfficeSignaller);
    const loom::WeaveId monitor_id =
        mount_office<ConflictMonitor>(bus, monitor_grant, kOfficeMonitor);

    // The signaller. May ask the interlocking office for routes, may talk to
    // track circuits, and may read the two things it needs to read.
    loom::Grant signaller_grant;
    signaller_grant.allow_to_role("PutInService", 1, kOfficeInterlocking)
        .allow_to_role("RouteRequest", 1, kOfficeInterlocking)
        .allow_to_role("RouteRelease", 1, kOfficeInterlocking)
        .allow_to_any("TrainEnters", 1)
        .allow_to_any("TrainLeaves", 1)
        .allow_observe("BoxStatus", 1)
        .allow_observe("Occupancy", 1);
    const loom::WeaveId signaller_id =
        mount_office<Signaller>(bus, signaller_grant, kOfficeSignaller, shift_plan());

    // The interlocking's authority, decided here, at the moment we decide to
    // trust it enough to load it at all. It may answer a request, claim its own
    // status as the office, and read occupancy. That is the entire list.
    loom::Grant box_grant;
    box_grant.allow_to_any("RouteVerdict", 1).allow_observe("Occupancy", 1);

    auto open_box = [&]() -> bool {
        const loom::LoadResult lr =
            kernel.load("signal-box", box_path, kOfficeInterlocking, box_grant);
        if (!lr.ok) {
            std::cerr << "LOAD FAILED: " << lr.error << "\n";
            return false;
        }
        std::cout << "  ..     signal box in service (weave " << lr.id.value << ")\n";
        return true;
    };

    if (!open_box()) {
        return 1;
    }

    // A tap, so the host can see refusals nobody was told about. Loom's own
    // seam: a sender never learns the fate of its send.
    std::vector<std::string> refusals;
    bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Refused) {
            refusals.push_back(std::string(loom::name_of(e.refusal.reason)) + " on " +
                               e.schema_name);
        }
    });

    // Prove the track circuits before the first train. The host is the engineer
    // here; the signaller does not commission the plant it is about to trust.
    for (const std::string& name : all_sections()) {
        bus.send_to_role(section_office(name), loom::Message(loom::to_value(PutInService{0})));
    }
    bus.pump();
    std::cout << "  ..     plant proved: " << all_sections().size() << " track circuits clear\n";

    // The host may look at what it registered; these are its own weaves.
    auto* monitor = static_cast<ConflictMonitor*>(bus.weave(monitor_id));
    auto* signaller = static_cast<Signaller*>(bus.weave(signaller_id));

    bool box_out_refusal_seen = false;
    std::string box_out_refusal;

    std::cout << "\n-- shift --\n";
    for (std::int64_t t = 1; t <= 26; ++t) {
        // Scripted equipment events, announced, so nothing in the log looks
        // like it happened by itself.
        if (t == 10) {
            std::cout << "  --     TAKING THE SIGNAL BOX OUT OF SERVICE\n";
            kernel.unload("signal-box");

            // A labelled control: what a request into an empty office actually
            // does. It refuses observably -- to an observer. The sender is told
            // nothing, which is why the signaller looks at the panel instead.
            const std::size_t before = refusals.size();
            bus.send_to_role(kOfficeInterlocking, loom::Message(loom::to_value(
                                                      RouteRequest{kRouteUp, "CONTROL"})));
            bus.pump();
            box_out_refusal_seen = refusals.size() > before;
            box_out_refusal = box_out_refusal_seen ? refusals.back() : "(nothing)";
            std::cout << "  --     CONTROL: request into the empty office -> " << box_out_refusal
                      << "\n";
        }
        if (t == 12) {
            std::cout << "  --     SIGNAL BOX BACK IN SERVICE\n";
            if (!open_box()) {
                return 1;
            }
        }
        if (t == 20) {
            // A labelled control: an unauthored SafetyFault. The host can send
            // it -- the honest weave API cannot -- and the signaller must not
            // act on it.
            std::cout << "  --     CONTROL: forged (unauthored) SafetyFault\n";
            bus.send_to_role(kOfficeSignaller,
                             loom::Message(loom::to_value(SafetyFault{"forged", t})));
        }
        if (t == 22) {
            // A labelled fault injection, so that "the monitor saw no faults"
            // is a measurement rather than an absence. The host is root and may
            // attribute a claim; no weave can do this.
            std::cout << "  --     INJECT: a BoxStatus claiming both routes set\n";
            const loom::WeaveId box = bus.role_holder(kOfficeInterlocking);
            bus.office_claim_as(box, kOfficeInterlocking,
                                loom::to_value(BoxStatus{true, true, true}));
        }

        bus.send(signaller_id, loom::Message(loom::to_value(Tick{t})));
        bus.send(monitor_id, loom::Message(loom::to_value(Sweep{t})));
        bus.pump();
    }

    // ---------------------------------------------------------------------
    // What the shift is allowed to claim afterwards.
    // ---------------------------------------------------------------------
    const SignallerState s = loom::from_value<SignallerState>(bus.weave(signaller_id)->snapshot());
    const MonitorState m = loom::from_value<MonitorState>(bus.weave(monitor_id)->snapshot());

    std::cout << "\n-- shift report --\n";
    std::cout << "  routes granted        " << s.granted << "\n";
    std::cout << "  routes refused        " << s.refused << "\n";
    std::cout << "  movements held        " << s.held << " (panel dark)\n";
    std::cout << "  routes re-set         " << s.re_set << " (after the box was replaced)\n";
    std::cout << "  requests unanswered   " << s.lost << "\n";
    std::cout << "  monitor sweeps        " << m.sweeps << "\n";
    std::cout << "  monitor faults        " << m.faults << "\n";
    std::cout << "  sweeps with no box    " << monitor->box_absent() << "\n";
    std::cout << "  forged faults ignored " << signaller->ignored_faults() << "\n";
    std::cout << "  orders left in plan   " << signaller->remaining() << "\n";
    std::cout << "  empty-office refusal  " << box_out_refusal << "\n";

    std::cout << "\n-- checks --\n";
    bool ok = true;
    ok &= check("a route was granted when the crossing was clear", s.granted >= 3);
    ok &= check("a conflicting route was refused, not granted", s.refused >= 2);
    ok &= check("movement was held while the panel was dark", s.held >= 2);
    ok &= check("a request into the empty office refused observably", box_out_refusal_seen);
    ok &= check("no request went unanswered", s.lost == 0);
    ok &= check("a replaced box was noticed and its route re-set", s.re_set >= 1);
    ok &= check("an unauthored SafetyFault was ignored", signaller->ignored_faults() >= 1);
    ok &= check("the monitor caught the injected conflict", m.faults == 1);
    ok &= check("the railway went to danger when told to by the monitor", s.at_danger);
    ok &= check("the monitor swept every tick", m.sweeps == 26);
    ok &= check("the monitor saw the box missing while it was out", monitor->box_absent() >= 2);
    ok &= check("the whole shift plan was worked through", signaller->remaining() == 0);

    std::cout << "\n" << (ok ? "SHIFT OK" : "SHIFT FAILED") << "\n";
    return ok ? 0 : 1;
}
