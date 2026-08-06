// The theatre: the departments, the deck, the company stage manager, one act of
// The Salt Harvest, and the show report.
//
// The house owns the performance -- it moves the act forward a beat at a time
// and it decides when the book changes hands. It is not a weave and it holds no
// office; it is the building.
//
// Three runs live here, and the second and third exist so that the first is
// worth believing:
//
//   (default)          the act, with the book handed over live at beat 15
//   --drop-the-book    the same act, with a relief briefed with NOTHING.
//                      Passes only if the show report NOTICES.
//   --wrong-book       the same act, with a relief handed the wrong edition.
//                      It refuses; the outgoing DSM carries on and finishes.

#include "prompt_book.hpp"

#include <zen/host/prepared_replacement.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace promptcorner;

namespace {

// ---------------------------------------------------------------------------
// Registering a weave that holds an office.
//
// `mount()` and `mount_granted()` take no role, and `register_weave` -- the only
// binder -- is the raw door, so it does not do the zen_set_self() wiring mount
// does. Every office-holder in this building therefore needs these six lines.
// (Night Lab has now written them twice; see FRICTION F-04.)
// ---------------------------------------------------------------------------
template <class W, class... Args>
W* mount_office(loom::Switchboard& bus, loom::Grant grant, const std::string& office,
                loom::WeaveId& out_id, Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    out_id = bus.register_weave(std::move(weave), std::move(grant), office);
    raw->zen_set_self(out_id); // silent to omit, and everything still half-works
    return raw;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

void erase_one(std::vector<std::string>& v, const std::string& s) {
    const auto it = std::find(v.begin(), v.end(), s);
    if (it != v.end()) {
        v.erase(it);
    }
}

void line(const std::string& tag, const std::string& text, std::int64_t at = -1) {
    std::cout << "  ";
    if (at >= 0) {
        std::cout << "t" << std::setw(2) << std::setfill('0') << at << " ";
    } else {
        std::cout << "--  ";
    }
    std::cout << std::setw(10) << std::setfill(' ') << std::left << tag << std::right << " "
              << text << "\n";
}

// ---------------------------------------------------------------------------
// A department: somebody sitting at a board, a desk or a rail, waiting to be
// warned and then told.
//
// The two rules it enforces are its own and it enforces them against everybody,
// including whoever is currently calling the show:
//
//   a GO that was not authored by the caller's office is not a cue
//   a GO for a cue nobody was stood by for is not taken
//
// The fly floor has one extra duty: it cannot say it is standing by until the
// deck under the bar has been looked at, so it takes the answer away with it and
// spends it when the crew reports.
// ---------------------------------------------------------------------------
class Department : public loom::WeaveBase<Department, DeptState,
                                          loom::Accept<Standby, Go, RailClear>,
                                          loom::Emit<StandingBy, CheckTheRail>> {
public:
    Department(std::string name, bool rail) : rail_(rail) {
        state_.dept = std::move(name);
        state_.live = "dark";
    }

    void on(const Standby& s, loom::Mail& mail) {
        if (contains(state_.took, s.cue)) {
            // "We've had that cue." Refused rather than re-armed: an operator
            // who re-arms a cue they already took is one GO away from doing it
            // twice, and on a fly floor that is a bar coming in on somebody.
            ++state_.queried;
            line(state_.dept, "QUERY  " + s.cue + " -- we have had that cue");
            return;
        }
        ++state_.warned;
        if (!rail_) {
            if (!contains(state_.standing, s.cue)) {
                state_.standing.push_back(s.cue);
            }
            mail.answer(StandingBy{s.cue});
            return;
        }
        // Take the answer away and go and look. Every ask gets its own -- if two
        // people warned us, two people get told.
        pending_.emplace_back(s.cue, mail.defer_answer());
        mail.send_to_role(kOfficeDeck, CheckTheRail{s.cue});
    }

    void on(const RailClear& r, loom::Mail& mail) {
        if (!contains(state_.standing, r.cue)) {
            state_.standing.push_back(r.cue);
        }
        std::vector<std::pair<std::string, loom::DeferredAnswer>> keep;
        for (auto& p : pending_) {
            if (p.first != r.cue) {
                keep.push_back(std::move(p));
                continue;
            }
            const loom::Ticket t = loom::answer_deferred(p.second, mail, StandingBy{r.cue});
            if (t.valid()) {
                ++spends_taken_;
            } else {
                ++spends_refused_;
            }
        }
        pending_ = std::move(keep);
    }

    void on(const Go& g, loom::Mail& mail) {
        // WHO SAID IT. Not "may this weave send me a Go" -- anybody granted the
        // shape can -- but "did the person calling the show say it".
        if (!mail.authored_from_role(kOfficeCaller)) {
            ++state_.unauthored;
            line(state_.dept, "IGNORED " + g.cue + " -- not from the corner", g.at);
            return;
        }
        if (contains(state_.took, g.cue)) {
            ++state_.queried;
            line(state_.dept, "QUERY  " + g.cue + " -- we have had that cue", g.at);
            return;
        }
        if (!contains(state_.standing, g.cue)) {
            ++state_.no_standby;
            line(state_.dept, "HELD   " + g.cue + " -- we were never stood by", g.at);
            return;
        }
        state_.live = g.effect;
        state_.took.push_back(g.cue);
        took_by_.push_back(mail.sender().value);
        erase_one(state_.standing, g.cue);
        line(state_.dept, g.cue + "  " + g.effect, g.at);
    }

    const DeptState& report() const { return state_; }
    const std::vector<std::uint64_t>& took_by() const { return took_by_; }
    std::int64_t spends_taken() const { return spends_taken_; }
    std::int64_t spends_refused() const { return spends_refused_; }

private:
    bool rail_ = false;
    std::vector<std::pair<std::string, loom::DeferredAnswer>> pending_;
    std::vector<std::uint64_t> took_by_;
    std::int64_t spends_taken_ = 0;
    std::int64_t spends_refused_ = 0;
};

// ---------------------------------------------------------------------------
// The deck crew. Somebody has to walk under the bar and look, and they do it on
// the next beat rather than the instant they are asked, which is why fly
// standbys go three pages early.
// ---------------------------------------------------------------------------
class Deck : public loom::WeaveBase<Deck, DeckState, loom::Accept<CheckTheRail, Moment>,
                                    loom::Emit<RailClear>> {
public:
    void on(const CheckTheRail& c, loom::Mail&) {
        if (!contains(state_.waiting, c.cue)) {
            state_.waiting.push_back(c.cue);
        }
    }

    void on(const Moment&, loom::Mail& mail) {
        for (const std::string& cue : state_.waiting) {
            mail.send_to_role(kOfficeFlys, RailClear{cue});
            ++state_.cleared;
        }
        state_.waiting.clear();
    }

    const DeckState& report() const { return state_; }
};

// ---------------------------------------------------------------------------
// The company stage manager: the only person in the building with the authority
// to take the book off one DSM and give it to another, and the one who does the
// talking while it happens.
//
// It holds a pointer to the house's PreparedReplacement handle. That is the
// documented coordinator friction, not an invention of this application: the
// handle is host-owned and offer_current_answer() must run inside the
// coordinator's own delivery, so the two have to meet somewhere.
// ---------------------------------------------------------------------------
class Csm : public loom::WeaveBase<
                Csm, CsmState,
                loom::Accept<BeginHandover, TheBook, HaveTheBook, CannotTakeTheBook>,
                loom::Emit<StandDown, CarryOn>> {
public:
    void attach(loom::PreparedReplacement* upgrade) { upgrade_ = upgrade; }

    void on(const BeginHandover& b, loom::Mail& mail) {
        ++state_.handovers;
        edition_ = b.edition;
        carry_ = b.carry_the_book;
        line("CSM", "relieving the corner", b.at);
        mail.send_to_role(kOfficeCaller, StandDown{b.at});
    }

    /// The outgoing DSM's exact position, authored after it stopped calling.
    void on(const TheBook& book, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return; // a lookalike is not the book
        }
        BriefTheRelief brief;
        brief.edition = edition_.empty() ? book.edition : edition_;
        brief.next_index = carry_ ? book.next_index : 0;
        brief.given = carry_ ? book.given : std::vector<std::string>{};
        std::string given_text;
        for (const std::string& g : brief.given) {
            given_text += (given_text.empty() ? "" : ", ") + g;
        }
        line("CSM", "briefing the relief: next is #" + std::to_string(brief.next_index) +
                        ", standing by [" + (given_text.empty() ? "-" : given_text) + "]");
        ++state_.briefed;
        if (upgrade_ != nullptr) {
            const loom::TxnResult r = upgrade_->ask(brief);
            if (!r.ok) {
                state_.last = std::string("ask refused: ") + loom::name_of(r.why);
            }
        }
    }

    void on(const HaveTheBook& h, loom::Mail&) {
        ++state_.ready;
        line("CSM", "the relief has the book, from #" + std::to_string(h.from_index));
        if (upgrade_ != nullptr) {
            const loom::TxnResult r =
                upgrade_->offer_current_answer(loom::PreparationAnswer::Ready);
            state_.last = r.ok ? "ready offered" : std::string("offer refused: ") +
                                                       loom::name_of(r.why);
        }
    }

    void on(const CannotTakeTheBook& no, loom::Mail& mail) {
        ++state_.refused;
        line("CSM", "the relief will not take it: " + no.why);
        if (upgrade_ != nullptr) {
            const loom::TxnResult r =
                upgrade_->offer_current_answer(loom::PreparationAnswer::Refused);
            state_.last = r.ok ? "refusal offered" : std::string("offer refused: ") +
                                                        loom::name_of(r.why);
        }
        // Somebody still has to call the show.
        line("CSM", "as you were");
        mail.send_to_role(kOfficeCaller, CarryOn{0});
    }

    const CsmState& report() const { return state_; }

private:
    loom::PreparedReplacement* upgrade_ = nullptr;
    std::string edition_;
    bool carry_ = true;
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: theatre <caller.so> [--drop-the-book|--wrong-book]\n";
        return 2;
    }
    const std::string caller_path = argv[1];
    const std::string mode = (argc > 2) ? argv[2] : "";
    const bool drop_the_book = (mode == "--drop-the-book");
    const bool wrong_book = (mode == "--wrong-book");
    if (!mode.empty() && !drop_the_book && !wrong_book) {
        std::cerr << "unknown mode: " << mode << "\n";
        return 2;
    }

