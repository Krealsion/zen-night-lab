// lighthouse-lamp — the first toy: a beam that never stops sweeping.
//
// Chosen as toy #1 because it is the smallest creation that is genuinely
// ALIVE: it moves on its own time, is visible through the Surface package's
// intent vocabulary, and is composed entirely of other people's services —
// the Timer package paces it, a Skin paints it, the lamp only decides where
// the light is. Its rhythm is part of what it is, so it is a TimedWeave.
//
// ZEN_EXPOSE: the whole state is poke-open on purpose. Reaching into a
// running lighthouse and moving its beam by hand is exactly the kind of thing
// a Serious Playground should allow.

#include "surface/vocabulary.hpp"
#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <string>

namespace lighthouse {

namespace surface = zengine::surface;
namespace ztimer = zengine::timer;

inline constexpr const char* kSlot = "lighthouse";
inline constexpr std::int64_t kSweepMs = 100;

struct LampState {
    std::int64_t position = 0;
    std::int64_t direction = 1;
    std::int64_t field = 21; ///< cells in the beam's field
    std::int64_t sweeps = 0; ///< completed end-to-end passes
    ZEN_EXPOSE();
    ZEN_SHAPE(LampState, 1, ZEN_FIELD(position), ZEN_FIELD(direction), ZEN_FIELD(field),
              ZEN_FIELD(sweeps));
};

class Lamp : public ztimer::TimedWeave<Lamp, LampState, loom::Accept<>,
                                       loom::Emit<surface::SurfaceText>> {
public:
    Lamp()
        : beat_(timers().repeat("lighthouse.sweep", std::chrono::milliseconds(kSweepMs),
                                &Lamp::on_beat)) {}

    using TimedWeave::on;

    void on_beat(const ztimer::TimerFired&, loom::Mail& mail) {
        advance();
        mail.publish(surface::SurfaceText{kSlot, frame()});
    }

private:
    void advance() {
        if (state_.field < 3) {
            state_.field = 3; // a beam needs somewhere to go, even poked cruelly
        }
        state_.position += state_.direction;
        if (state_.position <= 0) {
            state_.position = 0;
            state_.direction = 1;
            ++state_.sweeps;
        } else if (state_.position >= state_.field - 1) {
            state_.position = state_.field - 1;
            state_.direction = -1;
            ++state_.sweeps;
        }
    }

    std::string frame() const {
        std::string beam(static_cast<std::size_t>(state_.field), '.');
        beam[static_cast<std::size_t>(state_.position)] = '#';
        return "[" + beam + "]  sweeps: " + std::to_string(state_.sweeps);
    }

    Handle beat_;
};

} // namespace lighthouse

ZEN_EXPORT_WEAVE(lighthouse::Lamp)
