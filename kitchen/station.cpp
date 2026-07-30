// A cooking station — the thing that actually holds a job while it is being
// done, and therefore the thing whose disappearance is the experiment.
//
// One source, several libraries. `KITCHEN_STATION_FRYER` picks the menu;
// `KITCHEN_STATION_LABEL` names the generation so a successor is legible in a
// tap without changing a single rule:
//
//     kitchen-grill    / kitchen-grill-2   station "grill"
//     kitchen-fryer                        station "fryer"
//
// WHAT A STATION OWNS. A menu, a pass rate, and PROGRESS. Nothing else. It does
// not know who ordered, does not know what a preference is, cannot refuse for
// policy reasons, and has no opinion about fallbacks. It answers exactly one
// question — "can you cook this, and how far along is it?" — which is why its
// letter is short and why its successor can be a different library.
//
// TIME IS ASKED FOR, NEVER READ. The station holds no clock and never sleeps: it
// asks the Zengine Timer package for a repeating beat addressed to its own ROLE
// and makes one pass over its tickets per beat. Role-addressed on purpose — the
// beat belongs to the station slot, so a successor inherits the pulse instead of
// waiting for its own first ask to be delivered.
//
// WHY THE RAW TIMER PROTOCOL AND NOT `timer::TimedWeave`. The binding is the
// nicer way to say all of this, and it owns `on(zen.Activated)` to do it. This
// weave needs that same moment for two other things — claiming its letter and
// announcing itself — and a derived `on(const loom::Activated&, Mail&)`
// SUPPRESSES the binding's (C++ name hiding through a using-declaration), which
// would silently stop every timer being established rather than failing to
// compile. So the ceremony is written out here, once, deliberately. This is
// recorded as friction in the morning report, not as a complaint: it is a real
// boundary of a real convenience.

#include "vocabulary.hpp"

#include "answering.hpp"

#include "timer/vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace nightlab::kitchen;
namespace timer = zengine::timer;

// ---- the two stations, as data ----------------------------------------------

struct Recipe {
    const char* dish;
    std::int64_t passes;
};

#if defined(KITCHEN_STATION_FRYER)
constexpr const char* kStationName = "fryer";
constexpr Recipe kMenu[] = {{"fries", 2}, {"wings", 3}};
#else
constexpr const char* kStationName = "grill";
// The grill CAN do fries and is bad at them — which is what makes "send it to
// the specialist" a decision with a visible consequence rather than a slogan.
constexpr Recipe kMenu[] = {{"steak", 3}, {"burger", 2}, {"fries", 6}, {"brisket", 14}};
#endif

#if defined(KITCHEN_STATION_LABEL)
constexpr const char* kLabel = KITCHEN_STATION_LABEL;
#else
constexpr const char* kLabel = "a";
#endif

/// How often a station makes a pass over its tickets. Fast relative to the
/// expediter's patience (kSweepMs * kOrderPatienceSweeps), so a station that is
/// merely slow is never mistaken for one that is gone.
constexpr std::int64_t kPassMs = 20;

/// The state a station holds. `tickets` lives in the ZEN_SHAPE state rather than
/// beside it, so a RELOAD IN PLACE transplants work-in-progress through the gate
/// for free — the letter below is for the harder case, where the successor is a
/// different library and nothing can be transplanted at all.
struct StationState {
    std::vector<StationTicket> tickets;
    std::int64_t plated = 0;
    std::int64_t declined = 0;
    std::int64_t inherited = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(StationState, 1, ZEN_FIELD(tickets), ZEN_FIELD(plated), ZEN_FIELD(declined),
              ZEN_FIELD(inherited));
};

