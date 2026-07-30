#ifndef ZENGINE_TIMER_VOCABULARY_HPP
#define ZENGINE_TIMER_VOCABULARY_HPP

// The Timer package's message vocabulary — time, message-shaped.
//
// The rule this package installs: games and packages do not read the OS clock
// and do not sleep. A weave that wants time ASKS for it (StartTimer /
// StartRoleTimer) and time ARRIVES like everything else does — as a message
// (TimerFired), delivered by the TimerService weave holding `zengine.timer`.
// The service is the one place in the running system that owns a monotonic
// clock and the one nap; everyone else just hears beats. There is no polling
// API in V1: time is a stream of events, not a state to be asked about.
//
// THE V1 CONTRACT (Vision's phase prompt) is four shapes: StartTimer v1,
// CancelTimer v1, CancelAllMyTimers v1, TimerFired v1 — spelled here exactly
// as locked; the timer suite pins the spellings by content-id.
//
// NAMED ADDITIONS (the contract proved insufficient here; recorded face-up
// per the report-back rule, never silently):
//   - StartRoleTimer v1 — the prompt's own "or to a role if the design makes
//     that cleaner" option, made a distinct shape so StartTimer stays exactly
//     as specified. A requester-addressed timer dies with its requester — and
//     a weave cannot observe another weave's death (the bus shows a sender no
//     outcomes, and no unload broadcast exists), so a heartbeat that must
//     SURVIVE its starter being swapped (the input pump, the skin pump) is
//     addressed to the ROLE: whoever holds the slot at each firing hears the
//     beat. That is also what heals a freshly swapped-in skin on a dead-quiet
//     bus: the beat belongs to the role, so the successor inherits it. (What
//     a standing timer does NOT survive is the TIMER SERVICE itself being
//     replaced — the table lives in the service. See Drive.)
//   - TimerReady v1 — the service's availability notice, published once per
//     ACCEPTED ACTIVATION. Since R2A-2 it is no longer the system's only
//     first breath (every weave gets its own `zen.Activated`); its remaining
//     job is the opposite load order and the service's own succession. See
//     TimerReady below.
//   - Drive v2 — the beat itself, and since R2A-2 a claim of ownership. A
//     weave runs only when a message arrives, so the service keeps itself
//     alive by re-sending Drive to its role at the end of every beat; inside
//     the beat it naps to the next deadline (the one sleep in the system) and
//     fires what came due. **The host does not wind the clock.** The service
//     seeds its own chain when the Loom's control door activates it, and each
//     valid beat seeds exactly its one successor. A Drive now carries the
//     activation it belongs to and its serial, so a stale, duplicated,
//     replayed, inherited or foreign one establishes nothing. See Drive below.
//   - EnsureTimer v1 / EnsureRoleTimer v1 / TimerResolution v1 — the ORDERED
//     forms and their receipt (R2B-0). The raw Start* shapes are
//     fire-and-forget and mean exactly "restart/upsert"; they can neither
//     express a preference about surviving a replacement nor report what the
//     service did. Rather than reinterpret them invisibly, the ordered forms
//     are NEW shapes that carry a preference and a fallback, and answer the
//     stamped requester with what actually happened. See the continuity block.
//   - TimerHandoffEntry v1 / TimerHandoff v1 — the package's own letter
//     (R2B-0). The Loom supplies the replacement moment and carries the
//     envelope; what crosses death is the Timer's decision, said in the
//     Timer's own vocabulary. See the handoff block.
//
// R2B-0 — THE THREE KINDS OF STATE THIS PACKAGE HOLDS, named so that a
// continuity decision can be about one of them rather than about "the timer":
//
//   INTENT            what the consumer wants (id, delay, repeat, addressing).
//                     Declared by the consumer; the consumer re-declares it on
//                     its own activation, so it never needs to survive here.
//   PROGRESS          how far a schedule has advanced — the remaining duration
//                     to the next firing. This is the only thing a Timer
//                     succession can carry that a re-ask cannot reconstruct,
//                     and it is what the letter is for.
//   BINDING LIFECYCLE waiting / spent / canceled. Local to a CONSUMER
//                     incarnation (timer/binding.hpp) and deliberately NOT
//                     carried here: it is the consumer's truth, not the
//                     service's.
//
// The phase law: **death is universal, inheritance is authored.** The Loom
// says a replacement is happening; this package decides what it can offer,
// what a successor understands, what a consumer prefers, what fallback is
// acceptable, and what actually survived — and then SAYS which of those it
// did.

