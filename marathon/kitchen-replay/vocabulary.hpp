#ifndef MARATHON_KITCHEN_VOCABULARY_HPP
#define MARATHON_KITCHEN_VOCABULARY_HPP

// The job kitchen's message vocabulary — the whole contract in one file.
//
// THIS IS A REPLAY, NOT A KITCHEN 2. The domain, the three roles, the two
// conversation styles and the architectural question are Night One's, kept
// deliberately recognisable so that what CHANGED is the substrate underneath and
// not the application on top. Everything new in this file is marked REPLAY and
// exists for one reason: the substrate grew a prepared-replacement ceremony, and
// an application has to say something to use it.
//
// THE ORIGINAL QUESTION, unchanged:
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
// TWO CONVERSATION STYLES, ON PURPOSE:
//
//   * The RECEIPT is an authenticated answer, deferred across the policy round
//     trip (`Mail::defer_answer`). Trustworthy — the diner knows Loom picked the
//     recipient — and un-inheritable: an answer right belongs to the life that
//     earned it, so an expediter being replaced must close its own open
//     conversations at the one moment it is given (PrepareShutdown).
//   * The OUTCOME (Served / OrderLost) and the kitchen's internal traffic
//     (Prep / Plated) are ORDINARY role-addressed messages. They survive either
//     party being replaced — and they carry no attestation at all, so every
//     recipient owes the consumer obligation by hand and can only do half of it.
//
// ONE THING NIGHT ONE HAD IS GONE FROM THIS FILE'S NEIGHBOURHOOD: `answering.hpp`.
// Night One found `Mail::answer()` did nothing, silently, from a dynamically
// loaded weave, and carried a one-function workaround at every answering call
// site. ABI v4 grew the door. The workaround is deleted rather than kept, and
// `repro_answer_seam.cpp` MEASURES that rather than assuming it.
//
// Nothing here is a framework. Every shape is one package's word for one thing.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace marathon::kitchen {

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
// tap. An UNKNOWN SPELLING IS REFUSED, NEVER GUESSED: guessing would make the
// kitchen decide something the diner did not say.

/// `PlaceOrder::prefer` may name a station, or say "no preference".
inline constexpr const char* kPreferAny = "any";

/// `PlaceOrder::fallback` is a CLOSED menu of two words.
///   any_station — if the preferred station cannot take it, any station that can
///                 is acceptable.
///   (empty)     — the preference is REQUIRED; if it is unavailable the order is
///                 refused and nothing is cooked.
/// Refusal is deliberately not a fallback you can ask for: refusal is what
/// HAPPENS when a preference cannot be honoured.
inline constexpr const char* kFallbackAnyStation = "any_station";
inline constexpr const char* kFallbackNone = "";

/// What can actually have happened to a routing decision. A receipt states the
/// RESULT, never merely success.
inline constexpr const char* kRoutedPreferred = "routed_preferred";
inline constexpr const char* kRoutedFallback = "routed_fallback";
inline constexpr const char* kRoutedRefused = "refused";

// ---- what a diner says ------------------------------------------------------

/// One order. `order_id` is the DINER's own name for it — scoped to the diner —
/// so two diners naming an order "1" never collide.
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
struct OrderReceipt {
    std::string order_id;
    std::string resolved; ///< one of the three kRouted* spellings
    std::string station;  ///< the station actually chosen; empty when refused
    std::string reason;
    ZEN_SHAPE(OrderReceipt, 1, ZEN_FIELD(order_id), ZEN_FIELD(resolved), ZEN_FIELD(station),
              ZEN_FIELD(reason));
};

/// The dish arrived. An ordinary directed message, NOT an answer: Loom grants
/// one authenticated answer per request and the receipt already spent it.
struct Served {
    std::string order_id;
    std::string dish;
    std::string station;
    ZEN_SHAPE(Served, 1, ZEN_FIELD(order_id), ZEN_FIELD(dish), ZEN_FIELD(station));
};

/// The promise cannot be kept, and here is the honest reason. THE POINT OF THE
/// WHOLE EXPERIMENT: this message exists because Loom will not send it.
struct OrderLost {
    std::string order_id;
    std::string station; ///< where it was last seen; empty if it never left routing
    std::string reason;
    ZEN_SHAPE(OrderLost, 1, ZEN_FIELD(order_id), ZEN_FIELD(station), ZEN_FIELD(reason));
};