class Station
    : public loom::WeaveBase<
          Station, StationState,
          loom::Accept<Prep, loom::Activated, timer::TimerReady, timer::TimerFired,
                       loom::PrepareShutdown, loom::Bequest, loom::Refused>,
          loom::Emit<Plated, PrepDeclined, StationOpen, timer::StartRoleTimer, loom::ClaimBequest,
                     loom::Bequest>> {
public:
    // ---- arrival ------------------------------------------------------------

    /// This incarnation is live. Three things happen here and nowhere else:
    /// ask for the letter, say hello, and ask for the pulse.
    ///
    /// The cursor owns the trust rule (Loom must attest the commit for THIS
    /// incarnation) and the duplicate rule. An unattested `zen.Activated` — a
    /// perfectly-shaped message from any weave that knows the public schema —
    /// is refused here and makes nothing happen.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        claim_open_ = true;
        mail.send_to_role(loom::kManagerRole, loom::ClaimBequest{my_role()}, kClaimCorrelation);
        announce(mail);
        ask_for_the_pulse(mail);
    }

    /// The Timer service is available — possibly for the first time (this
    /// station was loaded before it), possibly again with an empty table after
    /// its own replacement. Either way the pulse is what this weave wants to
    /// exist, so ask again. It does NOT re-announce: announcing is once per
    /// activation, asking is idempotent and must stay repeatable. (The Zengine
    /// Skin learned that distinction the hard way; borrowed, not rediscovered.)
    void on(const timer::TimerReady&, loom::Mail& mail) { ask_for_the_pulse(mail); }

    // ---- the work -----------------------------------------------------------

    /// One pass over every ticket. A ticket that reaches zero is up.
    ///
    /// The consumer obligation on a role beat: an id this station never asked
    /// for is DATA, not a drive. A role can be aimed at by anyone.
    void on(const timer::TimerFired& f, loom::Mail& mail) {
        if (f.id != kPassTimerId) {
            return;
        }
        std::size_t at = 0;
        while (at < state_.tickets.size()) {
            StationTicket& t = state_.tickets[at];
            if (--t.passes_left > 0) {
                ++at;
                continue;
            }
            ++state_.plated;
            mail.send_to_role(kExpediterRole, Plated{t.job, t.dish, kStationName});
            state_.tickets.erase(state_.tickets.begin() + static_cast<std::ptrdiff_t>(at));
        }
    }

    /// Take a job, or say why not. A decline is Loom's AUTHENTICATED answer —
    /// safe to spend the one answer here precisely because it is immediate:
    /// nothing has to survive a replacement for it to arrive. Acceptance is
    /// deliberately silent; the plate is the answer, and it comes later as an
    /// ordinary message because it must survive both parties being replaced.
    void on(const Prep& p, loom::Mail& mail) {
        const std::int64_t passes = passes_for(p.dish);
        if (passes == 0) {
            decline(mail, p, "station '" + std::string(kStationName) + "' does not cook '" +
                                 p.dish + "'");
            return;
        }
        if (state_.tickets.size() >= kMaxStationTickets) {
            decline(mail, p, "station '" + std::string(kStationName) + "' is full (" +
                                 std::to_string(kMaxStationTickets) + " tickets)");
            return;
        }
        for (const StationTicket& t : state_.tickets) {
            if (t.job == p.job) {
                decline(mail, p, "job '" + p.job + "' is already on this station's pass");
                return;
            }
        }
        state_.tickets.push_back(StationTicket{p.job, p.dish, passes});
    }

    // ---- succession ---------------------------------------------------------

    /// "You are being replaced." The one moment a station gets to say anything
    /// about what it was holding.
    ///
    /// It describes progress rather than copying memory: passes remaining, never
    /// a due time, because the successor's beat has a different origin. The
    /// honest consequence, stated: replacement downtime is PAUSED. A dish three
    /// passes from done is three passes from done afterwards, however long the
    /// gap was.
    ///
    /// Nothing is fired or dropped here — being asked to describe the work is
    /// not an event in the work's life. The answer goes to the STAMPED SENDER
    /// (the steward that asked) echoing its correlation; PrepareShutdown arrives
    /// by send, so reply_to is deliberately unset.
    void on(const loom::PrepareShutdown&, loom::Mail& mail) {
        StationHandoff letter;
        letter.station = kStationName;
        for (const StationTicket& t : state_.tickets) {
            if (letter.tickets.size() >= kMaxStationTickets) {
                break; // the published bound, honoured from the writing side
            }
            letter.tickets.push_back(t);
        }
        loom::Bequest envelope;
        envelope.role = my_role();
        envelope.items.push_back(loom::bequeath_item(letter));
        mail.send(mail.sender(), envelope, mail.correlation());
    }

    /// The steward's answer to our claim: a letter.
    void on(const loom::Bequest& envelope, loom::Mail& mail) {
        if (!answers_our_claim(mail)) {
            return; // unsolicited, stale, or arriving after we had already decided
        }
        claim_open_ = false;
        adopt(envelope);
    }

    /// The steward's other answer: "there is nothing for you." A real answer,
    /// and the fastest honest way to a clean start.
    void on(const loom::Refused&, loom::Mail& mail) {
        if (!answers_our_claim(mail)) {
            return;
        }
        claim_open_ = false;
    }

