// The dispatcher — the party that HOLDS THE PROMISE, owns the queue, and is the
// only one that can tell a requester the truth.
//
// It knows no recipes, runs no builds, and has no opinion about toolchains. It
// owns four things and nothing else: capacity, the ORDER work is done in, the
// promise, and the farm's belief about who is here.
//
// ---- TWO ANSWERS TO ABSENCE, AND THEY COVER DIFFERENT CASES -----------------
//
// The kitchen had one: a watchdog, because a station that vanished produced
// nothing at all and the only evidence of absence was a promise going unkept.
// This farm has two, deliberately, because two different things can happen:
//
//   RECONCILIATION — somebody ARRIVES. A worker announces `WorkerOpen` on every
//   activation, and it lists what it is ACTUALLY holding. Anything this
//   dispatcher believes that worker has, and the worker does not list, died with
//   the incarnation that held it. That is known IMMEDIATELY, with no timeout, and
//   it is the case that a replacement always produces.
//
//   THE SWEEP — nobody arrives. A worker evicted with nothing taking its place
//   produces silence, and silence still needs a clock. The sweep spends patience
//   per assignment and requeues when it runs out.
//
// Neither subsumes the other, and knowing which covers which is the whole reason
// the farm is not just the kitchen again.
//
// ---- THE CONTINUITY CONTRACT ------------------------------------------------
//
// A build interrupted by anything is REQUEUED as a new ATTEMPT, not failed —
// because a build is re-derivable from its intent and its intent is three
// strings. Bounded at `kMaxAttempts`, so "resume it" cannot become "loop
// forever", and every ending is still a message.

#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using namespace marathon::farm;
namespace timer = zengine::timer;

/// One build in the farm's book. `worker` empty means it is queued.
struct Build {
    std::string job;         ///< the FARM's name for it
    std::string id;          ///< the REQUESTER's name for it
    std::string requester;   ///< canonical decimal of the requester's WeaveId
    std::int64_t correlation = 0;
    std::string project;
    std::string revision;
    std::string target;
    std::string worker;      ///< empty while queued
    /// Who had it last. A requeued build PREFERS a different worker, because the
    /// commonest reason a build came back is that something is wrong where it
    /// was — and trying the same place first is how a farm turns one bad worker
    /// into three wasted attempts.
    std::string last_worker;
    std::int64_t attempt = 1;
    std::int64_t patience = 0;
    ZEN_SHAPE(Build, 1, ZEN_FIELD(job), ZEN_FIELD(id), ZEN_FIELD(requester),
              ZEN_FIELD(correlation), ZEN_FIELD(project), ZEN_FIELD(revision), ZEN_FIELD(target),
              ZEN_FIELD(worker), ZEN_FIELD(last_worker), ZEN_FIELD(attempt),
              ZEN_FIELD(patience));
};

struct DispatcherState {
    std::vector<Build> builds; ///< queue order IS vector order
    std::vector<std::string> workers;
    std::int64_t next_job = 1;
    std::int64_t accepted = 0;
    std::int64_t rejected = 0;
    std::int64_t succeeded = 0;
    std::int64_t failed = 0;
    std::int64_t requeued_by_reconciliation = 0;
    std::int64_t requeued_by_sweep = 0;
    std::int64_t withdrawn = 0;
    std::int64_t ignored = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(DispatcherState, 1, ZEN_FIELD(builds), ZEN_FIELD(workers), ZEN_FIELD(next_job),
              ZEN_FIELD(accepted), ZEN_FIELD(rejected), ZEN_FIELD(succeeded), ZEN_FIELD(failed),
              ZEN_FIELD(requeued_by_reconciliation), ZEN_FIELD(requeued_by_sweep),
              ZEN_FIELD(withdrawn), ZEN_FIELD(ignored));
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

class Dispatcher
    : public loom::WeaveBase<
          Dispatcher, DispatcherState,
          loom::Accept<SubmitBuild, WithdrawBuild, FarmStatus, AssignDeclined, StageDone, JobDone,
                       WorkerOpen, loom::Activated, timer::TimerReady, timer::TimerFired>,
          loom::Emit<BuildAccepted, BuildRejected, BuildProgress, BuildSucceeded, BuildFailed,
                     AssignBuild, AbandonJob, timer::StartRoleTimer, loom::Result, loom::Ack,
                     loom::Refused>> {
public:
    // ---- arrival ------------------------------------------------------------

    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        ask_for_the_sweep(mail);
    }

