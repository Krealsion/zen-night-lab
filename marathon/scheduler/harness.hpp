#ifndef MARATHON_MAINT_HARNESS_HPP
#define MARATHON_MAINT_HARNESS_HPP

// The maintenance scheduler's host — and the one place in the marathon where a
// single coordinator drives TWO prepared replacements against TWO different
// vocabularies: this package's own worker, and **the Timer service itself**,
// through the Timer package's own `PrepareTimerHandover` / `TimerCandidate*`
// shapes.
//
// That is the composition question made concrete. The finding, recorded here
// because this is where it is visible: the handle did not care. `start`, `ask`,
// `offer_current_answer`, `commit`, `take_outcome` are identical for both, and
// the ONLY difference in application code is which domain shape goes into `ask`
// and which handler offers the answer.
//
// The one substitution is the Timer's CLOCK (`zengine-timer-virtual.so`) — and
// in this project it is also the thing being REPLACED, by `-virtual-v2`, which
// is a genuinely different artifact of the same service.

#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include <zen/host/prepared_replacement.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace marathon::maint_testing {

namespace maint = marathon::maint;
namespace timer = zengine::timer;

#ifndef MARATHON_MAINT_RUNTIME_DIR
#error "MARATHON_MAINT_RUNTIME_DIR must be defined by the build"
#endif

inline std::string weave_path(const std::string& stem) {
    return std::string(MARATHON_MAINT_RUNTIME_DIR) + "/" + stem + ".so";
}

// ---- the operator ----------------------------------------------------------

struct OperatorLog {
    std::map<std::uint64_t, std::string> pending;
    std::vector<std::string> answers;
    std::uint64_t next_correlation = 1;
};

struct OperatorState {
    std::int64_t answers = 0;
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

class Operator : public loom::WeaveBase<Operator, OperatorState,
                                        loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                        loom::Emit<loom::LoadWeave>> {
public:
    explicit Operator(OperatorLog& log) : log_(&log) {}
    void on(const loom::Result& r, loom::Mail& mail) { record(mail, "-> " + r.value); }
    void on(const loom::Ack&, loom::Mail& mail) { record(mail, "-> done"); }
    void on(const loom::Refused& r, loom::Mail& mail) { record(mail, "-> refused: " + r.reason); }

private:
    void record(loom::Mail& mail, const std::string& outcome) {
        ++state_.answers;
        const auto it = log_->pending.find(mail.correlation());
        if (it == log_->pending.end()) {
            log_->answers.push_back("(unsolicited) " + outcome);
            return;
        }
        log_->answers.push_back(it->second + " " + outcome);
        log_->pending.erase(it);
    }
    OperatorLog* log_;
};

// ---- the engineer: ONE coordinator, TWO vocabularies -----------------------

struct EngineerDesk {
    loom::PreparedReplacement* upgrade = nullptr;
    std::vector<loom::TxnResult> offers;
    std::vector<std::string> fleet;
    std::int64_t fleet_checks = 0;
    bool fleet_described = false;
    std::vector<std::string> notes;
    std::int64_t offered_without_handle = 0;
};

struct EngineerState {
    std::int64_t worker_ready = 0;
    std::int64_t worker_not_ready = 0;
    std::int64_t clock_ready = 0;
    std::int64_t clock_declined = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(EngineerState, 1, ZEN_FIELD(worker_ready), ZEN_FIELD(worker_not_ready),
              ZEN_FIELD(clock_ready), ZEN_FIELD(clock_declined));
};

/// THE COMPOSITION, in one class. Two candidate vocabularies, two handlers, and
/// the SAME three lines of handle work behind both.
class Engineer
    : public loom::WeaveBase<Engineer, EngineerState,
                             loom::Accept<maint::FleetDescribed, maint::MaintWorkerReady,
                                          maint::MaintWorkerNotReady,
                                          timer::TimerCandidatePrepared,
                                          timer::TimerCandidateDeclined>,
                             loom::Emit<maint::DescribeFleet, maint::PrepareMaintWorker,
                                        timer::PrepareTimerHandover>> {
public:
    explicit Engineer(EngineerDesk& desk) : desk_(&desk) {}

    void on(const maint::FleetDescribed& d, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        desk_->fleet = d.machines;
        desk_->fleet_checks = d.checks_run;
        desk_->fleet_described = true;
        desk_->notes.push_back("the worker services " + std::to_string(d.machines.size()) +
                               " machine(s) and has run " + std::to_string(d.checks_run) +
                               " check(s)");
    }

    // ---- this package's own service -----------------------------------------

    void on(const maint::MaintWorkerReady& r, loom::Mail&) {
        ++state_.worker_ready;
        desk_->notes.push_back("worker candidate says READY (fleet " +
                               std::to_string(r.fleet) + ")");
        offer(loom::PreparationAnswer::Ready);
    }

    void on(const maint::MaintWorkerNotReady& r, loom::Mail&) {
        ++state_.worker_not_ready;
        desk_->notes.push_back("worker candidate says NOT READY: " + r.reason);
        offer(loom::PreparationAnswer::Refused);
    }

    // ---- somebody else's service, and the handle does not notice ------------

    void on(const timer::TimerCandidatePrepared&, loom::Mail&) {
        ++state_.clock_ready;
        desk_->notes.push_back("CLOCK candidate says PREPARED");
        offer(loom::PreparationAnswer::Ready);
    }

    void on(const timer::TimerCandidateDeclined& d, loom::Mail&) {
        ++state_.clock_declined;
        desk_->notes.push_back("CLOCK candidate DECLINED: " + d.reason);
        offer(loom::PreparationAnswer::Refused);
    }

private:
    void offer(loom::PreparationAnswer answer) {
        if (desk_->upgrade == nullptr) {
            ++desk_->offered_without_handle;
            return;
        }
        desk_->offers.push_back(desk_->upgrade->offer_current_answer(answer));
    }
    EngineerDesk* desk_;
};

// ---- the supervisor: the party that asks for schedules ---------------------

struct SupervisorLog {
    std::vector<std::string> heard;
    std::int64_t reports = 0;
    std::int64_t unhealthy = 0;
    std::int64_t opens = 0;
    std::map<std::string, std::int64_t> runs_of;

