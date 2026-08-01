// The maintenance scheduler — a `timer::TimedWeave`, using the binding layer at
// full strength and reaching for the raw Timer protocol nowhere.
//
// ---- WHAT THIS FILE IS EVIDENCE FOR ----------------------------------------
//
// 1. `on_timed_activation()` EXISTS AND WORKS. Night One's friction 4 was that
//    the binding owned `on(zen.Activated)` and a derived handler suppressed it
//    SILENTLY, so three of four weaves in that kitchen wanted the activation
//    moment and none could use the binding. The hook below does real domain
//    startup work — it announces, and it runs the fleet's first checks — and it
//    runs after the bindings reconciled, only for an activation the cursor
//    accepted.
//
// 2. THE ONE LINE OF CEREMONY IS STILL THERE, and it is the right kind: a
//    missing `using TimedWeave::on;` is a hard compile error with a sentence in
//    it, and a derived raw activation handler is a compile-time refusal that
//    names the alternative. Nothing here can fail silently.
//
// 3. THE BINDING TABLE IS AUTHORED, NOT DYNAMIC — and that is this project's own
//    finding. This weave's job is to run things on a rhythm the OPERATOR
//    chooses, and none of those rhythms can be a binding: `reconcile` belongs to
//    the layer (correctly — an author reconciling would be a second scheduler),
//    so a binding declared after construction stays `Waiting` until the layer
//    happens to reconcile again. So the scheduler declares ONE authored beat and
//    counts it, which is exactly what the kitchen's expediter did with the raw
//    protocol two projects ago.
//
// 4. THE TWO SUGARS COMPOSE IN ONE DIRECTION FOR FREE. When the Timer service is
//    replaced underneath this weave, the new service publishes `TimerReady`, the
//    binding layer reconciles, and every authored binding is re-established with
//    no application code whatsoever. That is the nicest thing in this project and
//    it required nothing.

#include "vocabulary.hpp"

#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using namespace marathon::maint;
namespace timer = zengine::timer;

/// One domain schedule. `left` counts the scheduler's OWN authored beats — see
/// the header for why a schedule cannot be a binding.
struct Schedule {
    std::string name;
    std::string machine;
    std::string action;
    std::int64_t every = 0; ///< 0 for a one-shot
    std::int64_t left = 0;
    std::string subscriber; ///< canonical decimal of the asker's WeaveId
    std::int64_t correlation = 0;
    std::int64_t runs = 0;
    /// A check has been asked for and its answer has not come back yet.
    ///
    /// IT EXISTS BECAUSE A ONE-SHOT USED TO LEAVE THE BOOK THE MOMENT IT WAS
    /// ASKED. The worker's answer then matched no schedule, and the party that
    /// asked for the action was never told what happened to it -- the exact
    /// silent-failure shape four earlier projects were built to refuse. A
    /// schedule now leaves the book when its RESULT lands, not when its question
    /// goes out.
    bool pending = false;
    ZEN_SHAPE(Schedule, 1, ZEN_FIELD(name), ZEN_FIELD(machine), ZEN_FIELD(action),
              ZEN_FIELD(every), ZEN_FIELD(left), ZEN_FIELD(subscriber),
              ZEN_FIELD(correlation), ZEN_FIELD(runs), ZEN_FIELD(pending));
};

struct SchedulerState {
    std::vector<Schedule> book;
    std::int64_t sweeps = 0;
    std::int64_t warmups = 0;
    std::int64_t audits = 0;
    std::int64_t late_fires = 0;
    std::int64_t asked = 0;
    std::int64_t reports = 0;
    std::int64_t unhealthy = 0;
    std::int64_t startups = 0; ///< times on_timed_activation ran
    ZEN_EXPOSE();
    ZEN_SHAPE(SchedulerState, 1, ZEN_FIELD(book), ZEN_FIELD(sweeps), ZEN_FIELD(warmups),
              ZEN_FIELD(audits), ZEN_FIELD(late_fires), ZEN_FIELD(asked), ZEN_FIELD(reports),
              ZEN_FIELD(unhealthy), ZEN_FIELD(startups));
};

std::uint64_t parse_u64(const std::string& text) {
    std::uint64_t v = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const std::from_chars_result r = std::from_chars(first, last, v);
    if (r.ec != std::errc{} || r.ptr != last) {
        return 0;
    }
    return v;
}

