// The expediter — the party that HOLDS THE PROMISE, and therefore the only party
// that can keep it honest.
//
// Everything in this file exists because of one gap, and the gap is real rather
// than hypothetical:
//
//     Loom guarantees that a conversation ENDS when a participant dies. It does
//     not — and today cannot — TELL the other participant that it ended. An
//     ordinary weave has no way to ask who holds a role, no way to learn that a
//     weave was unloaded, and no way to see the fate of its own send. So a cook
//     that vanishes mid-dish produces exactly nothing: no refusal, no event, no
//     word.
//
// A kitchen that made promises and relied on the substrate to report failures
// would therefore lose orders silently. This one does not, and the whole
// mechanism is three ordinary things: a bounded ticket book, a role-addressed
// watchdog beat, and the discipline that EVERY ticket leaves the book through a
// message to the diner. Silence is never an outcome here.
//
// WHAT THIS WEAVE OWNS, AND WHAT IT REFUSES TO OWN. It owns capacity, the book,
// the watchdog, the roster of who has announced themselves, and the final word.
// It owns NO recipes and makes NO routing decision — that is kitchen.policy, and
// the split is what makes the policy swappable underneath a running kitchen. It
// also owns no clock: like every weave here it asks the Zengine Timer package
// for a beat addressed to its own ROLE, so the pulse belongs to the slot and a
// successor inherits it.
//
// THE TWO CONVERSATION STYLES, and the asymmetry that is this experiment's
// sharpest finding:
//
//   * The RECEIPT is Loom's authenticated answer, DEFERRED across the policy
//     round trip (`Mail::defer_answer`). Trustworthy, and un-inheritable: an
//     answer right belongs to the life that earned it. So a ticket still being
//     routed when this weave is replaced is CLOSED HERE, at PrepareShutdown —
//     the only moment a weave gets to speak about its own ending.
//   * The OUTCOME (Served / OrderLost) is an ordinary directed message. It
//     survives replacement of either party, and it carries no attestation at
//     all, so the diner can only do half of the consumer obligation on it.
//
// WHY THE RAW TIMER PROTOCOL AND NOT `timer::TimedWeave`: see station.cpp. The
// binding owns `on(zen.Activated)`, and this weave needs that moment for its own
// letter-claim; a derived handler would suppress the binding's silently.

#include "vocabulary.hpp"

#include "answering.hpp"

#include "timer/vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using namespace nightlab::kitchen;
namespace timer = zengine::timer;

/// The book, the roster, and the tally. All of it is an ordinary ZEN_SHAPE and
/// all of it is exposed: a kitchen that kept private books would be exactly the
/// kind of thing this substrate exists to make unnecessary. It is also what
/// makes a RELOAD IN PLACE free — same shape, state through the gate.
struct ExpediterState {
    std::vector<ExpediterTicket> tickets;
    std::vector<std::string> open_stations;
    std::vector<std::string> open_menus; ///< parallel to open_stations, comma-joined
    std::int64_t next_job = 1;
    std::int64_t placed = 0;   ///< orders accepted into the book
    std::int64_t served = 0;   ///< dishes delivered to a diner
    std::int64_t lost = 0;     ///< promises the kitchen had to break, out loud
    std::int64_t refused = 0;  ///< orders answered with a refusal receipt
    std::int64_t struck = 0;   ///< stations removed from the roster for losing a dish
    std::int64_t ignored = 0;  ///< arrivals that failed the consumer obligation
    std::int64_t held = 0;     ///< arrivals parked through a handover and replayed after it
    ZEN_EXPOSE();
    ZEN_SHAPE(ExpediterState, 1, ZEN_FIELD(tickets), ZEN_FIELD(open_stations),
              ZEN_FIELD(open_menus), ZEN_FIELD(next_job), ZEN_FIELD(placed), ZEN_FIELD(served),
              ZEN_FIELD(lost), ZEN_FIELD(refused), ZEN_FIELD(struck), ZEN_FIELD(ignored),
              ZEN_FIELD(held));
};

