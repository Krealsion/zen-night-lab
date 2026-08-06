// entry-control — a working fire in a warehouse, two ways in, six wearers.
//
// Crews rig, hand over their tallies, are committed, work, are watched by a
// board that can only project, and come out. Then somebody counts.
//
// The whole of the domain is in one sentence: EVERYBODY WHO WENT IN CAME OUT.
// Everything else here — the tallies, the turn-around pressures, the times due
// out, the pressure checks, the emergency crew, the two independent boards —
// exists to make that sentence checkable by somebody standing in a car park at
// three in the morning.
//
// There are two ways for this program to print a completely clean incident and
// be wrong, and they are not the same shape at all:
//
//     THE BOARD WAS CLOSED ON A VOICE     the entry control officer books a
//                                         crew out on the radio report instead
//                                         of on the tally. The board reads
//                                         zero. Two tallies are in the
//                                         officer's hand.
//                                         Caught by: the board's own arithmetic.
//                                         Invisible to: the roll.
//
//     SOMEBODY WENT IN UNBOOKED           the emergency crew is committed at a
//                                         run and one tally is not taken. The
//                                         board is PERFECTLY balanced: one
//                                         entry opened, one closed, one tally
//                                         taken, one returned.
//                                         Caught by: the roll.
//                                         Invisible to: the board's arithmetic.
//
// Same symptom. Different cause. Different remedy. And — the reason the pair is
// worth having — DIFFERENT CHECK, each structurally blind to the other's
// failure. Both are the domain's own; neither was arranged.

#include "fireground.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

using namespace fireground;

// ---------------------------------------------------------------------------
// The log. The minute is always on the left, because that is how you read an
// incident afterwards.
// ---------------------------------------------------------------------------

std::int64_t g_now = 0;

void say(const std::string& who, const std::string& what) {
    char stamp[16];
    std::snprintf(stamp, sizeof stamp, "  %3lld   ", static_cast<long long>(g_now));
    std::cout << stamp;
    std::string w = who;
    w.resize(9, ' ');
    std::cout << w << what << "\n";
}

void note(const std::string& what) { std::cout << "  --    " << what << "\n"; }

std::string bar(std::int64_t b) { return std::to_string(b) + " bar"; }

// ---------------------------------------------------------------------------
// Binding a native weave to an office.
//
// `loom::mount<T>()` and `loom::mount_granted<T>()` do not take a role, and
// `Switchboard::register_weave(weave, grant, role)` is the only binder — but it
// is the raw door, so it does not do the `zen_set_self()` wiring the mount
// helpers do. The four people at this incident who are not wearing a set all
// hold an office, so all four need this. Written from scratch like the six
// experiments before it; the duplication is the finding, so it stays local.
// (F-04, seventh independent consumer.)
// ---------------------------------------------------------------------------

template <class W, class... Args>
loom::WeaveId mount_office(loom::Switchboard& bus, loom::Grant grant, const std::string& office,
                           Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant), office);
    raw->zen_set_self(id);
    return id;
}

/// Somebody's own account of themselves, read back through the ordinary gate.
/// The incident cannot hold a typed pointer into a wearer's shared library, and
/// it should not hold one into a native weave either if it wants the answer to
/// be that participant's rather than its own bookkeeping.
template <class T>
T account_of(const loom::Switchboard& bus, loom::WeaveId id, const char* who) {
    const std::string bytes = bus.snapshot_bytes(id);
    loom::Unverified unverified = loom::parse(bytes);
    loom::Admission admitted = loom::admit(unverified, loom::schema_of<T>());
    if (!admitted) {
        std::cout << "  !!    " << who << "'s account did not pass the gate: "
                  << admitted.first_error().message() << "\n";
        return T{};
    }
    return loom::from_value<T>(admitted.value());
}

/// How often the entry control officer asks for pressures.
constexpr std::int64_t kCheckEvery = 6;

/// Two unanswered checks and the officer stops waiting.
constexpr std::int64_t kMissedBeforeOverdue = 2;

// ---------------------------------------------------------------------------
// THE ENTRY CONTROL POINT.
//
// One officer, one board, one channel, and one job: know, at every moment, who
// is inside and how much air they have left. It cannot see into the building
// and it cannot see the other entry control point — those two facts are what
// make it worth having two of them.
//
// It PROJECTS. That is the interesting part. It has an entry pressure, a
// nominal rate printed on the board, and whatever the last pressure check said,
// and from those it works out when somebody will reach turn-around. A wearer
// who breathes faster than nominal is invisible until the first check and
// obvious after the second, which is why the procedure asks more than once.
// ---------------------------------------------------------------------------

