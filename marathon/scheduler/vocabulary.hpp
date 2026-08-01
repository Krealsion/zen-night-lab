#ifndef MARATHON_MAINT_VOCABULARY_HPP
#define MARATHON_MAINT_VOCABULARY_HPP

// A maintenance scheduler — the whole contract in one file.
//
// THE ARCHITECTURAL QUESTION, and it is not "does Timer work":
//
//     What does a moderately ordinary application feel like when the Timer
//     binding layer and prepared replacement have to coexist? Did the two
//     individually pleasant APIs compose, or do they become awkward together?
//
// So this project deliberately uses BOTH sugars at full strength and nothing
// else: the scheduler is a `timer::TimedWeave` with authored bindings and an
// `on_timed_activation` hook, and BOTH of its dependencies get replaced through
// `loom::PreparedReplacement` — the maintenance worker (an ordinary service of
// this package's own) and **the Timer service itself**, through the Timer
// package's own preparation vocabulary.
//
// ---- NIGHT ONE'S FRICTION 4, RE-TESTED --------------------------------------
//
// Night One found that `timer::TimedWeave` and the activation moment were
// MUTUALLY EXCLUSIVE: the binding owned `on(zen.Activated)`, a derived handler
// suppressed it silently through C++ name hiding, and three of four weaves in
// that kitchen wanted the moment and none could use the binding.
//
// That is closed. The binding layer now ships `on_timed_activation()`, a
// compile-time refusal for a derived raw activation handler that names the
// alternative, and a static_assert for the missing `using TimedWeave::on;`.
// This package uses the hook for real domain startup work and the suite pins it.
//
// ---- THE FRICTION THIS PROJECT FOUND INSTEAD --------------------------------
//
// **The binding table is authored, not dynamic.** Bindings are declared (in
// practice, in the constructor) and reconciled by the binding layer on an
// accepted activation or a `TimerReady`. An author cannot reconcile — that is
// the layer's, correctly. So a weave whose RHYTHM IS DATA — which is exactly
// what a scheduler is — cannot express its schedules as bindings at all. It
// declares one authored beat and counts it.
//
// That is not a defect; it is a boundary, and it is worth knowing because the
// binding layer reads like the general answer to "I want something to happen
// periodically" and is in fact the answer to "THIS WEAVE has a fixed rhythm".
// The suite pins the boundary with a probe (`DeclareLateBinding`) that declares
// a binding after activation and measures exactly when it starts firing — which
// turns out to be the next `TimerReady`, i.e. the next time the Timer service is
// replaced. Delightful, honest, and not something anyone would guess.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace marathon::maint {

inline constexpr const char* kSchedulerRole = "maint.scheduler";
inline constexpr const char* kWorkerRole = "maint.worker";

// ---- what an operator asks the scheduler ------------------------------------

/// "Check this machine every N sweeps, forever."
struct ScheduleCheck {
    std::string name;
    std::string machine;
    std::int64_t every_sweeps = 0;
    ZEN_SHAPE(ScheduleCheck, 1, ZEN_FIELD(name), ZEN_FIELD(machine), ZEN_FIELD(every_sweeps));
};

/// "Do this to that machine once, N sweeps from now."
struct ScheduleOnce {
    std::string name;
    std::string machine;
    std::string action;
    std::int64_t after_sweeps = 0;
    ZEN_SHAPE(ScheduleOnce, 1, ZEN_FIELD(name), ZEN_FIELD(machine), ZEN_FIELD(action),
              ZEN_FIELD(after_sweeps));
};

struct CancelSchedule {
    std::string name;
    ZEN_SHAPE(CancelSchedule, 1, ZEN_FIELD(name));
};

/// Cancel THE AUTHORED AUDIT BINDING — a `timer::TimerHandle::cancel`, not a
/// book entry. It is a separate shape because it is a different kind of act: one
/// stops a domain schedule this weave invented, the other stops a timer this
/// weave declared and the Timer service is holding.
struct CancelAudit {
    ZEN_SHAPE(CancelAudit, 1);
};

/// A PROBE, and labelled as one. It makes the scheduler declare a NEW repeating
/// binding at run time, long after construction, so the suite can measure when —
/// if ever — the binding layer establishes it. See the header.
struct DeclareLateBinding {
    ZEN_SHAPE(DeclareLateBinding, 1);
};

struct MaintStatus {
    ZEN_SHAPE(MaintStatus, 1);
};

// ---- what the scheduler says ------------------------------------------------

