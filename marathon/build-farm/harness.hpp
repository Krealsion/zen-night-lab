#ifndef MARATHON_FARM_HARNESS_HPP
#define MARATHON_FARM_HARNESS_HPP

// The build farm's host: a REAL host running REAL .so weaves through the REAL
// kernel, the REAL Weave Manager and REAL prepared replacements, with exactly
// one substitution — the Timer's CLOCK (`zengine-timer-virtual.so`, the Timer
// package's own suite artifact, whose nap books the duration and returns).
//
// THIRD PROJECT, THIRD TIME WRITING THIS FILE. What repeats, verbatim in shape
// if not in name, across kitchen-replay, download-manager and here:
//
//     an Operator weave holding a target-scoped grant on the Weave Manager
//     a coordinator weave holding a raw `loom::PreparedReplacement*`
//     a Rogue weave with hand-written `allow_to_any` grants
//     a beat-counting observer that stops the bus on a budget
//     a `watch(WeaveId)` tap that records delivery ORDER
//     a MARATHON_TRACE=1 observer
//
// Recorded in FRICTION.md, not extracted. A test fixture repeating is weaker
// evidence than an application repeating — but three times is three times, and
// the last two items are the ones that had to be invented rather than copied
// from anything Zen ships.

#include "requester.hpp"
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

namespace marathon::farm_testing {

namespace farm = marathon::farm;
namespace timer = zengine::timer;

#ifndef MARATHON_FARM_RUNTIME_DIR
#error "MARATHON_FARM_RUNTIME_DIR must be defined by the build"
#endif

inline std::string weave_path(const std::string& stem) {
    return std::string(MARATHON_FARM_RUNTIME_DIR) + "/" + stem + ".so";
}

// ---- the operator ----------------------------------------------------------

struct OperatorLog {
    std::map<std::uint64_t, std::string> pending;
    std::vector<std::string> answers;
    std::uint64_t next_correlation = 1;