class EntryControl
    : public loom::WeaveBase<EntryControl, PointState,
                             loom::Accept<ReportForEntry, Gauge, OutOfBuilding, AtTheBoard,
                                          Commit, Minute>,
                             loom::Emit<TallyTaken, TallyReturned, PressureCheck, Withdraw,
                                        Committal, Overdue, PointReport>,
                             loom::Claims<Board>> {
public:
    EntryControl(std::string point, std::string office, std::string other_office,
                 bool close_on_the_radio, bool rush_the_booking)
        : office_(std::move(office)), other_office_(std::move(other_office)),
          close_on_the_radio_(close_on_the_radio), rush_the_booking_(rush_the_booking) {
        state_.point = std::move(point);
    }

    void on(const ReportForEntry& r, loom::Mail& mail) {
        rigged_.push_back(Rigged{r.tally, r.crew, r.gauge_bar, mail.sender()});
        say(name(), r.tally + " rigged, " + bar(r.gauge_bar) + ", " + r.crew + " crew");
    }

    void on(const Commit& c, loom::Mail& mail) {
        if (c.point != state_.point) {
            return;
        }
        // NOBODY GOES IN WITHOUT AN EMERGENCY CREW. The officer reads BA main
        // control's claim — the one Sense it may read — and refuses if there
        // is not one. The emergency crew's own committal is the exception, for
        // the obvious reason.
        if (!c.emergency) {
            const loom::SenseReading e = mail.latest_from_office<EmergencyCrew>("ba-main");
            const bool have = e && loom::from_value<EmergencyCrew>(*e.value).available;
            if (!have) {
                ++state_.committals_refused;
                say(name(), "NOT COMMITTING " + c.crew + " crew: no emergency crew available");
                mail.as_role(office_).send_to_role(
                    "command", Committal{c.crew, state_.point, false,
                                         "no emergency crew available"});
                return;
            }
        }

        bool took_one = false;
        for (const Rigged& p : rigged_) {
            if (p.crew != c.crew || on_board(p.tally) != nullptr) {
                continue;
            }
            // THE RUSH. On an emergency committal the crew is already moving
            // and the tallies are taken as they pass the board. This is where
            // one gets missed, and it is not a hypothetical: it is the moment
            // every entry control officer is trained to slow down for.
            if (rush_the_booking_ && c.emergency && took_one) {
                say(name(), "(" + p.tally + " went past -- no tally taken)");
                continue;
            }
            took_one = true;
            take_tally(p, mail);
        }

        mail.as_role(office_).send_to_role("command",
                                           Committal{c.crew, state_.point, true, ""});
        mail.as_role(office_).send_to_role("ba-main",
                                           PointReport{state_.point, c.crew, "committed"});
        claim_board(mail);
    }

    void on(const Gauge& g, loom::Mail& mail) {
        OnTheBoard* e = on_board(g.tally);
        if (e == nullptr) {
            // A PRESSURE READING FROM SOMEBODY WHO IS NOT ON MY BOARD.
            //
            // A real officer hearing this would act on it at once, and so would
            // this one — it is written down, and it is the loudest thing on the
            // page. THIS COUNTER STAYS AT ZERO, and nothing written here is why.
            ++state_.not_on_my_board;
            say(name(), "!! " + g.tally + " is not on my board and has just passed me " +
                            bar(g.bar));
            return;
        }
        if (g.wear != e->wear) {
            // "What is this about?" A reading from a previous wear is not a
            // reading about now. The domain supplies the number for free.
            return;
        }
        ++state_.checks_answered;
        e->last_gauge_bar = g.bar;
        e->last_gauge_minute = now_;
        missed_[g.tally] = 0;
        say(name(), g.tally + " " + bar(g.bar) +
                        " (board expected " + bar(expected_bar(*e)) + ")");
        claim_board(mail);
    }

    void on(const OutOfBuilding& o, loom::Mail& mail) {
        OnTheBoard* e = on_board(o.tally);
        if (e == nullptr) {
            return;
        }
        e->last_gauge_bar = o.bar;
        e->last_gauge_minute = now_;
        say(name(), o.tally + " reports out of the building, " + bar(o.bar));
        if (close_on_the_radio_) {
            // THE FALSE GREEN. The paperwork is updated from a voice on a
            // radio. It is quick, it is what a busy officer does, and it books
            // out a person who is not standing in front of them.
            say(name(), "(booking " + o.tally + " out on the radio report)");
            close_entry(o.tally);
            claim_board(mail);
        }
    }

    void on(const AtTheBoard& a, loom::Mail& mail) {
        OnTheBoard* e = on_board(a.tally);
        if (e == nullptr) {
            // Already booked out. There is no entry to close and the officer,
            // having closed it, does not go looking for the tally again.
            say(name(), "(" + a.tally + " is at the board and already booked out)");
            return;
        }
        close_entry(a.tally);
        hand_back(a.tally, a.wear, mail);
        claim_board(mail);
    }

    void on(const Minute& m, loom::Mail& mail) {
        now_ = m.at;
        for (OnTheBoard& e : state_.still_committed) {
            const std::int64_t since = now_ - e.entry_minute;
            if (since > 0 && since % kCheckEvery == 0) {
                ++state_.checks_asked;
                ++missed_[e.tally];
                mail.as_role(office_).send(wearer_of(e.tally),
                                           PressureCheck{e.tally, e.wear});
            }
        }
        overdue_sweep(mail);
        projection_sweep(mail);
        claim_board(mail);
    }

private:
    struct Rigged {
        std::string tally;
        std::string crew;
        std::int64_t gauge;
        loom::WeaveId who;
    };

    const char* name() const {
        return state_.point == "alpha" ? "ALPHA" : "BRAVO";
    }

    OnTheBoard* on_board(const std::string& tally) {
        for (OnTheBoard& e : state_.still_committed) {
            if (e.tally == tally) {
                return &e;
            }
        }
        return nullptr;
    }

    loom::WeaveId wearer_of(const std::string& tally) const {
        for (const Rigged& p : rigged_) {
            if (p.tally == tally) {
                return p.who;
            }
        }
        return loom::WeaveId{};
    }

    /// What the board thinks this wearer's gauge reads right now, from the last
    /// thing it actually measured and the rate it has measured since entry.
    std::int64_t expected_bar(const OnTheBoard& e) const {
        const std::int64_t elapsed = now_ - e.entry_minute;
        return e.entry_bar - elapsed * kNominalRate;
    }

    void take_tally(const Rigged& p, loom::Mail& mail) {
        ++state_.tallies_taken;
        ++state_.entries_opened;
        state_.tallies_in_hand.push_back(p.tally);
        OnTheBoard e{};
        e.tally = p.tally;
        e.crew = p.crew;
        e.wear = ++wear_;
        e.entry_bar = p.gauge;
        e.entry_minute = now_;
        e.turn_around_bar = turn_around_bar(p.gauge);
        e.due_out_minute = due_out_minute(now_, p.gauge);
        e.last_gauge_bar = p.gauge;
        e.last_gauge_minute = now_;
        state_.still_committed.push_back(e);
        missed_[p.tally] = 0;
        say(name(), "tally " + p.tally + " on the board: in at " + bar(p.gauge) +
                        ", turn round at " + bar(e.turn_around_bar) + ", due out minute " +
                        std::to_string(e.due_out_minute));
        mail.as_role(office_).send(
            p.who, TallyTaken{p.tally, e.wear, e.entry_minute, e.turn_around_bar,
                              e.due_out_minute});
    }

    void close_entry(const std::string& tally) {
        auto it = std::find_if(state_.still_committed.begin(), state_.still_committed.end(),
                               [&](const OnTheBoard& e) { return e.tally == tally; });
        if (it == state_.still_committed.end()) {
            return;
        }
        state_.still_committed.erase(it);
        ++state_.entries_closed;
    }

    void hand_back(const std::string& tally, std::int64_t wear, loom::Mail& mail) {
        auto it = std::find(state_.tallies_in_hand.begin(), state_.tallies_in_hand.end(), tally);
        if (it == state_.tallies_in_hand.end()) {
            return;
        }
        state_.tallies_in_hand.erase(it);
        ++state_.tallies_returned;
        say(name(), tally + " -- tally back in their hand");
        mail.as_role(office_).send(wearer_of(tally), TallyReturned{tally, wear});
    }

    void overdue_sweep(loom::Mail& mail) {
        for (const OnTheBoard& e : state_.still_committed) {
            if (missed_[e.tally] < kMissedBeforeOverdue || declared_.count(e.tally) != 0) {
                continue;
            }
            declared_.insert(e.tally);
            say(name(), "!! " + e.tally + " has missed " +
                            std::to_string(missed_[e.tally]) +
                            " checks -- I cannot say whether that is the radio or the wearer");

            // THE OFFICER LOOKS FOR THEM AT THE OTHER END OF THE BUILDING.
            //
            // Perfectly well meant, and the wrong door: the two boards are the
            // incident's two independent accounts, and an officer who could
            // read the other one would collapse them into one. The refusal
            // names the reason and sends the officer to BA main control, which
            // is where the question actually belongs.
            const loom::SenseReading other =
                mail.latest_from_office<Board>(other_office_);
            if (!other) {
                ++state_.board_reads_refused;
                state_.board_read_refusal = loom::name_of(other.refusal);
                say(name(), "tried to read the " + other_office_ + " board: " +
                                loom::name_of(other.refusal) +
                                " -- asking BA main control instead");
            }

            mail.as_role(office_).send_to_role(
                "command", Overdue{e.crew, state_.point, e.tally, missed_[e.tally]});
            mail.as_role(office_).send_to_role(
                "ba-main", PointReport{state_.point, e.crew, "overdue: " + e.tally});
        }
    }

    /// The board's own arithmetic, once a minute. When the projection says
    /// somebody will be at turn-around before they can get out, the CREW comes
    /// out — not the individual. A crew comes out on the first person's air.
    void projection_sweep(loom::Mail& mail) {
        std::vector<std::string> crews;
        for (const OnTheBoard& e : state_.still_committed) {
            if (withdrawn_.count(e.crew) != 0) {
                continue;
            }
            const std::int64_t used = e.entry_bar - e.last_gauge_bar;
            const std::int64_t elapsed = e.last_gauge_minute - e.entry_minute;
            const std::int64_t at = projected_minute_at(e.last_gauge_bar, e.last_gauge_minute,
                                                        used, elapsed, e.turn_around_bar);
            if (at <= now_ + kTripOut &&
                std::find(crews.begin(), crews.end(), e.crew) == crews.end()) {
                crews.push_back(e.crew);
                say(name(), "the board makes " + e.tally + " turn-around at minute " +
                                std::to_string(at) + " -- " + e.crew + " crew out now");
            }
        }
        for (const std::string& crew : crews) {
            withdrawn_.insert(crew);
            for (const OnTheBoard& e : state_.still_committed) {
                if (e.crew == crew) {
                    mail.as_role(office_).send(wearer_of(e.tally),
                                               Withdraw{e.tally, e.wear, "turn-around"});
                }
            }
        }
    }

    void claim_board(loom::Mail& mail) {
        Board b{};
        b.point = state_.point;
        b.at_minute = now_;
        b.committed = state_.still_committed;
        b.tallies_held = static_cast<std::int64_t>(state_.tallies_in_hand.size());
        b.entries_opened = state_.entries_opened;
        b.entries_closed = state_.entries_closed;
        mail.as_role(office_).claim(b);
    }

    std::string office_;
    std::string other_office_;
    bool close_on_the_radio_ = false;
    bool rush_the_booking_ = false;
    std::int64_t now_ = 0;
    std::int64_t wear_ = 0;
    std::vector<Rigged> rigged_;
    std::map<std::string, std::int64_t> missed_;
    std::set<std::string> declared_;
    std::set<std::string> withdrawn_;
};