    bool saw(const std::string& needle) const {
        for (const std::string& l : heard) {
            if (l.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    std::size_t count(const std::string& needle) const {
        std::size_t n = 0;
        for (const std::string& l : heard) {
            n += l.find(needle) != std::string::npos ? 1u : 0u;
        }
        return n;
    }
};

struct SupervisorState {
    std::int64_t acks = 0;
    std::int64_t refusals = 0;
    ZEN_SHAPE(SupervisorState, 1, ZEN_FIELD(acks), ZEN_FIELD(refusals));
};

/// A test-only nudge, granted explicitly rather than derived from `Emit<...>` —
/// F8, learned twice, applied without being bitten a third time.
struct AskScheduler {
    std::string kind; ///< "check" | "once" | "cancel" | "audit" | "late" | "status"
    std::string name;
    std::string machine;
    std::string action;
    std::int64_t n = 0;
    ZEN_SHAPE(AskScheduler, 1, ZEN_FIELD(kind), ZEN_FIELD(name), ZEN_FIELD(machine),
              ZEN_FIELD(action), ZEN_FIELD(n));
};

class Supervisor
    : public loom::WeaveBase<Supervisor, SupervisorState,
                             loom::Accept<AskScheduler, maint::HealthReport,
                                          maint::SchedulerOpen, loom::Result, loom::Ack,
                                          loom::Refused>,
                             loom::Emit<maint::ScheduleCheck, maint::ScheduleOnce,
                                        maint::CancelSchedule, maint::CancelAudit,
                                        maint::DeclareLateBinding, maint::MaintStatus>> {
public:
    Supervisor(SupervisorLog& log, std::vector<std::string>& status)
        : log_(&log), status_(&status) {}

    void on(const AskScheduler& a, loom::Mail& mail) {
        if (a.kind == "check") {
            mail.send_to_role(maint::kSchedulerRole,
                              maint::ScheduleCheck{a.name, a.machine, a.n}, corr_for(a.name));
        } else if (a.kind == "once") {
            mail.send_to_role(maint::kSchedulerRole,
                              maint::ScheduleOnce{a.name, a.machine, a.action, a.n},
                              corr_for(a.name));
        } else if (a.kind == "cancel") {
            mail.send_to_role(maint::kSchedulerRole, maint::CancelSchedule{a.name});
        } else if (a.kind == "audit") {
            mail.send_to_role(maint::kSchedulerRole, maint::CancelAudit{});
        } else if (a.kind == "late") {
            mail.send_to_role(maint::kSchedulerRole, maint::DeclareLateBinding{});
        } else {
            mail.send_to_role(maint::kSchedulerRole, maint::MaintStatus{});
        }
    }

    void on(const maint::HealthReport& r, loom::Mail&) {
        ++log_->reports;
        log_->unhealthy += r.ok ? 0 : 1;
        log_->runs_of[r.name] = r.run;
        log_->heard.push_back("report " + r.name + " " + r.machine + ": " +
                              (r.ok ? "ok" : "UNHEALTHY") + " (" + r.detail + ") run " +
                              std::to_string(r.run));
    }

    void on(const maint::SchedulerOpen& o, loom::Mail&) {
        ++log_->opens;
        log_->heard.push_back("scheduler open: " + std::to_string(o.bindings) +
                              " binding(s), " + std::to_string(o.adopted) + " schedule(s)");
    }

    void on(const loom::Result& r, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        status_->push_back(r.value);
    }
    void on(const loom::Ack&, loom::Mail&) { ++state_.acks; }
    void on(const loom::Refused& r, loom::Mail&) {
        ++state_.refusals;
        log_->heard.push_back("refused: " + r.reason);
    }

private:
    std::uint64_t corr_for(const std::string& name) {
        const auto it = corr_.find(name);
        if (it != corr_.end()) {
            return it->second;
        }
        const std::uint64_t c = ++next_;
        corr_[name] = c;
        return c;
    }

    SupervisorLog* log_;
    std::vector<std::string>* status_;
    std::map<std::string, std::uint64_t> corr_;
    std::uint64_t next_ = 500;
};

// ---- the fixture -----------------------------------------------------------

class Maintenance {
public:
    Maintenance() {
        control_ = loom::mount_control(kernel_, bus_);
        manager_ = loom::mount_manager(control_, bus_);

        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager_);
        op_ = loom::mount_granted<Operator>(bus_, std::move(reach), oplog_);

        engineer_ = loom::mount<Engineer>(bus_, desk_);

        loom::Grant sup;
        sup.allow_to_any(AskScheduler::zen_name, AskScheduler::zen_version);
        sup.allow_to_role(maint::ScheduleCheck::zen_name, maint::ScheduleCheck::zen_version,
                          maint::kSchedulerRole);
        sup.allow_to_role(maint::ScheduleOnce::zen_name, maint::ScheduleOnce::zen_version,
                          maint::kSchedulerRole);
        sup.allow_to_role(maint::CancelSchedule::zen_name, maint::CancelSchedule::zen_version,
                          maint::kSchedulerRole);
        sup.allow_to_role(maint::CancelAudit::zen_name, maint::CancelAudit::zen_version,
                          maint::kSchedulerRole);
        sup.allow_to_role(maint::DeclareLateBinding::zen_name,
                          maint::DeclareLateBinding::zen_version, maint::kSchedulerRole);
        sup.allow_to_role(maint::MaintStatus::zen_name, maint::MaintStatus::zen_version,
                          maint::kSchedulerRole);
        supervisor_ = loom::mount_granted<Supervisor>(bus_, std::move(sup), suplog_, status_);

        if (const char* on = std::getenv("MARATHON_TRACE"); on != nullptr && *on == '1') {
            bus_.add_observer([](const loom::BusEvent& ev) {
                const char* kind = ev.kind == loom::EventKind::Delivered  ? "deliver"
                                   : ev.kind == loom::EventKind::Refused  ? "REFUSED"
                                                                          : "life";
                std::fprintf(stderr, "[trace] %-7s %s v%u  %llu -> %llu  %s\n", kind,
                             ev.schema_name.c_str(), ev.schema_version,
                             static_cast<unsigned long long>(ev.sender.value),
                             static_cast<unsigned long long>(ev.target.value),
                             ev.kind == loom::EventKind::Refused ? ev.refusal.message().c_str()
                                                                 : "");
            });
        }

        bus_.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered ||
                ev.schema_name != timer::Drive::zen_name) {
                return;
            }
            ++beats_;
            if (budget_ > 0 && --budget_ == 0) {
                bus_.stop();
            }
        });
    }