// ---- what the expediter asks the policy -------------------------------------

/// The routing question. The expediter supplies the ROSTER it believes in; the
/// policy supplies the DECISION. Liveness is bookkeeping; routing is domain
/// policy, and only the second one is replaceable in isolation.
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

// TWO NAMINGS, ONE PER BOUNDARY. `order_id` is the DINER's name for a thing;
// `job` is the KITCHEN's name for the same thing, minted by the expediter. A
// station never learns a diner's name for an order, and a diner never learns a
// job number. The expediter is the one place the two are tied together — which
// is exactly what it means to be the party holding the promise.

/// Cook this. Sent to the station's ROLE, so it reaches whoever holds the
/// station now — not the incarnation that happened to announce it.
struct Prep {
    std::string job;
    std::string dish;
    ZEN_SHAPE(Prep, 1, ZEN_FIELD(job), ZEN_FIELD(dish));
};

/// "I cannot cook that." The station's authenticated answer to a Prep it will
/// not take — an answer, never silence. Safe to make this an ANSWER precisely
/// because it is immediate: nothing has to survive a replacement for it.
struct PrepDeclined {
    std::string job;
    std::string station;
    std::string reason;
    ZEN_SHAPE(PrepDeclined, 1, ZEN_FIELD(job), ZEN_FIELD(station), ZEN_FIELD(reason));
};

/// The dish is up. Sent to the expediter's ROLE — an ordinary message, because
/// it must survive BOTH parties being replaced between the Prep and the plate.
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
/// discovered: an ordinary weave has no way to ask Loom who holds a role.
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
// PROGRESS IS THE ONLY THING WORTH CARRYING: intent can be re-declared by
// whoever wanted it, and binding lifecycle belongs to the asker's incarnation —
// but how far a dish has cooked is knowledge that exists nowhere else.

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

// ---- REPLAY: the prepared-replacement conversation --------------------------
//
// NEW IN THE REPLAY, and the only genuinely new vocabulary in the kitchen.
//
// The Weave Manager's graceful swap (PrepareShutdown -> Bequest -> ClaimBequest)
// is a conversation with the OUTGOING holder: it preserves work and verifies
// nothing about the successor, and there is a window in which the role has moved
// to a weave nobody asked a single question of.
//
// A prepared replacement is the mirror image: a conversation with the INCOMING
// holder, conducted while it is sealed outside the world and the incumbent is
// still serving. It verifies the successor and moves the role in one event with
// no window at all — and it says NOTHING to the incumbent, whose work is
// therefore simply gone at the moment of admission.
//
// You get exactly one of those two properties from the substrate. THIS
// VOCABULARY IS THE APPLICATION'S ATTEMPT TO HAVE BOTH: the preparation window
// is the one interval in which the incumbent is alive AND the successor is
// reachable, so the kitchen asks the incumbent to describe its work and hands
// that description to the candidate as part of the ask. Whether that composition
// is honest is what the suite measures — see REPORT.md.

/// "Describe what you are holding." An ORDINARY ask to the live incumbent, from
/// the owner. It is not a lifecycle message and changes nothing: being asked to
/// describe the work is not an event in the work's life. Any weave may ask; the
/// answer is a description, not a capability.
struct DescribeWork {
    ZEN_SHAPE(DescribeWork, 1);
};

/// The incumbent's authenticated answer to DescribeWork.
struct WorkDescribed {
    std::string station;
    std::vector<StationTicket> tickets;
    ZEN_SHAPE(WorkDescribed, 1, ZEN_FIELD(station), ZEN_FIELD(tickets));
};

/// THE PREPARATION ASK: "be the station named here, and take these over."
///
/// It carries NO transaction id, deliberately. The bus proves which conversation
/// an answer belongs to; a transaction id in a domain payload would be at best a
/// routing hint and at worst an invitation to treat it as authority.
struct PrepareStation {
    std::string station;               ///< the station this candidate must agree to BE
    std::vector<StationTicket> carry;  ///< work to take over; may be empty
    bool consult = false;              ///< if set, the candidate must ask before answering
    ZEN_SHAPE(PrepareStation, 1, ZEN_FIELD(station), ZEN_FIELD(carry), ZEN_FIELD(consult));
};

