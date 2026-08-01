#ifndef MARATHON_FARM_VOCABULARY_HPP
#define MARATHON_FARM_VOCABULARY_HPP

// A build farm — the whole contract in one file.
//
// WHY THIS EXISTS AND WHAT IT MUST NOT BE. The download manager already showed
// that "request → acknowledged responsibility → progress → terminal" is
// expressible. Repeating it would prove nothing. So this package is built to be
// STRUCTURALLY different from the download manager in four ways that a domain
// actually demands, and the comparison in REPORT.md is the deliverable:
//
//   1. A QUEUE, not unbounded concurrency. Workers have capacity 1. Work waits.
//      The download service ran everything at once and never had to decide who
//      goes next.
//   2. TWO SERVICE TIERS, not one. A dispatcher that owns the promise and
//      workers that own the work, so a worker can be replaced while the FARM
//      stays available. The download service was a single role holding
//      everything.
//   3. DISCRETE progress. A build moves through named stages; a download moved
//      through a byte count. One is a small closed vocabulary, the other is a
//      number, and they turn out to want different things.
//   4. A DIFFERENT CONTINUITY CONTRACT, and this is the sharp one.
//
// ---- ON CONTINUITY, AND WHY THIS PACKAGE DISAGREES WITH THE LAST ONE --------
//
// The download manager could not carry work across a replacement: a
// half-downloaded file IS its bytes, so its successor inherits the OBLIGATION to
// report a failure and nothing else.
//
// A build is different in a way that is a property of the DOMAIN and not of the
// substrate: **a build is re-derivable from its intent.** Project, revision,
// target — that is the whole input, it is three strings, and running it again
// produces the same artifact. So this farm makes the opposite choice:
//
//     A build interrupted by a worker being replaced is RESUMED — from the
//     beginning, by the successor, as a new ATTEMPT — and the requester is told
//     which attempt it is watching.
//
// The honest cost is stated on the wire rather than hidden: **progress can go
// backwards.** `BuildProgress::attempt` exists so a requester can tell a restart
// from a regression, because a client that saw "compile" and then "fetch" with
// no explanation would be right to think something was broken.
//
// That contrast — the same substrate, the same ceremony, opposite contracts, both
// honest — is what the two projects are for.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace marathon::farm {

// ---- the addresses that outlive their holders -------------------------------

/// The front desk. Requesters address it by role, so they keep their reach
/// across the dispatcher being replaced.
inline constexpr const char* kDispatcherRole = "buildfarm.dispatcher";

/// A worker's role is derived from its name, so "which worker" is data on the
/// wire and never a compiled-in list in the dispatcher.
inline std::string worker_role(const std::string& worker) {
    return "buildfarm.worker." + worker;
}

// ---- the stages a build moves through ---------------------------------------
//
// A CLOSED VOCABULARY, spelled as Text on the wire so a stranger, a console or a
// tap can read it without this header. It is deliberately small: progress that
// can be enumerated is progress a requester can reason about, and a percentage
// would have been a number nobody could act on.

inline constexpr const char* kStages[] = {"fetch", "configure", "compile", "link", "test"};
inline constexpr std::size_t kStageCount = sizeof(kStages) / sizeof(kStages[0]);

// ---- what a requester says --------------------------------------------------

/// Build this. `id` is the REQUESTER's own name for the build, scoped to the
/// requester, so two requesters naming a build "nightly" never collide.
struct SubmitBuild {
    std::string id;
    std::string project;
    std::string revision;
    std::string target;
    ZEN_SHAPE(SubmitBuild, 1, ZEN_FIELD(id), ZEN_FIELD(project), ZEN_FIELD(revision),
              ZEN_FIELD(target));
};

/// Take it back. A build that has not started is simply dropped; one already
/// running is abandoned by the farm and the worker is told to stop.
struct WithdrawBuild {
    std::string id;
    ZEN_SHAPE(WithdrawBuild, 1, ZEN_FIELD(id));
};

// ---- what the farm says -----------------------------------------------------

/// "I have taken responsibility for this." `queued_behind` is the farm telling
/// the truth about what it just promised: a build accepted behind three others
/// is accepted, and a requester that was not told would think it had been lost.
struct BuildAccepted {
    std::string id;
    std::int64_t queued_behind = 0;
    ZEN_SHAPE(BuildAccepted, 1, ZEN_FIELD(id), ZEN_FIELD(queued_behind));
};

/// "I will not take responsibility for this, and here is why."
struct BuildRejected {
    std::string id;
    std::string reason;
    ZEN_SHAPE(BuildRejected, 1, ZEN_FIELD(id), ZEN_FIELD(reason));
};

