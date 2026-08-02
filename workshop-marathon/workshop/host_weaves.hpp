#ifndef WORKSHOP_HOST_WEAVES_HPP
#define WORKSHOP_HOST_WEAVES_HPP

// The Workshop's native hands: the operator (turns Manager answers into
// published launch facts, honors StopWish) and the governor (a bounded run,
// spoken as ordinary intent). Shared between the shell and the test suite so
// the tests witness the same weaves the real Workshop runs — a witness of a
// copy would prove nothing.

#include "vocabulary.hpp"

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include <zen/weave/poke.hpp>

#include <zen/kernel/control.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace workshop {

struct Pending {
    std::string part; ///< spec part name, or "(service) ...", "(workshop) ..."
    std::string stem;
    std::string role;
    int action = 0; ///< 0 none; 100+i = skin i swapped in (flip on the ANSWER)
};

/// A skin the interactive operator can put on the surface.
struct SkinChoice {
    std::string stem;
    std::string path;
    std::string label;
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
    /// The refusal demo: when the last boot ANSWER arrives (not when the boot
    /// wishes were queued — the steward's door is itself message-composed, so
    /// "after the boots" in queue order is BEFORE any load has happened), ask
    /// this role a question it has no door for.
    std::string refusal_role;
    bool refusal_fired = false;

    // ---- interactive alteration (Gate 3) -----------------------------------
    bool interactive = false;
    std::string last_status;         ///< re-offered when a fresh skin says hello
    std::vector<SkinChoice> skins;   ///< the swap cycle; [0] is what booted
    std::size_t skin_idx = 0;
    std::vector<KnobSpec> knobs;     ///< the creation's declared reach-in points
    std::vector<std::size_t> knob_at; ///< current value index per knob
    std::size_t knob_cursor = 0;
    std::string alter_part;          ///< first role-holding part: reload target
    std::string alter_stem;
    std::string alter_path;
};

struct OperatorState {
    std::int64_t answers = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

/// The Workshop's hand on the bus. It hears the Manager's answers and turns
/// them into published launch facts — the answer, not the wish, is what
/// publishes. It hears StopWish (the governor's, or anyone's) and stops the
/// world through the shell's one lever. Interactively it hears KeyPressed and
/// reaches inside the live world the ordinary ways: SwapWeave (skin), Poke
/// (declared knobs), ReloadWeave (code in place, state riding the gate).
class OperatorWeave
    : public loom::WeaveBase<OperatorWeave, OperatorState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused, StopWish,
                                          zengine::input::KeyPressed,
                                          zengine::surface::SurfaceReady>,
                             loom::Emit<loom::LoadWeave, loom::SwapWeave, loom::ReloadWeave,
                                        loom::PokeWrite, PartUp, PartFailed,
                                        zengine::surface::SurfaceText>> {
public:
    explicit OperatorWeave(OperatorContext& ctx) : ctx_(&ctx) {}

    void on(const loom::Result& r, loom::Mail& mail) { answered(mail, r.value, false); }
    void on(const loom::Ack&, loom::Mail& mail) { answered(mail, "done", false); }
    void on(const loom::Refused& r, loom::Mail& mail) { answered(mail, r.reason, true); }

    void on(const StopWish& wish, loom::Mail& mail) {
        status(mail, "stop: " + wish.reason);
        quit();
    }

    /// A fresh skin said hello: re-offer the status line so it starts complete.
    void on(const zengine::surface::SurfaceReady&, loom::Mail& mail) {
        if (!ctx_->last_status.empty()) {
            mail.publish(zengine::surface::SurfaceText{zengine::surface::kSlotStatus,
                                                       ctx_->last_status});
        }
    }

    void on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        namespace scan = zengine::input::scan;
        if (!ctx_->interactive) {
            return;
        }
        if (k.scancode == scan::k1 && ctx_->skins.size() > 1) {
            const std::size_t next = (ctx_->skin_idx + 1) % ctx_->skins.size();
            const SkinChoice& s = ctx_->skins[next];
            command(mail, "swap skin -> " + s.label, static_cast<int>(100 + next),
                    loom::SwapWeave{zengine::surface::kSkinRole, s.stem, s.path,
                                    /*graceful=*/false});
        } else if (k.scancode == scan::kP && !ctx_->knobs.empty()) {
            const KnobSpec& knob = ctx_->knobs[ctx_->knob_cursor];
            if (!knob.values.empty()) {
                std::size_t& at = ctx_->knob_at[ctx_->knob_cursor];
                at = (at + 1) % knob.values.size();
                const std::string& value = knob.values[at];
                const std::uint64_t corr = ctx_->next_corr++;
                ctx_->pending[corr] =
                    Pending{"knob '" + knob.name + "' = " + value, knob.field, knob.role};
                mail.send_to_role(knob.role, loom::PokeWrite{knob.field, value}, corr);
                status(mail, "knob '" + knob.name + "' -> " + value + " ...");
            }
        } else if (k.scancode == scan::kO && ctx_->knobs.size() > 1) {
            ctx_->knob_cursor = (ctx_->knob_cursor + 1) % ctx_->knobs.size();
            status(mail, "knob selected: " + ctx_->knobs[ctx_->knob_cursor].name);
        } else if (k.scancode == scan::kR && !ctx_->alter_stem.empty()) {
            command(mail, "reload " + ctx_->alter_part + " in place (state rides the gate)", 0,
                    loom::ReloadWeave{ctx_->alter_stem, ctx_->alter_path});
        } else if (k.scancode == scan::kQ) {
            quit();
        } else if (k.scancode == scan::kC && k.name == "Ctrl+C") {
            // The backends' dressed convenience name, trusted as a courtesy —
            // same debt the snake host carries, same spelling.
            quit();
        }
    }

private:
    template <class Cmd>
    void command(loom::Mail& mail, const std::string& label, int action, const Cmd& cmd) {
        const std::uint64_t corr = ctx_->next_corr++;
        ctx_->pending[corr] = Pending{label, "", ""};
        ctx_->pending[corr].action = action;
        mail.send(ctx_->manager, cmd, corr);
        status(mail, label + " ...");
    }

