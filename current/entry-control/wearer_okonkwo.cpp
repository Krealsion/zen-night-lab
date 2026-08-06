// Okonkwo — GREEN-1, emergency crew leader.
//
// The emergency crew's whole job is to be standing at the entry control point,
// rigged, doing nothing at all. Nobody may be committed unless they are there,
// and the moment they are committed they stop being the emergency crew — which
// is why the next thing that happens after they go in is that nothing else can.
//
// Okonkwo goes on the shout, not on the paperwork. That is the correct
// behaviour and it is also exactly why an emergency committal is the moment
// somebody gets into a building without being written down.

#include "fireground.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>

namespace {

using namespace fireground;

class Okonkwo
    : public loom::WeaveBase<
          Okonkwo, WearerState,
          loom::Accept<ReportTo, TallyTaken, TallyReturned, PressureCheck, Withdraw, Evacuate,
                       Minute, GoIn>,
          loom::Emit<ReportForEntry, Gauge, OutOfBuilding, AtTheBoard>> {
public:
    Okonkwo() {
        state_.name = "Okonkwo";
        state_.tally = "GREEN-1";
        state_.rate = 8;
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
        state_.gauge -= state_.rate;
        if (state_.gauge <= kSafetyMargin && !state_.wears.empty()) {
            state_.wears.back().on_the_whistle = true;
        }
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
            // Asked, heard, and there is nothing to answer with: this set is
            // off. Counted so that "checks asked" closes against the boards.
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
        start_out();
    }

    void on(const Evacuate&, loom::Mail& mail) {
        if (!mail.authored_from_role("command")) {
            ++state_.not_from_command;
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
    static const char* role() { return "wearer.green-1"; }

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
    std::int64_t turn_around_ = 0;
    std::int64_t trip_ = 0;
    std::int64_t to_board_ = 0;
    bool inside_ = false;
    bool coming_out_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(Okonkwo)