// CROSSING INTO THIS CONTRACT IS REPLACEMENT, NOT RELOAD — recorded so nobody
// reads the refusal as a regression. R2A-2 changed accepted-message contracts:
// the Timer now accepts `zen.Activated` and `Drive v2`, and the official
// consumers accept `zen.Activated`. The Loom enforces EXACT accepted-contract
// equality on reload-in-place (R2A-1), so `zen.ReloadWeave` from a pre-R2A-2
// artifact to one of these refuses cleanly with "accepted schema contract
// mismatch; reload refused". That is the substrate telling the truth: a weave
// whose doors changed is a REPLACEMENT (`zen.SwapWeave`), not a reload. Within
// this contract, reloading in place works and stays live — probe B pins exactly
// that.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::timer {

/// Ask for time, delivered back to YOU (the sender): TimerFired{id} arrives
/// at the requesting weave after delay_ms, once (repeat=false) or every
/// delay_ms (repeat=true). `id` is the caller's own name for the timer
/// ("snake.tick", "my.cooldown"); it is scoped to the requester, so two
/// weaves using the same id never collide. Asking again with the same id
/// REPLACES the schedule (an upsert — which is also how a cadence changes).
/// A repeating delay below 1ms is clamped to 1ms (a 0ms repeat would be a
/// hot spin wearing a timer's clothes); a negative one-shot delay fires on
/// the next beat. Sent by a root (no weave identity), there is no one to
/// deliver to: dropped, counted on the service's `dropped` poke counter.
struct StartTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    ZEN_SHAPE(StartTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat));
};

/// Ask for time, delivered to a ROLE: TimerFired{id} goes to whoever holds
/// `role` at each firing. The beat outlives any particular holder of THAT
/// role — a swap's successor inherits it without asking — and an unheld role
/// refuses the delivery cleanly (the beat waits for the next holder; it is
/// the slot's pulse, not a weave's). It also outlives its own starter: the
/// entry stays, and only cancel rights are stranded (see CancelTimer).
/// Upsert key is (role, id), ACROSS requesters: a successor re-asking
/// replaces its predecessor's schedule instead of doubling the beat. Same
/// clamps as StartTimer.
///
/// The one succession it does NOT survive is the TimerService's own: the
/// standing-timer table is the service instance's private state, not
/// gate-carried state, so a replaced or reloaded service starts with an
/// empty table. Since R2A-2 that heals the same way for BOTH reload and
/// swap: the new incarnation is activated, publishes TimerReady, and
/// standing consumers refill the table by re-asking (see Drive).
struct StartRoleTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role;
    ZEN_SHAPE(StartRoleTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat),
              ZEN_FIELD(role));
};

/// Cancel by id: removes the sender's own (requester, id) timer, and any
/// role timer with this id that the sender itself started. V1 edge, honest:
/// a role timer whose starter is gone is cancellable only by a successor
/// first re-asking (upsert takes ownership) and then cancelling.
struct CancelTimer {
    std::string id;
    ZEN_SHAPE(CancelTimer, 1, ZEN_FIELD(id));
};