// ---------------------------------------------------------------------------
// BA MAIN CONTROL.
//
// Established because there is more than one entry control point, and holding
// the only thing neither officer can hold: both boards at once. It is the ONLY
// participant granted observation of a board, and that is not an arrangement —
// it is why the role exists on a real fireground.
// ---------------------------------------------------------------------------

class BaMain : public loom::WeaveBase<BaMain, MainState,
                                      loom::Accept<PointReport, Minute>,
                                      loom::Emit<EmergencyCrewGone>,
                                      loom::Claims<EmergencyCrew>> {
public:
    void on(const PointReport& r, loom::Mail& mail) {
        state_.log.push_back(r.point + "/" + r.crew + ": " + r.what);
        if (r.what == "standing by") {
            ++state_.emergency_crews_provided;
            emergency_crew_ = r.crew;
            emergency_point_ = r.point;
            say("BA MAIN", r.crew + " crew rigged and standing by at " + r.point +
                               " as the emergency crew");
            mail.as_role("ba-main").claim(EmergencyCrew{true, r.crew, r.point});
            return;
        }
        if (r.what == "committed") {
            ++state_.crews_committed;
            if (r.crew == emergency_crew_) {
                say("BA MAIN", r.crew + " crew committed -- THERE IS NO EMERGENCY CREW");
                mail.as_role("ba-main").claim(EmergencyCrew{false, "", ""});
                mail.as_role("ba-main").send_to_role(
                    "command", EmergencyCrewGone{r.crew, emergency_point_});
            }
        }
    }

    void on(const Minute&, loom::Mail& mail) {
        // Both boards, laid side by side. Nobody else at this incident can do
        // this, and every claim the incident makes about where people are
        // comes from here.
        read(mail, "entry-control.alpha");
        read(mail, "entry-control.bravo");
    }

private:
    void read(loom::Mail& mail, const char* office) {
        const loom::SenseReading r = mail.latest_from_office<Board>(office);
        if (!r) {
            return;
        }
        ++state_.board_reads;
        last_[office] = loom::from_value<Board>(*r.value);
    }

    std::string emergency_crew_;
    std::string emergency_point_;
    std::map<std::string, Board> last_;
};

