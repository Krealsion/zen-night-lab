// The worker — the office whose completion reports and whose open-for-work
// announcement the farm acts on. One deliberate act per statement:
//
//     mail.as_role("farm.worker.a").send_to_role("farm.dispatcher", JobDone{...});
//     mail.as_role("farm.worker.a").publish(WorkerOpen{...});
//
// The first carries TWO offices without conflating them — authored as the
// worker, delivered to the dispatcher. The second is the case the pull
// workaround could never cover: every observer of the publication can verify
// the office, and a rogue's same-shaped announcement verifies as nothing.

#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace {

using namespace loom;

struct WorkerState {
    std::int64_t jobs = 0;
    ZEN_SHAPE(WorkerState, 1, ZEN_FIELD(jobs));
};

class Worker : public WeaveBase<Worker, WorkerState, Accept<farm::RunJob>,
                                Emit<farm::JobDone, farm::WorkerOpen>> {
public:
    void on(const farm::RunJob& cmd, Mail& mail) {
        ++state_.jobs;
        const farm::JobDone done{cmd.job, "worker.a"};
        const farm::WorkerOpen open{"worker.a"};
        if (cmd.personal) {
            // The rogue's whole toolkit: the same shapes, personally.
            (void)mail.send_to_role("farm.dispatcher", done);
            (void)mail.publish(open);
            return;
        }
        (void)mail.as_role("farm.worker.a").send_to_role("farm.dispatcher", done);
        (void)mail.as_role("farm.worker.a").publish(open);
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Worker)