/// The convenience for a weave that is going away politely: every timer the
/// sender started — requester-addressed and role-addressed alike — dies.
///
/// TOTAL AND NEUTRAL, CHOSEN (decided 2026-07-27, and pinned as chosen in the
/// suite so the choice cannot erode into an accident). It would be easy to
/// make this shape spare role timers on the theory that a role beat was
/// "meant" to outlive its starter. It deliberately does not: succession here
/// is AUTHORED, never system-guessed, and a shape that guessed would be
/// deciding, on the weave's behalf, which of its beats were bequests. The
/// mechanism stays a plain "everything I started"; the policy lives with the
/// author, taught here:
///
///   - being REPLACED? Leave your role beats standing. They are addressed to
///     the slot, not to you, and your successor inherits them without asking
///     — cancelling them would make the swap a gap in the pulse for no
///     reason. `zen.PrepareShutdown` is exactly the signal that a replacement
///     is coming, so it is the right place to make this decision knowingly.
///   - RETIRING with no heir? Cancel. Nothing is coming to claim the beat.
///
/// The honest consequence of the retiring-weave case done wrong: an unclaimed
/// role beat is a LEAKED TIMER. It keeps firing into an unheld role, each
/// delivery a clean refusal (the same bounded floor a dead requester's timer
/// rides, with the consumer obligation covering anyone who does hold the role
/// later). Sad, benign — and NOT collectable, which is the part worth being
/// clear-eyed about: a role beat is never provably garbage, because being
/// reachable by a future holder is the whole point of it. Only a current
/// holder can declare one unwanted, by re-asking to take ownership and then
/// cancelling.
///
/// Forward: when the steward speaks about shutdown (the lifecycle session,
/// R2), its notice must distinguish REPLACEMENT from RETIREMENT — that is
/// what turns this convention from a taught default into a mechanically
/// informed one.
struct CancelAllMyTimers {
    ZEN_SHAPE(CancelAllMyTimers, 1);
};

/// A timer came due. `id` is the one the requester chose; the consumer
/// obligation applies — match it against YOUR OWN asks and ignore the rest
/// (a role can be aimed at by anyone; an unknown id is data, not a command).
struct TimerFired {
    std::string id;
    ZEN_SHAPE(TimerFired, 1, ZEN_FIELD(id));
};

/// The service's availability notice, published once per ACCEPTED ACTIVATION:
///
///   "The Timer service has accepted an activation and is available;
///    re-establish the timers you require."
///
/// It is no longer the only first breath available to a consumer, and that
/// changes what it is FOR. Since R2A-2 a consumer arranges its own time on its
/// OWN `zen.Activated`, so this shape's remaining job is the opposite load
/// order and the service's own succession:
///   - consumer loaded AFTER the Timer — its own activation makes it ask; it
///     needs nothing from here;
///   - consumer loaded BEFORE the Timer — its activation-time ask went nowhere,
///     and this is what tells it to try again. WHERE it went is worth being
///     exact about: a loaded weave's send crosses the library seam as bytes and
///     the host resolves the claimed schema against the bus registry BEFORE
///     routing, so with no service present nobody accepts StartTimer /
///     StartRoleTimer, the shape is unregistered, and the send is rejected AT
///     THE SEAM — earlier than role resolution, with no envelope and no refusal
///     event. The asker cannot tell, which is exactly why this notice exists;
///   - Timer reloaded or swapped — the new incarnation's private schedule table
///     is empty, and this is what gets standing consumers to refill it.
///
/// RE-ASKING WITH A RAW SHAPE IS CARDINALITY-IDEMPOTENT, NOT TIMING-NEUTRAL,
/// and the difference is worth the extra words. The upsert keys guarantee a
/// re-ask never produces a second entry or a doubled beat. They do NOT make it
/// free: a raw re-ask REPLACES the schedule and RE-ANCHORS it, so the next
/// firing is a full delay from now rather than from the original ask, and a
/// timer reconciled mid-cycle loses the remainder of that cycle. That was the
/// right trade for a service that may just have come back with an empty table —
/// and it was a real cost, not a no-op.
///
/// R2B-0 gave a consumer a way to say it would rather not pay it: an ORDERED
/// re-ask (EnsureTimer / EnsureRoleTimer) can prefer `preserve_remaining`, in
/// which case a matching standing schedule is kept exactly as it is and the
/// re-ask really is free — and where there is nothing to preserve, the declared
/// fallback restarts it and a TimerResolution says which of the two happened.
/// The raw shapes keep their original meaning, unchanged and unreinterpreted.
///
/// Published on the ACTIVATION, not on the first beat — a consumer should not
/// have to wait a nap to learn the service exists. A duplicate or non-newer
/// activation republishes nothing.
///
/// HISTORICAL: before R2A-2 this was published on the service's first beat and
/// was the system's only first breath, which made a weave loaded after the wind
/// permanently deaf (measured: probe D — a late `snake-clock` whose tick count
/// stayed 0 forever). That is fixed at the source now: the latecomer gets its
/// own activation.
struct TimerReady {
    ZEN_SHAPE(TimerReady, 1);
};

