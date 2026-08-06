// A practice night at St Cuthbert's, Wenbourne.
//
// Six ringers, a tower captain who calls, somebody in the corner pricking the
// touch, and a method that arrives as a separately-built artifact and is put
// back on the shelf afterwards.
//
//   ./practice ./plain-bob-doubles.so ./plain-bob-minor.so
//
// It rings a 120 of Plain Bob Doubles -- every one of the hundred and twenty
// possible orders of five bells, once each -- then stands, gets Plain Bob Minor
// up, and rings a plain course of that. Afterwards it checks what the evening is
// allowed to claim, and exits non-zero if any of it is untrue.
//
// THE THING THIS PROGRAM CANNOT DO is work out what a row ought to be. It
// includes `tower.hpp` and not `method.hpp`, so there is no place notation
// anywhere in this translation unit and no code here that could reconstruct the
// rows from the method and the composition. Whether the touch was true is
// therefore decided by one thing only: what the pricker heard.

#include "tower.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace tower;

namespace {

// ===========================================================================
// Wiring
// ===========================================================================

// `loom::mount<T>()` and `mount_granted<T>()` take no role, and
// `register_weave(weave, grant, role)` is the raw door -- so it does not do the
// zen_set_self() wiring mount() does. Every office-holding native weave in this
// application therefore needs this, and forgetting the one line is silent.
// (`current/FRICTION.md` F-04; three other experiments wrote this same helper.)
template <class W, class... Args>
loom::WeaveId mount_office(loom::Switchboard& bus, loom::Grant grant, const std::string& office,
                           Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant), office);
    raw->zen_set_self(id);
    return id;
}

std::string row_text(const std::vector<std::int64_t>& places) {
    std::string out;
    for (std::size_t p = 1; p < places.size(); ++p) {
        out += static_cast<char>('0' + places[p]);
    }
    return out;
}

// ===========================================================================
// A ringer
// ===========================================================================

// One person, one rope, one line. A ringer knows the positions its own bell
// occupies through a lead and what a call does to it at the lead end, and that
// is the whole of what it knows: it is never told where anybody else is, it
// never sees a row, and it could not describe one if asked.
//
// It hears exactly two things during a touch -- the band pulling off, and the
// conductor -- and it answers to neither unless the conductor's voice is the
// conductor's.
class Ringer : public loom::WeaveBase<Ringer, RingerState,
                                      loom::Accept<RingUp, RingDown, LearnTheMethod, YourLine,
                                                   Call, Pull>,
                                      loom::Emit<WhatIsMyLine, Struck>, loom::Claims<BellUp>> {
public:
    Ringer(std::int64_t bell, std::string who, std::int64_t mishears_call = 0)
        : who_(std::move(who)), mishears_(mishears_call) {
        state_.bell = bell;
        state_.place_bell = bell;
    }

    const std::string& who() const { return who_; }

    void on(const RingUp&, loom::Mail& mail) {
        state_.up = true;
        say_so(mail);
    }

    void on(const RingDown&, loom::Mail& mail) {
        state_.up = false;
        say_so(mail);
    }

    /// Get the method up. A ringer learns every place bell, because over a
    /// course its own bell rings each of them in turn -- that is what a line IS.
    /// The asks all happen here, before a note is struck; during the ringing
    /// this weave sends the method nothing at all.
    void on(const LearnTheMethod& m, loom::Mail& mail) {
        lines_.clear();
        state_.learned = false;
        state_.covering = false;
        state_.place_bell = state_.bell;
        state_.row_in_lead = 1;
        at_ = state_.bell;
        pending_.clear();
        calls_heard_ = 0;
        band_ = m.bells_in_tower;
        for (std::int64_t p = 1; p <= band_; ++p) {
            // The correlation is the question: an answer that does not carry it
            // back is not the answer to this ask, whatever it says inside.
            mail.send_to_role(kOfficeMethod, WhatIsMyLine{p}, static_cast<std::uint64_t>(p));
        }
    }

    void on(const YourLine& line, loom::Mail& mail) {
        // You learn a method out of the book. A voice telling you your line is
        // not the book, however confident it sounds, and Loom's own word for
        // "this answers the question you asked" is the only thing consulted.
        if (!mail.answers_ask() || mail.correlation() != static_cast<std::uint64_t>(line.place_bell)) {
            ++state_.not_from_the_method;
            return;
        }
        if (!line.covering && !reachable(line)) {
            // A bell moves at most one place a blow. A line that asks for more
            // is not a line, and no ringer would take it.
            ++state_.unreachable;
            return;
        }
        lines_[line.place_bell] = line;
        lead_ = line.lead;
        method_ = line.method;
        if (static_cast<std::int64_t>(lines_.size()) == band_) {
            state_.learned = true;
            state_.covering = lines_[state_.bell].covering;
        }
    }

    void on(const Call& c, loom::Mail& mail) {
        // Somebody on the stairs saying "Bob" is not a call. The band must be
        // able to tell without looking up, which is what office authorship is.
        if (!mail.authored_from_role(kOfficeConductor)) {
            ++state_.not_the_conductor;
            return;
        }
        ++calls_heard_;
        if (mishears_ != 0 && calls_heard_ == mishears_) {
            return; // LABELLED: this ringer does not hear this one
        }
        if (c.what == kBob || c.what == kSingle) {
            pending_ = c.what;
        }
    }

    void on(const Pull& p, loom::Mail& mail) {
        if (!state_.up || !state_.learned) {
            ++state_.refused;
            return;
        }
        const YourLine& line = lines_.at(state_.place_bell);

        std::int64_t place = 0;
        bool lead_end = false;
        if (line.covering) {
            place = band_; // the tenor behind: last, every row, for ever
        } else if (state_.row_in_lead < lead_) {
            place = line.path[static_cast<std::size_t>(state_.row_in_lead) - 1];
        } else {
            lead_end = true;
            place = pending_ == kBob      ? line.bob_end
                    : pending_ == kSingle ? line.single_end
                                          : line.plain_end;
        }

        if (place < 1 || (place > at_ + 1) || (place < at_ - 1)) {
            ++state_.unreachable;
            return;
        }

        at_ = place;
        ++state_.blows;
        // A BELL IS HEARD, not addressed. Published, as the rope's own office,
        // so that what reaches the pricker's paper is a bell and not a rumour.
        const loom::OfficePublication heard =
            mail.as_role(rope(state_.bell)).publish(Struck{state_.bell, place, p.row});
        if (heard.authored) {
            ears_ += heard.recipients;
        } else {
            ++unauthored_;
        }

        if (line.covering) {
            return;
        }
        if (lead_end) {
            state_.place_bell = place; // the place bell you become IS where you finished
            state_.row_in_lead = 1;
            pending_.clear();
        } else {
            ++state_.row_in_lead;
        }
    }

    std::int64_t ears() const { return ears_; }
    std::int64_t unauthored() const { return unauthored_; }
    const std::string& method() const { return method_; }

private:
    void say_so(loom::Mail& mail) {
        mail.as_role(rope(state_.bell)).claim(BellUp{state_.bell, state_.up});
    }

    /// Could this bell actually ring this line? Every step, including the step
    /// out of the lead head and the three possible steps into the next one.
    bool reachable(const YourLine& line) const {
        std::int64_t prev = line.place_bell;
        for (std::int64_t x : line.path) {
            if (x < 1 || x > line.bells || x > prev + 1 || x < prev - 1) {
                return false;
            }
            prev = x;
        }
        for (std::int64_t end : {line.plain_end, line.bob_end, line.single_end}) {
            if (end < 1 || end > line.bells || end > prev + 1 || end < prev - 1) {
                return false;
            }
        }
        return true;
    }

    std::string who_;
    std::int64_t mishears_ = 0;
    std::int64_t calls_heard_ = 0;
    std::map<std::int64_t, YourLine> lines_;
    std::int64_t band_ = 0;
    std::int64_t lead_ = 0;
    std::int64_t at_ = 0; ///< the position struck last blow; only this bell's own
    std::string pending_;
    std::string method_;
    std::int64_t ears_ = 0;
    std::int64_t unauthored_ = 0;
};

