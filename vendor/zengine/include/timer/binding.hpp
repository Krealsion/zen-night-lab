#ifndef ZENGINE_TIMER_BINDING_HPP
#define ZENGINE_TIMER_BINDING_HPP

// The timer binding — one Zengine word for a sentence everybody was writing.
//
//   A weave DECLARES the time behaviour it wants; the binding owns the Timer
//   protocol and the lifecycle reconciliation required to maintain it.
//
// R2A-2 made the raw Timer conversation correct, and in doing so made its
// ceremony visible. Every consumer that wanted a heartbeat had to write the
// same seven steps: accept `zen.Activated`, deduplicate it, accept
// `TimerReady`, send a StartTimer/StartRoleTimer, accept `TimerFired`, filter
// by id, and re-ask whenever the service appeared or came back. That machinery
// is real and none of it is a domain decision — so it belongs in package
// vocabulary, written once, not in every author's file.
//
// WHAT THIS IS NOT. It is not a scheduler, not an event framework, and not a
// second interpretation of time. The raw Timer vocabulary stays public and
// unchanged; the Timer stays a separate weave; adapter weaves stay the right
// answer wherever time-to-domain translation is independently replaceable
// policy (snake-clock is exactly that, and survives this phase as a weave —
// only its ceremony left). Nothing here relays, buffers, or reinterprets a
// firing: the callback runs on the ordinary Loom execution thread, inside the
// ordinary handler, with the ordinary `Mail`.
//
// WHAT IT COSTS, said up front. A weave that mixes this in declares the WHOLE
// Timer protocol the binding can speak — `zen.Activated`, `TimerReady`,
// `TimerFired`, `TimerResolution` accepted; `EnsureTimer`, `EnsureRoleTimer`,
// `CancelTimer` emitted — even if it only uses one addressing mode. That is
// deliberate: the
// declaration says what this weave's code MAY say, the binding layer's code can
// say all three, and the manifest is the honest answer to "what conversation is
// this weave in?". The convenience hides ceremony from the author; it does not
// hide the conversation from Loom. Inspect any migrated weave and the Timer
// protocol is right there in its accept and emit sets.
//
// THE ONE LINE OF CEREMONY THAT REMAINS, and why it cannot go. `WeaveBase`
// dispatches by calling `self->on(shape, mail)` on the DERIVED type, and a
// derived class that declares any `on` overload HIDES every base-class `on`.
// So an author with their own handlers must write `using TimedWeave::on;`.
// Forgetting it is a hard compile error (never a silent miss), and there is a
// static_assert below that says so in words instead of template soup. Removing
// even that line would need a Loom change to how handlers are discovered, which
// this phase deliberately did not take.

#include "vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/weave.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace zengine::timer {

template <class Self>
class TimerBindings;

/// Where one binding stands in this incarnation's life.
///
/// R2B-0 replaced a single `desired` bool with these three, because that bool
/// was answering two different questions at once and getting one of them wrong.
/// "Should reconciliation re-establish this?" and "has this already happened?"
/// are not the same question, and a one-shot that fired needed to answer NO to
/// the first for a reason the second explains.
enum class BindingState {
    Waiting,  ///< wanted, and not yet finished with
    Spent,    ///< a one-shot that has fired; done unless explicitly restarted
    Canceled, ///< no longer wanted; the service has been told
};

/// One declared binding, as the author described it. This is DESIRED LOCAL
/// STATE and nothing else: it names a schedule this incarnation wants to exist,
/// not a schedule that does.
template <class Self>
struct Binding {
    using Callback = void (Self::*)(const TimerFired&, loom::Mail&);

    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role; ///< empty = requester-addressed; else role-addressed
    ContinuityOrder order{};
    BindingState state = BindingState::Waiting;
    Callback callback = nullptr;
    std::string resolved; ///< the last receipt's outcome ("" until one arrives)
    std::string reason;   ///< and its self-contained why
};

/// A small local handle to one declared binding.
///
/// It is an INDEX, not an owner: bindings are declared once (normally in the
/// constructor) and never removed, so the index is stable for the incarnation's
/// life. Copying a handle copies a reference to the same binding.
template <class Self>
class TimerHandle {
public:
    TimerHandle() = default;

