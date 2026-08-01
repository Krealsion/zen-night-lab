#ifndef ZENGINE_ACTIVATION_ACTIVATION_HPP
#define ZENGINE_ACTIVATION_ACTIVATION_HPP

// The activation cursor — Zengine's shared reading of `zen.Activated`.
//
// The Loom's control door tells a freshly committed dynamic incarnation, once,
// that it is live (weave/lifecycle.hpp). That fact is narrow on purpose: it
// says a new incarnation committed at this address and NOTHING else — not
// healthy, not ready, not "start a loop". What a weave DOES with it is the
// weave's own business, and in Zengine four weaves want the same thing from it:
// "this is my first breath; arrange the time I need."
//
// This is the small amount of bookkeeping that answer requires, written once.
// It is not a framework and not a Loom abstraction — it is a two-field cursor
// plus the comparison rule, kept here so Timer, Input, Skin and SnakeClock read
// an activation the same way instead of four subtly different ways.
//
// WHAT IT NOW IS, and what it still is not (R2B-1). It answers TWO questions,
// in this order, and keeping them apart is the whole of the design:
//
//   PROVENANCE  "did Loom itself authorize a lifecycle commit for me?"
//               Answered by Loom, not by this file: `mail.lifecycle_attested()`
//               is a delivery fact the bus sets and no payload can carry. The
//               attested sequence is compared against the payload's own, so an
//               attestation minted for one activation cannot authenticate
//               another.
//   LINEAGE     "have I already acted on this one?"
//               Answered here, as before: positive, newer, per-sender.
//
// The trust anchor that was missing is now present, and the sentence it replaces
// is worth quoting so nobody re-derives it: activation identity is NO LONGER
// inferred from an arbitrary stamped sender. Before R2B-1 any weave granted the
// public shape could manufacture a first breath for someone else's incarnation
// and a consumer had no way to tell; a different sender was simply read as a new
// lineage. Now an unattested activation is not a lineage at all — it is an
// ordinary message wearing a lifecycle costume, and is ignored entirely.
//
// The sender half is still load-bearing, and its meaning is now sharper: among
// ATTESTED activations, a different sender is a different authorized operator's
// lineage. A bare sequence remains a small integer, and treating one as an
// identity would still make a replayed number indistinguishable from a real
// succession.
//
// WHAT IS STILL NOT CLAIMED, said plainly: this proves Loom authorized the
// commit, not that any particular host wiring is the "right" one — the host
// decides who holds the lifecycle authority, and a host that hands it to two
// operators has two lineages by its own choice. Nor does it reach across a
// process boundary: an out-of-process weave receives no attestation at all and
// therefore accepts no activation (Loom's weave_host_main says so at the seam).

#include <zen/switchboard/message.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <string>

namespace zengine {

/// Tracks which activation a weave is currently living under.
///
/// A freshly constructed cursor is UNACTIVATED, and that is the point: a new
/// incarnation begins owing nothing to anything its predecessor queued. Whatever
/// was in flight for the previous incarnation cannot make this one act, even if
/// it arrives first.
class ActivationCursor {
public:
    /// Offer an arriving activation; true iff it becomes the current one — i.e.
    /// iff the weave should do its once-per-activation work now.
    ///
    /// ONE CALL OWNS BOTH HALVES, deliberately: a consumer should not have to
    /// rediscover the trust rule, and four packages each writing their own
    /// version of it is four chances to write it slightly wrong.
    ///
    ///   1. PROVENANCE. Loom must attest a lifecycle commit for THIS incarnation,
    ///      and the sequence it attested must be the one the payload states. An
    ///      unattested `zen.Activated` — however well-formed, however plausible
    ///      its sequence, whoever sent it — is refused here and goes no further.
    ///   2. LINEAGE. Then the old rules, unchanged: the sequence must be
    ///      positive, and either from a DIFFERENT (attested) sender — a new
    ///      operator lineage replacing the current one — or NEWER than the last
    ///      seen from the current sender. A same-sender, non-newer sequence is a
    ///      duplicate or a replay: ignored entirely, so re-delivery cannot make
    ///      anything happen twice.
    ///
    /// It takes the whole `Mail` rather than a sender and a number because the
    /// deciding facts are DELIVERY facts. A signature of loose integers would
    /// invite a caller to pass values it read off a payload, which is precisely
    /// the mistake this phase exists to make unrepresentable.
    bool accept(const loom::Mail& mail, const loom::Activated& activated) {
        if (!mail.lifecycle_attested()) {
            return false; // not Loom's word: an ordinary message wearing a costume
        }
        if (mail.attested_sequence() != activated.sequence) {
            return false; // a proof for one activation is not a proof for another
        }
        const loom::WeaveId sender = mail.sender();
        const std::int64_t sequence = activated.sequence;
        if (!sender.valid() || sequence <= 0) {
            return false;
        }
        if (activated_ && sender == sender_ && sequence <= sequence_) {
            return false;
        }
        sender_ = sender;
        sequence_ = sequence;
        activated_ = true;
        return true;
    }

    bool activated() const { return activated_; }
    loom::WeaveId sender() const { return sender_; }
    std::int64_t sequence() const { return sequence_; }

    /// The sender half, as it travels on a wire.
    ///
    /// A `WeaveId` is an unsigned 64-bit value and Zen's wire `Int` is signed,
    /// so putting one in an Int field would narrow the top half of the range
    /// silently. Canonical decimal Text is lossless, and it is already the
    /// house spelling for a WeaveId on the wire: the kernel's control door
    /// answers a load with `zen.Result{std::to_string(id.value)}` and the Weave
    /// Manager parses it back. Same representation, same reasons.
    std::string sender_text() const { return std::to_string(sender_.value); }

    /// Does a carried activation key name the activation this cursor is living
    /// under? Both halves must match — a matching sequence under a different
    /// sender is a different lineage's beat, not ours.
    bool matches(const std::string& sender_text_, std::int64_t sequence) const {
        return activated_ && sequence == sequence_ && sender_text_ == sender_text();
    }

private:
    loom::WeaveId sender_{};
    std::int64_t sequence_ = 0;
    bool activated_ = false;
};

} // namespace zengine

#endif // ZENGINE_ACTIVATION_ACTIVATION_HPP