// ===========================================================================
// The conductor
// ===========================================================================

// The tower captain, standing out and calling. Holds the composition, hears
// every bell, and does three things with what it hears: calls a bob one whole
// pull before the lead end, says "That's all" when it hears rounds, and stops
// the band when what it heard was not a row.
//
// It never proves anything. A conductor knowing its own composition is true is
// not evidence that the band rang it.
class Conductor : public loom::WeaveBase<Conductor, ConductorState,
                                         loom::Accept<TheComposition, LearnTheMethod, YourLine,
                                                      LookRound, Struck>,
                                         loom::Emit<Call, WhatIsMyLine>> {
public:
    void on(const TheComposition& c, loom::Mail&) {
        name_ = c.name;
        calls_ = c.calls;
        band_ = c.band;
        row_.assign(static_cast<std::size_t>(band_) + 1, 0);
        filled_ = 0;
        at_row_ = 0;
        called_.clear();
        not_ready_.clear();
        state_.rows_heard = 0;
        state_.calls_made = 0;
        state_.going = false;
        state_.came_round = false;
        state_.stood = false;
        stood_because_.clear();
    }

    /// A conductor learns the method like everybody else. All it needs from it
    /// is how long a lead is, because that is what says when to call.
    void on(const LearnTheMethod&, loom::Mail& mail) {
        lead_ = 0;
        mail.send_to_role(kOfficeMethod, WhatIsMyLine{1}, 1);
    }

    void on(const YourLine& line, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        lead_ = line.lead;
    }

    /// "Are we all here?" Reading the ropes, not asking the band: whether a bell
    /// is up is a standing fact about the bell.
    void on(const LookRound&, loom::Mail& mail) {
        not_ready_.clear();
        for (std::int64_t b = 1; b <= band_; ++b) {
            const loom::SenseReading r = mail.latest_from_office<BellUp>(rope(b));
            if (!r) {
                not_ready_.push_back(std::to_string(b) + " (" +
                                     std::string(loom::name_of(r.refusal)) + ")");
                continue;
            }
            const BellUp u = loom::from_value<BellUp>(*r.value);
            if (!u.up) {
                not_ready_.push_back(std::to_string(b) + " (down)");
            }
        }
        if (lead_ == 0) {
            not_ready_.emplace_back("the method (not learnt)");
        }
        state_.going = not_ready_.empty();
    }

    void on(const Struck& s, loom::Mail& mail) {
        if (!mail.authored_from_role(rope(s.bell))) {
            ++state_.strikes_ignored;
            return;
        }
        if (!state_.going) {
            return;
        }
        if (s.row != at_row_) {
            if (filled_ != 0 && filled_ < band_) {
                stand(mail, "only " + std::to_string(filled_) + " bells in row " +
                                std::to_string(at_row_));
                return;
            }
            at_row_ = s.row;
            std::fill(row_.begin(), row_.end(), 0);
            filled_ = 0;
        }
        if (s.place < 1 || s.place > band_ || row_[static_cast<std::size_t>(s.place)] != 0) {
            stand(mail, "two bells in " + std::to_string(s.place) + " at row " +
                            std::to_string(s.row));
            return;
        }
        row_[static_cast<std::size_t>(s.place)] = s.bell;
        ++filled_;
        if (filled_ < band_) {
            return;
        }

        ++state_.rows_heard;
        filled_ = 0;

        if (is_rounds()) {
            mail.as_role(kOfficeConductor).publish(Call{kThatsAll, state_.rows_heard});
            state_.came_round = true;
            state_.going = false;
            return;
        }
        if (lead_ <= 1) {
            return;
        }
        // A call is made one whole pull before the lead end, so that everybody
        // has heard it by the time it matters.
        if (state_.rows_heard % lead_ != lead_ - 1) {
            return;
        }
        const std::size_t lead_index =
            static_cast<std::size_t>((state_.rows_heard + 1) / lead_) - 1;
        if (lead_index >= calls_.size() || calls_[lead_index].empty()) {
            return;
        }
        mail.as_role(kOfficeConductor).publish(Call{calls_[lead_index], state_.rows_heard + 1});
        ++state_.calls_made;
        called_.push_back(calls_[lead_index] + " before row " +
                          std::to_string(state_.rows_heard + 1));
    }

    const std::vector<std::string>& not_ready() const { return not_ready_; }
    const std::vector<std::string>& called() const { return called_; }
    const std::string& stood_because() const { return stood_because_; }
    std::int64_t lead() const { return lead_; }

private:
    bool is_rounds() const {
        for (std::int64_t p = 1; p <= band_; ++p) {
            if (row_[static_cast<std::size_t>(p)] != p) {
                return false;
            }
        }
        return true;
    }

    void stand(loom::Mail& mail, const std::string& why) {
        stood_because_ = why;
        state_.stood = true;
        state_.going = false;
        mail.as_role(kOfficeConductor).publish(Call{kStand, at_row_});
    }

    std::string name_;
    std::vector<std::string> calls_;
    std::vector<std::string> called_;
    std::vector<std::string> not_ready_;
    std::string stood_because_;
    std::vector<std::int64_t> row_;
    std::int64_t band_ = 0;
    std::int64_t lead_ = 0;
    std::int64_t filled_ = 0;
    std::int64_t at_row_ = 0;
};