// ---------------------------------------------------------------------------
// COMMAND.
//
// Decides what is committed and where, and is the only participant that may
// order a tactical withdrawal. It does not read the boards: it is told, by BA
// main control and by the entry control officers, which is how a fireground
// actually works and is why BA main control is load-bearing here rather than
// decorative.
// ---------------------------------------------------------------------------

class Command : public loom::WeaveBase<Command, CommandState,
                                       loom::Accept<Commit, Committal, Overdue,
                                                    EmergencyCrewGone, Minute>,
                                       loom::Emit<Commit, GoIn, Evacuate>> {
public:
    void on(const Commit& c, loom::Mail& mail) {
        ++state_.committals_ordered;
        task_[c.crew] = c.task;
        say("COMMAND", c.crew + " crew to " + c.point + ": " + c.task);
        mail.as_role("command").send_to_role("entry-control." + c.point, c);
        if (c.emergency) {
            // An emergency committal does not wait for the paperwork.
            mail.as_role("command").publish(GoIn{c.crew, c.task, true});
        }
    }

    void on(const Committal& r, loom::Mail& mail) {
        if (!r.committed) {
            ++state_.committals_refused;
            say("COMMAND", r.crew + " crew NOT committed at " + r.point + ": " + r.why);
            return;
        }
        if (sent_.count(r.crew) != 0) {
            return;
        }
        sent_.insert(r.crew);
        mail.as_role("command").publish(GoIn{r.crew, task_[r.crew], false});
    }

    void on(const Overdue& o, loom::Mail& mail) {
        ++state_.crews_overdue;
        say("COMMAND", o.point + " cannot account for " + o.tally + " -- committing the "
                                                                   "emergency crew");
        const loom::SenseReading e = mail.latest_from_office<EmergencyCrew>("ba-main");
        if (!e) {
            return;
        }
        const EmergencyCrew ec = loom::from_value<EmergencyCrew>(*e.value);
        if (!ec.available) {
            say("COMMAND", "there is no emergency crew to commit");
            return;
        }
        ++state_.committals_ordered;
        task_[ec.crew] = "find " + o.tally;
        mail.as_role("command").send_to_role(
            "entry-control." + ec.point, Commit{ec.crew, ec.point, task_[ec.crew], true});
        mail.as_role("command").publish(GoIn{ec.crew, task_[ec.crew], true});
        sent_.insert(ec.crew);
    }

    void on(const EmergencyCrewGone& g, loom::Mail&) {
        say("COMMAND", "no emergency crew (" + g.crew + " are committed) -- nobody else goes in");
    }

    void on(const Minute&, loom::Mail&) {}

private:
    std::map<std::string, std::string> task_;
    std::set<std::string> sent_;
};

} // namespace

