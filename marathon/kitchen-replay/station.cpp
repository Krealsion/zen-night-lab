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
// policy reasons, and has no opinion about fallbacks.
//
// TIME IS ASKED FOR, NEVER READ. The station holds no clock and never sleeps: it
// asks the Zengine Timer package for a repeating beat addressed to its own ROLE
// and makes one pass over its tickets per beat.
//
// WHY THE RAW TIMER PROTOCOL AND NOT `timer::TimedWeave`. The binding is the
// nicer way to say all of this, and it owns `on(zen.Activated)` to do it. This
// weave needs that same moment for two other things — claiming its letter and
// announcing itself — and a derived `on(const loom::Activated&, Mail&)`
// SUPPRESSES the binding's (C++ name hiding through a using-declaration), which
// would silently stop every timer being established rather than failing to
// compile. Night One recorded this as friction; the replay re-tests it in the
// suite rather than assuming it is still true.
//
// ---- REPLAY: THIS STATION CAN BE REPLACED TWO DIFFERENT WAYS ----------------
//
// Night One had one ceremony. There are now two, and this file is where both
// land, which makes it the honest place to say how they differ:
//
//   GRACEFUL SWAP (Weave Manager)      PREPARED REPLACEMENT (PreparedReplacement)
//   a conversation with the OUTGOING   a conversation with the INCOMING holder
//   holder                             (sealed, outside the world)
//   PrepareShutdown -> Bequest         PrepareStation -> StationReady/NotReady
//   -> ClaimBequest
//   preserves work                     verifies the successor
//   verifies nothing about the heir    says NOTHING to the incumbent
//   there is a window in which the     no window at all: admission and
//   role holder was never asked        activation are one event
//   anything
//
// Neither ceremony gives both properties. What this file attempts — and what the
// suite measures rather than asserts — is whether an APPLICATION can compose
// them: the preparation window is the one interval in which the incumbent is
// alive AND the successor is reachable, so the owner asks the incumbent to
// DESCRIBE its work (an ordinary question that changes nothing) and hands that
// description to the candidate inside the preparation ask.

#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace marathon::kitchen;
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

/// The state a station holds. `tickets` lives in the ZEN_SHAPE state rather than
/// beside it, so a RELOAD IN PLACE transplants work-in-progress through the gate
/// for free — the letter and the preparation ask are for the harder case, where
/// the successor is a different library and nothing can be transplanted at all.
struct StationState {
    std::vector<StationTicket> tickets;
    std::int64_t plated = 0;
    std::int64_t declined = 0;
    std::int64_t inherited = 0;  ///< tickets adopted from a letter OR a preparation ask
    std::int64_t described = 0;  ///< times this station described its work to an owner
    std::int64_t prepared = 0;   ///< preparation asks answered Ready
    std::int64_t refused_prep = 0; ///< preparation asks answered NotReady
    ZEN_EXPOSE();
    ZEN_SHAPE(StationState, 1, ZEN_FIELD(tickets), ZEN_FIELD(plated), ZEN_FIELD(declined),
              ZEN_FIELD(inherited), ZEN_FIELD(described), ZEN_FIELD(prepared),
              ZEN_FIELD(refused_prep));
};