class Scheduler
    : public timer::TimedWeave<
          Scheduler, SchedulerState,
          loom::Accept<ScheduleCheck, ScheduleOnce, CancelSchedule, CancelAudit,
                       DeclareLateBinding, MaintStatus, CheckResult>,
          loom::Emit<RunCheck, HealthReport, SchedulerOpen, loom::Result, loom::Ack,
                     loom::Refused>> {
public:
    /// DECLARATION IS NOT EXECUTION. Nothing here sends anything: there is no
    /// `Mail` in a constructor, and there may be no Timer service in the process
    /// at all. The layer reconciles later.
    Scheduler() {
        // Role-addressed, so the beat belongs to the SLOT and a successor
        // inherits the pulse instead of starting a second one.
        sweep_ = timers().repeat_to_role(kSweepId, std::chrono::milliseconds{kSweepMs},
                                         kSchedulerRole, &Scheduler::on_sweep);
        // Requester-addressed: this one is THIS incarnation's own warm-up and a
        // successor has no business inheriting it.
        warmup_ = timers().once(kWarmupId, std::chrono::milliseconds{kWarmupMs},
                                &Scheduler::on_warmup);
        audit_ = timers().once_to_role(kAuditId, std::chrono::milliseconds{kAuditMs},
                                       kSchedulerRole, &Scheduler::on_audit);
    }

    /// THE ONE LINE OF CEREMONY. Without it, this weave's own `on` overloads hide
    /// every one the binding layer declares and no timer would ever be ordered —
    /// which is a hard compile error with a sentence in it, not a silent miss.
    using timer::TimedWeave<Scheduler, SchedulerState,
                            loom::Accept<ScheduleCheck, ScheduleOnce, CancelSchedule, CancelAudit,
                                         DeclareLateBinding, MaintStatus, CheckResult>,
                            loom::Emit<RunCheck, HealthReport, SchedulerOpen, loom::Result,
                                       loom::Ack, loom::Refused>>::on;

    /// THE AUTHOR'S EXTENSION POINT — the thing Night One could not have.
    ///
    /// It runs after this weave accepted an activation and after every waiting
    /// binding was reconciled, and never for an unattested, duplicate, replayed
    /// or foreign one. So this is ordinary domain startup work with no lifecycle
    /// bookkeeping of its own: there is one cursor, it belongs to the layer, and
    /// it has already spoken.
    void on_timed_activation(const loom::Activated&, loom::Mail& mail) {
        ++state_.startups;
        mail.publish(SchedulerOpen{static_cast<std::int64_t>(timers().size()),
                                   static_cast<std::int64_t>(state_.book.size())});
        // A fresh scheduler knows nothing about the fleet's current state, so it
        // asks about everything it is responsible for at once rather than waiting
        // out a full period. THIS is the domain work the hook exists for.
        for (Schedule& s : state_.book) {
            ask_worker(mail, s);
        }
    }

    // ---- the operator's book ------------------------------------------------

    void on(const ScheduleCheck& c, loom::Mail& mail) {
        if (c.every_sweeps <= 0) {
            (void)mail.answer(loom::Refused{"a repeating check needs a period of at least one "
                                            "sweep"});
            return;
        }
        add(mail, c.name, c.machine, "health", c.every_sweeps, c.every_sweeps);
    }

    void on(const ScheduleOnce& o, loom::Mail& mail) {
        if (o.after_sweeps <= 0) {
            (void)mail.answer(loom::Refused{"a delayed action needs a delay of at least one "
                                            "sweep"});
            return;
        }
        add(mail, o.name, o.machine, o.action, /*every=*/0, o.after_sweeps);
    }

    void on(const CancelSchedule& c, loom::Mail& mail) {
        for (std::size_t i = 0; i < state_.book.size(); ++i) {
            if (state_.book[i].name == c.name) {
                state_.book.erase(state_.book.begin() + static_cast<std::ptrdiff_t>(i));
                (void)mail.answer(loom::Ack{});
                return;
            }
        }
        (void)mail.answer(loom::Refused{"no schedule named '" + c.name + "' is in this book"});
    }

    /// CANCELLING AN AUTHORED BINDING — a different act from cancelling a book
    /// entry, and it is the handle's, not this weave's. Both halves happen: the
    /// local one stops a later `TimerReady` re-establishing it, the remote one
    /// stops the service firing it.
    void on(const CancelAudit&, loom::Mail& mail) {
        audit_.cancel(mail);
        (void)mail.answer(loom::Ack{});
    }

    /// THE PROBE. Declares a repeating binding long after construction, so the
    /// suite can measure when the layer establishes it. See the file header.
    void on(const DeclareLateBinding&, loom::Mail& mail) {
        if (!late_.valid()) {
            late_ = timers().repeat(kLateId, std::chrono::milliseconds{kLateMs},
                                    &Scheduler::on_late);
        }
        (void)mail.answer(loom::Ack{});
    }

    void on(const MaintStatus&, loom::Mail& mail) {
        (void)mail.answer(loom::Result{
            "maint: sweeps=" + std::to_string(state_.sweeps) + " warmups=" +
            std::to_string(state_.warmups) + " audits=" + std::to_string(state_.audits) +
            " late_fires=" + std::to_string(state_.late_fires) + " asked=" +
            std::to_string(state_.asked) + " reports=" + std::to_string(state_.reports) +
            " unhealthy=" + std::to_string(state_.unhealthy) + " startups=" +
            std::to_string(state_.startups) + " book=" + std::to_string(state_.book.size()) +
            " audit=" + binding_state(audit_) + " warmup=" + binding_state(warmup_) +
            " sweep=" + binding_state(sweep_)});
    }

    /// The worker's authenticated answer.
    void on(const CheckResult& r, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return; // the consumer obligation, and here it is fully performable
        }
        ++state_.reports;
        state_.unhealthy += r.ok ? 0 : 1;
        for (std::size_t i = 0; i < state_.book.size(); ++i) {
            Schedule& s = state_.book[i];
            if (s.name != r.name) {
                continue;
            }
            ++s.runs;
            s.pending = false;
            const loom::WeaveId who{parse_u64(s.subscriber)};
            if (who.valid()) {
                mail.send(who, HealthReport{r.name, r.machine, r.ok, r.detail, s.runs},
                          static_cast<std::uint64_t>(s.correlation));
            }
            if (s.every > 0) {
                s.left = s.every;
            } else {
                // A one-shot is finished the moment its answer lands, and not one
                // beat sooner.
                state_.book.erase(state_.book.begin() + static_cast<std::ptrdiff_t>(i));
            }
            return;
        }
    }

    // ---- the authored bindings' callbacks -----------------------------------

    /// The rhythm. Every domain schedule is counted in these beats, because a
    /// schedule cannot be a binding — see the file header.
    void on_sweep(const timer::TimerFired&, loom::Mail& mail) {
        ++state_.sweeps;
        for (Schedule& s : state_.book) {
            if (s.pending || --s.left > 0) {
                continue;
            }
            ask_worker(mail, s);
        }
    }

    /// The one-shot: run everything once, sooner than the rhythm would.
    void on_warmup(const timer::TimerFired&, loom::Mail& mail) {
        ++state_.warmups;
        for (Schedule& s : state_.book) {
            ask_worker(mail, s);
        }
    }

    void on_audit(const timer::TimerFired&, loom::Mail&) { ++state_.audits; }

    void on_late(const timer::TimerFired&, loom::Mail&) { ++state_.late_fires; }

