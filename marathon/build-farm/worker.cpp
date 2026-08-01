// A build worker — capacity one, and that is the whole point.
//
// One source, several libraries. `FARM_WORKER_NAME` picks the slot this artifact
// agrees to fill; `FARM_WORKER_LABEL` names the generation so a tap can tell two
// of them apart without changing a rule:
//
//     farm-worker-a  / farm-worker-a2      worker "a"
//     farm-worker-b                        worker "b"
//
// WHAT A WORKER OWNS. One assignment, a stage cursor, and a step counter.
// Nothing else. It does not know who asked, does not know what a queue is, has
// no opinion about priority, and cannot see another worker. It answers exactly
// one question — "will you build this, and how far have you got?" — which is why
// its description is four strings and why its successor can be a different
// library.
//
// TIME IS ASKED FOR, NEVER READ. The worker holds no clock: it asks the Zengine
// Timer package for a repeating beat addressed to its own ROLE and takes one
// step per beat. Role-addressed, so a successor inherits the pulse.
//
// ---- THE CONTINUITY CONTRACT, FROM THE WORKER'S SIDE ------------------------
//
// A worker that is replaced hands over the INTENT of what it was doing —
// project, revision, target — and not one byte of its progress. The successor
// starts at stage zero as a new ATTEMPT.
//
// This is the opposite of the kitchen (which carried `passes_left` because
// progress was the only thing that existed nowhere else) and the opposite of the
// download manager (which could carry nothing at all because its progress WAS
// its bytes). The difference is not a preference: a build is re-derivable from
// three strings and a download is not, and neither of those is a fact about
// Loom. WHETHER CONTINUITY IS POSSIBLE IS A PROPERTY OF THE DOMAIN.

#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace marathon::farm;
namespace timer = zengine::timer;

#if defined(FARM_WORKER_NAME)
constexpr const char* kWorkerName = FARM_WORKER_NAME;
#else
constexpr const char* kWorkerName = "a";
#endif

#if defined(FARM_WORKER_LABEL)
constexpr const char* kLabel = FARM_WORKER_LABEL;
#else
constexpr const char* kLabel = "gen1";
#endif

/// What this artifact says its toolchain is, when a coordinator asks during
/// preparation. Two generations of the same worker slot may honestly differ.
#if defined(FARM_WORKER_TOOLCHAIN)
constexpr const char* kToolchain = FARM_WORKER_TOOLCHAIN;
#else
constexpr const char* kToolchain = "house";
#endif

/// The one assignment, plus where it has got to. `stage` and `step` are the
/// parts that do NOT cross a replacement.
struct Work {
    std::string job;
    std::string project;
    std::string revision;
    std::string target;
    std::int64_t attempt = 1;
    std::int64_t stage = 0; ///< index into kStages
    std::int64_t step = 0;  ///< steps done within this stage
    ZEN_SHAPE(Work, 1, ZEN_FIELD(job), ZEN_FIELD(project), ZEN_FIELD(revision),
              ZEN_FIELD(target), ZEN_FIELD(attempt), ZEN_FIELD(stage), ZEN_FIELD(step));
};

struct WorkerState {
    std::vector<Work> holding; ///< at most kWorkerCapacity
    std::int64_t assigned = 0;
    std::int64_t declined = 0;
    std::int64_t finished = 0;
    std::int64_t abandoned = 0;
    std::int64_t resumed = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(WorkerState, 1, ZEN_FIELD(holding), ZEN_FIELD(assigned), ZEN_FIELD(declined),
              ZEN_FIELD(finished), ZEN_FIELD(abandoned), ZEN_FIELD(resumed));
};