/// How far along, and WHICH ATTEMPT. Ordinary and directed, never an answer.
///
/// `attempt` is the field that makes this package's continuity contract honest.
/// A build resumed on a successor starts again at `fetch`, and a requester
/// watching a monotone `stage` sequence suddenly go backwards would be right to
/// think the farm was broken. The number says: it is not, you are watching a new
/// attempt.
struct BuildProgress {
    std::string id;
    std::string worker;
    std::string stage;
    std::int64_t step = 0;
    std::int64_t of_steps = 0;
    std::int64_t attempt = 1;
    ZEN_SHAPE(BuildProgress, 1, ZEN_FIELD(id), ZEN_FIELD(worker), ZEN_FIELD(stage),
              ZEN_FIELD(step), ZEN_FIELD(of_steps), ZEN_FIELD(attempt));
};

struct BuildSucceeded {
    std::string id;
    std::string worker;
    std::string artifact;
    std::int64_t attempts = 1;
    ZEN_SHAPE(BuildSucceeded, 1, ZEN_FIELD(id), ZEN_FIELD(worker), ZEN_FIELD(artifact),
              ZEN_FIELD(attempts));
};

struct BuildFailed {
    std::string id;
    std::string worker;
    std::string stage; ///< where it died; empty if it never reached a worker
    std::string reason;
    ZEN_SHAPE(BuildFailed, 1, ZEN_FIELD(id), ZEN_FIELD(worker), ZEN_FIELD(stage),
              ZEN_FIELD(reason));
};

// ---- diagnostics ------------------------------------------------------------

struct FarmStatus {
    ZEN_SHAPE(FarmStatus, 1);
};

// ---- what the dispatcher says to a worker -----------------------------------
//
// TWO NAMINGS, ONE PER BOUNDARY — the kitchen's discipline, arrived at again
// because the same boundary exists. `id` is the REQUESTER's name for a build;
// `job` is the FARM's, minted by the dispatcher. A worker never learns a
// requester's name for a build and a requester never learns a job number.

/// Do this build. Sent to the worker's ROLE, so it reaches whoever holds the
/// worker slot now.
struct AssignBuild {
    std::string job;
    std::string project;
    std::string revision;
    std::string target;
    std::int64_t attempt = 1;
    ZEN_SHAPE(AssignBuild, 1, ZEN_FIELD(job), ZEN_FIELD(project), ZEN_FIELD(revision),
              ZEN_FIELD(target), ZEN_FIELD(attempt));
};

/// "I will not take that." The worker's AUTHENTICATED answer to an assignment it
/// refuses — safe to spend the one answer here precisely because it is
/// immediate. Acceptance is deliberately silent; the stages are the answer.
struct AssignDeclined {
    std::string job;
    std::string worker;
    std::string reason;
    ZEN_SHAPE(AssignDeclined, 1, ZEN_FIELD(job), ZEN_FIELD(worker), ZEN_FIELD(reason));
};

/// A stage finished. Ordinary and role-addressed, because it must survive both
/// parties being replaced between the assignment and the stage.
struct StageDone {
    std::string job;
    std::string worker;
    std::string stage;
    std::int64_t index = 0;
    std::int64_t attempt = 1;
    ZEN_SHAPE(StageDone, 1, ZEN_FIELD(job), ZEN_FIELD(worker), ZEN_FIELD(stage),
              ZEN_FIELD(index), ZEN_FIELD(attempt));
};

/// The build ended, one way or the other.
///
/// `attempt` is here for a reason a test found: THE ATTEMPT NUMBER HAS TWO
/// POSSIBLE OWNERS and only one of them can be right. The dispatcher mints
/// attempt+1 when it requeues; a candidate WORKER mints attempt+1 when it
/// resumes a build through a preparation, and the dispatcher is not told. The
/// party that STARTS an attempt is the one that knows which attempt is running,
/// so the worker's number is authoritative and every worker->dispatcher message
/// carries it.
struct JobDone {
    std::string job;
    std::string worker;
    bool ok = false;
    std::string stage;    ///< where it failed, when it failed
    std::string detail;   ///< the artifact name, or the failure's reason
    std::int64_t attempt = 1;
    ZEN_SHAPE(JobDone, 1, ZEN_FIELD(job), ZEN_FIELD(worker), ZEN_FIELD(ok), ZEN_FIELD(stage),
              ZEN_FIELD(detail), ZEN_FIELD(attempt));
};

/// "Stop that one." Sent when a requester withdraws.
struct AbandonJob {
    std::string job;
    ZEN_SHAPE(AbandonJob, 1, ZEN_FIELD(job));
};

/// "I am here, this is my capacity, and this is what I am actually holding."
///
/// THE `holding` FIELD IS THIS PACKAGE'S ANSWER TO ABSENCE, and it is a
/// different answer from the kitchen's. The kitchen could only learn a station
/// was gone by a promise going unkept, because a station's disappearance
/// produced nothing at all. A worker being REPLACED produces something: the
/// successor's own announcement. So the dispatcher reconciles — anything it
/// believes a worker is holding, that the worker does not list, was lost with
/// the incarnation that held it, and is requeued at once instead of after a
/// timeout.
///
/// The watchdog still exists, because reconciliation only covers the case where
/// somebody arrives. A worker that vanishes with nothing taking its place still
/// produces silence, and silence still needs a clock. Which mechanism covers
/// which case is written up in REPORT.md.
struct WorkerOpen {
    std::string worker;
    std::int64_t capacity = 1;
    std::vector<std::string> holding; ///< job numbers this incarnation really has
    ZEN_SHAPE(WorkerOpen, 1, ZEN_FIELD(worker), ZEN_FIELD(capacity), ZEN_FIELD(holding));
};

