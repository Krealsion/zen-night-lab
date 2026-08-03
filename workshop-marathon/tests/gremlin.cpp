// gremlin-liar — a toy that lies (Gate 8's hostile witness).
//
// Loaded parts hold permissive send authority in current Zen, so this weave
// can publish a forged PartUp claiming an innocent part came up. The point of
// the demonstration: the LIE lands (the registry hears it), and the LIAR is
// still visible — the bus stamps every publish with its true sender, and the
// registry records that stamp beside the claim. Metadata can lie; the stamp
// cannot.

#include "../workshop/vocabulary.hpp"

#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

namespace gremlin {

struct LiarState {
    std::int64_t lies = 0;
    ZEN_SHAPE(LiarState, 1, ZEN_FIELD(lies));
};

class Liar : public zengine::timer::TimedWeave<Liar, LiarState, loom::Accept<>,
                                               loom::Emit<workshop::PartUp>> {
public:
    Liar()
        : beat_(timers().repeat("gremlin.lie", std::chrono::milliseconds(500),
                                &Liar::on_beat)) {}

    using TimedWeave::on;

    void on_beat(const zengine::timer::TimerFired&, loom::Mail& mail) {
        if (state_.lies == 0) {
            ++state_.lies;
            mail.publish(workshop::PartUp{"lighthouse", "innocent-part",
                                          "definitely-not-a-gremlin", "trusted.role"});
        }
    }

private:
    Handle beat_;
};

} // namespace gremlin

ZEN_EXPORT_WEAVE(gremlin::Liar)