class Station
    : public loom::WeaveBase<
          Station, StationState,
          loom::Accept<Prep, loom::Activated, timer::TimerReady, timer::TimerFired,
                       loom::PrepareShutdown, loom::Bequest, loom::Refused, DescribeWork,
                       PrepareStation, HousePassRate>,
          loom::Emit<Plated, PrepDeclined, StationOpen, timer::StartRoleTimer, loom::ClaimBequest,
                     loom::Bequest, WorkDescribed, StationReady, StationNotReady,
                     AskHousePassRate>> {
public:
    // ---- arrival ------------------------------------------------------------

    /// This incarnation is live. Three things happen here and nowhere else:
    /// ask for the letter, say hello, and ask for the pulse.
    ///
    /// The cursor owns the trust rule (Loom must attest the commit for THIS
    /// incarnation) and the duplicate rule. An unattested `zen.Activated` — a
    /// perfectly-shaped message from any weave that knows the public schema —
    /// is refused here and makes nothing happen.
    ///
    /// REPLAY: this handler is reached by BOTH ceremonies. After a graceful swap
    /// the ClaimBequest below finds a letter; after a prepared replacement it
    /// finds nothing (there is no steward in that story) and the steward's
    /// `Refused` is the honest, fastest answer. Either way the station announces
    /// and asks for its pulse, because those are true of any live incarnation.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        claim_open_ = true;
        mail.send_to_role(loom::kManagerRole, loom::ClaimBequest{my_role()}, kClaimCorrelation);
        announce(mail);
        ask_for_the_pulse(mail);
    }

    /// The Timer service is available — possibly for the first time, possibly
    /// again with an empty table after its own replacement. Either way the pulse
    /// is what this weave wants to exist, so ask again. It does NOT re-announce:
    /// announcing is once per activation, asking is idempotent.
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

    // ---- REPLAY: the prepared-replacement conversation ----------------------

    /// "Describe what you are holding." Asked of the LIVE incumbent by the owner,
    /// while the incumbent is still the service.
    ///
    /// It is an ordinary question and it changes NOTHING — no ticket is fired, no
    /// pass is skipped, nothing is marked as leaving. That matters: unlike
    /// `PrepareShutdown`, being asked to describe the work is not an event in the
    /// work's life, so an owner that asks and then decides not to replace anyone
    /// has done nothing at all. The station keeps cooking either way, and if the
    /// replacement does go ahead the tickets it plates in the meantime are simply
    /// plated twice-described and once-delivered (the expediter forgets a job on
    /// the first plate, so the second finds nothing).
    void on(const DescribeWork&, loom::Mail& mail) {
        ++state_.described;
        WorkDescribed described;
        described.station = kStationName;
        for (const StationTicket& t : state_.tickets) {
            if (described.tickets.size() >= kMaxStationTickets) {
                break;
            }
            described.tickets.push_back(t);
        }
        (void)mail.answer(described);
    }

    /// THE PREPARATION ASK, heard from inside the seal.
    ///
    /// This weave is outside the world when this arrives: it holds no role, its
    /// beat does not reach it, and the only party it may speak to is the
    /// coordinator that sealed it. What it is being asked is a real question with
    /// a real right to say no, and it says no to three things:
    ///
    ///   * being a station it is not (the fryer will not become the grill);
    ///   * more carried work than any honest predecessor could have held;
    ///   * work it cannot cook, which would be accepting a promise it must break.
    ///
    /// A refusal here is AUTHENTIC — it spends the one answer authority the ask
    /// earned this candidate — so the transaction ends with the successor's own
    /// verdict rather than with a mechanism failure, and the incumbent continues
    /// serving without ever learning that any of this happened.
    void on(const PrepareStation& p, loom::Mail& mail) {
        if (p.station != kStationName) {
            refuse_prep(mail, "this artifact is station '" + std::string(kStationName) +
                                  "' and was asked to become '" + p.station + "'");
            return;
        }
        if (p.carry.size() > kMaxStationTickets) {
            refuse_prep(mail, "a preparation may hand over at most " +
                                  std::to_string(kMaxStationTickets) + " tickets, and this one "
                                  "carries " + std::to_string(p.carry.size()));
            return;
        }
        for (const StationTicket& t : p.carry) {
            if (passes_for(t.dish) == 0) {
                refuse_prep(mail, "station '" + std::string(kStationName) + "' does not cook '" +
                                      t.dish + "', so it cannot take over job '" + t.job + "'");
                return;
            }
        }
        if (p.consult) {
            // THE CANDIDATE ASKS BACK. A sealed candidate may speak to exactly one
            // party — the coordinator that sealed it — and this is what that door
            // is for: preparation is a conversation, not a form. The readiness
            // answer is TAKEN AWAY and held until the reply lands.
            pending_ = mail.defer_answer();
            if (!pending_.valid()) {
                refuse_prep(mail, "this preparation carried no answer authority to hold");
                return;
            }
            carried_ = p.carry;
            mail.send(mail.sender(), AskHousePassRate{kStationName});
            return;
        }
        become_ready(mail, p.carry);
    }

    /// The owner's answer to the candidate's own question. Only now does the
    /// readiness answer get spent.
    void on(const HousePassRate& r, loom::Mail& mail) {
        if (!mail.answers_ask() || !pending_.valid()) {
            return; // not an answer to our ask, or we are not holding a preparation
        }
        if (r.pass_ms <= 0) {
            std::vector<StationTicket> nothing;
            carried_.swap(nothing);
            ++state_.refused_prep;
            (void)loom::answer_deferred(
                pending_, mail,
                StationNotReady{kStationName, "the house pass rate came back as " +
                                                  std::to_string(r.pass_ms) +
                                                  "ms, which is not a rate anything can cook at"});
            pending_ = loom::DeferredAnswer{};
            return;
        }
        std::vector<StationTicket> carry;
        carry.swap(carried_);
        const std::int64_t adopted = adopt_tickets(carry);
        ++state_.prepared;
        (void)loom::answer_deferred(pending_, mail, StationReady{kStationName, adopted});
        pending_ = loom::DeferredAnswer{};
    }

    // ---- succession, the other way ------------------------------------------

    /// "You are being replaced." The one moment a station gets to say anything
    /// about what it was holding — and it is reached by the Weave Manager's
    /// graceful swap ONLY. A prepared replacement never sends it.
    ///
    /// It describes progress rather than copying memory: passes remaining, never
    /// a due time, because the successor's beat has a different origin. The
    /// honest consequence, stated: replacement downtime is PAUSED. A dish three
    /// passes from done is three passes from done afterwards, however long the
    /// gap was.
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
        (void)mail.answer(PrepDeclined{p.job, kStationName, std::move(reason)});
    }

    void refuse_prep(loom::Mail& mail, std::string reason) {
        ++state_.refused_prep;
        (void)mail.answer(StationNotReady{kStationName, std::move(reason)});
    }

    void become_ready(loom::Mail& mail, const std::vector<StationTicket>& carry) {
        const std::int64_t adopted = adopt_tickets(carry);
        ++state_.prepared;
        (void)mail.answer(StationReady{kStationName, adopted});
    }

    /// ONE adoption rule, used by BOTH ceremonies. A ticket that arrives twice —
    /// once in a letter and once as a fresh Prep — is the newer one's, and the
    /// bound is the station's own published bound whichever door the work came
    /// through. Having two adoption rules would be exactly how the two ceremonies
    /// start to mean different things.
    std::int64_t adopt_tickets(const std::vector<StationTicket>& incoming) {
        std::int64_t adopted = 0;
        for (const StationTicket& t : incoming) {
            if (state_.tickets.size() >= kMaxStationTickets) {
                break;
            }
            bool already = false;
            for (const StationTicket& mine : state_.tickets) {
                already = already || mine.job == t.job;
            }
            if (already) {
                continue; // a fresh Prep that beat the handover wins; it is newer
            }
            state_.tickets.push_back(t);
            ++state_.inherited;
            ++adopted;
        }
        return adopted;
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
    /// untrusted input rather than a large truth.
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
        (void)adopt_tickets(letter->tickets);
    }

    /// Per-incarnation, and never state: a fresh incarnation begins unactivated
    /// and owing nothing to whatever its predecessor left queued.
    zengine::ActivationCursor activation_;
    bool claim_open_ = false;

    /// The readiness answer, taken away while the candidate asks its own
    /// question. A capability, so per-incarnation and never state — which is
    /// trivially true here: a candidate that never becomes ready is discarded.
    loom::DeferredAnswer pending_;
    std::vector<StationTicket> carried_;
};

} // namespace

// The label is compiled in but unused by any rule — it exists so a tap can tell
// two generations of the same station apart. Reference it once so -Werror keeps
// it honest rather than letting it rot.
static_assert(kLabel[0] != '\0', "a station generation needs a label");

ZEN_EXPORT_WEAVE(Station)