    void on(const timer::TimerReady&, loom::Mail& mail) { ask_for_the_sweep(mail); }

    // ---- the front desk -----------------------------------------------------

    /// Take responsibility, or say why not. Two things are decided here and
    /// nothing else: is there room, and is this build already open.
    ///
    /// THE TARGET IS NOT CHECKED HERE, deliberately. The dispatcher knows no
    /// recipes; a worker is the authority on what it can build and says so by
    /// declining. That split is what lets a worker be replaced by one with a
    /// different catalogue without touching this weave.
    void on(const SubmitBuild& s, loom::Mail& mail) {
        const std::string requester = std::to_string(mail.sender().value);
        if (state_.builds.size() >= kMaxOpenBuilds) {
            reject(mail, s.id, "the farm is holding its maximum of " +
                                   std::to_string(kMaxOpenBuilds) + " builds");
            return;
        }
        for (const Build& b : state_.builds) {
            if (b.id == s.id && b.requester == requester) {
                reject(mail, s.id, "you already have a build named '" + s.id + "' in this farm");
                return;
            }
        }
        if (s.project.empty() || s.revision.empty() || s.target.empty()) {
            reject(mail, s.id, "a build needs a project, a revision and a target; this one names "
                               "only " +
                                   std::to_string((s.project.empty() ? 0 : 1) +
                                                  (s.revision.empty() ? 0 : 1) +
                                                  (s.target.empty() ? 0 : 1)) +
                                   " of the three");
            return;
        }

        Build b;
        b.job = std::to_string(state_.next_job++);
        b.id = s.id;
        b.requester = requester;
        b.correlation = static_cast<std::int64_t>(mail.correlation());
        b.project = s.project;
        b.revision = s.revision;
        b.target = s.target;
        b.attempt = 1;
        state_.builds.push_back(b);
        ++state_.accepted;

        // THE TRUTH ABOUT THE QUEUE, said at accept time. A build accepted behind
        // three others is accepted; a requester that was not told would think it
        // had been lost.
        (void)mail.answer(BuildAccepted{s.id, queued_ahead_of(b.job)});
        dispatch(mail);
    }

    /// Take it back. A queued build is simply dropped; a running one is abandoned
    /// and its worker told to stop. Either way the requester still gets the ONE
    /// terminal message it was promised, because "you asked me to stop" is a
    /// perfectly good ending and silence is not.
    void on(const WithdrawBuild& w, loom::Mail& mail) {
        const std::string requester = std::to_string(mail.sender().value);
        for (std::size_t i = 0; i < state_.builds.size(); ++i) {
            const Build& b = state_.builds[i];
            if (b.id != w.id || b.requester != requester) {
                continue;
            }
            if (!b.worker.empty()) {
                mail.send_to_role(worker_role(b.worker), AbandonJob{b.job});
            }
            ++state_.withdrawn;
            fail(mail, i, b.worker, std::string{}, "withdrawn by the requester");
            (void)mail.answer(loom::Ack{});
            dispatch(mail);
            return;
        }
        (void)mail.answer(
            loom::Refused{"no build named '" + w.id + "' is open for you in this farm"});
    }

    void on(const FarmStatus&, loom::Mail& mail) {
        std::int64_t queued = 0;
        std::int64_t running = 0;
        for (const Build& b : state_.builds) {
            (b.worker.empty() ? queued : running) += 1;
        }
        std::string roster;
        for (const std::string& w : state_.workers) {
            roster += (roster.empty() ? "" : ",") + w;
        }
        (void)mail.answer(loom::Result{
            "farm: accepted=" + std::to_string(state_.accepted) + " rejected=" +
            std::to_string(state_.rejected) + " succeeded=" + std::to_string(state_.succeeded) +
            " failed=" + std::to_string(state_.failed) + " withdrawn=" +
            std::to_string(state_.withdrawn) + " requeued=" +
            std::to_string(state_.requeued_by_reconciliation) + "reconciled/" +
            std::to_string(state_.requeued_by_sweep) + "swept" + " queued=" +
            std::to_string(queued) + " running=" + std::to_string(running) + " workers=" +
            (roster.empty() ? "(none)" : roster)});
    }

    // ---- what workers say ---------------------------------------------------

