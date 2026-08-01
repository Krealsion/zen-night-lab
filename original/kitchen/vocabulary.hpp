#ifndef NIGHT_LAB_KITCHEN_VOCABULARY_HPP
#define NIGHT_LAB_KITCHEN_VOCABULARY_HPP

// The job kitchen's message vocabulary — the whole contract in one file.
//
// THE EXPERIMENT. A kitchen makes PROMISES. A diner places an order and is owed
// two things: a receipt (was it accepted, by whom, and why that one) and an
// outcome (the dish, or an honest word about why it will never come). The
// architectural question this package exists to answer is the second one:
//
//     Loom's law is that death ends a conversation. But death is SILENT to the
//     other party. So can a kitchen keep an honest promise for every order it
//     accepts — including orders whose cook disappears mid-dish — using only
//     existing public Loom and Zengine behaviour?
//
// The three roles are deliberately split so the answer is not smuggled into one
// weave that knows everything:
//
//     kitchen.expediter   owns the TICKET BOOK: capacity, the promise, the
//                         watchdog, and the honest final word. It knows no
//                         recipes and makes no routing decisions.
//     kitchen.policy      owns ROUTING: preference, fallback, and the reason.
//                         Pure — a function of the query. Replaceable by swap.
//     kitchen.station.*   owns COOKING: a menu, a pass rate, and progress.
//                         Replaceable, and its progress is what a letter carries.
//
// TWO CONVERSATION STYLES, ON PURPOSE — this is the finding the package is built
// to make visible rather than to hide:
//
//   * The RECEIPT is an authenticated answer, deferred across the policy round
//     trip (`Mail::defer_answer`; and see answering.hpp for why the IMMEDIATE
//     form has to be spelled the long way). It is trustworthy — the diner knows Loom
//     picked the recipient — and it CANNOT be inherited: an answer right belongs
//     to the life that earned it, so an expediter being replaced must close its
//     open conversations itself, at the one moment it is given (PrepareShutdown).
//   * The OUTCOME (Served / OrderLost) and the kitchen's internal traffic
//     (Prep / Plated) are ORDINARY role-addressed messages. They survive either
//     party being replaced — and they carry no attestation at all, so every
//     recipient owes the consumer obligation by hand and can only do half of it.
//
// Nothing here is a framework. Every shape is one package's word for one thing.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nightlab::kitchen {

// ---- the addresses that outlive their holders -------------------------------

/// The front of house. Everything a diner says goes here, by role, so a diner
/// keeps its reach across the expediter being replaced.
inline constexpr const char* kExpediterRole = "kitchen.expediter";

/// The routing brain. The expediter addresses it by role and never by id — the
/// whole point is that it can be swapped underneath.
inline constexpr const char* kPolicyRole = "kitchen.policy";

/// A station's role is derived from its name, so "which station" is data on the
/// wire and never a compiled-in list in the expediter.
inline std::string station_role(const std::string& station) {
    return "kitchen.station." + station;
}

// ---- the order menu: preference, fallback, and the four outcomes ------------
//
// Spelled as Text on the wire — self-describing for a stranger, a console or a
// tap, exactly as the Timer package spells its continuity menu. An UNKNOWN
// SPELLING IS REFUSED, NEVER GUESSED: guessing would make the kitchen decide
// something the diner did not say.

/// `PlaceOrder::prefer` may name a station, or say "no preference".
inline constexpr const char* kPreferAny = "any";

/// `PlaceOrder::fallback` is a CLOSED menu of two words.
///   any_station — if the preferred station cannot take it, any station that can
///                 is acceptable.
///   (empty)     — the preference is REQUIRED; if it is unavailable the order is
///                 refused and nothing is cooked.
/// Refusal is deliberately not a fallback you can ask for: refusal is what
/// HAPPENS when a preference cannot be honoured. (The Timer package's rule,
/// borrowed because it was right, not because a shared framework exists.)
inline constexpr const char* kFallbackAnyStation = "any_station";
inline constexpr const char* kFallbackNone = "";

/// What can actually have happened to a routing decision. A receipt states the
/// RESULT, never merely success.
inline constexpr const char* kRoutedPreferred = "routed_preferred";
inline constexpr const char* kRoutedFallback = "routed_fallback";
inline constexpr const char* kRoutedRefused = "refused";