// ---- the replacement conversation -------------------------------------------
//
// THE INTENT CROSSES; THE PROGRESS DOES NOT. See the header.

/// One assignment, described as its INTENT. Note what is absent: no stage, no
/// step, no partial artifact. Those are the successor's to re-derive, and a
/// letter that carried them would be describing work the successor does not have.
struct AssignedJob {
    std::string job;
    std::string project;
    std::string revision;
    std::string target;
    std::int64_t attempt = 1;
    ZEN_SHAPE(AssignedJob, 1, ZEN_FIELD(job), ZEN_FIELD(project), ZEN_FIELD(revision),
              ZEN_FIELD(target), ZEN_FIELD(attempt));
};

/// "What are you working on?" An ORDINARY ask to the LIVE incumbent worker. It
/// changes nothing: no stage is skipped, no dispatcher is told anything.
struct DescribeAssignments {
    ZEN_SHAPE(DescribeAssignments, 1);
};

struct AssignmentsDescribed {
    std::string worker;
    std::vector<AssignedJob> jobs;
    ZEN_SHAPE(AssignmentsDescribed, 1, ZEN_FIELD(worker), ZEN_FIELD(jobs));
};

/// THE PREPARATION ASK: "be this worker, and resume these builds from the top."
struct PrepareWorker {
    std::string worker;             ///< the worker slot this candidate must agree to BE
    std::vector<AssignedJob> resume;
    bool consult = false;           ///< make the candidate ask before answering
    ZEN_SHAPE(PrepareWorker, 1, ZEN_FIELD(worker), ZEN_FIELD(resume), ZEN_FIELD(consult));
};

/// The candidate's own question, asked FROM INSIDE THE SEAL to the one party it
/// may speak to.
struct AskToolchain {
    std::string worker;
    ZEN_SHAPE(AskToolchain, 1, ZEN_FIELD(worker));
};

struct ToolchainIs {
    std::string name;
    ZEN_SHAPE(ToolchainIs, 1, ZEN_FIELD(name));
};

struct WorkerReady {
    std::string worker;
    std::int64_t resumed = 0;
    ZEN_SHAPE(WorkerReady, 1, ZEN_FIELD(worker), ZEN_FIELD(resumed));
};

struct WorkerNotReady {
    std::string worker;
    std::string reason;
    ZEN_SHAPE(WorkerNotReady, 1, ZEN_FIELD(worker), ZEN_FIELD(reason));
};

// ---- the catalogue of buildable things --------------------------------------
//
// Deterministic and in-memory. A target that fails does so at a NAMED STAGE,
// because "the build broke" and "the build broke in `link`" are different
// amounts of help.

struct Target {
    const char* name;
    /// Which stage this target dies in, or kStageCount for one that succeeds.
    std::size_t dies_at;
    /// How many steps each stage takes. One number for the whole target: the
    /// point is countable progress, not a simulation.
    std::int64_t steps_per_stage;
};

inline constexpr Target kTargets[] = {
    {"release", 5, 2},
    {"debug", 5, 1},
    {"asan", 5, 3},
    {"broken-link", 3, 1},  ///< dies in `link`
    {"broken-test", 4, 1},  ///< dies in `test`
};

inline constexpr std::size_t kTargetCount = sizeof(kTargets) / sizeof(kTargets[0]);

inline const Target* find_target(const std::string& name) {
    for (const Target& t : kTargets) {
        if (name == t.name) {
            return &t;
        }
    }
    return nullptr;
}

// ---- the published bounds ---------------------------------------------------

/// How many builds the farm will hold at once, queued and running together.
inline constexpr std::size_t kMaxOpenBuilds = 16;

/// How many assignments one worker will hold. ONE, deliberately: a farm whose
/// workers are unbounded is a farm with no queue, and the queue is the point.
inline constexpr std::size_t kWorkerCapacity = 1;

/// How many attempts a build gets before the farm gives up on it. Bounded, so
/// "resume it" cannot become "loop forever".
inline constexpr std::int64_t kMaxAttempts = 3;

/// How many sweeps an assigned build may sit without a word before the
/// dispatcher assumes its worker is gone and requeues it.
inline constexpr std::int64_t kAssignmentPatienceSweeps = 25;

/// The dispatcher's sweep, and the worker's step. Both ROLE-addressed, so a
/// successor inherits the pulse.
inline constexpr const char* kSweepTimerId = "buildfarm.sweep";
inline constexpr std::int64_t kSweepMs = 20;
inline constexpr const char* kStepTimerId = "buildfarm.step";
inline constexpr std::int64_t kStepMs = 20;

} // namespace marathon::farm

#endif // MARATHON_FARM_VOCABULARY_HPP