/// The service's own beat — and, since R2A-2, a CLAIM OF OWNERSHIP rather than
/// a bare nudge. A beat now carries the activation it belongs to and its place
/// in that chain, so the service can tell its own next breath from everything
/// else that might arrive wearing the same shape.
///
/// THE LAW THIS ENFORCES:
///
///   Every successfully activated Timer incarnation establishes exactly ONE
///   beat chain. A new activation owns a new chain; stale, duplicate,
///   replayed, inherited, or foreign Drives cannot establish another.
///
/// A Drive is acted on only when ALL of these hold — anything else is ignored
/// completely (no nap, no firing, no beat count, no re-wind):
///   - the service is activated at all (a fresh incarnation is not, which is
///     what makes a predecessor's queued Drive inert even if it arrives first);
///   - its bus-stamped sender is the service's own chain sender;
///   - `activation_sender` + `activation_sequence` name the activation the
///     service is currently living under;
///   - `serial` is exactly the one expected next.
///
/// `activation_sender` is TEXT, and deliberately: a `WeaveId` is unsigned
/// 64-bit while the wire's `Int` is signed, so an Int field would silently
/// narrow the top half of the range. Canonical decimal Text is lossless and is
/// already the house spelling for a WeaveId on the wire — the kernel's control
/// door answers a load with `zen.Result{std::to_string(id.value)}` and the
/// Weave Manager parses it back.
///
/// It stays ROLE-addressed (a loaded weave cannot address itself — see the
/// service header), but role addressing is no longer what establishes
/// ownership. The activation key, the serial, and the stamped sender are.
///
/// v2: the three fields joined the shape. `Drive v1` was empty — it carried no
/// question, no answer and no authority, which was elegant and was also exactly
/// the problem: an empty beat is indistinguishable from any other empty beat,
/// so a second one seeded a permanent second chain and a predecessor's parked
/// beat could drive a successor. The version bump is the immutable-published-
/// schema rule paid honestly: `(Drive, 1)` meant "an anonymous nudge" and still
/// does, forever.
///
/// HISTORICAL, and kept because the audit record depends on it: before R2A-2
/// the HOST sent the first Drive (the wind) and liveness was accidental —
/// `zen.ReloadWeave` happened to preserve the chain because the WeaveId
/// survived, `zen.SwapWeave` killed it because the parked beat's sender was
/// gone (CapabilityDenied at delivery, sender-death rather than role vacancy),
/// a stray second Drive seeded a permanent conserved second chain, and a
/// consumer loaded after the hello was permanently deaf. All four were measured
/// (the trust-gate audit of 2026-07-26; probes A–D). R2A-2 replaced the
/// mechanism rather than patching it: **there is no host wind**, and the
/// substrate behaviour those probes measured has not changed — the old parked
/// beat is still refused CapabilityDenied on a swap. What changed is that the
/// successor is ACTIVATED, and authors a new chain of its own.
struct Drive {
    std::string activation_sender;      ///< canonical decimal of the activating sender's WeaveId
    std::int64_t activation_sequence = 0;
    std::int64_t serial = 0;
    ZEN_SHAPE(Drive, 2, ZEN_FIELD(activation_sender), ZEN_FIELD(activation_sequence),
              ZEN_FIELD(serial));
};

// ---- continuity: the order, and the receipt (R2B-0) -------------------------
//
// THE FIRST CONFIGURATION ORDER IN ZEN, AND IT IS PACKAGE-LOCAL ON PURPOSE.
// This is not a universal negotiation framework and must not become one. It is
// four words the Timer package understands about its own state, in the one
// shape of exchange that turned out to be needed:
//
//     request  ->  available menu  ->  resolved choice  ->  receipt
//
// The general pattern is worth naming and NOT worth promoting to Loom law:
// every package meets the same lifecycle moments, and every package authors its
// own menu of survivable state and acceptable degradation. A second package
// will want a different menu; when a THIRD one wants the same menu, that is the
// trigger for a shared vocabulary, and not before.