// ---- what a diner says ------------------------------------------------------

/// One order. `order_id` is the DINER's own name for it — scoped to the diner,
/// like a Timer id is scoped to its requester — so two diners naming an order
/// "1" never collide.
///
/// The kitchen answers this exactly once, with an OrderReceipt, through Loom's
/// authenticated answer. It answers it LATER (the routing round trip happens
/// first), which is what `Mail::defer_answer` is for.
struct PlaceOrder {
    std::string order_id;
    std::string dish;
    std::string prefer;   ///< a station name, or kPreferAny
    std::string fallback; ///< kFallbackAnyStation or kFallbackNone
    ZEN_SHAPE(PlaceOrder, 1, ZEN_FIELD(order_id), ZEN_FIELD(dish), ZEN_FIELD(prefer),
              ZEN_FIELD(fallback));
};

/// The resolved receipt: what the kitchen decided, who got it, and why.
///
/// `reason` is written for a stranger — self-contained, naming what mattered.
/// A reader with no kitchen header must be able to tell an unavailable
/// preference from an unknown word from a full book.
struct OrderReceipt {
    std::string order_id;
    std::string resolved; ///< one of the three kRouted* spellings
    std::string station;  ///< the station actually chosen; empty when refused
    std::string reason;
    ZEN_SHAPE(OrderReceipt, 1, ZEN_FIELD(order_id), ZEN_FIELD(resolved), ZEN_FIELD(station),
              ZEN_FIELD(reason));
};

/// The dish arrived. An ordinary directed message, NOT an answer: Loom grants
/// one authenticated answer per request and the receipt already spent it. The
/// diner therefore owes the consumer obligation by hand — match the correlation
/// against its own outstanding order — and cannot check the other half, because
/// the expediter that serves may not be the expediter that promised.
struct Served {
    std::string order_id;
    std::string dish;
    std::string station;
    ZEN_SHAPE(Served, 1, ZEN_FIELD(order_id), ZEN_FIELD(dish), ZEN_FIELD(station));
};

/// The promise cannot be kept, and here is the honest reason. THE POINT OF THE
/// WHOLE EXPERIMENT: this message exists because Loom will not send it. A cook
/// that is unloaded mid-dish ends its conversations silently, so the only party
/// that can turn that silence into a word is the one that made the promise.
struct OrderLost {
    std::string order_id;
    std::string station; ///< where it was last seen; empty if it never left routing
    std::string reason;
    ZEN_SHAPE(OrderLost, 1, ZEN_FIELD(order_id), ZEN_FIELD(station), ZEN_FIELD(reason));
};

// ---- what the expediter asks the policy -------------------------------------

/// The routing question. The expediter supplies the ROSTER it believes in; the
/// policy supplies the DECISION. Neither half works alone, and that is the
/// split: liveness is bookkeeping (who announced themselves, who broke a
/// promise), routing is domain policy (what a preference means, what a fallback
/// is allowed to reach for).
struct RouteQuery {
    std::string order_id;
    std::string dish;
    std::string prefer;
    std::string fallback;
    std::vector<std::string> open_stations; ///< names, in announcement order
    std::vector<std::string> open_menus;    ///< parallel: each station's dishes, comma-joined
    ZEN_SHAPE(RouteQuery, 1, ZEN_FIELD(order_id), ZEN_FIELD(dish), ZEN_FIELD(prefer),
              ZEN_FIELD(fallback), ZEN_FIELD(open_stations), ZEN_FIELD(open_menus));
};

/// The routing answer, carried back as Loom's authenticated answer to the query.
/// The expediter checks `Mail::answers_ask()` before believing a word of it.
struct RouteChoice {
    std::string order_id;
    std::string station;  ///< empty when refused
    std::string resolved; ///< one of the three kRouted* spellings
    std::string reason;
    ZEN_SHAPE(RouteChoice, 1, ZEN_FIELD(order_id), ZEN_FIELD(station), ZEN_FIELD(resolved),
              ZEN_FIELD(reason));
};

// ---- what the kitchen says to a station -------------------------------------