/// The candidate's own question, asked FROM INSIDE THE SEAL. A sealed candidate
/// may speak to exactly one party — its coordinator — and this is the shape that
/// uses that. It is what makes preparation a conversation rather than a form.
struct AskHousePassRate {
    std::string station;
    ZEN_SHAPE(AskHousePassRate, 1, ZEN_FIELD(station));
};

/// The owner's answer to the candidate's question.
struct HousePassRate {
    std::int64_t pass_ms = 0;
    ZEN_SHAPE(HousePassRate, 1, ZEN_FIELD(pass_ms));
};

/// "I am ready to be that station." The candidate's authenticated answer to
/// PrepareStation. The owner offers the delivery to the transaction's gate; the
/// Switchboard, never the owner, decides whether it counts.
struct StationReady {
    std::string station;
    std::int64_t adopted = 0; ///< how many carried tickets it actually took
    ZEN_SHAPE(StationReady, 1, ZEN_FIELD(station), ZEN_FIELD(adopted));
};

/// "I will not be that station, and here is why." An AUTHENTIC refusal: the
/// candidate spends the one answer authority its ask earned it to say no. The
/// transaction ends with the successor's own judgement rather than with a
/// mechanism failure, and the incumbent simply continues.
struct StationNotReady {
    std::string station;
    std::string reason;
    ZEN_SHAPE(StationNotReady, 1, ZEN_FIELD(station), ZEN_FIELD(reason));
};

// ---- the expediter's letter -------------------------------------------------
//
// WHAT CAN AND CANNOT CROSS. A ticket whose RECEIPT HAS ALREADY BEEN ISSUED
// crosses fine: what remains of it is an ordinary directed message owed to a
// diner, and an address plus a correlation is describable in words. A ticket
// still being ROUTED cannot cross at all: what remains of it is Loom's
// authenticated answer right, which belongs to the life that earned it — a
// capability, not a fact, so there is nothing to write down.

/// One promise in flight, described in the expediter's own words.
///
/// `diner` is canonical decimal Text for the house reason: a WeaveId is unsigned
/// 64-bit and the wire's Int is signed, so an Int field would silently narrow the
/// top half of the range.
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

/// How many orders the ticket book holds. A full book REFUSES visibly.
inline constexpr std::size_t kMaxOpenTickets = 32;

/// How many tickets one station holds, how many a letter carries, and how many a
/// preparation ask may hand over. ONE number for all three, so a station can
/// never be handed work it could not have held.
inline constexpr std::size_t kMaxStationTickets = 16;

// ---- the two beats the kitchen runs on --------------------------------------
//
// Both are ROLE-addressed Timer beats. A role beat belongs to the slot, so the
// successor of a replaced expediter or station inherits the pulse.

/// The correlation a kitchen weave puts on its one letter-claim per activation.
/// PUBLISHED ON PURPOSE: a correlation is a conversation LABEL, never a secret.
/// What makes the steward's answer trustworthy is Loom's attestation, which no
/// payload, correlation or role name can produce.
inline constexpr std::uint64_t kClaimCorrelation = 0xC0FFEE;

/// The expediter's sweep: the watchdog that turns silence into a word.
inline constexpr const char* kSweepTimerId = "kitchen.sweep";
inline constexpr std::int64_t kSweepMs = 20;

/// How many sweeps an order may sit before the kitchen gives up on it. Deadlines
/// are counted in SWEEPS and not in milliseconds on purpose: what the kitchen is
/// bounding is its own attention, and a sweep is the unit of attention it has.
inline constexpr std::int64_t kOrderPatienceSweeps = 40;

/// A station's pass over its tickets. The id is shared by every station because
/// it is scoped to the station's own ROLE — (role, id) is the Timer's upsert key.
inline constexpr const char* kPassTimerId = "kitchen.pass";

/// The house pass rate, which is what a candidate asks the owner for when a
/// preparation is conducted with `consult` set.
inline constexpr std::int64_t kPassMs = 20;

} // namespace marathon::kitchen

#endif // MARATHON_KITCHEN_VOCABULARY_HPP