// WHAT THIS VOCABULARY DOES NOT SAY, recorded so nobody reads it in later. An
// ABSOLUTE ALARM — "wake me at this wall-clock moment" — is a DISTINCT FUTURE
// TIMER KIND whose intent is a DEADLINE, not a delay. It must not be
// approximated silently with relative-one-shot semantics: a relative one-shot
// pauses across replacement downtime by construction (see TimerHandoffEntry),
// which is exactly the behaviour a deadline must NOT have. Every shape here is
// relative. When absolute alarms arrive they bring their own intent, their own
// clock (wall, not monotonic), and their own continuity answer.

/// What a requester would like to happen to an existing schedule.
///
/// Spelled as Text on the wire — self-describing for a stranger, a console, or
/// a tap, and the same reason `Drive.activation_sender` is Text: the wire is
/// read by people as well as by code. An unknown spelling is REFUSED, never
/// guessed at (see kResolutionRefused).
inline constexpr const char* kPreserveRemaining = "preserve_remaining";
inline constexpr const char* kRestartDelay = "restart_delay";
inline constexpr const char* kDrop = "drop";

/// The four things that can actually have happened. A receipt states the
/// RESULT, never merely success.
inline constexpr const char* kResolutionPreserved = "preserved_remaining";
inline constexpr const char* kResolutionRestarted = "restarted_delay";
inline constexpr const char* kResolutionDropped = "dropped";
inline constexpr const char* kResolutionRefused = "refused";

/// The menu, as C++ says it. Three CHOICES — refusal is deliberately not among
/// them, because refusal is what HAPPENS when a choice cannot be honoured, not
/// something a caller can ask for.
enum class Continuity { PreserveRemaining, RestartDelay, Drop };

inline const char* spelling_of(Continuity c) {
    switch (c) {
    case Continuity::PreserveRemaining:
        return kPreserveRemaining;
    case Continuity::RestartDelay:
        return kRestartDelay;
    case Continuity::Drop:
        return kDrop;
    }
    return ""; // unreachable; an unspellable choice must not become a silent one
}

/// Read a spelling off the wire. Nothing is guessed: an unknown word is an
/// empty answer, and the service refuses the order rather than picking for the
/// caller.
inline std::optional<Continuity> continuity_from(std::string_view text) {
    if (text == kPreserveRemaining) {
        return Continuity::PreserveRemaining;
    }
    if (text == kRestartDelay) {
        return Continuity::RestartDelay;
    }
    if (text == kDrop) {
        return Continuity::Drop;
    }
    return std::nullopt;
}

/// What a requester would like, and what it will settle for.
///
/// THE DEFAULT IS THE INTERESTING PART: prefer preserving the remaining time,
/// accept restarting the delay. It gives a graceful replacement real continuity
/// while letting an initial load, a hard replacement and a reload all start
/// cleanly — the three cases where there is nothing to preserve and no honest
/// way to pretend otherwise.
///
/// A `fallback` of nothing means the preference is REQUIRED: if it is
/// unavailable the order is refused and no schedule is created or changed.
struct ContinuityOrder {
    Continuity preferred = Continuity::PreserveRemaining;
    std::optional<Continuity> fallback = Continuity::RestartDelay;
};

/// The fallback as it travels: empty Text means "none acceptable".
inline std::string fallback_spelling(const ContinuityOrder& order) {
    return order.fallback ? std::string(spelling_of(*order.fallback)) : std::string();
}

/// Ask for time WITH A PREFERENCE about what should happen to a schedule that
/// already exists — delivered back to YOU (the sender), like StartTimer.
///
/// `preferred` and `fallback` carry the spellings above. An EMPTY `fallback`
/// means "no fallback is acceptable": if the preference is unavailable the
/// order is REFUSED and no schedule is created or changed. Refusal is an
/// outcome, not a menu choice — there is deliberately no "refuse" spelling to
/// prefer.
///
/// WHY A NEW SHAPE RATHER THAN A FIELD ON StartTimer. `StartTimer` v1 means
/// exactly "upsert this schedule, re-anchored from now", and a caller that
/// wants that says so. Growing it a preference field would change what a frozen
/// (name, version) means for every existing caller — the immutable-published-
/// schema rule forbids it — and quietly reinterpreting the raw protocol is the
/// specific dishonesty this phase exists to avoid. Raw and ordered are two
/// vocabularies with two different promises, and both stay public.
struct EnsureTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string preferred;
    std::string fallback; ///< empty = none acceptable; an unavailable preference refuses
    ZEN_SHAPE(EnsureTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat),
              ZEN_FIELD(preferred), ZEN_FIELD(fallback));
};