// ===========================================================================
// The pricker
// ===========================================================================

// Somebody sat in the corner writing the touch down, blow by blow, as it is
// struck. It is the only account of what was actually rung, and it is built
// from nothing but bells:
//
//   it is not given the composition
//   it does not accept `Call`, so it cannot hear one
//   it has never heard of a method
//   its grant is EMPTY -- it may say nothing to anybody, ever
//
// A witness that could hear the conductor could be tempted to check the
// conductor. This one has nothing to check against except the paper in front
// of it.
class Pricker : public loom::WeaveBase<Pricker, PrickerState,
                                       loom::Accept<TakeUpYourPen, Struck>, loom::Emit<>> {
public:
    void on(const TakeUpYourPen& t, loom::Mail&) {
        bells_ = t.bells;
        what_ = t.what;
        paper_.clear();
        seen_.clear();
        struck_.clear();
        row_.assign(static_cast<std::size_t>(bells_) + 1, 0);
        filled_ = 0;
        at_row_ = 0;
        first_repeat_at_ = 0;
        first_rung_at_ = 0;
        first_repeat_row_.clear();
        state_ = PrickerState{};
    }

    void on(const Struck& s, loom::Mail& mail) {
        // A bell's voice is its own. Anything else banging in the chamber is not
        // a bell, and does not go on the paper.
        if (!mail.authored_from_role(rope(s.bell))) {
            ++state_.ignored;
            return;
        }
        ++state_.strikes;

        if (s.row != at_row_) {
            if (filled_ != 0 && filled_ < bells_) {
                ++state_.short_rows;
            }
            at_row_ = s.row;
            std::fill(row_.begin(), row_.end(), 0);
            struck_.clear();
            filled_ = 0;
        }
        if (s.place < 1 || s.place > bells_ || row_[static_cast<std::size_t>(s.place)] != 0 ||
            struck_.count(s.bell) != 0) {
            ++state_.clashes;
            return;
        }
        row_[static_cast<std::size_t>(s.place)] = s.bell;
        struck_.insert(s.bell);
        ++filled_;
        if (filled_ < bells_) {
            return;
        }

        const std::string written = row_text(row_);
        ++state_.rows;
        filled_ = 0;
        if (row_[static_cast<std::size_t>(bells_)] == bells_) {
            ++state_.covered;
        }
        const auto it = seen_.find(written);
        if (it == seen_.end()) {
            seen_.emplace(written, state_.rows);
        } else {
            ++state_.repeats;
            if (first_repeat_at_ == 0) {
                first_repeat_at_ = state_.rows;
                first_rung_at_ = it->second;
                first_repeat_row_ = written;
            }
        }
        paper_.push_back(written);
    }

    const std::vector<std::string>& paper() const { return paper_; }
    const std::string& what() const { return what_; }
    std::int64_t distinct() const { return static_cast<std::int64_t>(seen_.size()); }
    std::int64_t first_repeat_at() const { return first_repeat_at_; }
    std::int64_t first_rung_at() const { return first_rung_at_; }
    const std::string& first_repeat_row() const { return first_repeat_row_; }

    bool came_round() const {
        if (paper_.empty()) {
            return false;
        }
        std::string rounds;
        for (std::int64_t b = 1; b <= bells_; ++b) {
            rounds += static_cast<char>('0' + b);
        }
        return paper_.back() == rounds;
    }

    /// A touch is true when no row was rung twice -- and only then. Every other
    /// count here is a way of not having rung rows at all.
    bool truth() const {
        return !paper_.empty() && state_.repeats == 0 && state_.clashes == 0 &&
               state_.short_rows == 0;
    }

private:
    std::int64_t bells_ = 0;
    std::string what_;
    std::vector<std::string> paper_;
    std::map<std::string, std::int64_t> seen_;
    std::set<std::int64_t> struck_;
    std::vector<std::int64_t> row_;
    std::int64_t filled_ = 0;
    std::int64_t at_row_ = 0;
    std::int64_t first_repeat_at_ = 0;
    std::int64_t first_rung_at_ = 0;
    std::string first_repeat_row_;
};