private:
    static std::string my_role() { return station_role(kStationName); }

    static std::int64_t passes_for(const std::string& dish) {
        for (const Recipe& r : kMenu) {
            if (dish == r.dish) {
                return r.passes;
            }
        }
        return 0; // 0 means "not on this menu" — never a zero-pass instant dish
    }

    void announce(loom::Mail& mail) {
        StationOpen hello;
        hello.station = kStationName;
        for (const Recipe& r : kMenu) {
            hello.dishes.push_back(r.dish);
        }
        hello.pass_ms = kPassMs;
        mail.publish(hello);
    }

    void ask_for_the_pulse(loom::Mail& mail) {
        mail.send_to_role(timer::kTimerRole,
                          timer::StartRoleTimer{kPassTimerId, kPassMs, /*repeat=*/true, my_role()});
    }

    void decline(loom::Mail& mail, const Prep& p, std::string reason) {
        ++state_.declined;
        answer_across_the_seam(mail, PrepDeclined{p.job, kStationName, std::move(reason)});
    }

    /// Two checks, and neither is optional. Loom's attestation says this really
    /// is the answer to a question we asked (only the weave that received our
    /// claim can produce it); our own flag says we are still waiting for one.
    /// The correlation alone would be worthless — it is published.
    bool answers_our_claim(const loom::Mail& mail) const {
        return claim_open_ && mail.answers_ask() && mail.correlation() == kClaimCorrelation;
    }

    /// WHOLE OR NOT AT ALL. An over-bound letter, a letter for another station,
    /// or a letter that would overflow this station's own bound is refused
    /// entirely — an honest predecessor cannot produce one, so such a letter is
    /// untrusted input rather than a large truth, and adopting half of untrusted
    /// input is worse than starting fresh.
    void adopt(const loom::Bequest& envelope) {
        if (envelope.items.size() != 1) {
            return;
        }
        // The bytes are re-admitted through the real gate against THIS shape and
        // THIS version, so a letter written by a different station generation is
        // a clean nothing rather than a misread.
        const auto letter = loom::claim_item<StationHandoff>(envelope.items[0]);
        if (!letter || letter->station != kStationName) {
            return;
        }
        if (letter->tickets.size() > kMaxStationTickets ||
            state_.tickets.size() + letter->tickets.size() > kMaxStationTickets) {
            return;
        }
        for (const StationTicket& t : letter->tickets) {
            bool already = false;
            for (const StationTicket& mine : state_.tickets) {
                already = already || mine.job == t.job;
            }
            if (already) {
                continue; // a fresh Prep that beat the letter wins; it is newer
            }
            state_.tickets.push_back(t);
            ++state_.inherited;
        }
    }

    /// Per-incarnation, and never state: a fresh incarnation begins unactivated
    /// and owing nothing to whatever its predecessor left queued.
    zengine::ActivationCursor activation_;
    bool claim_open_ = false;
};

} // namespace

// The label is compiled in but unused by any rule — it exists so a tap can tell
// two generations of the same station apart. Reference it once so -Werror keeps
// it honest rather than letting it rot.
static_assert(kLabel[0] != '\0', "a station generation needs a label");

ZEN_EXPORT_WEAVE(Station)