/// The role-addressed twin: the ordered form of StartRoleTimer.
struct EnsureRoleTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role;
    std::string preferred;
    std::string fallback;
    ZEN_SHAPE(EnsureRoleTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat),
              ZEN_FIELD(role), ZEN_FIELD(preferred), ZEN_FIELD(fallback));
};

/// What the Timer actually did about one order. Sent to the STAMPED REQUESTER
/// (the weave that placed the order), never published: a receipt belongs to the
/// party that asked, and a role beat's receipt still belongs to whoever ordered
/// it rather than to whoever will hear it.
///
/// `reason` is self-contained — a stranger or a console must be able to read it
/// without holding this header. It says which preference was asked for, whether
/// it was available, and which choice was taken.
///
/// It is an ORDINARY declared message: the binding consumes it, but nothing
/// hides it from the Loom, so a tap sees both halves of every order.
struct TimerResolution {
    std::string id;
    std::string resolved; ///< one of the four kResolution* spellings
    std::string reason;
    ZEN_SHAPE(TimerResolution, 1, ZEN_FIELD(id), ZEN_FIELD(resolved), ZEN_FIELD(reason));
};

// ---- the letter: what the Timer offers its successor (R2B-0) ----------------
//
// The Loom's cooperative handoff (zen/weave/lifecycle.hpp) supplies the
// replacement moment and transports the envelope; the CONTENTS are this
// package's authored decision. No object memory is serialized: an entry is
// described in this package's own words, and the successor re-admits every byte
// through the real gate (loom::claim_item) before touching a field.
//
// WHAT IS DELIBERATELY ABSENT. Callbacks — they belong to consumers, and a
// consumer's binding table is its own incarnation's truth. Spent and canceled
// one-shots — they are not active service entries and a successor that revived
// one would be resurrecting something the consumer already finished with.
// Absolute due times — see below.

/// One transferable active schedule, described rather than copied.
///
/// `remaining_ms` and NOT an absolute deadline, and this is the whole reason
/// the letter can cross a gap at all: the successor's clock is a different
/// monotonic epoch (a fresh process-relative origin, a different backend, a
/// virtual clock in a suite), so a due time from the predecessor's epoch would
/// be a number with no meaning here. Remaining duration is epoch-free.
///
/// The consequence, said plainly: replacement DOWNTIME IS PAUSED. A timer with
/// two seconds left when the predecessor was asked has two seconds left when
/// the successor restores it, whatever happened in between. That is continuity
/// of a DELAY, and it is not — and must never be described as — preservation of
/// an absolute deadline.
///
/// `requester` is canonical decimal Text for the same reason `Drive` carries
/// its sender that way: a WeaveId is unsigned 64-bit and the wire's Int is
/// signed, so an Int field would silently narrow the top half of the range.
/// Lossless matters here more than anywhere — the requester is what a firing is
/// addressed to and what a later cancellation is matched against.
struct TimerHandoffEntry {
    std::string requester; ///< canonical decimal of the requesting weave's id
    std::string id;
    std::string role; ///< empty = requester-addressed
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::int64_t remaining_ms = 0; ///< to the next firing; 0 = due or overdue
    ZEN_SHAPE(TimerHandoffEntry, 1, ZEN_FIELD(requester), ZEN_FIELD(id), ZEN_FIELD(role),
              ZEN_FIELD(delay_ms), ZEN_FIELD(repeat), ZEN_FIELD(remaining_ms));
};

/// The whole letter: one bequest item, one shape, a bounded list.
///
/// It is versioned like everything else, and the version is what makes "a wrong
/// handoff version is not adopted" a mechanical fact rather than a promise:
/// `claim_item<TimerHandoff>` re-admits the bytes against THIS schema, so a
/// letter written by a different version simply is not this shape and comes
/// back as a clean nothing. The gate answers "which version is this?", not a
/// label the successor trusts.
struct TimerHandoff {
    std::vector<TimerHandoffEntry> entries;
    ZEN_SHAPE(TimerHandoff, 1, ZEN_FIELD(entries));
};

