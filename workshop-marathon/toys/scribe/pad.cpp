// scribe-pad — a tiny writing tool. Toy #3, chosen because it is NOT a game.
//
// What it attacks (Gate 5):
//   - the timer assumption: the pad has NO rhythm — it is a plain WeaveBase,
//     event-driven, and would be perfectly happy on a clockless bus. It still
//     cannot HAVE one: the Input weave polls on Timer beats, so a keyboard
//     tool drags the whole clock in. That dependency is the vote.
//   - the Input package's text story: there is no text vocabulary — only
//     scancodes and convenience names. The pad reconstructs text from key
//     names (single-character names pass through; Space/Return/Backspace are
//     handled by scancode), which means NO SHIFT, NO CASE, NO PUNCTUATION
//     beyond what a name spells. That ceiling is package evidence, not a bug
//     to paper over here.
//
// Keys: type; Return commits the line to the scroll; Backspace edits.
// The pad publishes its living line and the last committed line as intent.

#include "surface/vocabulary.hpp"

#include "input/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <string>
#include <vector>

namespace scribe {

namespace input = zengine::input;
namespace scan = zengine::input::scan;
namespace surface = zengine::surface;

struct PadState {
    std::string line;                 ///< the line being written
    std::vector<std::string> scroll;  ///< committed lines, oldest first
    std::int64_t keys = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(PadState, 1, ZEN_FIELD(line), ZEN_FIELD(scroll), ZEN_FIELD(keys));
};

class Pad : public loom::WeaveBase<Pad, PadState, loom::Accept<input::KeyPressed>,
                                   loom::Emit<surface::SurfaceText>> {
public:
    void on(const input::KeyPressed& k, loom::Mail& mail) {
        ++state_.keys;
        if (k.scancode == scan::kReturn) {
            if (!state_.line.empty()) {
                state_.scroll.push_back(state_.line);
                state_.line.clear();
            }
        } else if (k.scancode == scan::kBackspace) {
            if (!state_.line.empty()) {
                state_.line.pop_back();
            }
        } else if (k.scancode == scan::kSpace) {
            state_.line += ' ';
        } else if (k.name.size() == 1) {
            state_.line += k.name; // the honest ceiling: names, not characters
        }
        paint(mail);
    }

private:
    void paint(loom::Mail& mail) {
        mail.publish(surface::SurfaceText{"scribe", "> " + state_.line});
        if (!state_.scroll.empty()) {
            mail.publish(surface::SurfaceText{"scribe.last",
                                              "last: " + state_.scroll.back()});
        }
    }
};

} // namespace scribe

ZEN_EXPORT_WEAVE(scribe::Pad)