/// One unspent answer right, held across the policy round trip.
///
/// PER-INCARNATION, NEVER STATE — and not because it was inconvenient to
/// serialize. A `DeferredAnswer` is a CAPABILITY: the bus binds it to this
/// weave's life and this incarnation, and checks the speaker at the moment it is
/// spent. There is no representation of it that a successor could be handed,
/// which is precisely why the promises it guards are closed at PrepareShutdown
/// rather than bequeathed.
struct Promise {
    std::string job;
    loom::DeferredAnswer answer;
};

/// An order that arrived while this incarnation was still learning what it
/// inherited. It carries its own unspent answer right, so waiting costs the
/// diner nothing: the receipt is late, never absent.
struct HeldOrder {
    PlaceOrder order;
    std::string diner;
    std::uint64_t correlation = 0;
    loom::DeferredAnswer right;
};

/// A plate or a decline that arrived in the same window, kept as its own facts
/// rather than as a queued message — a weave cannot re-deliver a message, so
/// what is replayed is the decision, not the envelope.
struct HeldOutcome {
    bool declined = false;
    std::string job;
    std::string station;
    std::string dish;
    std::string reason;
    std::uint64_t correlation = 0;
};

/// How much the handover window will hold before it starts saying no. Bounded
/// and published for the same reason every other bound here is: a bound
/// discovered as a leak is a bound nobody chose. Overflow is VISIBLE — a held
/// order that cannot be held is refused with its reason, and a dropped outcome
/// is counted where a diagnostic can find it.
inline constexpr std::size_t kMaxHeldDuringHandover = 16;

