// echo-machine-fly — PLAY DETOUR. The machine itself becomes a firefly.
//
// Not built for coverage: built because the question was irresistible. The
// S-3 bridge already republishes every bus event as BusFact intent — so a
// creature can EAT the machine's own heartbeat: every `every` delivered
// facts, it flashes into the pond like any firefly. The pond's flashes are
// themselves bus traffic, so the machine-fly closes a feedback loop with the
// creatures it swims with. Whether it synchronizes, dominates, or just
// flickers is exactly what the detour exists to find out.
//
// Side-effect vote (P-008): this is the predicted second consumer of the
// observation vocabulary — a TOY wanting bus-facts, not an inspector.

#include "pond/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

// The workshop's observation vocabulary — a toy reaching for it is itself
// evidence about where this vocabulary wants to live.
#include "../../workshop/vocabulary.hpp"

namespace echo {

struct MachineFlyState {
    std::int64_t who = 9;      ///< the machine swims as fly #9 by default
    std::int64_t every = 150;  ///< flash per this many delivered facts
    std::int64_t seen = 0;
    std::int64_t flashes = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(MachineFlyState, 1, ZEN_FIELD(who), ZEN_FIELD(every), ZEN_FIELD(seen),
              ZEN_FIELD(flashes));
};

class MachineFly : public loom::WeaveBase<MachineFly, MachineFlyState,
                                          loom::Accept<workshop::BusFact>,
                                          loom::Emit<pond::FireflyFlash>> {
public:
    void on(const workshop::BusFact& fact, loom::Mail& mail) {
        if (fact.kind != "Delivered") {
            return;
        }
        if (++state_.seen % state_.every == 0) {
            ++state_.flashes;
            mail.publish(pond::FireflyFlash{state_.who});
        }
    }
};

} // namespace echo

ZEN_EXPORT_WEAVE(echo::MachineFly)
