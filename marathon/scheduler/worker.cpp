// The maintenance worker — the ordinary, non-Timer service this project replaces
// through `loom::PreparedReplacement`.
//
// It is deliberately SMALL. Three earlier projects settled long-running
// responsibility, so this one does not re-litigate it: a check is one hop and one
// authenticated answer. What it is here to contribute is the second half of the
// composition question — a scheduler on the Timer binding layer whose dependency
// is swapped underneath it, and a Timer service swapped underneath that.
//
// One source, three libraries. `-v2` is the successor; `-narrow` knows a smaller
// fleet, so a candidate has something honest to refuse about.

#include "vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace marathon::maint;

#if defined(MAINT_WORKER_NARROW)
/// A worker that only knows the presses. Enough to refuse a fleet it cannot
/// serve, for a domain reason of its own.
constexpr const char* kKnown[] = {"press-1", "press-2"};
constexpr const char* kLabel = "narrow";
#else
constexpr const char* kKnown[] = {"press-1", "press-2", "kiln", "conveyor"};
#if defined(MAINT_WORKER_LABEL)
constexpr const char* kLabel = MAINT_WORKER_LABEL;
#else
constexpr const char* kLabel = "v1";
#endif
#endif

constexpr std::size_t kKnownCount = sizeof(kKnown) / sizeof(kKnown[0]);

bool knows(const std::string& machine) {
    for (const char* m : kKnown) {
        if (machine == m) {
            return true;
        }
    }
    return false;
}

struct WorkerState {
    std::vector<std::string> fleet;
    std::int64_t checks = 0;
    std::int64_t refused = 0;
    std::int64_t inherited_checks = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(WorkerState, 1, ZEN_FIELD(fleet), ZEN_FIELD(checks), ZEN_FIELD(refused),
              ZEN_FIELD(inherited_checks));
};

class Worker : public loom::WeaveBase<Worker, WorkerState,
                                      loom::Accept<RunCheck, DescribeFleet, PrepareMaintWorker,
                                                   loom::Activated>,
                                      loom::Emit<CheckResult, FleetDescribed, MaintWorkerReady,
                                                 MaintWorkerNotReady>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        (void)mail;
        if (state_.fleet.empty()) {
            for (const char* m : kKnown) {
                state_.fleet.push_back(m);
            }
        }
    }

    /// One check, one authenticated answer. A machine this artifact does not know
    /// is a refusal with a reason, never a guess and never a silent nothing.
    void on(const RunCheck& r, loom::Mail& mail) {
        if (!knows(r.machine)) {
            ++state_.refused;
            (void)mail.answer(CheckResult{r.name, r.machine, false,
                                          "worker '" + std::string(kLabel) +
                                              "' does not service '" + r.machine + "'"});
            return;
        }
        ++state_.checks;
        const bool ok = r.machine != std::string(kSickMachine);
        (void)mail.answer(CheckResult{
            r.name, r.machine, ok,
            ok ? std::string("nominal (") + kLabel + ")"
               : std::string("temperature out of range (") + kLabel + ")"});
    }

    void on(const DescribeFleet&, loom::Mail& mail) {
        FleetDescribed described;
        described.machines = state_.fleet;
        described.checks_run = state_.checks;
        (void)mail.answer(described);
    }

    /// THE PREPARATION ASK, heard from inside the seal.
    void on(const PrepareMaintWorker& p, loom::Mail& mail) {
        for (const std::string& m : p.fleet) {
            if (!knows(m)) {
                (void)mail.answer(MaintWorkerNotReady{
                    "worker '" + std::string(kLabel) + "' does not service '" + m +
                    "', so it cannot take over this fleet"});
                return;
            }
        }
        if (p.fleet.size() > kKnownCount) {
            (void)mail.answer(MaintWorkerNotReady{
                "this artifact services " + std::to_string(kKnownCount) +
                " machines and was handed " + std::to_string(p.fleet.size())});
            return;
        }
        state_.fleet = p.fleet;
        // The predecessor's tally crosses as a NUMBER — a word, so it crosses —
        // which is the only thing about the previous incarnation worth having.
        state_.inherited_checks = p.checks_so_far;
        (void)mail.answer(MaintWorkerReady{static_cast<std::int64_t>(state_.fleet.size())});
    }

private:
    zengine::ActivationCursor activation_;
};

} // namespace

static_assert(kLabel[0] != '\0', "a worker generation needs a label");

ZEN_EXPORT_WEAVE(Worker)