// ===========================================================================
// The incident.
// ===========================================================================

namespace {

enum class Scenario { Working, OutOnTheRadio, StraightIn };

bool g_broken = false;

void must(bool ok, const std::string& what) {
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
    if (!ok) {
        g_broken = true;
    }
}

struct Tap {
    std::map<std::string, std::int64_t> delivered;
    std::vector<std::string> refusals;
};

struct Person {
    const char* artifact;
    const char* file_key;
    const char* role;
    const char* tally;
    const char* crew;
    const char* point;
};

constexpr Person kWatch[] = {
    {"aish", "RED-1", "wearer.red-1", "RED-1", "RED", "alpha"},
    {"ndlovu", "RED-2", "wearer.red-2", "RED-2", "RED", "alpha"},
    {"farrow", "BLUE-1", "wearer.blue-1", "BLUE-1", "BLUE", "bravo"},
    {"teague", "BLUE-2", "wearer.blue-2", "BLUE-2", "BLUE", "bravo"},
    {"okonkwo", "GREEN-1", "wearer.green-1", "GREEN-1", "GREEN", "bravo"},
    {"braddock", "GREEN-2", "wearer.green-2", "GREEN-2", "GREEN", "bravo"},
};

int run(Scenario scenario, const std::vector<std::string>& paths) {
    loom::Switchboard bus;
    loom::Kernel kernel(bus);

    std::cout << "\n  entry control -- a working fire, Bevington Road, 03:14\n";
    std::cout << "  containment: " << loom::Kernel::containment_note() << "\n\n";

    // ---- the people who are not wearing a set ------------------------------

    loom::Grant command_grant;
    command_grant.allow_to_role("Commit", 1, "entry-control.alpha")
        .allow_to_role("Commit", 1, "entry-control.bravo")
        .allow_to_any("GoIn", 1)
        .allow_to_any("Evacuate", 1)
        .allow_observe("EmergencyCrew", 1);
    const loom::WeaveId command =
        mount_office<Command>(bus, std::move(command_grant), "command");

    // BA MAIN CONTROL IS THE ONLY PARTICIPANT THAT MAY READ A BOARD.
    loom::Grant main_grant;
    main_grant.allow_to_role("EmergencyCrewGone", 1, "command").allow_observe("Board", 1);
    const loom::WeaveId ba_main = mount_office<BaMain>(bus, std::move(main_grant), "ba-main");

    // AN ENTRY CONTROL OFFICER MAY READ THE EMERGENCY CREW'S STATE AND NOTHING
    // ELSE. Not the other board. The two boards are the incident's two
    // independent accounts and neither officer can see the other's.
    auto point_grant = []() {
        loom::Grant g;
        g.allow_to_any("TallyTaken", 1)
            .allow_to_any("TallyReturned", 1)
            .allow_to_any("PressureCheck", 1)
            .allow_to_any("Withdraw", 1)
            .allow_to_role("Committal", 1, "command")
            .allow_to_role("Overdue", 1, "command")
            .allow_to_role("PointReport", 1, "ba-main")
            .allow_observe("EmergencyCrew", 1);
        return g;
    };
    const loom::WeaveId alpha = mount_office<EntryControl>(
        bus, point_grant(), "entry-control.alpha", "alpha", "entry-control.alpha",
        "entry-control.bravo", scenario == Scenario::OutOnTheRadio, false);
    const loom::WeaveId bravo = mount_office<EntryControl>(
        bus, point_grant(), "entry-control.bravo", "bravo", "entry-control.bravo",
        "entry-control.alpha", false, scenario == Scenario::StraightIn);

    // ---- the watch ---------------------------------------------------------
    //
    // A WEARER SPEAKS TO THEIR OWN ENTRY CONTROL POINT AND TO NOBODY ELSE. Each
    // point has its own channel, and that is the sentence every claim this
    // incident makes about where people are depends on: if a wearer could
    // report to either board, laying the two boards side by side would prove
    // nothing at all.
    std::map<std::string, loom::WeaveId> watch;
    for (std::size_t i = 0; i < std::size(kWatch); ++i) {
        const std::string point = std::string("entry-control.") + kWatch[i].point;
        loom::Grant g;
        g.allow_to_role("ReportForEntry", 1, point)
            .allow_to_role("Gauge", 1, point)
            .allow_to_role("OutOfBuilding", 1, point)
            .allow_to_role("AtTheBoard", 1, point);
        loom::LoadResult r =
            kernel.load(kWatch[i].artifact, paths[i], kWatch[i].role, std::move(g));
        if (!r.ok) {
            std::cout << "  !!    " << kWatch[i].tally << " could not rig: " << r.error << "\n";
            return 2;
        }
        watch[kWatch[i].tally] = r.id;
    }

    // ---- the tap -----------------------------------------------------------
    Tap tap;
    bus.add_observer([&tap](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Refused) {
            tap.refusals.push_back(std::string(loom::name_of(e.refusal.reason)) + " on " +
                                   e.schema_name);
            return;
        }
        if (e.kind == loom::EventKind::Delivered) {
            ++tap.delivered[e.schema_name];
        }
    });

    auto to_role = [&bus](const std::string& role, auto&& payload) {
        bus.send_to_role(role, loom::Message(loom::to_value(payload)));
        bus.pump();
    };

