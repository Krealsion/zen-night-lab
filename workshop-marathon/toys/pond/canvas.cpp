// pond-canvas — turns flashes into a row of light.
//
// The canvas discovers the pond by listening: whoever flashes gets a column.
// Brightness decays each paint beat, so a flash is a bright mark that fades —
// and when the pond synchronizes, the whole row breathes together.
//
// This is also the toy's vote on the Surface question (P-005): a pond wants a
// CANVAS; what it has is a line of text per slot. One row of glyphs is an
// honest minimum — and the strain is the evidence.

#include "vocabulary.hpp"

#include "surface/vocabulary.hpp"
#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <map>
#include <string>

namespace pond {

namespace surface = zengine::surface;
namespace ztimer = zengine::timer;

inline constexpr std::int64_t kPaintMs = 100;

struct CanvasState {
    std::int64_t paints = 0;
    ZEN_SHAPE(CanvasState, 1, ZEN_FIELD(paints));
};

class Canvas : public ztimer::TimedWeave<Canvas, CanvasState, loom::Accept<FireflyFlash>,
                                         loom::Emit<surface::SurfaceText>> {
public:
    Canvas()
        : beat_(timers().repeat("pond.paint", std::chrono::milliseconds(kPaintMs),
                                &Canvas::on_paint)) {}

    using TimedWeave::on;

    void on(const FireflyFlash& flash, loom::Mail&) { brightness_[flash.who] = 1.0; }

    void on_paint(const ztimer::TimerFired&, loom::Mail& mail) {
        if (brightness_.empty()) {
            return; // no flash heard yet; nothing to paint
        }
        ++state_.paints;
        std::string row = "|";
        for (auto& [who, level] : brightness_) {
            (void)who;
            row += glyph(level);
            row += "|";
            level *= 0.55;
        }
        mail.publish(surface::SurfaceText{kCanvasSlot, row});
    }

private:
    static char glyph(double level) {
        if (level > 0.8) {
            return '#';
        }
        if (level > 0.4) {
            return '+';
        }
        if (level > 0.1) {
            return '.';
        }
        return ' ';
    }

    /// Discovered, not declared: whoever flashes exists. NOT part of the
    /// snapshot state on purpose — a revived canvas honestly re-discovers its
    /// pond by listening, like the score weave counts what it witnesses.
    std::map<std::int64_t, double> brightness_;

    Handle beat_;
};

} // namespace pond

ZEN_EXPORT_WEAVE(pond::Canvas)