// ===========================================================================
// The evening
// ===========================================================================

constexpr std::int64_t kBells = 6;
constexpr std::int64_t kLastPull = 400; // the tower is locked at half past nine

struct Verdict {
    std::int64_t rows = 0;
    std::int64_t distinct = 0;
    std::int64_t blows = 0;       ///< blows the pricker wrote down
    std::int64_t clashes = 0;
    std::int64_t short_rows = 0;
    std::int64_t covered = 0;
    std::int64_t calls = 0;
    std::int64_t ignored = 0;     ///< strikes the paper refused as not-a-bell
    std::int64_t ears = 0;        ///< listeners reached, counted by the bells themselves
    std::int64_t on_the_tap = 0;  ///< `Struck` deliveries the host's own tap saw
    std::int64_t method_asked_while_ringing = 0;
    bool came_round = false;
    bool truth = false;
    bool stood = false;
};

bool check(const char* what, bool ok) {
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
    return ok;
}

void print_paper(const Pricker& pricker) {
    const std::vector<std::string>& paper = pricker.paper();
    std::cout << "\n-- the paper (" << pricker.what() << ") --\n";
    for (std::size_t i = 0; i < paper.size(); i += 10) {
        std::cout << "  " << (i + 1 < 100 ? " " : "") << (i + 1 < 10 ? " " : "") << (i + 1) << " ";
        for (std::size_t j = i; j < i + 10 && j < paper.size(); ++j) {
            std::cout << " " << paper[j];
        }
        std::cout << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: practice <plain-bob-doubles.so> <plain-bob-minor.so> "
                     "[--fumble|--false-touch|--bell-down]\n";
        return 2;
    }
    const std::string doubles_path = argv[1];
    const std::string minor_path = argv[2];

    bool fumble = false;
    bool false_touch = false;
    bool bell_down = false;
    for (int i = 3; i < argc; ++i) {
        const std::string flag = argv[i];
        fumble = fumble || flag == "--fumble";
        false_touch = false_touch || flag == "--false-touch";
        bell_down = bell_down || flag == "--bell-down";
    }

    loom::Switchboard bus;
    loom::Kernel kernel(bus);

    std::cout << "ringing-chamber -- St Cuthbert's, Wenbourne: a practice night\n";
    std::cout << "contain: " << loom::Kernel::containment_note() << "\n\n";

    // ---- the band -------------------------------------------------------
    //
    // A ringer may ask the method for a line and may sound its own bell. That
    // is the entire list: it cannot call, it cannot write anything down, and it
    // cannot read what anybody else claims.
    const std::vector<std::string> names = {"Nell", "Bram", "Ivo", "Peg", "Alma", "Ossie"};
    std::map<std::int64_t, loom::WeaveId> band;
    for (std::int64_t b = 1; b <= kBells; ++b) {
        loom::Grant g;
        g.allow_to_role("WhatIsMyLine", 1, kOfficeMethod).allow_to_any("Struck", 1);
        // LABELLED: in the fumble scenario Ivo does not hear the first call.
        // Everything else about this ringer is the same code as the other five.
        //
        // It has to be Ivo and not just anybody. At that first bob the third is
        // the fifth place bell and is the one making the bob, so missing it puts
        // Ivo where Bram is already going. Two of the five are UNAFFECTED by a
        // bob -- the treble and whoever is the fourth place bell, doing long
        // fifths -- and a ringer who mishears a call that was never going to
        // touch them gets away with it completely. The first draft of this
        // control picked one of those, rang a faultless true 120, and taught the
        // author some ringing.
        const std::int64_t mishears = (fumble && b == 3) ? 1 : 0;
        band[b] = mount_office<Ringer>(bus, g, rope(b), b, names[static_cast<std::size_t>(b - 1)],
                                       mishears);
    }

    // ---- the conductor --------------------------------------------------
    loom::Grant conductor_grant;
    conductor_grant.allow_to_role("WhatIsMyLine", 1, kOfficeMethod)
        .allow_to_any("Call", 1)
        .allow_observe("BellUp", 1);
    const loom::WeaveId conductor_id =
        mount_office<Conductor>(bus, conductor_grant, kOfficeConductor);

    // ---- the pricker ----------------------------------------------------
    //
    // An entirely empty grant. The witness may say nothing to anybody: it
    // listens, and that is all it is for.
    const loom::WeaveId pricker_id = mount_office<Pricker>(bus, loom::Grant{}, kOfficePricker);

    auto* conductor = static_cast<Conductor*>(bus.weave(conductor_id));
    auto* pricker = static_cast<Pricker*>(bus.weave(pricker_id));

    // ---- a tap ----------------------------------------------------------
    //
    // The host's own ears, for the things nobody is told: a refused delivery,
    // and whether anything reached the method while the bells were going.
    std::vector<std::string> refusals;
    loom::WeaveId method_id{};
    bool ringing = false;
    std::int64_t method_asked_while_ringing = 0;
    std::int64_t struck_on_the_tap = 0;
    bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Refused) {
            refusals.push_back(std::string(loom::name_of(e.refusal.reason)) + " on " +
                               e.schema_name);
        }
        if (e.kind != loom::EventKind::Delivered) {
            return;
        }
        // A count that must be non-zero, so that "nothing was refused" is a
        // measurement and not a tap that was never wired up.
        if (e.schema_name == "Struck") {
            ++struck_on_the_tap;
        }
        if (ringing && method_id.valid() && e.target == method_id) {
            ++method_asked_while_ringing;
        }
    });

    // -------------------------------------------------------------------
    // Ringing up.
    // -------------------------------------------------------------------
    std::cout << "-- ringing up --\n";
    for (std::int64_t b = 1; b <= kBells; ++b) {
        if (bell_down && b == 4) {
            std::cout << "  ..     the 4 is left down (LABELLED)\n";
            continue;
        }
        bus.send_to_role(rope(b), loom::Message(loom::to_value(RingUp{b})));
    }
    bus.pump();
    std::cout << "  ..     " << (bell_down ? kBells - 1 : kBells) << " up\n";

    // -------------------------------------------------------------------
    // One touch.
    // -------------------------------------------------------------------
    auto all_ears = [&]() {
        std::int64_t total = 0;
        for (std::int64_t b = 1; b <= kBells; ++b) {
            total += static_cast<const Ringer*>(bus.weave(band[b]))->ears();
        }
        return total;
    };

    auto ring = [&](const std::string& artifact, const std::string& what,
                    const std::vector<std::string>& calls, bool run_controls) -> Verdict {
        const std::int64_t ears_before = all_ears();
        const std::int64_t tap_before = struck_on_the_tap;
        std::cout << "\n-- getting the method up --\n";
        const loom::LoadResult lr = kernel.load("method", artifact, kOfficeMethod,
                                                loom::Grant{}.allow_to_any("YourLine", 1));
        if (!lr.ok) {
            std::cerr << "LOAD FAILED: " << lr.error << "\n";
            std::exit(1);
        }
        method_id = lr.id;
        std::cout << "  ..     the book is open (weave " << lr.id.value << ")\n";

        for (std::int64_t b = 1; b <= kBells; ++b) {
            bus.send_to_role(rope(b), loom::Message(loom::to_value(LearnTheMethod{kBells})));
        }
        bus.send_to_role(kOfficeConductor, loom::Message(loom::to_value(LearnTheMethod{kBells})));
        bus.pump();

        const MethodState ms = loom::from_value<MethodState>(bus.weave(method_id)->snapshot());
        std::cout << "  ..     " << ms.name << ": " << ms.bells << " bells changing, a lead of "
                  << ms.lead << ", " << ms.lines_given << " lines given out\n";
        for (std::int64_t b = 1; b <= kBells; ++b) {
            const auto* r = static_cast<const Ringer*>(bus.weave(band[b]));
            const RingerState rs = loom::from_value<RingerState>(r->snapshot());
            std::cout << "  ..     " << r->who() << " on the " << b << ": "
                      << (rs.covering ? "covering" : rs.learned ? "has the line" : "LOST") << "\n";
        }

        bus.send_to_role(kOfficeConductor,
                         loom::Message(loom::to_value(TheComposition{what, kBells, calls})));
        bus.send_to_role(kOfficePricker, loom::Message(loom::to_value(TakeUpYourPen{kBells, what})));
        bus.pump();

        std::cout << "\n-- " << what << " --\n";
        bus.send_to_role(kOfficeConductor, loom::Message(loom::to_value(LookRound{0})));
        bus.pump();
        const ConductorState before = loom::from_value<ConductorState>(conductor->snapshot());
        if (!before.going) {
            std::cout << "  --     WE ARE NOT ALL HERE -- ";
            for (const std::string& n : conductor->not_ready()) {
                std::cout << n << " ";
            }
            std::cout << "\n  --     the conductor will not go\n";
            kernel.unload("method");
            method_id = loom::WeaveId{};
            return Verdict{};
        }
        std::cout << "  ..     look to. Treble's going. She's gone.\n";

        method_asked_while_ringing = 0;
        ringing = true;
        std::int64_t row = 0;
        for (row = 1; row <= kLastPull; ++row) {
            const ConductorState cs = loom::from_value<ConductorState>(conductor->snapshot());
            if (!cs.going) {
                break;
            }
            for (std::int64_t b = 1; b <= kBells; ++b) {
                bus.send_to_role(rope(b), loom::Message(loom::to_value(Pull{row})));
            }
            if (run_controls && row == 25) {
                // LABELLED CONTROL: something in the chamber that is not a bell.
                // If the paper took it, row 25 would have the 3 in it twice.
                std::cout << "  --     CONTROL: a hand slapped on the wall, claiming to be the 3\n";
                bus.publish(loom::Message(loom::to_value(Struck{3, 1, row})));
            }
            bus.pump();

            const ConductorState now = loom::from_value<ConductorState>(conductor->snapshot());
            if (run_controls && now.rows_heard == 19) {
                // LABELLED CONTROL: a voice from the stairs, calling a bob at a
                // lead the composition leaves plain. If the band took it the
                // whole touch would go somewhere else.
                std::cout << "  --     CONTROL: somebody on the stairs shouts \"Bob!\"\n";
                bus.publish(loom::Message(loom::to_value(Call{kBob, 20})));
                bus.pump();
            }
            if (run_controls && now.rows_heard == 60) {
                // LABELLED CONTROL, and the only one here that has to be forged.
                // Every other refusal in this application is the domain's own --
                // a ringer deciding a voice was not the conductor's. That leaves
                // the grants themselves unexercised, so "the pricker may say
                // nothing to anybody" is, on the evidence so far, just a line of
                // code that was never tested.
                //
                // The pricker cannot express this attack: it has no verb that
                // sends. The host can, because `send_as` stamps the pricker as
                // the author and then authorises against the PRICKER's grant --
                // which is empty. This is the one bus refusal of the evening.
                std::cout << "  --     CONTROL: the pricker tries to call a bob\n";
                bus.send_as(pricker_id, band[1], loom::Message(loom::to_value(Call{kBob, 61})));
                bus.pump();
            }
            if (run_controls && now.rows_heard == 30) {
                // LABELLED CONTROL: a line handed to Alma by somebody who is not
                // the book, and which answers no question she asked.
                std::cout << "  --     CONTROL: somebody tells Alma her line is different\n";
                YourLine wrong;
                wrong.method = "hearsay";
                wrong.bells = 5;
                wrong.lead = 10;
                wrong.place_bell = 5;
                wrong.path = {1, 1, 1, 1, 1, 1, 1, 1, 1};
                wrong.plain_end = 1;
                wrong.bob_end = 1;
                wrong.single_end = 1;
                bus.send_to_role(rope(5), loom::Message(loom::to_value(wrong)));
                bus.pump();
            }
        }
        ringing = false;

        const ConductorState cs = loom::from_value<ConductorState>(conductor->snapshot());
        for (const std::string& c : conductor->called()) {
            std::cout << "  ..     \"" << c << "\"\n";
        }
        if (cs.came_round) {
            std::cout << "  ..     \"That's all.\"  (rounds at row " << cs.rows_heard << ")\n";
        } else if (cs.stood) {
            std::cout << "  !!     \"STAND.\"  -- " << conductor->stood_because() << "\n";
        } else {
            std::cout << "  !!     the tower closed and the band was still going\n";
        }

        print_paper(*pricker);

        const PrickerState ps = loom::from_value<PrickerState>(pricker->snapshot());
        Verdict v;
        v.rows = ps.rows;
        v.distinct = pricker->distinct();
        v.blows = ps.strikes;
        v.clashes = ps.clashes;
        v.short_rows = ps.short_rows;
        v.covered = ps.covered;
        v.calls = cs.calls_made;
        v.came_round = pricker->came_round();
        v.truth = pricker->truth();
        v.stood = cs.stood;
        v.ignored = ps.ignored;
        v.ears = all_ears() - ears_before;
        v.on_the_tap = struck_on_the_tap - tap_before;
        v.method_asked_while_ringing = method_asked_while_ringing;

        std::cout << "\n-- what was rung --\n";
        std::cout << "  rows                  " << v.rows << "\n";
        std::cout << "  distinct rows         " << v.distinct << "\n";
        std::cout << "  blows on the paper    " << v.blows << "\n";
        std::cout << "  listeners reached     " << v.ears << " (the bells' own count)\n";
        std::cout << "  on the tap            " << v.on_the_tap << " (the host's own ears)\n";
        std::cout << "  rows that were a row  " << (v.rows - v.short_rows) << " (clashes "
                  << v.clashes << ", short " << v.short_rows << ")\n";
        std::cout << "  tenor last            " << v.covered << " of " << v.rows << "\n";
        std::cout << "  came round            " << (v.came_round ? "yes" : "no") << "\n";
        std::cout << "  calls made            " << v.calls << "\n";
        std::cout << "  not a bell, ignored   " << v.ignored << " by the paper, "
                  << cs.strikes_ignored << " by the conductor\n";
        std::cout << "  the book, while the bells were going: " << v.method_asked_while_ringing
                  << " question(s)\n";
        if (v.truth) {
            std::cout << "  TRUE -- no row was rung twice\n";
        } else if (pricker->first_repeat_at() != 0) {
            std::cout << "  FALSE -- row " << pricker->first_repeat_at() << " ("
                      << pricker->first_repeat_row() << ") was already rung at row "
                      << pricker->first_rung_at() << "\n";
        } else {
            std::cout << "  NOT A TOUCH -- " << v.clashes << " clash(es), " << v.short_rows
                      << " short row(s)\n";
        }

        std::cout << "\n-- stand --\n";
        kernel.unload("method");
        method_id = loom::WeaveId{};
        std::cout << "  ..     the book is closed\n";
        return v;
    };

    // -------------------------------------------------------------------
    // Right then. A 120 of Plain Bob Doubles.
    //
    // Four leads make a plain course and it comes round, so a bob is called at
    // the end of every fourth lead to send the band off into the next course.
    // Three courses and you are back where you started, having rung every one
    // of the hundred and twenty orders of five bells exactly once.
    // -------------------------------------------------------------------
    const std::vector<std::string> the_120 = {"",    "", "", kBob, "", "",
                                              "",    kBob, "", "",   "", kBob};
    // A touch that comes round, is struck faultlessly, and is worth nothing.
    const std::vector<std::string> the_80 = {"", "", "", kBob, kBob, kBob, kBob, ""};

    const Verdict doubles =
        ring(doubles_path,
             false_touch ? "80 Plain Bob Doubles (bobs at 4,5,6,7)" : "120 Plain Bob Doubles",
             false_touch ? the_80 : the_120, !false_touch && !fumble && !bell_down);

    // -------------------------------------------------------------------
    // "Right -- Plain Bob Minor. Everybody got it?"
    //
    // A different artifact through the same door. Six bells changing now, so
    // Ossie has a line instead of a cover and the whole band has to learn it
    // again before a note is struck.
    // -------------------------------------------------------------------
    Verdict minor;
    const bool ring_minor = !bell_down && !fumble && !false_touch;
    if (ring_minor) {
        minor = ring(minor_path, "plain course of Plain Bob Minor", {"", "", "", "", ""}, false);
    }

    // -------------------------------------------------------------------
    // Ringing down.
    // -------------------------------------------------------------------
    if (!bell_down) {
        std::cout << "\n-- ringing down --\n";
        for (std::int64_t b = 1; b <= kBells; ++b) {
            bus.send_to_role(rope(b), loom::Message(loom::to_value(RingDown{b})));
        }
        bus.pump();
        std::cout << "  ..     all down\n";
    }

    // -------------------------------------------------------------------
    // What the evening is allowed to claim.
    // -------------------------------------------------------------------
    std::int64_t ears = 0;
    std::int64_t stray_calls = 0;
    std::int64_t stray_lines = 0;
    std::int64_t unreachable = 0;
    std::int64_t refused_blows = 0;
    for (std::int64_t b = 1; b <= kBells; ++b) {
        const auto* r = static_cast<const Ringer*>(bus.weave(band[b]));
        const RingerState rs = loom::from_value<RingerState>(r->snapshot());
        ears += r->ears();
        stray_calls += rs.not_the_conductor;
        stray_lines += rs.not_from_the_method;
        unreachable += rs.unreachable;
        refused_blows += rs.refused;
    }
    const PrickerState final_paper = loom::from_value<PrickerState>(pricker->snapshot());

    std::cout << "\n-- the evening --\n";
    std::cout << "  blows heard by somebody   " << ears << " (each bell is heard by two: the\n";
    std::cout << "                            conductor and the pricker)\n";
    std::cout << "  the same, on the tap      " << struck_on_the_tap << "\n";
    std::cout << "  voices ignored            " << stray_calls << " call(s), " << stray_lines
              << " line(s)\n";
    std::cout << "  blows not struck          " << refused_blows << " (bell down or no line)\n";
    std::cout << "  \"cannot get there\"        " << unreachable << "\n";
    std::cout << "  bus refusals seen         " << refusals.size();
    if (!refusals.empty()) {
        std::cout << "  [" << refusals.front() << (refusals.size() > 1 ? " ..." : "") << "]";
    }
    std::cout << "\n";

    std::cout << "\n-- checks --\n";
    bool ok = true;

    if (bell_down) {
        ok &= check("the conductor would not go with a bell down", doubles.rows == 0);
        ok &= check("nothing was rung", final_paper.strikes == 0);
        ok &= check("the 4 was the reason", !conductor->not_ready().empty() &&
                                                conductor->not_ready().front().rfind("4", 0) == 0);
        std::cout << "\n" << (ok ? "CONTROL OK -- a bell down stops the band" : "CONTROL FAILED")
                  << "\n";
        return ok ? 0 : 1;
    }

    if (fumble) {
        ok &= check("the band was stood", doubles.stood);
        ok &= check("what stopped it was not a row", doubles.clashes >= 1);
        ok &= check("the touch did not come round", !doubles.came_round);
        ok &= check("and it is not a 120", doubles.rows < 120);
        ok &= check("the paper says so", !doubles.truth);
        std::cout << "\n"
                  << (ok ? "CONTROL OK -- the same method and the same composition as the good\n"
                           "             run; only the band was different, and the paper knew"
                         : "CONTROL FAILED")
                  << "\n";
        return ok ? 0 : 1;
    }

    if (false_touch) {
        ok &= check("it came round", doubles.came_round);
        ok &= check("it was struck without a fault", doubles.clashes == 0 &&
                                                         doubles.short_rows == 0);
        ok &= check("every bell was heard on every row", doubles.blows == doubles.rows * kBells);
        ok &= check("the conductor was happy", !doubles.stood);
        ok &= check("AND IT IS FALSE", !doubles.truth);
        ok &= check("the paper names the repeat", pricker->first_repeat_at() == 50 &&
                                                      pricker->first_rung_at() == 30);
        ok &= check("thirty rows were rung twice and thirty were never rung",
                    doubles.rows == 80 && doubles.distinct == 50);
        std::cout << "\n"
                  << (ok ? "CONTROL OK -- a perfectly struck touch that is worth nothing"
                         : "CONTROL FAILED")
                  << "\n";
        return ok ? 0 : 1;
    }

    // The practice night proper.
    ok &= check("the 120 came round", doubles.came_round);
    ok &= check("it is a hundred and twenty rows", doubles.rows == 120);
    ok &= check("seven hundred and twenty blows were heard", doubles.blows == 720);
    ok &= check("every row was a row (no clash, none short)",
                doubles.clashes == 0 && doubles.short_rows == 0);
    ok &= check("the tenor covered every row", doubles.covered == 120);
    ok &= check("no row was rung twice -- the touch is TRUE", doubles.truth);
    ok &= check("and that is every order of the five bells", doubles.distinct == 120);
    ok &= check("three bobs were called", doubles.calls == 3);
    ok &= check("the book was shut while the bells were going",
                doubles.method_asked_while_ringing == 0);

    ok &= check("the plain course of Minor came round", minor.came_round);
    ok &= check("sixty rows of it", minor.rows == 60);
    ok &= check("and it is true", minor.truth);
    ok &= check("all six bells changed (the tenor did not cover)", minor.covered < 60);
    ok &= check("no calls in a plain course", minor.calls == 0);

    ok &= check("the shout from the stairs was not a call", stray_calls == kBells);
    // The whole evening produced exactly one bus refusal, and it is the one a
    // labelled control forced. Every other refusal here belongs to the domain.
    ok &= check("the pricker's empty grant is real, and is the evening's only "
                "bus refusal",
                refusals.size() == 1 && refusals.front() == "CapabilityDenied on Call");
    ok &= check("the hand on the wall was not a bell", doubles.ignored == 1);
    ok &= check("hearsay is not a line", stray_lines == 1);
    ok &= check("nobody missed a blow", refused_blows == 0 && unreachable == 0);
    ok &= check("every bell was heard by both listeners", ears == (720 + 360) * 2);
    // Three accounts of the same ringing, and they agree: the bells counted who
    // heard them, the paper counted what it wrote down, and the host's own tap
    // counted deliveries. The tap is TWO higher than the bells, and those two are
    // the hand slapped on the wall -- delivered to both listeners, written down
    // by neither.
    ok &= check("the tap heard exactly the bells, plus the hand on the wall",
                doubles.on_the_tap == doubles.ears + 2 && minor.on_the_tap == minor.ears);
    ok &= check("the paper is the bells and nothing else",
                doubles.blows == doubles.rows * kBells && minor.blows == minor.rows * kBells);

    std::cout << "\n" << (ok ? "A GOOD NIGHT'S RINGING" : "SOMETHING WAS WRONG") << "\n";
    return ok ? 0 : 1;
}