    // ---- the first few minutes --------------------------------------------

    note("two ways in: ALPHA at the front, BRAVO at the loading bay");
    note("BA main control established -- there is more than one entry control point");

    for (const Person& p : kWatch) {
        to_role(p.role, ReportTo{std::string("entry-control.") + p.point, p.crew});
    }
    // The emergency crew is rigged and standing by. Nobody may be committed
    // without them.
    to_role("ba-main", PointReport{"bravo", "GREEN", "standing by"});

    to_role("command", Commit{"RED", "alpha", "search the first floor", false});
    to_role("command", Commit{"BLUE", "bravo", "a jet into the rear store", false});

    // ---- the incident ------------------------------------------------------

    bool edge_a_fired = false;
    bool edge_b_fired = false;
    bool unofficed_fired = false;
    bool second_committal_tried = false;
    std::int64_t zero_at = -1;

    for (std::int64_t minute = 1; minute <= 60; ++minute) {
        g_now = minute;

        if (minute == 10 && !edge_a_fired) {
            // ---- EDGE A -- THE TOPOLOGY, IN ITS STRONGEST FORM --------------
            //
            // Teague, behind the racking and unable to raise Bravo, comes up on
            // the other point's channel. Speaking AS THEMSELVES: wearer.blue-2
            // is an office Teague genuinely holds, so authorship succeeds.
            //
            // Every other check in this application would accept the act. The
            // shape is one Alpha accepts, the sender really is who they say
            // they are, the content is exactly what a wearer in trouble
            // transmits, and ALPHA WOULD ACT ON IT — its handler writes down
            // any reading from anybody not on its board and says so as loudly
            // as it says anything. THERE IS NO DOMAIN RULE BEHIND THIS ONE.
            // The grant is the only thing keeping the two boards independent.
            //
            // Teague has no verb for it — a wearer's code addresses the point
            // it reported to — so the incident forges it with the verified host
            // door, which stamps the sender from its own root authority and
            // then applies every ordinary delivery law.
            edge_a_fired = true;
            note("CONTROL: Teague, unable to raise Bravo, comes up on Alpha's channel");
            bus.office_send_to_role_as(watch["BLUE-2"], "wearer.blue-2", "entry-control.alpha",
                                       loom::Message(loom::to_value(Gauge{"BLUE-2", 1, 150})));
            bus.pump();
        }

        if (minute == 15 && !edge_b_fired) {
            // ---- EDGE B -- AUTHORSHIP --------------------------------------
            //
            // Bravo has a wearer it cannot account for and the roof is starting
            // to go. Everything about the act is right except one fact: the
            // tactical withdrawal is the incident commander's and nobody
            // else's. Bravo says it in command's name, to the crew that is
            // actually in the most danger.
            //
            // Authorship is decided BEFORE the grant is consulted, so Bravo's
            // own grant — which has no Evacuate rule and would also have
            // refused this — is never reached.
            edge_b_fired = true;
            note("CONTROL: Bravo orders the withdrawal, in command's name");
            bus.office_send_to_role_as(bravo, "command", "wearer.blue-1",
                                       loom::Message(loom::to_value(
                                           Evacuate{"the roof is going"})));
            bus.pump();
        }

        if (minute == 16 && !unofficed_fired) {
            // The other half, and a different owner: this one IS delivered, and
            // the WEARER discriminates. A shout of "get out" that nobody
            // authored as command is a voice on the fireground.
            unofficed_fired = true;
            note("CONTROL: somebody shouts 'get out' on the fireground, as themselves");
            bus.send(watch["BLUE-1"],
                     loom::Message(loom::to_value(Evacuate{"a voice in the smoke"})));
            bus.pump();
        }

        if (minute == 22 && !second_committal_tried) {
            // The emergency crew is inside, so there is no emergency crew. The
            // rule is absolute, and it is the reason an entry control officer
            // reads BA main control's claim before it takes a single tally.
            second_committal_tried = true;
            note("COMMAND wants another crew at Alpha to assist at the rear");
            to_role("command", Commit{"RED", "alpha", "assist at the rear", false});
        }

        to_role("command", Minute{minute});
        to_role("ba-main", Minute{minute});
        to_role("entry-control.alpha", Minute{minute});
        to_role("entry-control.bravo", Minute{minute});
        for (const Person& p : kWatch) {
            bus.send(watch[p.tally], loom::Message(loom::to_value(Minute{minute})));
        }
        bus.pump();

        // The incident is over when both boards read zero. THAT IS THE
        // SENTENCE THIS APPLICATION EXISTS TO ARGUE WITH.
        const loom::SenseReading a = bus.observe_office("entry-control.alpha", "Board", 1);
        const loom::SenseReading b = bus.observe_office("entry-control.bravo", "Board", 1);
        if (a && b && minute > 12) {
            const Board ab = loom::from_value<Board>(*a.value);
            const Board bb = loom::from_value<Board>(*b.value);
            if (ab.committed.empty() && bb.committed.empty() && zero_at < 0) {
                zero_at = minute;
                note("both boards read zero committed");
            }
        }
        // An officer in charge does not close an incident the second a board
        // reads zero. There is a walk round the building and a roll call, and
        // that interval is exactly when the tallies are supposed to come back.
        if (zero_at > 0 && minute >= zero_at + 8) {
            break;
        }
    }