    /// Stop wanting this timer, and tell the service so. Both halves matter:
    /// the local half is what stops a later `TimerReady` from re-establishing
    /// it, and the remote half is what stops the service firing it.
    ///
    /// HONEST EDGE, inherited from the raw contract and deliberately not
    /// smoothed over: an already-in-flight firing can still arrive after this,
    /// and it still reaches the callback. The binding layer does not silently
    /// change Timer semantics to look tidier.
    void cancel(loom::Mail& mail) {
        if (owner_ != nullptr) {
            owner_->cancel_at(index_, mail);
        }
    }

    /// Want it again, and ask now: Spent or Canceled becomes Waiting and ONE
    /// ordered request goes out. This is the only way a spent one-shot arms
    /// again — nothing else, and no lifecycle event, resurrects one.
    void restart(loom::Mail& mail) {
        if (owner_ != nullptr) {
            owner_->restart_at(index_, mail);
        }
    }

    /// Where this binding stands. (Local truth, not a service query — a weave
    /// cannot ask the Timer what it holds.)
    BindingState state() const {
        return owner_ != nullptr ? owner_->state_at(index_) : BindingState::Canceled;
    }
    bool waiting() const { return state() == BindingState::Waiting; }
    bool spent() const { return state() == BindingState::Spent; }
    bool canceled() const { return state() == BindingState::Canceled; }

    /// The last thing the Timer said it did about this binding — one of the
    /// kResolution* spellings, or empty if no receipt has arrived yet. This is
    /// what makes "what actually happened to my timer?" an answerable question
    /// for the author rather than something only a tap can see.
    const std::string& resolution() const {
        require_owner();
        return owner_->resolution_at(index_);
    }
    const std::string& resolution_reason() const {
        require_owner();
        return owner_->reason_at(index_);
    }

    /// LOUD ON AN INVALID HANDLE, deliberately. `valid()` exists, so a default-
    /// constructed handle is part of this type's public surface and asking one
    /// for its id is a programmer error, not a runtime condition — it takes the
    /// project's established path for one (throw), never a null dereference and
    /// never a quiet empty string that would flow on as a real timer id.
    const std::string& id() const {
        require_owner();
        return owner_->id_at(index_);
    }
    bool valid() const { return owner_ != nullptr; }

    // THERE IS DELIBERATELY NO DESTRUCTOR, and its absence is a promise kept
    // rather than a detail overlooked. A destructor that "cancelled" would be
    // claiming something it cannot do: during teardown there is no valid `Mail`
    // — no bus, no stamped sender, no authorized send — so it could only either
    // lie or reach around the one door a weave speaks through. Letting a handle
    // die is therefore a purely local event; the service still holds the
    // schedule, and a repeating timer whose requester is gone fires into clean
    // refusals until something cancels it or the service is replaced. That is
    // the raw contract's dead-requester floor, unchanged and still open, and
    // this layer does not pretend to have closed it.

private:
    friend class TimerBindings<Self>;
    TimerHandle(TimerBindings<Self>* owner, std::size_t index) : owner_(owner), index_(index) {}

    void require_owner() const {
        if (owner_ == nullptr) {
            throw std::logic_error(
                "zengine::timer::TimerHandle: this handle names no binding (default-constructed "
                "or never assigned) — check valid() before asking it about one");
        }
    }

    TimerBindings<Self>* owner_ = nullptr;
    std::size_t index_ = 0;
};

/// The declared-bindings table for one weave. Reached through `timers()`.
///
/// DECLARATION IS NOT EXECUTION. Every factory here records desire and sends
/// nothing: there is no `Mail` during construction, there may be no Timer
/// service in the process at all, and a weave that contacted anything from its
/// constructor would be reaching outside the one place a weave is allowed to
/// speak. The bindings are reconciled later, while handling an ordinary
/// message, from an accepted activation or a `TimerReady`.
template <class Self>
class TimerBindings {
public:
    using Callback = typename Binding<Self>::Callback;
    using Handle = TimerHandle<Self>;