/// The letter's bound, and it is ONE number used by both sides of the gap.
///
/// The Timer's schedule table is unbounded today (a real remaining edge, named
/// in the service header), so the handoff introduces the explicit bound it
/// needs rather than borrowing one that does not exist. The rule, both
/// directions, stated once:
///
///   - a PREDECESSOR writes at most this many entries, in table order, and
///     carries no more;
///   - a SUCCESSOR refuses a letter that claims more than this many, WHOLE. An
///     honest predecessor cannot produce one, so an over-bound letter is
///     untrusted input, not a large truth — and adopting half of an untrusted
///     letter is worse than starting fresh.
///
/// SAY THE CONSEQUENCE PLAINLY, because "adopted whole or not at all" is easy
/// to over-read: WHOLE means the whole LETTER, and the letter holds at most the
/// published bounded subset of the table — not the table. A service standing
/// more than `kMaxHandoffEntries` timers offers continuity for the first
/// `kMaxHandoffEntries` of them in table order, **and the rest are not offered
/// continuity at all**. They are not preserved, not restored, and not reported
/// as missing to anyone: their consumers simply meet an unavailable
/// preservation on their next ordered re-ask and fall back exactly as they
/// would after a hard replacement. Nothing here claims that an arbitrarily
/// large active table crosses completely, and nothing should be written that
/// implies it.
///
/// Bounded and published, like the Loom's own kMaxBequestItems: a bound
/// discovered as a leak is a bound that was never really chosen.
inline constexpr std::size_t kMaxHandoffEntries = 32;

/// How many schedule operations the service will hold while it is still
/// deciding what it inherited.
///
/// Requests can arrive in the window between "this incarnation is live" and
/// "this incarnation knows what it inherited", and letting them race the
/// restoration would make the outcome depend on queue timing. They are held and
/// replayed in arrival order AFTER restoration, so a fresh request always beats
/// inherited state for the same key. The hold is bounded, and overflow is
/// visible rather than silent: `deferred_dropped` counts it, and an ORDERED
/// request additionally gets a `refused` receipt, because it has somewhere to
/// hear one.
inline constexpr std::size_t kMaxDeferredOps = 32;

/// The correlation the service puts on its one claim per activation.
///
/// PUBLISHED ON PURPOSE, and the reason is the whole shape of R2B-1. This number
/// is not a secret and was never capable of being one: any weave can watch it on
/// the bus, and before R2B-1 knowing it was most of what an impersonator needed
/// to answer a claim the heir could not otherwise authenticate. It is a
/// CONVERSATION LABEL — it says which of this weave's asks an answer belongs to
/// — and nothing else. What makes an answer trustworthy is Loom's attestation
/// (`Mail::answers_ask()`), which no payload, correlation or role name can
/// produce. Writing the number down here keeps that division honest, and lets
/// the suite forge with everything the threat model grants.
inline constexpr std::uint64_t kClaimCorrelation = 0x71E5;

/// The bootstrap length: how many of its own beats a fresh incarnation spends
/// waiting for an answer to its claim before deciding it inherited nothing.
///
/// Two, and the number is derived rather than tuned — see the service header's
/// bootstrap block for the queue trace that produces it. It is a count of
/// QUEUE TURNS, never of milliseconds: there is no wall-clock timeout here, no
/// spin, and no permanent dependency on a steward existing at all.
inline constexpr std::int64_t kBootstrapBeats = 2;

/// The role slot the TimerService holds: the address "whoever provides
/// time", which outlives any particular implementation being swapped in.
inline constexpr const char* kTimerRole = "zengine.timer";

/// The beat cap: the longest the service will nap when nothing is due
/// sooner, which is also the worst-case lateness of a firing and the arrival
/// bound on a StartTimer being considered. 10ms — the responsiveness the old
/// host loop's nap gave the whole system, now owned by the one weave allowed
/// to sleep.
inline constexpr std::int64_t kBeatCapMs = 10;

} // namespace zengine::timer

#endif // ZENGINE_TIMER_VOCABULARY_HPP