    void quit() {
        ctx_->quit = true;
        if (ctx_->request_stop) {
            ctx_->request_stop();
        }
    }
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
            if (p.action >= 100) {
                ctx_->skin_idx = static_cast<std::size_t>(p.action - 100);
            }
            ++ctx_->up;
            mail.publish(PartUp{ctx_->project, p.part, p.stem, p.role});
            status(mail, p.part + " " + words);
        }
        ctx_->pending.erase(it);

        if (ctx_->pending.empty() && !ctx_->refusal_role.empty() && !ctx_->refusal_fired) {
            ctx_->refusal_fired = true;
            mail.send_to_role(ctx_->refusal_role, QueryRunning{});
            status(mail, "refusal demo: asked '" + ctx_->refusal_role +
                             "' a question it has no door for");
        }
    }

    void status(loom::Mail& mail, const std::string& text) {
        ctx_->last_status = "[workshop] " + text;
        if (ctx_->interactive) {
            ctx_->last_status += "   (1 skin | p knob | r reload | q quit)";
        }
        mail.publish(zengine::surface::SurfaceText{zengine::surface::kSlotStatus,
                                                   ctx_->last_status});
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

/// mount(), but keeping the instance pointer — the same construction the
/// installed mount<>() performs inline, for native host hands the shell (or a
/// test) wants to read afterwards.
template <class Self, class... Args>
std::pair<loom::WeaveId, Self*> mount_keeping(loom::Switchboard& bus, loom::Grant grant,
                                              Args&&... args) {
    auto weave = std::make_unique<Self>(std::forward<Args>(args)...);
    Self* raw = weave.get();
    loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant));
    raw->zen_set_self(id);
    return {id, raw};
}

struct AskState {
    std::int64_t asked = 0;
    ZEN_SHAPE(AskState, 1, ZEN_FIELD(asked));
};

/// A native hand that asks a role one question and records the one answer.
/// The beat chain keeps the queue alive forever, so hearing the answer pulls
/// the same stop lever the operator uses; a quiescent bus returns on its own.
template <class Query, class Report>
class AskOnce : public loom::WeaveBase<AskOnce<Query, Report>, AskState, loom::Accept<Report>,
                                       loom::Emit<Query>> {
public:
    explicit AskOnce(std::function<void()> done) : done_(std::move(done)) {}

    void on(const Report& r, loom::Mail& mail) {
        ++answers;
        authenticated = mail.answers_ask();
        report = r;
        if (done_) {
            done_();
        }
    }

    int answers = 0;
    bool authenticated = false;
    Report report;

private:
    std::function<void()> done_;
};

/// Ask `role` one `Query`, pump until the answer (or quiescence), return what
/// came back. The ask rides the gated role-addressed path with a real grant,
/// exactly as a toy would ask.
template <class Query, class Report>
AskOnce<Query, Report>* ask_role_once(loom::Switchboard& bus, const char* role, const Query& q) {
    loom::Grant reach;
    reach.allow_to_role(Query::zen_name, Query::zen_version, role);
    auto [id, hand] =
        mount_keeping<AskOnce<Query, Report>>(bus, std::move(reach), [&bus] { bus.stop(); });
    bus.send_as_to_role(id, role, loom::Message(loom::to_value(q), id, id, 0));
    bus.pump();
    return hand;
}

} // namespace workshop

#endif // WORKSHOP_HOST_WEAVES_HPP