    std::string answer_for(const std::string& needle) const {
        std::string found;
        for (const std::string& a : answers) {
            if (a.find(needle) != std::string::npos) {
                found = a;
            }
        }
        return found;
    }
};

struct OperatorState {
    std::int64_t answers = 0;
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

class Operator : public loom::WeaveBase<Operator, OperatorState,
                                        loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                        loom::Emit<loom::LoadWeave, loom::SwapWeave,
                                                   loom::ListLoaded>> {
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

// ---- the shift lead: coordinator of a worker replacement -------------------

struct LeadDesk {
    loom::PreparedReplacement* upgrade = nullptr;
    std::vector<loom::TxnResult> offers;
    std::vector<farm::AssignedJob> described;
    std::string described_worker;
    bool described_arrived = false;
    std::vector<std::string> notes;
    std::string toolchain_answer = "house";
    std::int64_t offered_without_handle = 0;
    std::int64_t unattested = 0;
};

struct LeadState {
    std::int64_t descriptions = 0;
    std::int64_t ready = 0;
    std::int64_t not_ready = 0;
    std::int64_t consulted = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(LeadState, 1, ZEN_FIELD(descriptions), ZEN_FIELD(ready), ZEN_FIELD(not_ready),
              ZEN_FIELD(consulted));
};

class ShiftLead : public loom::WeaveBase<ShiftLead, LeadState,
                                         loom::Accept<farm::AssignmentsDescribed,
                                                      farm::WorkerReady, farm::WorkerNotReady,
                                                      farm::AskToolchain>,
                                         loom::Emit<farm::DescribeAssignments,
                                                    farm::PrepareWorker, farm::ToolchainIs>> {
public:
    explicit ShiftLead(LeadDesk& desk) : desk_(&desk) {}

    void on(const farm::AssignmentsDescribed& d, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ++desk_->unattested;
            return;
        }
        ++state_.descriptions;
        desk_->described = d.jobs;
        desk_->described_worker = d.worker;
        desk_->described_arrived = true;
        desk_->notes.push_back("worker " + d.worker + " is holding " +
                               std::to_string(d.jobs.size()) + " build(s)");
    }

    void on(const farm::WorkerReady& r, loom::Mail&) {
        ++state_.ready;
        desk_->notes.push_back("candidate says READY as '" + r.worker + "' (resumed " +
                               std::to_string(r.resumed) + ")");
        offer(loom::PreparationAnswer::Ready);
    }

    void on(const farm::WorkerNotReady& r, loom::Mail&) {
        ++state_.not_ready;
        desk_->notes.push_back("candidate says NOT READY: " + r.reason);
        offer(loom::PreparationAnswer::Refused);
    }

    void on(const farm::AskToolchain&, loom::Mail& mail) {
        ++state_.consulted;
        (void)mail.answer(farm::ToolchainIs{desk_->toolchain_answer});
    }

private:
    void offer(loom::PreparationAnswer answer) {
        if (desk_->upgrade == nullptr) {
            ++desk_->offered_without_handle;
            return;
        }
        desk_->offers.push_back(desk_->upgrade->offer_current_answer(answer));
    }
    LeadDesk* desk_;
};

// ---- a rogue ---------------------------------------------------------------

struct RogueState {
    std::int64_t sent = 0;
    ZEN_SHAPE(RogueState, 1, ZEN_FIELD(sent));
};

/// "Tell the dispatcher this job is done." The forgery this domain invites.
struct ForgeJobDone {
    std::string job;
    std::string worker;
    bool ok = true;
    std::string detail;
    ZEN_SHAPE(ForgeJobDone, 1, ZEN_FIELD(job), ZEN_FIELD(worker), ZEN_FIELD(ok),
              ZEN_FIELD(detail));
};

/// "Tell the dispatcher a worker just arrived holding nothing." The forgery
/// aimed at RECONCILIATION, which is this project's alternative to a watchdog —
/// and therefore this project's new attack surface.
struct ForgeWorkerOpen {
    std::string worker;
    ZEN_SHAPE(ForgeWorkerOpen, 1, ZEN_FIELD(worker));
};

struct ForgeWorkerReady {
    std::string worker;
    ZEN_SHAPE(ForgeWorkerReady, 1, ZEN_FIELD(worker));
};

class Rogue : public loom::WeaveBase<Rogue, RogueState,
                                     loom::Accept<ForgeJobDone, ForgeWorkerOpen,
                                                  ForgeWorkerReady>,
                                     loom::Emit<farm::JobDone, farm::WorkerOpen,
                                                farm::WorkerReady>> {
public:
    explicit Rogue(loom::WeaveId lead) : lead_(lead) {}

    void on(const ForgeJobDone& f, loom::Mail& mail) {
        ++state_.sent;
        mail.send_to_role(farm::kDispatcherRole,
                          farm::JobDone{f.job, f.worker, f.ok, std::string{}, f.detail});
    }

    void on(const ForgeWorkerOpen& f, loom::Mail& mail) {
        ++state_.sent;
        farm::WorkerOpen hello;
        hello.worker = f.worker;
        hello.capacity = 1;
        mail.publish(hello); // holding nothing: the reconciliation lie
    }

    void on(const ForgeWorkerReady& f, loom::Mail& mail) {
        ++state_.sent;
        mail.send(lead_, farm::WorkerReady{f.worker, 0});
    }

private:
    loom::WeaveId lead_;
};

// ---- the inspector ---------------------------------------------------------

struct InspectorState {
    std::int64_t results = 0;
    std::int64_t ignored = 0;
    ZEN_SHAPE(InspectorState, 1, ZEN_FIELD(results), ZEN_FIELD(ignored));
};

struct AskStatus {
    ZEN_SHAPE(AskStatus, 1);
};

class Inspector : public loom::WeaveBase<Inspector, InspectorState,
                                         loom::Accept<AskStatus, loom::Result>,
                                         loom::Emit<farm::FarmStatus>> {
public:
    explicit Inspector(std::vector<std::string>& seen) : seen_(&seen) {}
    void on(const AskStatus&, loom::Mail& mail) {
        mail.send_to_role(farm::kDispatcherRole, farm::FarmStatus{});
    }
    void on(const loom::Result& r, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ++state_.ignored;
            return;
        }
        ++state_.results;
        seen_->push_back(r.value);
    }

private:
    std::vector<std::string>* seen_;
};

// ---- the fixture -----------------------------------------------------------

class Farm {
public:
    Farm() {
        control_ = loom::mount_control(kernel_, bus_);
        manager_ = loom::mount_manager(control_, bus_);

        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager_);
        reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager_);
        reach.allow(loom::ListLoaded::zen_name, loom::ListLoaded::zen_version, manager_);
        op_ = loom::mount_granted<Operator>(bus_, std::move(reach), oplog_);

        lead_ = loom::mount<ShiftLead>(bus_, desk_);
        requester_ = loom::mount<farm::Requester>(bus_, book_);
        second_ = loom::mount<farm::Requester>(bus_, second_book_);

        loom::Grant rogue_grant;
        rogue_grant.allow_to_any(ForgeJobDone::zen_name, ForgeJobDone::zen_version);
        rogue_grant.allow_to_any(ForgeWorkerOpen::zen_name, ForgeWorkerOpen::zen_version);
        rogue_grant.allow_to_any(ForgeWorkerReady::zen_name, ForgeWorkerReady::zen_version);
        rogue_grant.allow_to_any(farm::JobDone::zen_name, farm::JobDone::zen_version);
        rogue_grant.allow_to_any(farm::WorkerOpen::zen_name, farm::WorkerOpen::zen_version);
        rogue_grant.allow_to_any(farm::WorkerReady::zen_name, farm::WorkerReady::zen_version);
        rogue_ = loom::mount_granted<Rogue>(bus_, std::move(rogue_grant), lead_);

