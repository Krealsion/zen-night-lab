#ifndef WORKSHOP_HOST_WEAVES_HPP
#define WORKSHOP_HOST_WEAVES_HPP

// The Workshop's native hands: the operator (turns Manager answers into
// published launch facts, honors StopWish) and the governor (a bounded run,
// spoken as ordinary intent). Shared between the shell and the test suite so
// the tests witness the same weaves the real Workshop runs — a witness of a
// copy would prove nothing.

#include "vocabulary.hpp"

#include "surface/vocabulary.hpp"
#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace workshop {

struct Pending {
    std::string part; ///< spec part name, or "(service) ...", "(workshop) ..."
    std::string stem;
    std::string role;
};

struct OperatorContext {
    std::string project;
    std::map<std::uint64_t, Pending> pending;
    std::uint64_t next_corr = 1;
    bool quit = false;
    std::int64_t up = 0;
    std::int64_t failed = 0;
    loom::WeaveId manager{};
    std::function<void()> request_stop;
};

struct OperatorState {
    std::int64_t answers = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

/// The Workshop's hand on the bus. It hears the Manager's answers and turns
/// them into published launch facts — the answer, not the wish, is what
/// publishes. It also hears StopWish (the governor's, or anyone's) and stops
/// the world through the shell's one lever.
class OperatorWeave
    : public loom::WeaveBase<OperatorWeave, OperatorState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused, StopWish>,
                             loom::Emit<loom::LoadWeave, PartUp, PartFailed,
                                        zengine::surface::SurfaceText>> {
public:
    explicit OperatorWeave(OperatorContext& ctx) : ctx_(&ctx) {}

    void on(const loom::Result& r, loom::Mail& mail) { answered(mail, r.value, false); }
    void on(const loom::Ack&, loom::Mail& mail) { answered(mail, "done", false); }
    void on(const loom::Refused& r, loom::Mail& mail) { answered(mail, r.reason, true); }

    void on(const StopWish& wish, loom::Mail& mail) {
        status(mail, "stop: " + wish.reason);
        ctx_->quit = true;
        if (ctx_->request_stop) {
            ctx_->request_stop();
        }
    }

private:
    void answered(loom::Mail& mail, const std::string& words, bool refused) {
        ++state_.answers;
        const auto it = ctx_->pending.find(mail.correlation());
        if (it == ctx_->pending.end()) {
            status(mail, "unsolicited answer: " + words);
            return;
        }
        const Pending& p = it->second;
        if (refused) {
            ++ctx_->failed;
            mail.publish(PartFailed{ctx_->project, p.part, p.stem, words});
            status(mail, p.part + " REFUSED: " + words);
        } else {
            ++ctx_->up;
            mail.publish(PartUp{ctx_->project, p.part, p.stem, p.role});
            status(mail, p.part + " up");
        }
        ctx_->pending.erase(it);
    }

    void status(loom::Mail& mail, const std::string& text) {
        mail.publish(zengine::surface::SurfaceText{zengine::surface::kSlotStatus,
                                                   "[workshop] " + text});
    }

    OperatorContext* ctx_;
};

/// The reach a native TimedWeave needs: the Timer protocol its binding layer
/// speaks (found the hard way — an under-granted governor's EnsureTimer is a
/// CapabilityDenied only a tap can see, and the run simply never ends).
inline void allow_timed_weave(loom::Grant& grant) {
    namespace zt = zengine::timer;
    grant.allow_to_role(zt::EnsureTimer::zen_name, zt::EnsureTimer::zen_version, zt::kTimerRole);
    grant.allow_to_role(zt::EnsureRoleTimer::zen_name, zt::EnsureRoleTimer::zen_version,
                        zt::kTimerRole);
    grant.allow_to_role(zt::CancelTimer::zen_name, zt::CancelTimer::zen_version, zt::kTimerRole);
}

struct GovernorState {
    std::int64_t seconds = 0;
    std::int64_t limit = 0;
    ZEN_SHAPE(GovernorState, 1, ZEN_FIELD(seconds), ZEN_FIELD(limit));
};

/// A bounded run, spoken as ordinary intent: after N seconds the governor
/// WISHES the world stopped; the operator honors the wish. Nothing here
/// touches the host — a toy could publish the same shape.
class Governor : public zengine::timer::TimedWeave<Governor, GovernorState, loom::Accept<>,
                                                  loom::Emit<StopWish>> {
public:
    explicit Governor(std::int64_t limit_seconds)
        : beat_(timers().repeat("workshop.governor", std::chrono::milliseconds(1000),
                                &Governor::on_beat)) {
        state_.limit = limit_seconds;
    }

    using TimedWeave::on;

    void on_beat(const zengine::timer::TimerFired&, loom::Mail& mail) {
        if (++state_.seconds >= state_.limit) {
            mail.publish(StopWish{"governor: " + std::to_string(state_.limit) + "s elapsed"});
        }
    }

private:
    Handle beat_;
};

} // namespace workshop

#endif // WORKSHOP_HOST_WEAVES_HPP