    // The tactical withdrawal, said properly, by the one participant that may.
    note("COMMAND: incident closed, all crews withdrawn");
    to_role("command", Minute{g_now});

    // =======================================================================
    // The debrief.
    // =======================================================================

    std::cout << "\n  the two boards, at the close\n\n";
    for (const char* office : {"entry-control.alpha", "entry-control.bravo"}) {
        const loom::SenseReading r = bus.observe_office(office, "Board", 1);
        if (!r) {
            std::cout << "    " << office << ": " << loom::name_of(r.refusal) << "\n";
            continue;
        }
        const Board b = loom::from_value<Board>(*r.value);
        std::cout << "    " << b.point << ": " << b.entries_opened << " opened, "
                  << b.entries_closed << " closed, " << b.committed.size() << " still in, "
                  << b.tallies_held << " tallies in hand\n";
    }

    std::cout << "\n  the roll -- everybody who rigged, from their own account\n\n";
    std::vector<WearerState> roll;
    for (const Person& p : kWatch) {
        WearerState w = account_of<WearerState>(bus, watch[p.tally], p.tally);
        roll.push_back(w);
        for (const WearRecord& r : w.wears) {
            char line[220];
            std::snprintf(line, sizeof line,
                          "    %-9s %-8s %-5s  in %3lld  out %3lld   %3lld -> %3lld bar   "
                          "%s%s%s",
                          w.name.c_str(), w.tally.c_str(), w.crew.c_str(),
                          static_cast<long long>(r.entered_minute),
                          static_cast<long long>(r.out_minute),
                          static_cast<long long>(r.gauge_at_entry),
                          static_cast<long long>(r.gauge_at_exit),
                          r.booked_in ? "booked in" : "** NOT BOOKED IN **",
                          r.tally_back ? ", tally back" : ", TALLY NOT BACK",
                          r.on_the_whistle ? ", ON THE WHISTLE" : "");
            std::cout << line << "\n";
        }
    }

    std::cout << "\n  what the board planned on, and what people actually breathed\n\n";
    for (const WearerState& w : roll) {
        for (const WearRecord& r : w.wears) {
            if (!r.went_in || r.out_minute < 0) {
                continue;
            }
            const std::int64_t mins = r.out_minute - r.entered_minute;
            const std::int64_t used = r.gauge_at_entry - r.gauge_at_exit;
            char line[200];
            std::snprintf(line, sizeof line,
                          "    %-9s %2lld min, %3lld bar used, %2lld bar/min "
                          "(the board planned on %lld)",
                          w.name.c_str(), static_cast<long long>(mins),
                          static_cast<long long>(used),
                          static_cast<long long>(mins > 0 ? used / mins : 0),
                          static_cast<long long>(kNominalRate));
            std::cout << line << "\n";
        }
    }

    // ---- the three accounts ------------------------------------------------

    std::cout << "\n";
    const PointState ap = account_of<PointState>(bus, alpha, "Alpha");
    const PointState bp = account_of<PointState>(bus, bravo, "Bravo");
    const MainState mstate = account_of<MainState>(bus, ba_main, "BA main control");
    const CommandState cstate = account_of<CommandState>(bus, command, "command");

    // ACCOUNT ONE -- each board's own arithmetic. Catches a board closed on a
    // voice. Structurally cannot see somebody it was never told about.
    bool board_arithmetic_ok = true;
    for (const PointState& p : {ap, bp}) {
        const std::int64_t held = static_cast<std::int64_t>(p.tallies_in_hand.size());
        const std::int64_t open = static_cast<std::int64_t>(p.still_committed.size());
        const bool a1 = p.entries_opened == p.entries_closed + open;
        const bool a2 = p.tallies_taken == p.tallies_returned + held;
        const bool a3 = open == held;
        if (!(a1 && a2 && a3)) {
            board_arithmetic_ok = false;
            std::cout << "  WRONG " << p.point << ": " << p.entries_opened << " opened, "
                      << p.entries_closed << " closed, " << open << " still in, " << held
                      << " tallies in hand -- the paperwork and the tallies disagree\n";
            for (const std::string& t : p.tallies_in_hand) {
                std::cout << "        tally " << t << " is still on this board\n";
            }
        }
    }

    // ACCOUNT TWO -- the roll. Catches somebody who went in unbooked.
    // Structurally cannot see a board closed on a voice: those wearers were
    // booked in perfectly.
    bool roll_ok = true;
    for (const WearerState& w : roll) {
        for (const WearRecord& r : w.wears) {
            if (r.went_in && !r.booked_in) {
                roll_ok = false;
                std::cout << "  WRONG " << w.name << " (" << w.tally
                          << ") entered the building and is on nobody's board\n";
            }
        }
    }

    // ACCOUNT THREE -- the air. What the debrief is actually for.
    bool air_ok = true;
    for (const WearerState& w : roll) {
        for (const WearRecord& r : w.wears) {
            if (r.went_in && (r.on_the_whistle || r.gauge_at_exit < kSafetyMargin)) {
                air_ok = false;
                std::cout << "  WRONG " << w.name << " came out on " << r.gauge_at_exit
                          << " bar, below the " << kSafetyMargin << " bar margin\n";
            }
        }
    }