    loom::Switchboard bus;
    loom::Kernel kernel(bus);

    std::cout << "prompt-corner -- The Salt Harvest, one act\n";
    std::cout << "contain: " << loom::Kernel::containment_note() << "\n";
    if (drop_the_book) {
        std::cout << "CONTROL: the relief will be briefed with an EMPTY book\n";
    }
    if (wrong_book) {
        std::cout << "CONTROL: the relief will be briefed with the WRONG EDITION\n";
    }
    std::cout << "\n";

    // ---- the tap. The show report is written from what actually happened on
    // the bus, not from what anybody believes happened. ----------------------
    std::int64_t refusals = 0;
    std::int64_t stale_answers = 0;
    std::vector<std::string> refusal_log;
    const loom::ObserverId tap = bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind != loom::EventKind::Refused) {
            return;
        }
        ++refusals;
        // The one this application cares about by name: a department answering
        // "standing by" to the DSM who asked, after that DSM has been replaced.
        if (e.schema_name == "StandingBy" &&
            e.refusal.reason == loom::RefusalReason::NoSuchTarget) {
            ++stale_answers;
        }
        refusal_log.push_back(std::string(loom::name_of(e.refusal.reason)) + " on " +
                              e.schema_name);
    });

    // ---- the building ------------------------------------------------------
    loom::WeaveId lx_id{};
    loom::WeaveId sound_id{};
    loom::WeaveId flys_id{};
    loom::WeaveId deck_id{};
    loom::WeaveId csm_id{};

    loom::Grant board = loom::Grant{}.allow_to_any("StandingBy", 1);
    loom::Grant floor = loom::Grant{}
                            .allow_to_any("StandingBy", 1)
                            .allow_to_role("CheckTheRail", 1, kOfficeDeck);

    Department* lx = mount_office<Department>(bus, board, kOfficeLx, lx_id, "LX", false);
    Department* sound = mount_office<Department>(bus, board, kOfficeSound, sound_id, "SOUND", false);
    Department* flys = mount_office<Department>(bus, floor, kOfficeFlys, flys_id, "FLYS", true);
    Deck* deck = mount_office<Deck>(bus, loom::Grant{}.allow_to_role("RailClear", 1, kOfficeFlys),
                                    kOfficeDeck, deck_id);
    // The CSM also needs BriefTheRelief, and it is worth saying why out loud.
    // upgrade.ask() looks like the handle speaking, and it is not: the ask is
    // sent AS THE COORDINATOR and gated against the coordinator's own grant at
    // delivery. Leave the shape out and ask() still returns ok, the delivery is
    // refused CapabilityDenied on the tap, and the transaction's one preparation
    // conversation has been spent -- a second ask refuses PreparationAlreadyAsked
    // and the relief can never be briefed. See FRICTION F-06.
    // allow_to_any because the candidate's id does not exist at mount time.
    Csm* csm = mount_office<Csm>(bus,
                                 loom::Grant{}
                                     .allow_to_role("StandDown", 1, kOfficeCaller)
                                     .allow_to_role("CarryOn", 1, kOfficeCaller)
                                     .allow_to_any("BriefTheRelief", 1),
                                 kOfficeCsm, csm_id);

    // ---- what a DSM may do, and nothing else -------------------------------
    //
    // Give standbys, say GO as the office, and answer the CSM. It cannot reach
    // the deck, it cannot address the house, and it cannot publish. Written out
    // rather than allow_any() because "what is this person allowed to do" is a
    // real question in a theatre and the answer is short.
    const auto dsm_grant = []() {
        loom::Grant g;
        for (const char* office : {kOfficeLx, kOfficeSound, kOfficeFlys}) {
            g.allow_to_role("Standby", 1, office);
            g.allow_to_role("Go", 1, office);
        }
        g.allow_to_any("TheBook", 1);
        g.allow_to_any("HaveTheBook", 1);
        g.allow_to_any("CannotTakeTheBook", 1);
        return g;
    };

    const loom::LoadResult first = kernel.load("dsm-a", caller_path, kOfficeCaller, dsm_grant());
    if (!first.ok) {
        std::cerr << "the DSM did not turn up: " << first.error << "\n";
        return 1;
    }
    const loom::WeaveId dsm_a = first.id;
    line("HOUSE", "book at the corner (weave " + std::to_string(dsm_a.value) + ")");

    // The show has to start being called by somebody. The house says beginners.
    bus.send_to_role(kOfficeCaller, loom::Message(loom::to_value(CarryOn{0})));
    bus.pump();

    // ---- the handover plan -------------------------------------------------
    constexpr std::int64_t kForgeAt = 13;    // a GO from the dark, for a cue that is standing by
    constexpr std::int64_t kCallReliefAt = 14; // the relief arrives and waits in the wings
    constexpr std::int64_t kHandoverAt = 15;

    loom::PreparedReplacement upgrade(bus, kernel);
    csm->attach(&upgrade);
    bool started = false;
    bool committed = false;
    std::string start_note;
    std::string outcome_note = "no handover was attempted";
    loom::WeaveId dsm_b{};

    std::cout << "-- the act --\n";
    for (std::int64_t at = 1; at <= kCurtainDown; ++at) {
        if (at == kForgeAt) {
            // A LABELLED CONTROL, and the sharpest one available: LX 5 is
            // genuinely standing by and its GO is genuinely coming. The only
            // thing between this message and a lighting state landing four
            // pages early is that the house is not the person calling the show.
            line("CONTROL", "a Go for LX 5, spoken by the house and not the corner", at);
            bus.send(lx_id, loom::Message(loom::to_value(Go{"LX 5", "the salt house", at})));
        }

        if (at == kCallReliefAt) {
            // The relief is loaded with EXACTLY the incumbent's grant, sealed by
            // hand, and the transaction begun around it.
            //
            // PreparedReplacement::start() would have been one call, but it
            // loads the candidate through Kernel::load_candidate(), which takes
            // no Grant -- so the successor would have arrived with allow_any()
            // while the incumbent has the six rules above. A DSM is not allowed
            // to do more on their second night. See FRICTION F-07.
            const loom::LoadResult relief =
                kernel.load("dsm-b", caller_path, /*role=*/std::string{}, dsm_grant());
            if (!relief.ok) {
                start_note = "the relief did not turn up: " + relief.error;
            } else if (!bus.seal_weave(relief.id, csm_id)) {
                start_note = "the relief could not be kept out of the world";
            } else {
                dsm_b = relief.id;
                const loom::PreparedReplacement::StartResult sr = upgrade.start_existing({
                    /*operator_id=*/csm_id,
                    /*coordinator=*/csm_id,
                    /*role=*/kOfficeCaller,
                    /*candidate=*/relief.id,
                    /*budget=*/4,
                });
                started = sr.ok;
                start_note = sr.ok ? "in the wings"
                                   : std::string("start refused: ") + loom::name_of(sr.begin_reason);
                line("HOUSE", "relief in the wings (weave " + std::to_string(relief.id.value) +
                                  "), sealed -- " + start_note, at);
            }
        }

        if (at == kHandoverAt && started) {
            BeginHandover b;
            b.at = at;
            b.edition = wrong_book ? "The Salt Harvest / revised, act two" : std::string{};
            b.carry_the_book = !drop_the_book;
            bus.send_to_role(kOfficeCsm, loom::Message(loom::to_value(b)));
        }

        bus.send_to_role(kOfficeCaller, loom::Message(loom::to_value(Moment{at})));
        bus.send_to_role(kOfficeDeck, loom::Message(loom::to_value(Moment{at})));
        bus.pump();

        // Nothing commits by itself. The house decides, and only once the
        // transaction really is Ready -- read from the Switchboard, every time.
        if (started && !committed && upgrade.state() == loom::TxnState::Ready) {
            // The number is the API's, not the domain's: this theatre has no
            // lineage that wants to be counted. See FRICTION F-08.
            const loom::TxnResult c = upgrade.commit(1);
            if (!c.ok) {
                outcome_note = std::string("commit refused: ") + loom::name_of(c.why);
            } else {
                bus.pump(); // the admission is scheduled, not done
                committed = true;
            }
        }
        if (started && upgrade.state() == loom::TxnState::Aborted) {
            if (auto out = upgrade.take_outcome()) {
                outcome_note = std::string("handover ended ") + loom::name_of(out->state) + " (" +
                               loom::name_of(out->reason) + ")";
                line("HOUSE", outcome_note, at);
                started = false;
            }
        }
        if (committed) {
            if (auto out = upgrade.take_outcome()) {
                outcome_note = std::string("handover ended ") + loom::name_of(out->state) + " (" +
                               loom::name_of(out->reason) + ")";
                line("HOUSE", outcome_note, at);
                started = false;
            }
        }
    }
    std::cout << "\n";

    bus.remove_observer(tap);

    // ---- the show report ---------------------------------------------------
    //
    // Written from what the departments actually did, never from what the corner
    // believes it called. Those two agreeing is the entire claim of this
    // application; the controls exist so that agreement is a measurement.

    const loom::WeaveId corner_now = bus.role_holder(kOfficeCaller);

    struct DeptView {
        const char* office;
        Department* d;
    };
    const std::vector<DeptView> depts = {
        {kOfficeLx, lx}, {kOfficeSound, sound}, {kOfficeFlys, flys}};

    std::vector<std::string> uncalled;
    std::vector<std::string> out_of_order;
    std::int64_t taken_total = 0;
    std::int64_t taken_by_a = 0;
    std::int64_t taken_by_b = 0;
    std::int64_t queried = 0;
    std::int64_t no_standby = 0;
    std::int64_t unauthored = 0;
    std::int64_t warned = 0;

    for (const DeptView& v : depts) {
        const DeptState& r = v.d->report();
        std::vector<std::string> expected;
        for (const Cue& c : the_book()) {
            if (std::string(c.dept) == std::string(v.office)) {
                expected.push_back(c.id);
            }
        }
        for (const std::string& e : expected) {
            if (!contains(r.took, e)) {
                uncalled.push_back(e);
            }
        }
        // In book order, with nothing extra: the department's own list, read
        // against the department's own page of the book.
        std::vector<std::string> filtered;
        for (const std::string& t : r.took) {
            if (contains(expected, t)) {
                filtered.push_back(t);
            }
        }
        std::vector<std::string> in_order;
        for (const std::string& e : expected) {
            if (contains(filtered, e)) {
                in_order.push_back(e);
            }
        }
        if (filtered != in_order) {
            out_of_order.push_back(v.office);
        }
        taken_total += static_cast<std::int64_t>(r.took.size());
        queried += r.queried;
        no_standby += r.no_standby;
        unauthored += r.unauthored;
        warned += r.warned;
        for (std::uint64_t who : v.d->took_by()) {
            if (who == dsm_a.value) {
                ++taken_by_a;
            } else if (dsm_b.valid() && who == dsm_b.value) {
                ++taken_by_b;
            }
        }
    }

    const std::int64_t book_size = static_cast<std::int64_t>(the_book().size());

    std::cout << "-- show report --\n";
    std::cout << "  cues in the book      " << book_size << "\n";
    std::cout << "  cues taken            " << taken_total << "\n";
    std::cout << "  called by dsm-a       " << taken_by_a << "\n";
    std::cout << "  called by dsm-b       " << taken_by_b << "\n";
    std::cout << "  cues never called     " << uncalled.size();
    if (!uncalled.empty()) {
        std::cout << "  [";
        for (std::size_t i = 0; i < uncalled.size(); ++i) {
            std::cout << (i ? ", " : "") << uncalled[i];
        }
        std::cout << "]";
    }
    std::cout << "\n";
    std::cout << "  departments out of order " << out_of_order.size() << "\n";
    std::cout << "  standbys accepted        " << warned << " (book is " << book_size << ")\n";
    std::cout << "  queried (already had it) " << queried << "\n";
    std::cout << "  refused (no standby)     " << no_standby << "\n";
    std::cout << "  ignored (not the corner) " << unauthored << "\n";
    std::cout << "  rail checks cleared      " << deck->report().cleared << "\n";
    std::cout << "  fly standbys answered    " << flys->spends_taken() << " taken, "
              << flys->spends_refused() << " refused\n";
    std::cout << "  answers to a gone DSM    " << stale_answers << "\n";
    std::cout << "  handover                 " << outcome_note << "\n";
    std::cout << "  book now held by         weave " << corner_now.value
              << (corner_now == dsm_a ? " (dsm-a)" : (corner_now == dsm_b ? " (dsm-b)" : ""))
              << "\n";
    std::cout << "  outgoing DSM after it     alive=" << (bus.alive(dsm_a) ? "yes" : "no")
              << " artifact=" << loom::name_of(kernel.status("dsm-a")) << "\n";
    if (dsm_b.valid()) {
        std::cout << "  relief after it           alive=" << (bus.alive(dsm_b) ? "yes" : "no")
                  << " artifact=" << loom::name_of(kernel.status("dsm-b")) << "\n";
    }
    std::cout << "  bus refusals seen        " << refusals;
    if (!refusal_log.empty()) {
        std::cout << "  [";
        for (std::size_t i = 0; i < refusal_log.size(); ++i) {
            std::cout << (i ? "; " : "") << refusal_log[i];
        }
        std::cout << "]";
    }
    std::cout << "\n\n";

    // ---- what the run is allowed to claim ----------------------------------
    int bad = 0;
    const auto check = [&](bool ok, const std::string& what) {
        std::cout << "  " << (ok ? "ok   " : "FAIL ") << what << "\n";
        if (!ok) {
            ++bad;
        }
    };

    std::cout << "-- checks --\n";

    // True of every run: the departments' own discipline, and the office rule.
    check(unauthored == 1, "a Go that was not authored by the corner was ignored");
    check(no_standby == 0, "no department took a cue it had not been stood by for");
    check(out_of_order.empty(), "every department took its cues in book order");
    check(deck->report().cleared >= 2, "the deck reported on every rail it was asked about");

    if (drop_the_book) {
        // THE POINT OF THIS RUN. The handover succeeded by every mechanical
        // measure -- the candidate was verified, it answered, it committed, the
        // role moved -- and the show is still wrong. If the report could not
        // tell, "replacement succeeded" would be a ceremony.
        check(committed, "the handover committed even though the brief carried nothing");
        check(corner_now == dsm_b, "the relief really did take the book");
        check(!uncalled.empty(), "THE REPORT NOTICED: cues were left uncalled");
        check(taken_total < book_size, "the show did not finish");
        check(queried > 0, "the departments queried the cues they had already had");
        check(taken_by_b == 0, "the relief called nothing, having been told nothing");
        check(stale_answers == 1, "an answer owed to the replaced DSM was refused NoSuchTarget");
        std::cout << "\n"
                  << (bad == 0 ? "CONTROL OK -- a handover that carried nothing was detected"
                               : "CONTROL FAILED")
                  << "\nexit=" << (bad == 0 ? 0 : 1) << "\n";
        return bad == 0 ? 0 : 1;
    }

    if (wrong_book) {
        check(!committed, "the handover did not commit");
        check(corner_now == dsm_a, "the outgoing DSM still has the book");
        check(taken_total == book_size, "the act was called in full anyway");
        check(uncalled.empty(), "no cue was left uncalled");
        check(taken_by_a == book_size, "one DSM called the whole act");
        check(csm->report().refused == 1, "the relief refused, once, for itself");
        check(warned == book_size, "no standby was re-given: the book never moved");
        check(stale_answers == 0, "no answer was left owed to anybody");
        std::cout << "\n"
                  << (bad == 0 ? "CONTROL OK -- a relief that could not take the book refused, "
                                 "and the show did not stop"
                               : "CONTROL FAILED")
                  << "\nexit=" << (bad == 0 ? 0 : 1) << "\n";
        return bad == 0 ? 0 : 1;
    }

    check(taken_total == book_size, "every cue in the book was taken");
    check(uncalled.empty(), "no cue was left uncalled");
    check(queried == 0, "no cue was offered twice");
    check(committed, "the book changed hands mid-act");
    check(corner_now == dsm_b, "the relief is calling the show");
    check(taken_by_a > 0 && taken_by_b > 0, "both DSMs called part of the act");
    check(taken_by_a + taken_by_b == book_size, "every cue was called by one of them");
    // The two standbys that were in the air at the boundary crossed in the brief
    // and were re-given by the relief -- which is the whole of what this
    // application asked continuity to carry.
    check(warned == book_size + 2, "the relief re-gave both standbys it inherited");
    // And why re-giving is load-bearing rather than good manners: the fly
    // floor's answer to the OUTGOING DSM was refused, by name, because that DSM
    // no longer exists. FLY 2 was still called, and it could only have been
    // called off an answer the relief obtained for itself.
    check(stale_answers == 1, "an answer owed to the replaced DSM was refused NoSuchTarget");
    check(contains(flys->report().took, "FLY 2"), "and the fly cue went anyway");
    std::cout << "\n" << (bad == 0 ? "SHOW OK" : "SHOW WRONG") << "\nexit=" << (bad == 0 ? 0 : 1)
              << "\n";
    return bad == 0 ? 0 : 1;
}