class Expediter
    : public loom::WeaveBase<
          Expediter, ExpediterState,
          loom::Accept<PlaceOrder, RouteChoice, PrepDeclined, Plated, StationOpen, KitchenStatus,
                       loom::Activated, timer::TimerReady, timer::TimerFired,
                       loom::PrepareShutdown, loom::Bequest, loom::Refused>,
          loom::Emit<RouteQuery, OrderReceipt, Served, OrderLost, Prep, loom::Result,
                     timer::StartRoleTimer, loom::ClaimBequest, loom::Bequest>> {
public:
    // ---- arrival ------------------------------------------------------------

    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return; // unattested, duplicate or replayed: nothing happens twice
        }
        claim_open_ = true;
        mail.send_to_role(loom::kManagerRole, loom::ClaimBequest{kExpediterRole},
                          kClaimCorrelation);
        ask_for_the_sweep(mail);
    }

    void on(const timer::TimerReady&, loom::Mail& mail) { ask_for_the_sweep(mail); }

    // ---- the front of house -------------------------------------------------

    /// A diner places an order. Two things are decided here and nothing else:
    /// is there room, and is this order already open. Routing is the policy's.
    ///
    /// THE ANSWER IS TAKEN AWAY, NOT GIVEN. The receipt cannot be written until
    /// the policy has answered, so the immediate answer right is CONVERTED into
    /// a deferred one. If that conversion fails there is no honest way to reply
    /// at all (the delivery never carried an answer right — a root send, or a
    /// bus that is not a live delivery), so the order is not accepted: taking a
    /// job while knowing the receipt can never be written is exactly the silent
    /// failure this weave exists to prevent.
    void on(const PlaceOrder& o, loom::Mail& mail) {
        const std::string diner = std::to_string(mail.sender().value);
        if (state_.tickets.size() + held_orders_.size() >= kMaxOpenTickets) {
            answer_across_the_seam(mail, OrderReceipt{o.order_id, kRoutedRefused, "",
                                     "the kitchen's ticket book is full (" +
                                         std::to_string(kMaxOpenTickets) +
                                         " orders); nothing was started"});
            ++state_.refused;
            return;
        }
        for (const ExpediterTicket& t : state_.tickets) {
            if (t.order_id == o.order_id && t.diner == diner) {
                answer_across_the_seam(mail, OrderReceipt{o.order_id, kRoutedRefused, "",
                                         "you already have an order named '" + o.order_id +
                                             "' open in this kitchen"});
                ++state_.refused;
                return;
            }
        }
        loom::DeferredAnswer pending = mail.defer_answer();
        if (!pending.valid()) {
            ++state_.ignored;
            return; // no answer right to convert; accepting would promise silence
        }
        // THE HANDOVER WINDOW. Until this incarnation knows what it inherited it
        // must not mint a job number: the letter carries the predecessor's
        // numbering, and a fresh order taking a number the letter also uses would
        // make one of the two tickets vanish. So the order WAITS — with its
        // answer right held, which is what makes waiting honest.
        if (bootstrapping_) {
            held_orders_.push_back(HeldOrder{o, diner, mail.correlation(), std::move(pending)});
            ++state_.held;
            return;
        }
        begin_order(mail, o, diner, mail.correlation(), std::move(pending));
    }

    /// The policy has decided. THE WALL: Loom's attestation, first and always.
    /// The expediter asked a ROLE, so it cannot know which incarnation received
    /// the query and cannot pre-bind the answer's sender — `answers_ask()` is
    /// the only thing that closes that gap, and a `RouteChoice` without it is a
    /// stranger's opinion however well-shaped, however lucky its correlation.
    void on(const RouteChoice& c, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ++state_.ignored;
            return;
        }
        ExpediterTicket* t = routing_ticket_for(mail.correlation());
        if (t == nullptr) {
            ++state_.ignored;
            return;
        }
        const std::string job = t->job;
        if (c.resolved == kRoutedRefused || c.station.empty()) {
            answer_receipt(mail, job,
                           OrderReceipt{t->order_id, kRoutedRefused, "", c.reason});
            ++state_.refused;
            forget(job);
            return;
        }
        answer_receipt(mail, job,
                       OrderReceipt{t->order_id, c.resolved, c.station, c.reason});
        // Re-find: answer_receipt cannot move the book, but the ticket reference
        // is only kept across calls that provably do not touch it.
        t = ticket_for_job(job);
        if (t == nullptr) {
            return;
        }
        t->station = c.station;
        t->patience_left = kOrderPatienceSweeps; // the clock restarts at the pass
        mail.send_to_role(station_role(c.station), Prep{t->job, t->dish},
                          job_correlation(t->job));
    }

    /// The station will not take it. Authenticated (it answers the Prep we sent),
    /// so this is one of the few failures the kitchen learns about immediately
    /// rather than by waiting.
    void on(const PrepDeclined& d, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ++state_.ignored;
            return;
        }
        if (bootstrapping_) {
            hold_late(HeldOutcome{true, d.job, d.station, "", d.reason, mail.correlation()});
            return;
        }
        settle_decline(mail, d.job, d.station, d.reason, mail.correlation());
    }

    /// A dish is up.
    ///
    /// THE HONEST LIMIT OF THIS CHECK, stated where it lives rather than in a
    /// report nobody reads next to the code: a `Plated` is an ordinary message,
    /// and there is no way for this weave to ask Loom whether its sender holds
    /// the station role it claims. The standing consumer obligation is "match
    /// correlation AND bus-stamped sender"; the second half is IMPOSSIBLE for a
    /// role-addressed conversation across replacement, because the incarnation
    /// that plates is legitimately not the one that was sent the Prep. So this
    /// does everything that is available — the job must be open, it must be past
    /// routing, and the claimed station must be the one this job was actually
    /// sent to — and a weave that holds a grant for `Plated` can still finish
    /// someone else's dish. That is measured by the suite's forgery case, not
    /// waved at.
    void on(const Plated& p, loom::Mail& mail) {
        // THE RACE THIS EXPERIMENT ACTUALLY FOUND, and it is not hypothetical: a
        // dish plated while the kitchen is changing hands arrives at the HEIR
        // before the heir has been activated, let alone before it has claimed its
        // letter. Handling it then would find no such job, ignore it as noise, and
        // leave the inherited ticket to time out — a promise broken by the
        // handover itself. So it waits for the inheritance, exactly as the Timer
        // package holds schedule operations through its own bootstrap.
        if (bootstrapping_) {
            hold_late(HeldOutcome{false, p.job, p.station, p.dish, "", mail.correlation()});
            return;
        }
        settle_plate(mail, p.job, p.dish, p.station);
    }

    /// A station announced itself. Presence is announced, never discovered.
    void on(const StationOpen& s, loom::Mail&) {
        if (s.station.empty()) {
            ++state_.ignored;
            return;
        }
        std::string menu;
        for (const std::string& d : s.dishes) {
            menu += menu.empty() ? d : "," + d;
        }
        for (std::size_t i = 0; i < state_.open_stations.size(); ++i) {
            if (state_.open_stations[i] == s.station) {
                state_.open_menus[i] = menu; // a returning station re-states its menu
                return;
            }
        }
        state_.open_stations.push_back(s.station);
        state_.open_menus.push_back(menu);
    }

    /// The one diagnostic, answered authentically so an asker knows it reached
    /// the kitchen and not a lookalike.
    void on(const KitchenStatus&, loom::Mail& mail) {
        std::string open;
        for (std::size_t i = 0; i < state_.open_stations.size(); ++i) {
            open += (open.empty() ? "" : " ") + state_.open_stations[i] + "[" +
                    state_.open_menus[i] + "]";
        }
        std::int64_t routing = 0;
        std::int64_t cooking = 0;
        for (const ExpediterTicket& t : state_.tickets) {
            (t.station.empty() ? routing : cooking) += 1;
        }
        answer_across_the_seam(mail, loom::Result{
            "kitchen: placed=" + std::to_string(state_.placed) + " served=" +
            std::to_string(state_.served) + " lost=" + std::to_string(state_.lost) + " refused=" +
            std::to_string(state_.refused) + " struck=" + std::to_string(state_.struck) +
            " ignored=" + std::to_string(state_.ignored) + " open=" + std::to_string(routing) +
            "routing/" + std::to_string(cooking) + "cooking" + " stations=" +
            (open.empty() ? "(none)" : open)});
    }

    // ---- the watchdog -------------------------------------------------------

    /// One sweep. This is the whole answer to "who tells the diner their cook
    /// died": nobody does, so the kitchen counts its own attention and, when it
    /// runs out, says the true thing.
    ///
    /// Patience is counted in SWEEPS rather than milliseconds because what is
    /// being bounded is this weave's attention, and a sweep is the unit of
    /// attention it has. It also makes the bound exact under a virtual clock,
    /// which is what lets the suite prove this without sleeping.
    void on(const timer::TimerFired& f, loom::Mail& mail) {
        if (f.id != kSweepTimerId) {
            return; // a role beat can be aimed at by anyone: data, not a drive
        }
        std::size_t at = 0;
        while (at < state_.tickets.size()) {
            ExpediterTicket& t = state_.tickets[at];
            if (--t.patience_left > 0) {
                ++at;
                continue;
            }
            const std::string job = t.job;
            if (t.station.empty()) {
                // Never routed. The policy never answered — most likely no policy
                // is loaded at all, which the expediter cannot see any other way.
                answer_receipt(mail, job,
                               OrderReceipt{t.order_id, kRoutedRefused, "",
                                            "no kitchen policy answered within " +
                                                std::to_string(kOrderPatienceSweeps) +
                                                " sweeps; nothing was started"});
                ++state_.refused;
            } else {
                tell_diner(mail, t,
                           OrderLost{t.order_id, t.station,
                                     "station '" + t.station +
                                         "' took this job and never plated it within " +
                                         std::to_string(kOrderPatienceSweeps) +
                                         " sweeps; it has been struck from the roster"});
                ++state_.lost;
                strike(t.station);
            }
            forget(job); // erases at `at`, so the cursor stays put
        }
    }

    // ---- succession ---------------------------------------------------------

    /// "You are being replaced." Two duties, in this order.
    ///
    /// FIRST, CLOSE WHAT CANNOT CROSS. Every ticket still being routed is holding
    /// an unspent answer right, and that right dies with this incarnation. This
    /// is the only moment at which it can still be spent, so it is spent — with
    /// a refusal that says exactly what happened. Releasing it instead would be
    /// silent, and silence is the failure mode this whole weave exists to
    /// prevent. (`release_deferred` exists and is deliberately not used.)
    ///
    /// SECOND, WRITE DOWN WHAT CAN. A ticket whose receipt was already issued is
    /// just an address, a correlation and a station — words, not capabilities —
    /// so it crosses. The roster crosses with it, because it was assembled from
    /// announcements that will not be repeated.
    void on(const loom::PrepareShutdown&, loom::Mail& mail) {
        std::size_t at = 0;
        while (at < state_.tickets.size()) {
            if (!state_.tickets[at].station.empty()) {
                ++at;
                continue;
            }
            const ExpediterTicket t = state_.tickets[at];
            answer_receipt(mail, t.job,
                           OrderReceipt{t.order_id, kRoutedRefused, "",
                                        "the expediter was replaced while this order was still "
                                        "being routed; nothing was started"});
            ++state_.refused;
            forget(t.job);
        }

        ExpediterHandoff letter;
        letter.next_job = state_.next_job;
        letter.open_stations = state_.open_stations;
        letter.open_menus = state_.open_menus;
        for (const ExpediterTicket& t : state_.tickets) {
            if (letter.tickets.size() >= kMaxOpenTickets) {
                break; // the published bound, honoured from the writing side
            }
            letter.tickets.push_back(t);
        }
        loom::Bequest envelope;
        envelope.role = kExpediterRole;
        envelope.items.push_back(loom::bequeath_item(letter));
        mail.send(mail.sender(), envelope, mail.correlation());
    }

    void on(const loom::Bequest& envelope, loom::Mail& mail) {
        if (!answers_our_claim(mail)) {
            return;
        }
        claim_open_ = false;
        adopt(envelope);
        finish_handover(mail);
    }

    void on(const loom::Refused&, loom::Mail& mail) {
        if (!answers_our_claim(mail)) {
            return;
        }
        claim_open_ = false;
        finish_handover(mail);
    }

