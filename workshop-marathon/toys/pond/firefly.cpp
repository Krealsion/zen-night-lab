// pond-firefly — one firefly; the pond is eight of this exact artifact.
//
// Chosen to attack the Workshop's young assumptions (Gate 5):
//   - one artifact, many instances — the launcher's name/stem conflation
//     failed here and was fixed (a toy's vote, landed)
//   - identity and cadence arrive as DECLARED `set` pokes from the project
//     description, not code — the same .so is every firefly
//   - the coupling constant is continuous (a double); the knob's value-cycle
//     model gets to feel awkward and say so
//
// The dynamics are Kuramoto-flavored: phase climbs at `rate` per beat; on
// wrap, flash (publish). Hearing a flash pulls a late phase forward and an
// early phase back by `pull`. With pull > 0 the pond synchronizes; with
// pull = 0 it stays a field of independent blinkers. Deterministic — no
// random anywhere; diversity comes from declared rates and phases.

#include "vocabulary.hpp"

#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

namespace pond {

namespace ztimer = zengine::timer;

inline constexpr std::int64_t kBeatMs = 50;

struct FireflyState {
    std::int64_t who = 0;   ///< declared by the project description
    double phase = 0.0;     ///< 0..1, flash at wrap
    double rate = 0.04;     ///< phase per beat — declared per instance
    double pull = 0.15;     ///< coupling: 0 = deaf pond, higher = faster sync
    std::int64_t flashes = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(FireflyState, 1, ZEN_FIELD(who), ZEN_FIELD(phase), ZEN_FIELD(rate),
              ZEN_FIELD(pull), ZEN_FIELD(flashes));
};

class Firefly : public ztimer::TimedWeave<Firefly, FireflyState, loom::Accept<FireflyFlash>,
                                          loom::Emit<FireflyFlash>> {
public:
    Firefly()
        : beat_(timers().repeat("pond.firefly", std::chrono::milliseconds(kBeatMs),
                                &Firefly::on_beat)) {}

    using TimedWeave::on;

    void on_beat(const ztimer::TimerFired&, loom::Mail& mail) {
        state_.phase += state_.rate;
        if (state_.phase >= 1.0) {
            state_.phase -= 1.0;
            ++state_.flashes;
            mail.publish(FireflyFlash{state_.who});
        }
    }

    void on(const FireflyFlash& flash, loom::Mail&) {
        if (flash.who == state_.who) {
            return; // my own light comes back through the fan-out; ignore it
        }
        if (state_.phase > 0.5) {
            state_.phase += state_.pull * (1.0 - state_.phase);
        } else {
            state_.phase -= state_.pull * state_.phase;
        }
    }

private:
    Handle beat_;
};

} // namespace pond

ZEN_EXPORT_WEAVE(pond::Firefly)
