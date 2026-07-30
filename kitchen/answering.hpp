#ifndef NIGHT_LAB_KITCHEN_ANSWERING_HPP
#define NIGHT_LAB_KITCHEN_ANSWERING_HPP

// ONE FUNCTION, AND IT IS A WORKAROUND. Read the whole comment before using it.
//
// THE FINDING. `loom::Mail::answer()` — Loom's authenticated one-shot answer — is
// NATIVE-ONLY. It does not cross the `.so` seam, and it fails SILENTLY when it
// is called from a dynamically loaded weave.
//
// The mechanism, exactly:
//   * `loom::Bus::answer()` has a base implementation that returns an invalid
//     Ticket and does nothing. Its own comment says why, and the reason is
//     sound: "a Bus that is not a live delivery (a library-side shim, a future
//     mailbox) truthfully answers nothing rather than pretending."
//   * `loom::detail::HostApiBus` (zen/kernel/export.hpp) is the Bus a loaded
//     weave is handed. It overrides send, publish, send_to_role,
//     make_deferred_answer, spend_deferred and release_deferred. It does NOT
//     override `answer` — and the C ABI (zen/kernel/abi.h, v3) has no door for
//     it: there is `defer_answer` and `answer_deferred`, and no `answer`.
//   * So a loaded weave calling `mail.answer(x)` inherits the base: nothing is
//     queued, no refusal event is raised, and the caller's only signal is an
//     invalid Ticket that most call sites — including every one in this lab's
//     first draft, and the shape of the call in `zen/weave/weave.hpp`'s own
//     documentation — do not look at.
//
// HOW IT PRESENTED, because the shape of the symptom is the point: the kitchen's
// routing policy received every `RouteQuery` and answered every one of them, and
// the expediter heard nothing at all, forever. The bus tap showed the query
// delivered and NO corresponding answer and NO refusal — the exact signature of
// a message that was never enqueued. It took a host-side tap to see it, because
// no participant can.
//
// THE WORKAROUND, and it is entirely public behaviour: take the answer right
// away with `defer_answer()` and spend it immediately. `defer_answer` and
// `spend_deferred` both cross the ABI, and `spend_deferred` returns a Ticket
// whose validity is meaningful across the seam — so this path is not merely
// available, it is the only one that can REPORT whether the answer went out.
// Loom still picks the recipient and the correlation; the authority is still the
// delivery's; the one-shot is still one shot. Nothing is widened. The only cost
// is a bus-side deferred-answer slot for the duration of one handler, and a
// function that should not have to exist.
//
// It is deliberately NOT named `answer` and deliberately NOT a member of
// anything: it should read as scaffolding at every call site, so that when the
// ABI grows an `answer` door this file is deleted rather than kept.

#include <zen/switchboard/message.hpp>
#include <zen/weave.hpp>

namespace nightlab::kitchen {

/// Answer the message being handled, from a weave that may be dynamically
/// loaded. Returns false when this delivery carried no answer right to spend —
/// which is a real condition (a root send has nobody to answer) and must not be
/// mistaken for the seam gap above.
template <class T>
bool answer_across_the_seam(loom::Mail& mail, const T& msg) {
    loom::DeferredAnswer right = mail.defer_answer();
    if (!right.valid()) {
        return false;
    }
    return loom::answer_deferred(right, mail, msg).valid();
}

} // namespace nightlab::kitchen

#endif // NIGHT_LAB_KITCHEN_ANSWERING_HPP