private:
    // ---- the handover window ------------------------------------------------
    //
    // IT OPENS AT CONSTRUCTION, NOT AT ACTIVATION, and that is deliberate: the
    // race this exists for is a message that arrives BEFORE the activation (the
    // trace that found it showed a `Plated` delivered to the heir two turns
    // ahead of its own `zen.Activated`). The Timer package made the same choice
    // for the same reason.
    //
    // THE CONSEQUENCE, SAID OUT LOUD: an expediter that is loaded and never
    // activated holds every order forever. In this lab that cannot happen — the
    // kernel's control door activates every dynamically loaded weave — but a
    // host-native mount would, and nothing here would say so. Named, not hidden.

    void hold_late(HeldOutcome outcome) {
        if (held_outcomes_.size() >= kMaxHeldDuringHandover) {
            ++state_.ignored; // visible in the diagnostic, never silent
            return;
        }
        held_outcomes_.push_back(std::move(outcome));
        ++state_.held;
    }

    /// The inheritance is settled. Replay in a deliberate order: ORDERS first,
    /// so every job number exists before anything refers to one; then outcomes,
    /// which can only ever refer to jobs that already existed.
    void finish_handover(loom::Mail& mail) {
        bootstrapping_ = false;
        std::vector<HeldOrder> orders;
        orders.swap(held_orders_);
        for (HeldOrder& h : orders) {
            begin_order(mail, h.order, h.diner, h.correlation, std::move(h.right));
        }
        std::vector<HeldOutcome> outcomes;
        outcomes.swap(held_outcomes_);
        for (const HeldOutcome& o : outcomes) {
            if (o.declined) {
                settle_decline(mail, o.job, o.station, o.reason, o.correlation);
            } else {
                settle_plate(mail, o.job, o.dish, o.station);
            }
        }
    }

    // ---- correlation: one number space, minted here -------------------------
    //
    // A job number is the kitchen's own name for a job, and it doubles as the
    // correlation on every question the kitchen asks about that job (the policy
    // query, the Prep). That is what lets an answer be matched to a ticket
    // without trusting a payload field — the same discipline the Weave Manager
    // uses for its swap chain, at a smaller scale.

    static std::uint64_t job_correlation(const std::string& job) {
        return parse_u64(job);
    }

    // ---- the three things that actually happen to a ticket -------------------
    //
    // One implementation each, because the live path and the handover replay must
    // not be able to drift apart — a promise kept differently depending on when it
    // arrived is not a promise.

    /// Mint a job, book it, and ask the policy where it goes.
    void begin_order(loom::Mail& mail, const PlaceOrder& o, const std::string& diner,
                     std::uint64_t diner_correlation, loom::DeferredAnswer right) {
        ExpediterTicket t;
        t.job = std::to_string(state_.next_job++);
        t.order_id = o.order_id;
        t.dish = o.dish;
        t.station = ""; // still routing
        t.diner = diner;
        t.diner_correlation = static_cast<std::int64_t>(diner_correlation);
        t.patience_left = kOrderPatienceSweeps;
        const std::uint64_t job_corr = job_correlation(t.job);
        state_.tickets.push_back(t);
        promises_.push_back(Promise{t.job, std::move(right)});
        ++state_.placed;

        RouteQuery q;
        q.order_id = o.order_id;
        q.dish = o.dish;
        q.prefer = o.prefer;
        q.fallback = o.fallback;
        q.open_stations = state_.open_stations;
        q.open_menus = state_.open_menus;
        mail.send_to_role(kPolicyRole, q, job_corr);
    }

    void settle_plate(loom::Mail& mail, const std::string& job, const std::string& dish,
                      const std::string& station) {
        ExpediterTicket* t = ticket_for_job(job);
        if (t == nullptr || t->station.empty() || t->station != station) {
            ++state_.ignored;
            return;
        }
        tell_diner(mail, *t, Served{t->order_id, dish, station});
        ++state_.served;
        forget(job);
    }

    void settle_decline(loom::Mail& mail, const std::string& job, const std::string& station,
                        const std::string& reason, std::uint64_t correlation) {
        ExpediterTicket* t = cooking_ticket_for(correlation);
        if (t == nullptr || t->job != job) {
            ++state_.ignored;
            return;
        }
        tell_diner(mail, *t,
                   OrderLost{t->order_id, station,
                             "station '" + station + "' declined the job: " + reason});
        ++state_.lost;
        forget(job);
    }

    static std::uint64_t parse_u64(const std::string& text) {
        std::uint64_t v = 0;
        const char* first = text.data();
        const char* last = first + text.size();
        const std::from_chars_result r = std::from_chars(first, last, v);
        if (r.ec != std::errc{} || r.ptr != last) {
            return 0;
        }
        return v;
    }

    ExpediterTicket* ticket_for_job(const std::string& job) {
        for (ExpediterTicket& t : state_.tickets) {
            if (t.job == job) {
                return &t;
            }
        }
        return nullptr;
    }

    ExpediterTicket* routing_ticket_for(std::uint64_t correlation) {
        ExpediterTicket* t = ticket_for_job(std::to_string(correlation));
        return (t != nullptr && t->station.empty()) ? t : nullptr;
    }

    ExpediterTicket* cooking_ticket_for(std::uint64_t correlation) {
        ExpediterTicket* t = ticket_for_job(std::to_string(correlation));
        return (t != nullptr && !t->station.empty()) ? t : nullptr;
    }

    /// Spend the answer right this job is holding. A job with no promise (an
    /// INHERITED ticket, whose receipt its predecessor already issued) simply has
    /// nothing to spend — that is a fact about succession, not an error.
    void answer_receipt(loom::Mail& mail, const std::string& job, const OrderReceipt& receipt) {
        for (std::size_t i = 0; i < promises_.size(); ++i) {
            if (promises_[i].job != job) {
                continue;
            }
            loom::answer_deferred(promises_[i].answer, mail, receipt);
            promises_.erase(promises_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }

    /// The outcome, addressed to the diner that asked, echoing their own
    /// correlation so they can match it to the order they placed.
    template <class T>
    void tell_diner(loom::Mail& mail, const ExpediterTicket& t, const T& outcome) {
        const loom::WeaveId diner{parse_u64(t.diner)};
        if (!diner.valid()) {
            return;
        }
        mail.send(diner, outcome, static_cast<std::uint64_t>(t.diner_correlation));
    }

    /// Drop a job from the book, and any unspent promise with it. A promise that
    /// reaches here unspent would be a conversation ended in silence, so this is
    /// the one place it can happen and it is deliberately narrow: every caller
    /// spends or answers first.
    void forget(const std::string& job) {
        for (std::size_t i = 0; i < state_.tickets.size(); ++i) {
            if (state_.tickets[i].job == job) {
                state_.tickets.erase(state_.tickets.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
        for (std::size_t i = 0; i < promises_.size(); ++i) {
            if (promises_[i].job == job) {
                promises_.erase(promises_.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

    /// A station that let a dish die is closed until it says otherwise. This is
    /// the ONLY way a station ever leaves the roster, because a broken promise is
    /// the only evidence of absence this substrate makes available.
    void strike(const std::string& station) {
        for (std::size_t i = 0; i < state_.open_stations.size(); ++i) {
            if (state_.open_stations[i] != station) {
                continue;
            }
            state_.open_stations.erase(state_.open_stations.begin() +
                                       static_cast<std::ptrdiff_t>(i));
            state_.open_menus.erase(state_.open_menus.begin() + static_cast<std::ptrdiff_t>(i));
            ++state_.struck;
            return;
        }
    }

    void ask_for_the_sweep(loom::Mail& mail) {
        mail.send_to_role(timer::kTimerRole, timer::StartRoleTimer{kSweepTimerId, kSweepMs,
                                                                   /*repeat=*/true,
                                                                   kExpediterRole});
    }

    bool answers_our_claim(const loom::Mail& mail) const {
        return claim_open_ && mail.answers_ask() && mail.correlation() == kClaimCorrelation;
    }

    /// WHOLE OR NOT AT ALL, and re-admitted through the real gate.
    void adopt(const loom::Bequest& envelope) {
        if (envelope.items.size() != 1) {
            return;
        }
        const auto letter = loom::claim_item<ExpediterHandoff>(envelope.items[0]);
        if (!letter || letter->open_stations.size() != letter->open_menus.size() ||
            letter->tickets.size() > kMaxOpenTickets ||
            state_.tickets.size() + letter->tickets.size() > kMaxOpenTickets) {
            return;
        }
        for (const ExpediterTicket& t : letter->tickets) {
            if (ticket_for_job(t.job) == nullptr) {
                state_.tickets.push_back(t);
            }
        }
        // MERGED, NOT OVERWRITTEN. A station that announced itself to this
        // incarnation while the letter was in flight said something NEWER than
        // anything the letter can contain, and a successor that overwrote it
        // would be forgetting a live fact in favour of a remembered one.
        for (std::size_t i = 0; i < letter->open_stations.size(); ++i) {
            bool known = false;
            for (const std::string& s : state_.open_stations) {
                known = known || s == letter->open_stations[i];
            }
            if (!known) {
                state_.open_stations.push_back(letter->open_stations[i]);
                state_.open_menus.push_back(letter->open_menus[i]);
            }
        }
        if (letter->next_job > state_.next_job) {
            state_.next_job = letter->next_job;
        }
    }

    zengine::ActivationCursor activation_;
    bool claim_open_ = false;

    /// True from CONSTRUCTION until the letter question is answered one way or
    /// the other. Per-incarnation and never state: a successor begins not
    /// knowing, however much its predecessor knew.
    bool bootstrapping_ = true;
    std::vector<HeldOrder> held_orders_;
    std::vector<HeldOutcome> held_outcomes_;

    /// Unspent answer rights, per incarnation. See Promise above for why this is
    /// not — and cannot be — part of the state.
    std::vector<Promise> promises_;
};

} // namespace

ZEN_EXPORT_WEAVE(Expediter)