class Worker
    : public loom::WeaveBase<
          Worker, WorkerState,
          loom::Accept<AssignBuild, AbandonJob, DescribeAssignments, PrepareWorker, ToolchainIs,
                       loom::Activated, timer::TimerReady, timer::TimerFired>,
          loom::Emit<AssignDeclined, StageDone, JobDone, WorkerOpen, AssignmentsDescribed,
                     WorkerReady, WorkerNotReady, AskToolchain, timer::StartRoleTimer>> {
public:
    // ---- arrival ------------------------------------------------------------

    /// This incarnation is live. It announces WHAT IT IS ACTUALLY HOLDING, which
    /// is the dispatcher's reconciliation input: a fresh incarnation that resumed
    /// nothing says so, and the dispatcher requeues at once instead of waiting
    /// for a timeout.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        announce(mail);
        ask_for_the_pulse(mail);
    }

    void on(const timer::TimerReady&, loom::Mail& mail) { ask_for_the_pulse(mail); }

    // ---- the work -----------------------------------------------------------

    /// Take it, or say why not. A decline is Loom's AUTHENTICATED answer, safe to
    /// spend immediately because nothing has to survive a replacement for it to
    /// arrive. Acceptance is silent: the stages are the answer.
    void on(const AssignBuild& a, loom::Mail& mail) {
        if (state_.holding.size() >= kWorkerCapacity) {
            decline(mail, a.job, "worker '" + std::string(kWorkerName) + "' is already building '" +
                                     state_.holding[0].job + "'");
            return;
        }
        if (find_target(a.target) == nullptr) {
            decline(mail, a.job, "worker '" + std::string(kWorkerName) +
                                     "' has no recipe for target '" + a.target + "'");
            return;
        }
        state_.holding.push_back(Work{a.job, a.project, a.revision, a.target, a.attempt, 0, 0});
        ++state_.assigned;
    }

    /// One beat is one step. A stage that completes is announced; a target that
    /// dies in this stage ends the job here.
    void on(const timer::TimerFired& f, loom::Mail& mail) {
        if (f.id != kStepTimerId || state_.holding.empty()) {
            return; // an id this worker never asked for is DATA, not a drive
        }
        Work& w = state_.holding[0];
        const Target* t = find_target(w.target);
        if (t == nullptr) {
            finish(mail, false, std::string(kStages[0]), "the target vanished from this "
                                                         "worker's recipes mid-build");
            return;
        }
        if (++w.step < t->steps_per_stage) {
            return;
        }
        w.step = 0;
        const std::size_t stage = static_cast<std::size_t>(w.stage);
        if (stage == t->dies_at) {
            finish(mail, false, kStages[stage],
                   "target '" + w.target + "' does not survive its '" +
                       std::string(kStages[stage]) + "' stage");
            return;
        }
        mail.send_to_role(kDispatcherRole,
                          StageDone{w.job, kWorkerName, kStages[stage], w.stage, w.attempt});
        if (stage + 1 >= kStageCount) {
            finish(mail, true, std::string{},
                   w.project + "-" + w.revision + "-" + w.target + ".out");
            return;
        }
        ++w.stage;
    }

    /// "Stop that one." The dispatcher has taken the build back; nothing is owed
    /// and nothing is said about it, because the party that asked already knows.
    void on(const AbandonJob& a, loom::Mail&) {
        for (std::size_t i = 0; i < state_.holding.size(); ++i) {
            if (state_.holding[i].job == a.job) {
                state_.holding.erase(state_.holding.begin() + static_cast<std::ptrdiff_t>(i));
                ++state_.abandoned;
                return;
            }
        }
    }

    // ---- the replacement conversation ---------------------------------------

    /// "What are you working on?" Asked of the LIVE incumbent. It changes
    /// nothing: no step is skipped and no dispatcher is told anything.
    ///
    /// What comes back is INTENT ONLY. `stage` and `step` are deliberately absent
    /// from `AssignedJob` — a successor that was handed a stage cursor would be
    /// claiming to have compiled something it has never seen.
    void on(const DescribeAssignments&, loom::Mail& mail) {
        AssignmentsDescribed described;
        described.worker = kWorkerName;
        for (const Work& w : state_.holding) {
            described.jobs.push_back(
                AssignedJob{w.job, w.project, w.revision, w.target, w.attempt});
        }
        (void)mail.answer(described);
    }

    /// THE PREPARATION ASK, heard from inside the seal. This weave holds no role,
    /// its beat does not reach it, and the only party it may speak to is the
    /// coordinator that sealed it.
    void on(const PrepareWorker& p, loom::Mail& mail) {
        if (p.worker != kWorkerName) {
            refuse_prep(mail, "this artifact is worker '" + std::string(kWorkerName) +
                                  "' and was asked to become '" + p.worker + "'");
            return;
        }
        if (p.resume.size() > kWorkerCapacity) {
            refuse_prep(mail, "a worker holds at most " + std::to_string(kWorkerCapacity) +
                                  " build(s) and this preparation hands over " +
                                  std::to_string(p.resume.size()));
            return;
        }
        for (const AssignedJob& j : p.resume) {
            if (find_target(j.target) == nullptr) {
                refuse_prep(mail, "this worker has no recipe for target '" + j.target +
                                      "', so it cannot resume build '" + j.job + "'");
                return;
            }
            if (j.attempt >= kMaxAttempts) {
                refuse_prep(mail, "build '" + j.job + "' has already had " +
                                      std::to_string(j.attempt) +
                                      " attempts; resuming it would exceed the farm's bound");
                return;
            }
        }
        if (p.consult) {
            pending_ = mail.defer_answer();
            if (!pending_.valid()) {
                refuse_prep(mail, "this preparation carried no answer authority to hold");
                return;
            }
            staged_ = p.resume;
            mail.send(mail.sender(), AskToolchain{kWorkerName});
            return;
        }
        become_ready(mail, p.resume);
    }

    /// The coordinator's answer to the candidate's own question.
    void on(const ToolchainIs& t, loom::Mail& mail) {
        if (!mail.answers_ask() || !pending_.valid()) {
            return;
        }
        std::vector<AssignedJob> staged;
        staged.swap(staged_);
        if (t.name != kToolchain) {
            (void)loom::answer_deferred(
                pending_, mail,
                WorkerNotReady{kWorkerName, "the farm runs the '" + t.name +
                                                "' toolchain and this artifact is built for '" +
                                                std::string(kToolchain) + "'"});
            pending_ = loom::DeferredAnswer{};
            return;
        }
        resume(staged);
        (void)loom::answer_deferred(pending_, mail,
                                    WorkerReady{kWorkerName, static_cast<std::int64_t>(
                                                                 state_.holding.size())});
        pending_ = loom::DeferredAnswer{};
    }

