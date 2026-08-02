// workshop-registry — the Workshop's memory of what it launched.
//
// An ORDINARY loadable weave, on purpose: it holds the `workshop.registry`
// role, accumulates the launch facts the operator publishes, and answers
// QueryRunning asks. Nothing about it is privileged — it can be listed,
// inspected (ZEN_EXPOSE), reloaded, or replaced through the same Manager door
// as any toy part, which is exactly the standing Gate 10 will later test.
//
// Honesty stance: the registry counts what it WITNESSED (published facts that
// reached it). Loaded late, it knows less than the world — like snake's score
// weave, that gap is honest and deliberate, not a defect to paper over.

#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

namespace workshop {

struct RegistryState {
    std::vector<PartUp> up;
    std::vector<PartFailed> failed;
    ZEN_EXPOSE();
    ZEN_SHAPE(RegistryState, 1, ZEN_FIELD(up), ZEN_FIELD(failed));
};

class Registry : public loom::WeaveBase<Registry, RegistryState,
                                        loom::Accept<PartUp, PartFailed, QueryRunning>,
                                        loom::Emit<RunningReport>> {
public:
    void on(const PartUp& fact, loom::Mail&) { state_.up.push_back(fact); }

    void on(const PartFailed& fact, loom::Mail&) { state_.failed.push_back(fact); }

    void on(const QueryRunning&, loom::Mail& mail) {
        mail.answer(RunningReport{state_.up, state_.failed});
    }
};

} // namespace workshop

ZEN_EXPORT_WEAVE(workshop::Registry)