/// One check's outcome, sent to whoever asked for the schedule.
struct HealthReport {
    std::string name;
    std::string machine;
    bool ok = false;
    std::string detail;
    std::int64_t run = 0; ///< which run of this schedule this is
    ZEN_SHAPE(HealthReport, 1, ZEN_FIELD(name), ZEN_FIELD(machine), ZEN_FIELD(ok),
              ZEN_FIELD(detail), ZEN_FIELD(run));
};

/// The scheduler's own startup announcement, made from `on_timed_activation` —
/// which is the whole point of the hook existing.
struct SchedulerOpen {
    std::int64_t bindings = 0;
    std::int64_t adopted = 0;
    ZEN_SHAPE(SchedulerOpen, 1, ZEN_FIELD(bindings), ZEN_FIELD(adopted));
};

// ---- what the scheduler asks the worker -------------------------------------

struct RunCheck {
    std::string name;
    std::string machine;
    std::string action;
    ZEN_SHAPE(RunCheck, 1, ZEN_FIELD(name), ZEN_FIELD(machine), ZEN_FIELD(action));
};

/// The worker's AUTHENTICATED answer. One hop, one answer: this package
/// deliberately does not re-litigate long-running responsibility, which three
/// earlier projects settled.
struct CheckResult {
    std::string name;
    std::string machine;
    bool ok = false;
    std::string detail;
    ZEN_SHAPE(CheckResult, 1, ZEN_FIELD(name), ZEN_FIELD(machine), ZEN_FIELD(ok),
              ZEN_FIELD(detail));
};

// ---- the worker's replacement conversation ----------------------------------

struct DescribeFleet {
    ZEN_SHAPE(DescribeFleet, 1);
};

struct FleetDescribed {
    std::vector<std::string> machines;
    std::int64_t checks_run = 0;
    ZEN_SHAPE(FleetDescribed, 1, ZEN_FIELD(machines), ZEN_FIELD(checks_run));
};

struct PrepareMaintWorker {
    std::vector<std::string> fleet;
    std::int64_t checks_so_far = 0;
    ZEN_SHAPE(PrepareMaintWorker, 1, ZEN_FIELD(fleet), ZEN_FIELD(checks_so_far));
};

struct MaintWorkerReady {
    std::int64_t fleet = 0;
    ZEN_SHAPE(MaintWorkerReady, 1, ZEN_FIELD(fleet));
};

struct MaintWorkerNotReady {
    std::string reason;
    ZEN_SHAPE(MaintWorkerNotReady, 1, ZEN_FIELD(reason));
};

// ---- the published bounds and the authored rhythm ---------------------------

inline constexpr std::size_t kMaxSchedules = 12;

/// THE SCHEDULER'S OWN BEAT — the one authored repeating binding, addressed to
/// the scheduler's ROLE so a successor inherits the pulse rather than doubling
/// it. Every domain schedule is counted in these.
inline constexpr const char* kSweepId = "maint.sweep";
inline constexpr std::int64_t kSweepMs = 20;

/// The one-shot that does the fleet's first sweep sooner than the rhythm would.
inline constexpr const char* kWarmupId = "maint.warmup";
inline constexpr std::int64_t kWarmupMs = 40;

/// The one-shot the suite CANCELS, to prove an authored binding can be revoked.
///
/// DELIBERATELY FAR OUT OF REACH. At 200ms it fired inside the suite's ordinary
/// pumps, so `cancel()` was landing on a binding that was already Spent — and
/// "cancelled" and "already fired" then look identical from the outside. A
/// deadline nothing in this suite can reach is what makes the cancellation the
/// only thing that could have stopped it.
inline constexpr const char* kAuditId = "maint.audit";
inline constexpr std::int64_t kAuditMs = 100000;

/// The probe binding declared at run time.
inline constexpr const char* kLateId = "maint.late";
inline constexpr std::int64_t kLateMs = 20;

// ---- the fleet --------------------------------------------------------------

inline constexpr const char* kFleet[] = {"press-1", "press-2", "kiln", "conveyor"};
inline constexpr std::size_t kFleetSize = sizeof(kFleet) / sizeof(kFleet[0]);

/// Which machine a worker reports unhealthy, so a report has both verdicts.
inline constexpr const char* kSickMachine = "kiln";

inline bool in_fleet(const std::string& machine) {
    for (const char* m : kFleet) {
        if (machine == m) {
            return true;
        }
    }
    return false;
}

} // namespace marathon::maint

#endif // MARATHON_MAINT_VOCABULARY_HPP