private:
    static std::string my_role() { return worker_role(kWorkerName); }

    void announce(loom::Mail& mail) {
        WorkerOpen hello;
        hello.worker = kWorkerName;
        hello.capacity = static_cast<std::int64_t>(kWorkerCapacity);
        for (const Work& w : state_.holding) {
            hello.holding.push_back(w.job);
        }
        mail.publish(hello);
    }

    void ask_for_the_pulse(loom::Mail& mail) {
        mail.send_to_role(timer::kTimerRole,
                          timer::StartRoleTimer{kStepTimerId, kStepMs, /*repeat=*/true,
                                                my_role()});
    }

    void decline(loom::Mail& mail, const std::string& job, std::string reason) {
        ++state_.declined;
        (void)mail.answer(AssignDeclined{job, kWorkerName, std::move(reason)});
    }

    void refuse_prep(loom::Mail& mail, std::string reason) {
        (void)mail.answer(WorkerNotReady{kWorkerName, std::move(reason)});
    }

    void become_ready(loom::Mail& mail, const std::vector<AssignedJob>& jobs) {
        resume(jobs);
        (void)mail.answer(
            WorkerReady{kWorkerName, static_cast<std::int64_t>(state_.holding.size())});
    }

    /// ONE resumption rule. The stage cursor starts at zero and the attempt
    /// number goes up — the successor is honest about being a new attempt rather
    /// than pretending to be the same one.
    void resume(const std::vector<AssignedJob>& jobs) {
        for (const AssignedJob& j : jobs) {
            if (state_.holding.size() >= kWorkerCapacity) {
                break;
            }
            state_.holding.push_back(
                Work{j.job, j.project, j.revision, j.target, j.attempt + 1, 0, 0});
            ++state_.resumed;
        }
    }

    void finish(loom::Mail& mail, bool ok, std::string stage, std::string detail) {
        const Work w = state_.holding[0];
        state_.holding.erase(state_.holding.begin());
        ++state_.finished;
        mail.send_to_role(kDispatcherRole, JobDone{w.job, kWorkerName, ok, std::move(stage),
                                                   std::move(detail), w.attempt});
    }

    zengine::ActivationCursor activation_;
    loom::DeferredAnswer pending_;
    std::vector<AssignedJob> staged_;
};

} // namespace

static_assert(kLabel[0] != '\0', "a worker generation needs a label");

ZEN_EXPORT_WEAVE(Worker)