    /// Declare a repeating timer delivered back to THIS weave's identity.
    /// (The requester-addressed mode.)
    ///
    /// `order` says what should happen to a schedule that already exists when
    /// this binding is reconciled. The default — prefer preserving the
    /// remaining time, accept restarting the delay — is what makes a graceful
    /// Timer replacement continuous while letting an initial load, a hard
    /// replacement and a reload all start cleanly.
    Handle repeat(std::string id, std::chrono::milliseconds delay, Callback cb,
                  ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/true, /*role=*/{}, cb, order,
                       /*role_form=*/false);
    }

    /// Declare a one-shot delivered back to THIS weave's identity.
    ///
    /// ONCE MEANS ONCE PER BINDING INCARNATION, unless explicitly restarted —
    /// and that conditional clause is the thing the word `once` used to lack.
    /// When it fires the binding becomes Spent: it stops reconciling, and no
    /// Timer reload, replacement or availability notice brings it back.
    Handle once(std::string id, std::chrono::milliseconds delay, Callback cb,
                ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/false, /*role=*/{}, cb, order,
                       /*role_form=*/false);
    }

    /// Declare a repeating timer delivered to whoever HOLDS `role` at each
    /// firing. (The beat belongs to the slot, not to this incarnation, so a
    /// successor inherits it rather than doubling it.)
    ///
    /// Deliberately a DIFFERENT NAME rather than an overload that infers the
    /// mode from an extra string: the two addressing modes are different
    /// promises about who hears the beat and who may cancel it, and a caller
    /// should have to say which one they mean.
    Handle repeat_to_role(std::string id, std::chrono::milliseconds delay, std::string role,
                          Callback cb, ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/true, std::move(role), cb, order,
                       /*role_form=*/true);
    }

    /// The one-shot twin of repeat_to_role.
    Handle once_to_role(std::string id, std::chrono::milliseconds delay, std::string role,
                        Callback cb, ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/false, std::move(role), cb, order,
                       /*role_form=*/true);
    }

    std::size_t size() const { return bindings_.size(); }