private:
    void add(loom::Mail& mail, const std::string& name, const std::string& machine,
             const std::string& action, std::int64_t every, std::int64_t left) {
        if (state_.book.size() >= kMaxSchedules) {
            (void)mail.answer(loom::Refused{"this scheduler holds its maximum of " +
                                            std::to_string(kMaxSchedules) + " schedules"});
            return;
        }
        if (name.empty() || machine.empty()) {
            (void)mail.answer(loom::Refused{"a schedule needs a name and a machine"});
            return;
        }
        for (const Schedule& s : state_.book) {
            if (s.name == name) {
                (void)mail.answer(
                    loom::Refused{"a schedule named '" + name + "' is already in this book"});
                return;
            }
        }
        Schedule s;
        s.name = name;
        s.machine = machine;
        s.action = action;
        s.every = every;
        s.left = left;
        s.subscriber = std::to_string(mail.sender().value);
        s.correlation = static_cast<std::int64_t>(mail.correlation());
        state_.book.push_back(std::move(s));
        (void)mail.answer(loom::Ack{});
    }

    void ask_worker(loom::Mail& mail, Schedule& s) {
        if (s.pending) {
            return; // one question at a time; the answer is what moves it on
        }
        s.pending = true;
        ++state_.asked;
        mail.send_to_role(kWorkerRole, RunCheck{s.name, s.machine, s.action});
    }

    static const char* binding_state(const timer::TimerHandle<Scheduler>& h) {
        if (!h.valid()) {
            return "none";
        }
        return h.canceled() ? "canceled" : (h.spent() ? "spent" : "waiting");
    }

    timer::TimerHandle<Scheduler> sweep_;
    timer::TimerHandle<Scheduler> warmup_;
    timer::TimerHandle<Scheduler> audit_;
    timer::TimerHandle<Scheduler> late_;
};

} // namespace

ZEN_EXPORT_WEAVE(Scheduler)