    /// A worker refuses an assignment. AUTHENTICATED, so this is one of the few
    /// failures the farm learns immediately.
    ///
    /// A DECLINE IS A JUDGEMENT, NOT AN ABSENCE, so the build is FAILED rather
    /// than requeued: a worker that cannot build a target will not be able to
    /// build it on the next attempt either, and retrying would turn one honest
    /// refusal into three.
    void on(const AssignDeclined& d, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ++state_.ignored;
            return;
        }
        for (std::size_t i = 0; i < state_.builds.size(); ++i) {
            if (state_.builds[i].job != d.job) {
                continue;
            }
            fail(mail, i, d.worker, std::string{}, "worker declined: " + d.reason);
            dispatch(mail);
            return;
        }
        ++state_.ignored;
    }

    /// A stage finished. Patience is restored and the requester is told — with
    /// the ATTEMPT number, so progress that goes backwards is legible rather than
    /// alarming.
    void on(const StageDone& s, loom::Mail& mail) {
        Build* b = build_for_job(s.job);
        if (b == nullptr || b->worker != s.worker) {
            ++state_.ignored; // the itinerary is the half of the obligation we CAN check
            return;
        }
        b->patience = kAssignmentPatienceSweeps;
        // ADOPT THE WORKER'S NUMBER. A build resumed through a preparation was
        // started again by the CANDIDATE, which counted the attempt; the
        // dispatcher never saw a requeue and would otherwise report the terminal
        // message as attempt 1 while every progress line said 2. One number, one
        // owner: whoever started the attempt.
        b->attempt = s.attempt;
        tell(mail, *b,
             BuildProgress{b->id, s.worker, s.stage, s.index + 1,
                           static_cast<std::int64_t>(kStageCount), s.attempt});
    }

    void on(const JobDone& j, loom::Mail& mail) {
        for (std::size_t i = 0; i < state_.builds.size(); ++i) {
            Build& b = state_.builds[i];
            if (b.job != j.job || b.worker != j.worker) {
                continue;
            }
            b.attempt = j.attempt; // the worker started it; the worker counted it
            if (j.ok) {
                const Build done = b;
                ++state_.succeeded;
                tell(mail, done, BuildSucceeded{done.id, j.worker, j.detail, done.attempt});
                erase(i);
            } else {
                fail(mail, i, j.worker, j.stage, j.detail);
            }
            dispatch(mail);
            return;
        }
        ++state_.ignored;
    }

    /// A worker announced itself. TWO THINGS HAPPEN, and the second is this
    /// package's alternative to a watchdog.
    void on(const WorkerOpen& w, loom::Mail& mail) {
        if (w.worker.empty()) {
            ++state_.ignored;
            return;
        }
        bool known = false;
        for (const std::string& name : state_.workers) {
            known = known || name == w.worker;
        }
        if (!known) {
            state_.workers.push_back(w.worker);
        }

        // RECONCILIATION. This incarnation says what it really holds. Anything we
        // believe it has and it does not list went down with whoever held it, and
        // there is no reason to wait for a clock to tell us so.
        std::size_t at = 0;
        while (at < state_.builds.size()) {
            Build& b = state_.builds[at];
            if (b.worker != w.worker) {
                ++at;
                continue;
            }
            bool still_held = false;
            for (const std::string& job : w.holding) {
                still_held = still_held || job == b.job;
            }
            if (still_held) {
                b.patience = kAssignmentPatienceSweeps;
                ++at;
                continue;
            }
            ++state_.requeued_by_reconciliation;
            if (!requeue(mail, at, "the worker holding it was replaced")) {
                continue; // requeue failed it and erased it; the cursor stays put
            }
            ++at;
        }
        dispatch(mail);
    }

    // ---- the sweep ----------------------------------------------------------

    /// The other answer to absence: nobody arrived, so nobody said anything, so
    /// the only evidence is that a build has been quiet for too long.
    void on(const timer::TimerFired& f, loom::Mail& mail) {
        if (f.id != kSweepTimerId) {
            return;
        }
        std::size_t at = 0;
        while (at < state_.builds.size()) {
            Build& b = state_.builds[at];
            if (b.worker.empty() || --b.patience > 0) {
                ++at;
                continue;
            }
            ++state_.requeued_by_sweep;
            if (!requeue(mail, at, "worker '" + b.worker + "' took it and said nothing for " +
                                       std::to_string(kAssignmentPatienceSweeps) + " sweeps")) {
                continue;
            }
            ++at;
        }
        dispatch(mail);
    }

