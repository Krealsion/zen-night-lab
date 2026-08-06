// Teague — BLUE-2, Blue crew.
//
// THE PARTICIPANT THAT STOPS ANSWERING.
//
// Teague goes in behind the steel racking to check for casualties, and behind
// steel racking a fireground radio does not get out. From that moment Teague
// hears nothing and says nothing — and, being perfectly all right and perfectly
// well trained, comes out on their own turn-around pressure exactly as briefed.
//
// The entry control officer cannot tell any of that. What the officer has is a
// crew member who did not answer, twice, and three possibilities that look
// identical from a board:
//
//     the radio failed
//     the wearer is working and cannot answer
//     the wearer is in difficulty
//
// The procedure does not guess. It escalates on a clock and commits the
// emergency crew, and it is right to, because the cost of being wrong the other
// way is somebody dying in a building nobody is looking in.
//
// The honest ending of this incident is that it was the first of the three. The
// escalation was still correct. That is what a working fireground looks like.

#include "fireground.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>

namespace {

using namespace fireground;

/// Minutes after entry at which Teague goes in behind the racking.
constexpr std::int64_t kIntoTheVoid = 4;

class Teague
    : public loom::WeaveBase<
          Teague, WearerState,
          loom::Accept<ReportTo, TallyTaken, TallyReturned, PressureCheck, Withdraw, Evacuate,
                       Minute, GoIn>,
          loom::Emit<ReportForEntry, Gauge, OutOfBuilding, AtTheBoard>> {
public:
    Teague() {
        state_.name = "Teague";
        state_.tally = "BLUE-2";
        state_.rate = 9;
        state_.gauge = 300;
    }

    void on(const ReportTo& r, loom::Mail& mail) {
        point_ = r.point;
        state_.crew = r.crew;
        WearRecord w{};
        w.wear = static_cast<std::int64_t>(state_.wears.size()) + 1;
        w.point = r.point;
        w.gauge_at_entry = state_.gauge;
        // Every wearer knows their own turn-around pressure: half your usable
        // air, plus the margin. The board writes it down and reads it back to
        // you, and that is a confirmation rather than the source. A firefighter
        // whose tally was never taken still comes out on their figure.
        turn_around_ = turn_around_bar(state_.gauge);
        state_.wears.push_back(w);
        mail.as_role(role()).send_to_role(
            point_, ReportForEntry{state_.tally, state_.crew, state_.gauge});
    }

    void on(const TallyTaken& t, loom::Mail&) {
        if (t.tally != state_.tally || state_.wears.empty()) {
            return;
        }
        state_.wears.back().booked_in = true;
        state_.wears.back().wear = t.wear;
        turn_around_ = t.turn_around_bar;
    }

    void on(const GoIn& g, loom::Mail& mail) {
        if (g.crew != state_.crew || inside_ || state_.wears.empty()) {
            return;
        }
        if (!mail.authored_from_role("command")) {
            ++state_.not_from_command;
            return;
        }
        inside_ = true;
        in_for_ = 0;
        state_.wears.back().went_in = true;
        state_.wears.back().entered_minute = now_;
    }

    void on(const Minute& m, loom::Mail& mail) {
        now_ = m.at;
        // The walk from the door to the entry control point. A minute, and it
        // is the minute this whole application is about.
        if (to_board_ > 0 && --to_board_ == 0) {
            const std::int64_t w = state_.wears.empty() ? 0 : state_.wears.back().wear;
            mail.as_role(role()).send_to_role(point_, AtTheBoard{state_.tally, w});
        }
        if (!inside_) {
            return;
        }
        ++in_for_;
        state_.gauge -= state_.rate;
        if (state_.gauge <= kSafetyMargin && !state_.wears.empty()) {
            state_.wears.back().on_the_whistle = true;
        }
        // Behind the racking. The trained answer to losing the radio is not to
        // stop working; it is to keep to the plan you were given and come out
        // on your figure. Teague does exactly that.
        if (!coming_out_ && turn_around_ > 0 && state_.gauge <= turn_around_) {
            start_out();
            return;
        }
        if (coming_out_ && --trip_ <= 0) {
            step_outside(mail);
        }
    }

    void on(const PressureCheck& p, loom::Mail& mail) {
        if (p.tally != state_.tally) {
            return; // somebody else's call sign
        }
        if (!inside_) {
            ++state_.checks_unanswered;
            return;
        }
        if (!radio_gets_out()) {
            // Delivered, received, understood, and not answerable. The message
            // arrived; the reply cannot leave.
            ++state_.checks_unanswered;
            return;
        }
        ++state_.checks_answered;
        const std::int64_t wear = state_.wears.empty() ? 0 : state_.wears.back().wear;
        mail.as_role(role()).send_to_role(point_, Gauge{state_.tally, wear, state_.gauge});
    }

    void on(const Withdraw& w, loom::Mail&) {
        if (w.tally != state_.tally || !inside_ || coming_out_) {
            return;
        }
        if (!radio_gets_out()) {
            return; // never heard it
        }
        start_out();
    }

    void on(const Evacuate&, loom::Mail& mail) {
        if (!mail.authored_from_role("command")) {
            ++state_.not_from_command;
            return;
        }
        if (!radio_gets_out()) {
            return;
        }
        if (inside_ && !coming_out_) {
            start_out();
        }
    }

    void on(const TallyReturned& t, loom::Mail&) {
        if (t.tally != state_.tally || state_.wears.empty()) {
            return;
        }
        state_.wears.back().tally_back = true;
    }

private:
    static const char* role() { return "wearer.blue-2"; }

    /// Steel racking, and a hand-held set on a fireground channel. Once Teague
    /// starts back out along the passage it works again.
    bool radio_gets_out() const { return in_for_ < kIntoTheVoid || coming_out_; }

    void start_out() {
        coming_out_ = true;
        trip_ = kTripOut;
    }

    void step_outside(loom::Mail& mail) {
        inside_ = false;
        coming_out_ = false;
        to_board_ = 1;
        if (!state_.wears.empty()) {
            state_.wears.back().out_minute = now_;
            state_.wears.back().gauge_at_exit = state_.gauge;
        }
        const std::int64_t wear = state_.wears.empty() ? 0 : state_.wears.back().wear;
        mail.as_role(role()).send_to_role(point_,
                                          OutOfBuilding{state_.tally, wear, state_.gauge});
    }

    std::string point_{};
    std::int64_t now_ = 0;
    std::int64_t in_for_ = 0;
    std::int64_t turn_around_ = 0;
    std::int64_t trip_ = 0;
    std::int64_t to_board_ = 0;
    bool inside_ = false;
    bool coming_out_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(Teague)