        loom::Grant inspector_grant;
        inspector_grant.allow_to_any(AskStatus::zen_name, AskStatus::zen_version);
        inspector_grant.allow_to_role(farm::FarmStatus::zen_name, farm::FarmStatus::zen_version,
                                      farm::kDispatcherRole);
        inspector_ = loom::mount_granted<Inspector>(bus_, std::move(inspector_grant), status_);

        if (const char* on = std::getenv("MARATHON_TRACE"); on != nullptr && *on == '1') {
            bus_.add_observer([](const loom::BusEvent& ev) {
                const char* kind = ev.kind == loom::EventKind::Delivered  ? "deliver"
                                   : ev.kind == loom::EventKind::Refused  ? "REFUSED"
                                   : ev.kind == loom::EventKind::Died     ? "died"
                                                                          : "revived";
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
    farm::RequesterBook& book() { return book_; }
    farm::RequesterBook& second_book() { return second_book_; }
    OperatorLog& oplog() { return oplog_; }
    LeadDesk& desk() { return desk_; }
    loom::WeaveId lead() const { return lead_; }
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

    /// Make a worker walk out with nothing to replace it — the case
    /// reconciliation CANNOT cover, because nobody arrives to say anything.
    std::uint64_t evict(const std::string& role) {
        return command("evict " + role,
                       loom::SwapWeave{role, "no-such-worker",
                                       std::string(MARATHON_FARM_RUNTIME_DIR) +
                                           "/no-such-worker.so",
                                       /*graceful=*/false});
    }

    std::uint64_t submit(const std::string& id, const std::string& target,
                         const std::string& project = "zen", const std::string& revision = "r1") {
        const std::uint64_t corr = book_.open(id);
        bus_.send_as_to_role(requester_, farm::kDispatcherRole,
                             loom::Message(loom::to_value(farm::SubmitBuild{id, project, revision,
                                                                            target}),
                                           requester_, requester_, corr));
        return corr;
    }

    std::uint64_t submit_second(const std::string& id, const std::string& target) {
        const std::uint64_t corr = second_book_.open(id);
        bus_.send_as_to_role(second_, farm::kDispatcherRole,
                             loom::Message(loom::to_value(farm::SubmitBuild{id, "other", "r9",
                                                                            target}),
                                           second_, second_, corr));
        return corr;
    }

    void withdraw(const std::string& id, std::uint64_t correlation) {
        bus_.send_as_to_role(requester_, farm::kDispatcherRole,
                             loom::Message(loom::to_value(farm::WithdrawBuild{id}), requester_,
                                           requester_, correlation));
    }

    void describe_assignments(const std::string& worker) {
        desk_.described_arrived = false;
        bus_.send_as_to_role(lead_, farm::worker_role(worker),
                             loom::Message(loom::to_value(farm::DescribeAssignments{}), lead_,
                                           lead_, 0));
    }

    template <class T>
    void rogue_does(const T& order_) {
        bus_.send_as(rogue_, rogue_, loom::Message(loom::to_value(order_), rogue_, rogue_, 0));
    }

    void ask_status() {
        bus_.send_as(inspector_, inspector_,
                     loom::Message(loom::to_value(AskStatus{}), inspector_, inspector_, 0));
    }

    loom::PreparedReplacement& new_upgrade() {
        upgrade_ = loom::PreparedReplacement(bus_, kernel_);
        desk_.upgrade = &upgrade_;
        return upgrade_;
    }
    loom::PreparedReplacement& upgrade() { return upgrade_; }
    void forget_upgrade() { desk_.upgrade = nullptr; }

    /// Boot a farm: a clock, a dispatcher, and two workers.
    void boot(int workers = 2) {
        load("zengine-timer-virtual", timer::kTimerRole);
        load("farm-dispatcher", farm::kDispatcherRole);
        load("farm-worker-a", farm::worker_role("a"));
        if (workers > 1) {
            load("farm-worker-b", farm::worker_role("b"));
        }
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
    loom::WeaveId lead_{};
    loom::WeaveId requester_{};
    loom::WeaveId second_{};
    loom::WeaveId rogue_{};
    loom::WeaveId inspector_{};
    OperatorLog oplog_;
    LeadDesk desk_;
    farm::RequesterBook book_;
    farm::RequesterBook second_book_;
    std::vector<std::string> status_;
    loom::PreparedReplacement upgrade_{bus_, kernel_};
    std::deque<std::vector<std::string>> watched_;
    std::int64_t budget_ = 0;
    std::int64_t beats_ = 0;
};

} // namespace marathon::farm_testing

#endif // MARATHON_FARM_HARNESS_HPP