private:
    template <class S, class State, class A, class E>
    friend class TimedWeave;
    friend class TimerHandle<Self>;

    Handle declare(std::string id, std::chrono::milliseconds delay, bool repeat_, std::string role,
                   Callback cb, ContinuityOrder order, bool role_form) {
        if (id.empty()) {
            throw std::invalid_argument("zengine::timer: a binding needs an id");
        }
        if (cb == nullptr) {
            throw std::invalid_argument("zengine::timer: binding '" + id + "' has no callback");
        }
        // AN EMPTY ROLE IS REFUSED, LOUDLY, AT DECLARATION. `repeat_to_role` and
        // `once_to_role` are the names an author reaches for when the beat must
        // belong to a SLOT rather than to this incarnation. An empty role cannot
        // mean that, and the service treats a role-addressed ask with no role as
        // no ask at all — so accepting one here would leave the author with a
        // binding that quietly behaves like the requester-addressed mode they
        // deliberately did not choose. Different promises, different names, and
        // no silent degradation between them.
        if (role_form && role.empty()) {
            throw std::invalid_argument("zengine::timer: binding '" + id +
                                        "' asks for a role beat with no role — use repeat()/once() "
                                        "for the requester-addressed mode");
        }
        // DUPLICATE IDS ARE REFUSED, LOUDLY, AT DECLARATION. A timer id is the
        // only thing a firing carries, so two bindings sharing one could not be
        // told apart — dispatch would have to pick, and picking silently is how
        // a weave ends up running the wrong behaviour forever. This is a
        // programmer error, so it takes the project's established path for one:
        // throw. A weave loaded through the kernel turns that into a clean
        // "library create() returned null" load refusal (the ABI's create thunk
        // catches everything), and a natively mounted one fails loudly at mount.
        for (const Binding<Self>& b : bindings_) {
            if (b.id == id) {
                throw std::invalid_argument("zengine::timer: duplicate binding id '" + id +
                                            "' — one id, one callback");
            }
        }
        Binding<Self> b;
        b.id = std::move(id);
        b.delay_ms = static_cast<std::int64_t>(delay.count());
        b.repeat = repeat_;
        b.role = std::move(role);
        b.order = order;
        b.state = BindingState::Waiting;
        b.callback = cb;
        bindings_.push_back(std::move(b));
        return Handle{this, bindings_.size() - 1};
    }

    BindingState state_at(std::size_t i) const {
        return i < bindings_.size() ? bindings_[i].state : BindingState::Canceled;
    }
    const std::string& id_at(std::size_t i) const { return bindings_[i].id; }
    const std::string& resolution_at(std::size_t i) const { return bindings_[i].resolved; }
    const std::string& reason_at(std::size_t i) const { return bindings_[i].reason; }

    /// Both halves, and the local one first because it is the half a naive
    /// implementation forgets: stop wanting it, THEN tell the service. Without
    /// the local half the next `TimerReady` would faithfully re-establish the
    /// thing that was just cancelled.
    ///
    /// ROLE-TIMER AUTHORITY is the service's, unchanged: `CancelTimer` removes
    /// only what the STAMPED SENDER started, so a weave can cancel a role timer
    /// only while it is the requester currently associated with it — and a
    /// successor takes that position by re-asking first. This layer sends the
    /// same message any hand-written consumer would; it grants nothing extra.
    void cancel_at(std::size_t i, loom::Mail& mail) {
        if (i >= bindings_.size()) {
            return;
        }
        bindings_[i].state = BindingState::Canceled;
        mail.send_to_role(kTimerRole, CancelTimer{bindings_[i].id});
    }

    void restart_at(std::size_t i, loom::Mail& mail) {
        if (i >= bindings_.size()) {
            return;
        }
        bindings_[i].state = BindingState::Waiting;
        ask(bindings_[i], mail);
    }

    /// Re-establish every binding still WAITING.
    ///
    /// Spent and canceled bindings are skipped, and that skip is the whole
    /// difference the lifecycle state buys: reconciling a spent one-shot would
    /// resurrect something the author already finished with, every time the
    /// Timer service so much as came back.
    ///
    /// WHAT AN ORDERED RE-ASK MEANS. The raw asks were cardinality-idempotent
    /// but never timing-neutral: they replaced and RE-ANCHORED, so a binding
    /// reconciled mid-cycle silently lost the rest of that cycle. The ordered
    /// form is how a binding says it would rather not: prefer keeping the
    /// remaining time, accept restarting if there is nothing to keep. Where
    /// there is a matching schedule and a graceful succession preserved it, a
    /// re-ask now costs nothing at all; where there is not, it restarts and
    /// SAYS SO in a receipt rather than leaving the author to guess.
    void reconcile(loom::Mail& mail) {
        for (const Binding<Self>& b : bindings_) {
            if (b.state == BindingState::Waiting) {
                ask(b, mail);
            }
        }
    }

    void ask(const Binding<Self>& b, loom::Mail& mail) {
        const std::string preferred = spelling_of(b.order.preferred);
        const std::string fallback = fallback_spelling(b.order);
        if (b.role.empty()) {
            mail.send_to_role(kTimerRole,
                              EnsureTimer{b.id, b.delay_ms, b.repeat, preferred, fallback});
        } else {
            mail.send_to_role(kTimerRole, EnsureRoleTimer{b.id, b.delay_ms, b.repeat, b.role,
                                                          preferred, fallback});
        }
    }

    /// Record what the Timer said it did. The id is the key, exactly as a firing
    /// is: a receipt for an id this weave never declared is data, not news, and
    /// is ignored — the ordinary consumer obligation.
    void record(const TimerResolution& r) {
        for (Binding<Self>& b : bindings_) {
            if (b.id == r.id) {
                b.resolved = r.resolved;
                b.reason = r.reason;
                return;
            }
        }
    }

    /// Route one firing to the ONE binding that asked for it.
    ///
    /// Exact id match, first and only. An id nobody declared is data, not a
    /// drive (a role beat can be aimed at by anyone), and two bindings can never
    /// both answer because two bindings can never share an id.
    ///
    /// A ONE-SHOT IS MARKED SPENT BEFORE ITS CALLBACK RUNS, and the order is the
    /// point: the callback is exactly where an author might deliberately
    /// `restart(mail)`, and marking afterwards would overwrite that decision
    /// with a stale one. Mark first, then hand over — so the callback's word is
    /// the last one.
    void dispatch(Self* self, const TimerFired& f, loom::Mail& mail) {
        for (Binding<Self>& b : bindings_) {
            if (b.id != f.id) {
                continue;
            }
            const Callback cb = b.callback;
            if (!b.repeat && b.state == BindingState::Waiting) {
                b.state = BindingState::Spent;
            }
            (self->*cb)(f, mail);
            return;
        }
    }

    std::vector<Binding<Self>> bindings_;
};