// TWO NAMINGS, ONE PER BOUNDARY, and the split is deliberate. `order_id` is the
// DINER's name for a thing, scoped to the diner. `job` is the KITCHEN's name for
// the same thing, scoped to the kitchen and minted by the expediter. A station
// never learns a diner's name for an order (it has no business with it, and two
// diners naming an order "1" must not collide on a griddle), and a diner never
// learns a job number. The expediter is the one place the two are tied together
// — which is exactly what it means to be the party holding the promise.

/// Cook this. Sent to the station's ROLE, so it reaches whoever holds the
/// station now — not the incarnation that happened to announce it.
struct Prep {
    std::string job;
    std::string dish;
    ZEN_SHAPE(Prep, 1, ZEN_FIELD(job), ZEN_FIELD(dish));
};

/// "I cannot cook that." The station's authenticated answer to a Prep it will
/// not take — an answer, never silence. It is safe to make this an ANSWER (and
/// not an ordinary message) precisely because it is immediate: nothing has to
/// survive a replacement for it to arrive.
struct PrepDeclined {
    std::string job;
    std::string station;
    std::string reason;
    ZEN_SHAPE(PrepDeclined, 1, ZEN_FIELD(job), ZEN_FIELD(station), ZEN_FIELD(reason));
};

/// The dish is up. Sent to the expediter's ROLE — an ordinary message, because
/// it must survive BOTH parties being replaced between the Prep and the plate.
/// That is exactly why it carries no attestation, and why the expediter can only
/// do half of the consumer obligation on it (see the expediter's own notes).
struct Plated {
    std::string job;
    std::string dish;
    std::string station;
    ZEN_SHAPE(Plated, 1, ZEN_FIELD(job), ZEN_FIELD(dish), ZEN_FIELD(station));
};

/// "I am here, and this is what I cook." Published by a station on every
/// activation of a new incarnation.
///
/// SERVICE PRESENCE IS ANNOUNCED, NEVER DISCOVERED — because it cannot be
/// discovered: an ordinary weave has no way to ask Loom who holds a role, and no
/// way to learn that a weave died. So arrival is a message the arriving party
/// sends, and departure is something the kitchen can only INFER from a promise
/// going unkept.
struct StationOpen {
    std::string station;
    std::vector<std::string> dishes;
    std::int64_t pass_ms = 0; ///< how often this station makes a pass over its tickets
    ZEN_SHAPE(StationOpen, 1, ZEN_FIELD(station), ZEN_FIELD(dishes), ZEN_FIELD(pass_ms));
};

// ---- diagnostics ------------------------------------------------------------

/// Ask the kitchen how it is doing. Answered — authenticated — with a
/// `zen.Result` whose text is a one-line, stranger-readable tally.
struct KitchenStatus {
    ZEN_SHAPE(KitchenStatus, 1);
};

// ---- the station's letter ---------------------------------------------------
//
// PROGRESS IS THE ONLY THING WORTH CARRYING, and the Timer package named why:
// intent can be re-declared by whoever wanted it, and binding lifecycle belongs
// to the asker's incarnation — but how far a dish has cooked is knowledge that
// exists nowhere else. A successor that started every inherited ticket over
// would be discarding the only thing it was given.

/// One ticket, described rather than copied. `passes_left` and never a due
/// time: the successor's beat has a different origin, and pausing across
/// downtime is honest where inventing a deadline is not.
struct StationTicket {
    std::string job;
    std::string dish;
    std::int64_t passes_left = 0;
    ZEN_SHAPE(StationTicket, 1, ZEN_FIELD(job), ZEN_FIELD(dish), ZEN_FIELD(passes_left));
};

/// The whole letter: one bequest item, one shape, a bounded list. Versioned, so
/// "a letter from a different station generation is not adopted" is the gate's
/// answer and not a label the heir trusts.
struct StationHandoff {
    std::string station;
    std::vector<StationTicket> tickets;
    ZEN_SHAPE(StationHandoff, 1, ZEN_FIELD(station), ZEN_FIELD(tickets));
};

// ---- the expediter's letter -------------------------------------------------
//
// WHAT CAN AND CANNOT CROSS, and the asymmetry is the experiment's sharpest
// result. A ticket whose RECEIPT HAS ALREADY BEEN ISSUED crosses fine: what
// remains of it is an ordinary directed message owed to a diner, and an address
// plus a correlation is describable in words. A ticket still being ROUTED cannot
// cross at all: what remains of it is Loom's authenticated answer right, and an
// answer right belongs to the life that earned it — it is a capability, not a
// fact, so there is nothing to write down. The expediter therefore CLOSES those
// conversations itself, at the one moment it is given, rather than bequeathing a
// promise its heir could never keep.