private:
    // ---- the queue ----------------------------------------------------------

    /// Give every free worker the oldest queued build it could take. FIFO, and
    /// the order is the vector's order — a queue whose order depended on a map's
    /// iteration would be a queue nobody chose.
    ///
    /// TWO PASSES, and the first one is a real policy rather than an
    /// optimisation: a build that came back from worker X prefers anyone but X,
    /// because the commonest reason it came back is that something is wrong where
    /// it was. The second pass then places whatever is left anywhere free, so
    /// "prefer somebody else" never becomes "wait forever" on a one-worker farm.
    void dispatch(loom::Mail& mail) {
        assign_pass(mail, /*avoid_last=*/true);
        assign_pass(mail, /*avoid_last=*/false);
    }

    void assign_pass(loom::Mail& mail, bool avoid_last) {
        for (const std::string& worker : state_.workers) {
            if (busy(worker)) {
                continue;
            }
            for (Build& b : state_.builds) {
                if (!b.worker.empty()) {
                    continue;
                }
                if (avoid_last && b.last_worker == worker) {
                    continue;
                }
                b.worker = worker;
                b.last_worker = worker;
                b.patience = kAssignmentPatienceSweeps;
                mail.send_to_role(worker_role(worker),
                                  AssignBuild{b.job, b.project, b.revision, b.target, b.attempt});
                break;
            }
        }
    }

    bool busy(const std::string& worker) const {
        for (const Build& b : state_.builds) {
            if (b.worker == worker) {
                return true;
            }
        }
        return false;
    }

    /// How many builds are AHEAD of this one — running or waiting, both count.
    ///
    /// Counting only the *waiting* ones would have been the other reading and it
    /// is the wrong one: a requester told "queued behind 0" while a ten-beat
    /// build occupies the only worker has been told something true and useless.
    /// What it wants to know is its place in the line.
    std::int64_t queued_ahead_of(const std::string& job) const {
        std::int64_t ahead = 0;
        for (const Build& b : state_.builds) {
            if (b.job == job) {
                break;
            }
            ++ahead;
        }
        return ahead;
    }

    /// Put a build back at the FRONT of the queue as a new attempt, or end it if
    /// it has had all the attempts the farm allows.
    ///
    /// Returns true if the build is still in the book (requeued), false if it was
    /// failed and erased — the caller's cursor depends on knowing which.
    bool requeue(loom::Mail& mail, std::size_t at, const std::string& why) {
        Build& b = state_.builds[at];
        if (b.attempt >= kMaxAttempts) {
            const std::string worker = b.worker;
            fail(mail, at, worker, std::string{},
                 "gave up after " + std::to_string(kMaxAttempts) + " attempts; the last one ended "
                 "because " + why);
            return false;
        }
        ++b.attempt;
        b.worker.clear();
        b.patience = 0;
        return true;
    }

    // ---- endings ------------------------------------------------------------

    void reject(loom::Mail& mail, const std::string& id, std::string reason) {
        ++state_.rejected;
        (void)mail.answer(BuildRejected{id, std::move(reason)});
    }

    void fail(loom::Mail& mail, std::size_t at, const std::string& worker,
              const std::string& stage, std::string reason) {
        const Build b = state_.builds[at];
        ++state_.failed;
        tell(mail, b, BuildFailed{b.id, worker, stage, std::move(reason)});
        erase(at);
    }

    void erase(std::size_t at) {
        state_.builds.erase(state_.builds.begin() + static_cast<std::ptrdiff_t>(at));
    }

    Build* build_for_job(const std::string& job) {
        for (Build& b : state_.builds) {
            if (b.job == job) {
                return &b;
            }
        }
        return nullptr;
    }

    /// Say something to the requester, echoing the correlation it chose.
    template <class T>
    void tell(loom::Mail& mail, const Build& b, const T& msg) {
        const loom::WeaveId who{parse_u64(b.requester)};
        if (!who.valid()) {
            return;
        }
        mail.send(who, msg, static_cast<std::uint64_t>(b.correlation));
    }

    void ask_for_the_sweep(loom::Mail& mail) {
        mail.send_to_role(timer::kTimerRole, timer::StartRoleTimer{kSweepTimerId, kSweepMs,
                                                                   /*repeat=*/true,
                                                                   kDispatcherRole});
    }

    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Dispatcher)