    loom::Switchboard& bus() { return bus_; }
    loom::Kernel& kernel() { return kernel_; }
    EngineerDesk& desk() { return desk_; }
    SupervisorLog& sup() { return suplog_; }
    OperatorLog& oplog() { return oplog_; }
    loom::WeaveId engineer() const { return engineer_; }
    const std::vector<std::string>& status() const { return status_; }
    std::int64_t beats() const { return beats_; }

    std::vector<std::string>* watch(loom::WeaveId who) {
        watched_.push_back(std::vector<std::string>{});
        std::vector<std::string>* sink = &watched_.back();
        bus_.add_observer([sink, who](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Delivered && ev.target == who) {
                sink->push_back(ev.schema_name);
            }
        });
        return sink;
    }

    template <class Cmd>
    std::uint64_t command(std::string label, const Cmd& cmd) {
        const std::uint64_t corr = oplog_.next_correlation++;
        oplog_.pending[corr] = std::move(label);
        bus_.send_as(op_, manager_, loom::Message(loom::to_value(cmd), op_, op_, corr));
        return corr;
    }

    std::uint64_t load(const std::string& stem, const std::string& role = "") {
        return command("load " + stem, loom::LoadWeave{stem, weave_path(stem), role});
    }

    void ask(const std::string& kind, const std::string& name = {},
             const std::string& machine = {}, std::int64_t n = 0,
             const std::string& action = {}) {
        bus_.send_as(supervisor_, supervisor_,
                     loom::Message(loom::to_value(AskScheduler{kind, name, machine, action, n}),
                                   supervisor_, supervisor_, 0));
    }

    void describe_fleet() {
        desk_.fleet_described = false;
        bus_.send_as_to_role(engineer_, maint::kWorkerRole,
                             loom::Message(loom::to_value(maint::DescribeFleet{}), engineer_,
                                           engineer_, 0));
    }

    loom::PreparedReplacement& new_upgrade() {
        upgrade_ = loom::PreparedReplacement(bus_, kernel_);
        desk_.upgrade = &upgrade_;
        return upgrade_;
    }
    loom::PreparedReplacement& upgrade() { return upgrade_; }
    void forget_upgrade() { desk_.upgrade = nullptr; }

    /// Start a replacement of ANY role through the handle. The two candidate
    /// vocabularies differ; this call does not.
    loom::PreparedReplacement::StartResult begin(const std::string& role,
                                                 const std::string& stem,
                                                 std::uint32_t budget = 16) {
        return new_upgrade().start({
            .operator_id = engineer_,
            .coordinator = engineer_,
            .role = role,
            .candidate_name = stem,
            .candidate_path = weave_path(stem),
            .budget = budget,
        });
    }

    void boot(const std::string& clock = "zengine-timer-virtual",
              const std::string& worker = "maint-worker") {
        load(clock, timer::kTimerRole);
        load(worker, maint::kWorkerRole);
        load("maint-scheduler", maint::kSchedulerRole);
        pump(30);
    }

    void pump(std::int64_t beats) {
        budget_ = beats;
        bus_.pump();
        budget_ = 0;
    }

private:
    loom::Switchboard bus_;
    loom::Kernel kernel_{bus_};
    loom::WeaveId control_{};
    loom::WeaveId manager_{};
    loom::WeaveId op_{};
    loom::WeaveId engineer_{};
    loom::WeaveId supervisor_{};
    OperatorLog oplog_;
    EngineerDesk desk_;
    SupervisorLog suplog_;
    std::vector<std::string> status_;
    loom::PreparedReplacement upgrade_{bus_, kernel_};
    std::deque<std::vector<std::string>> watched_;
    std::int64_t budget_ = 0;
    std::int64_t beats_ = 0;
};

} // namespace marathon::maint_testing

#endif // MARATHON_MAINT_HARNESS_HPP