/// One promise in flight, described in the expediter's own words.
///
/// `diner` is canonical decimal Text for the house reason: a WeaveId is unsigned
/// 64-bit and the wire's Int is signed, so an Int field would silently narrow the
/// top half of the range. It is what an outcome is addressed to, so lossless
/// matters here more than anywhere.
struct ExpediterTicket {
    std::string job;
    std::string order_id;
    std::string dish;
    std::string station;
    std::string diner;              ///< canonical decimal of the diner's WeaveId
    std::int64_t diner_correlation = 0;
    std::int64_t patience_left = 0;
    ZEN_SHAPE(ExpediterTicket, 1, ZEN_FIELD(job), ZEN_FIELD(order_id), ZEN_FIELD(dish),
              ZEN_FIELD(station), ZEN_FIELD(diner), ZEN_FIELD(diner_correlation),
              ZEN_FIELD(patience_left));
};

/// The book, plus the roster it was keeping. The roster crosses because it is
/// hard-won knowledge: it was assembled from announcements that will not be
/// repeated, and a successor that started blind would refuse every order until
/// each station happened to be replaced.
struct ExpediterHandoff {
    std::vector<ExpediterTicket> tickets;
    std::vector<std::string> open_stations;
    std::vector<std::string> open_menus;
    std::int64_t next_job = 1;
    ZEN_SHAPE(ExpediterHandoff, 1, ZEN_FIELD(tickets), ZEN_FIELD(open_stations),
              ZEN_FIELD(open_menus), ZEN_FIELD(next_job));
};

// ---- the published bounds ---------------------------------------------------
//
// Said out loud, both sides, because a bound discovered as a leak is a bound
// nobody chose.

/// How many orders the ticket book holds. A full book REFUSES visibly — a
/// receipt saying so — rather than dropping an order into a queue nobody bounded.
inline constexpr std::size_t kMaxOpenTickets = 32;

/// How many tickets one station holds, and how many a letter carries. One
/// number for both, so a station can never write a letter it could not have
/// held, and a successor refusing an over-bound letter is refusing something an
/// honest predecessor could not have produced. Adopted WHOLE or not at all.
inline constexpr std::size_t kMaxStationTickets = 16;

// ---- the two beats the kitchen runs on --------------------------------------
//
// Both are ROLE-addressed Timer beats. A role beat belongs to the slot, so the
// successor of a replaced expediter or station inherits the pulse instead of
// waiting for its own first ask to land. (Zengine Timer, StartRoleTimer.)

/// The correlation a kitchen weave puts on its one letter-claim per activation.
///
/// PUBLISHED ON PURPOSE, for the reason the Timer package published its own: a
/// correlation is a conversation LABEL, never a secret, and any weave watching
/// the bus can read one. What makes the steward's answer trustworthy is Loom's
/// attestation (`Mail::answers_ask()`), which no payload, correlation or role
/// name can produce. Writing the number down keeps that division honest and lets
/// the suite forge with everything the threat model actually grants.
inline constexpr std::uint64_t kClaimCorrelation = 0xC0FFEE;

/// The expediter's sweep: the watchdog that turns silence into a word.
inline constexpr const char* kSweepTimerId = "kitchen.sweep";
inline constexpr std::int64_t kSweepMs = 20;

/// How many sweeps an order may sit before the kitchen gives up on it. Deadlines
/// are counted in SWEEPS and not in milliseconds on purpose: what the kitchen is
/// bounding is its own attention, and a sweep is the unit of attention it has.
inline constexpr std::int64_t kOrderPatienceSweeps = 40;

/// A station's pass over its tickets. The id is shared by every station because
/// it is scoped to the station's own ROLE — (role, id) is the Timer's upsert
/// key, so two stations using this id never collide.
inline constexpr const char* kPassTimerId = "kitchen.pass";

} // namespace nightlab::kitchen

#endif // NIGHT_LAB_KITCHEN_VOCABULARY_HPP