    std::cout << "\n  three accounts of the same incident\n\n";
    std::cout << "    the boards    " << ap.tallies_taken + bp.tallies_taken
              << " tallies taken, " << ap.tallies_returned + bp.tallies_returned
              << " returned, " << ap.tallies_in_hand.size() + bp.tallies_in_hand.size()
              << " in hand\n";
    std::int64_t went_in = 0;
    for (const WearerState& w : roll) {
        for (const WearRecord& r : w.wears) {
            went_in += r.went_in ? 1 : 0;
        }
    }
    std::cout << "    the watch     " << roll.size() << " rigged, " << went_in
              << " went into the building, by their own account\n";
    std::cout << "    the tap       " << tap.delivered["Gauge"] << " Gauge deliveries, "
              << tap.delivered["PressureCheck"] << " checks, "
              << tap.delivered["TallyTaken"] << " tallies taken\n";
    std::cout << "    BA main       " << mstate.board_reads << " board readings, "
              << mstate.crews_committed << " committals seen\n";

    std::cout << "\n  bus refusals seen  " << tap.refusals.size() << "  [";
    for (std::size_t i = 0; i < tap.refusals.size(); ++i) {
        std::cout << (i ? ", " : "") << tap.refusals[i];
    }
    std::cout << "]\n\n";

    // ---- what each scenario asserts ---------------------------------------

    std::int64_t checks_unanswered = 0;
    std::int64_t checks_answered = 0;
    std::int64_t not_from_command = 0;
    for (const WearerState& w : roll) {
        checks_unanswered += w.checks_unanswered;
        checks_answered += w.checks_answered;
        not_from_command += w.not_from_command;
    }

    // THE ARITHMETIC HAS TO CLOSE, across three accounts that share no counter:
    // what the two boards say they asked for, what the six wearers say they did
    // with it, and what the incident's own observer actually saw cross the bus.
    const std::int64_t asked = ap.checks_asked + bp.checks_asked;
    std::cout << "    the checks    " << asked << " asked by the boards = "
              << checks_answered << " answered + " << checks_unanswered
              << " unanswered, by the wearers' own count\n\n";
    must(asked == checks_answered + checks_unanswered,
         "every pressure check the boards asked for is accounted for by a wearer");
    must(tap.delivered["PressureCheck"] == asked,
         "and the tap saw exactly that many checks cross the bus");
    must(tap.delivered["Gauge"] == checks_answered &&
             ap.checks_answered + bp.checks_answered == checks_answered,
         "and every answer reached a board -- boards, wearers and tap agree");

    // The three Zen edges, in every scenario, because a control that only fires
    // on a good day is not a control.
    must(ap.not_on_my_board == 0,
         "a wearer on Bravo's channel could not reach Alpha (Alpha's "
         "not-on-my-board counter is 0)");
    must(std::count(tap.refusals.begin(), tap.refusals.end(),
                    "CapabilityDenied on Gauge") == 1,
         "and the bus said so: CapabilityDenied on Gauge");
    must(std::count(tap.refusals.begin(), tap.refusals.end(),
                    "RoleAuthorshipDenied on Evacuate") == 1,
         "an entry control officer cannot order the withdrawal in command's name");
    must(cstate.evacuations_ordered == 0 && not_from_command >= 1,
         "and an unauthored 'get out' was delivered and ignored by the wearer");
    must(bp.board_reads_refused >= 1 && bp.board_read_refusal == "NotAuthorized",
         "an entry control officer was refused the other point's board: " +
             bp.board_read_refusal);
    must(mstate.board_reads > 0,
         "and BA main control -- the only participant granted it -- read both boards " +
             std::to_string(mstate.board_reads) + " times");

    switch (scenario) {
        case Scenario::Working:
            must(board_arithmetic_ok, "every entry closed against a tally that came back");
            must(roll_ok, "everybody who went in was on a board");
            must(air_ok, "everybody came out above the safety margin");
            must(checks_unanswered >= 2, "a wearer missed two pressure checks");
            must(cstate.crews_overdue >= 1, "and the emergency crew was committed for them");
            must(cstate.committals_refused == 1,
                 "and with the emergency crew inside, nobody else could be committed");
            break;
        case Scenario::OutOnTheRadio:
            must(!board_arithmetic_ok,
                 "CONTROL: the board was closed on a voice, and the arithmetic caught it");
            must(roll_ok, "and the roll stayed silent -- they were both booked in");
            must(air_ok, "and the air account stayed silent -- they came out with air");
            break;
        case Scenario::StraightIn:
            must(!roll_ok,
                 "CONTROL: somebody went in unbooked, and the roll caught it");
            must(board_arithmetic_ok,
                 "and the board's own arithmetic stayed silent -- it balances perfectly");
            must(air_ok, "and the air account stayed silent");
            break;
    }

    std::cout << "\n  " << (g_broken ? "THE DEBRIEF FOUND THE WITNESS WRONG" : "DEBRIEF OK")
              << "\n\n";
    return g_broken ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
    Scenario scenario = Scenario::Working;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--out-on-the-radio") {
            scenario = Scenario::OutOnTheRadio;
        } else if (a == "--straight-in") {
            scenario = Scenario::StraightIn;
        } else {
            paths.push_back(a);
        }
    }
    if (paths.size() != std::size(kWatch)) {
        std::cout << "usage: incident [--out-on-the-radio|--straight-in] "
                     "<six wearer .so paths, in watch order>\n";
        return 2;
    }
    return run(scenario, paths);
}