/// The authoring base: `WeaveBase` plus the Timer protocol, already handled.
///
/// An author writes their own Accept/Emit as usual and this layer prepends what
/// the binding needs. The composed contract is what the manifest carries — no
/// wildcard acceptance, no widened grant, no undeclared emission, no host-root
/// send, no Switchboard reach.
template <class Self, class State, class AcceptList, class EmitList = loom::Emit<>>
class TimedWeave;

template <class Self, class State, class... A, class... E>
class TimedWeave<Self, State, loom::Accept<A...>, loom::Emit<E...>>
    : public loom::WeaveBase<
          Self, State,
          loom::Accept<loom::Activated, TimerReady, TimerFired, TimerResolution, A...>,
          loom::Emit<EnsureTimer, EnsureRoleTimer, CancelTimer, E...>> {
public:
    using Bindings = TimerBindings<Self>;
    using Handle = TimerHandle<Self>;

    /// The declared-bindings table. Call the factories on it during
    /// construction; they record desire and send nothing.
    Bindings& timers() {
        // The one line of ceremony, checked here rather than left to template
        // soup: this is instantiated when an author calls timers() from their
        // constructor, which is exactly the moment Self is complete enough to
        // ask whether its handlers are visible.
        static_assert(
            requires(Self& s, const TimerFired& f, loom::Mail& m) { s.on(f, m); },
            "zengine::timer::TimedWeave: this weave's own on() handlers HIDE the binding "
            "layer's. Add `using TimedWeave::on;` to the class. (WeaveBase dispatches via "
            "self->on(...) on the derived type, and a derived on() hides every base one.)");
        return bindings_;
    }
    const Bindings& timers() const { return bindings_; }

    // ---- the ceremony, owned here so no author writes it again -------------

    /// This incarnation is live: establish everything it declared.
    ///
    /// Activation trust AND deduplication live HERE, once, so no author has to
    /// rediscover the rule. Since R2B-1 the cursor requires Loom's attestation
    /// before it considers lineage at all — so a bound weave cannot be made to
    /// re-establish its timers by any weave that merely knows the public shape.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return; // unattested, duplicate or replayed: nothing is re-established
        }
        bindings_.reconcile(mail);
    }

    /// The Timer service is available — possibly for the first time, possibly
    /// again after a reload or swap with an empty table. Either way, the
    /// declared bindings are what this weave wants to exist, so re-establish
    /// them. This is the path that covers a consumer loaded BEFORE the service,
    /// whose activation-time asks could not reach it.
    void on(const TimerReady&, loom::Mail& mail) { bindings_.reconcile(mail); }

    /// A firing: exactly one binding's callback, or none.
    void on(const TimerFired& f, loom::Mail& mail) {
        bindings_.dispatch(static_cast<Self*>(this), f, mail);
    }

    /// A receipt: what the Timer actually did about one order.
    ///
    /// The binding CONSUMES it without hiding it — this is an ordinary declared
    /// message on an ordinary bus, so a tap and a console see the order and its
    /// answer whether or not any author ever reads one. What the binding adds is
    /// that the author CAN read it, from the handle, without writing protocol.
    void on(const TimerResolution& r, loom::Mail&) { bindings_.record(r); }

protected:
    /// Visible to the author only so a subclass can read its own activation
    /// state if it genuinely needs to; the binding layer already acts on it.
    const zengine::ActivationCursor& activation() const { return activation_; }

private:
    zengine::ActivationCursor activation_; ///< per-incarnation, never state
    Bindings bindings_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_BINDING_HPP
