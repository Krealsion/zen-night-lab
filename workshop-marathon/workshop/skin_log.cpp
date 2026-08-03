// workshop-skin-log — a Skin for the scrollback medium.
//
// The Surface package's snake-era TUI skins have rows for exactly two slots
// ("status", "score") and drop everything else; a toy that speaks a slot of
// its own would be invisible on them. This Skin paints EVERY slot as a plain
// line, so any creation's SurfaceText intent is observable with zero setup.
//
// It is an ordinary, replaceable Skin: it holds `zengine.skin` like the
// others, publishes the SurfaceReady hello on the first message it handles
// (so publishers re-offer their current lines), and can be swapped out for a
// real TUI or SDL skin mid-run through the same Manager door.
//
// Medium honesty: the medium here is stdout scrollback. Claiming it takes
// nothing from anyone (no raw mode, no alternate screen), so the constructor
// and destructor have genuinely nothing to do — that is the honest shape of
// this medium, not an omission. Consecutive updates to the SAME slot repaint
// one line (carriage return); a different slot starts a new line.

#include "vocabulary.hpp" // not used for shapes; keeps the include layout uniform

#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdio>
#include <string>
#include <unistd.h>

namespace workshop {

namespace surface = zengine::surface;

struct LogSkinState {
    std::int64_t lines = 0; ///< how many paints this incarnation performed
    ZEN_EXPOSE();
    ZEN_SHAPE(LogSkinState, 1, ZEN_FIELD(lines));
};

class LogSkin : public loom::WeaveBase<LogSkin, LogSkinState,
                                       loom::Accept<surface::SurfaceText, surface::PumpSurface>,
                                       loom::Emit<surface::SurfaceReady>> {
public:
    void on(const surface::SurfaceText& text, loom::Mail& mail) {
        hello(mail);
        ++state_.lines;
        // Styling is the skin's business: schematic rows print bare, so the
        // diagram reads as a diagram; everything else is labeled by its slot.
        const bool bare = text.slot.rfind("schematic.", 0) == 0;
        // A medium that is not a terminal gets no terminal tricks: piped or
        // redirected output is plain lines, capturable without `tr`. Found by
        // a cold user who had to filter carriage returns out of every command.
        if (!tty_) {
            if (bare) {
                std::printf("%s\n", text.text.c_str());
            } else {
                std::printf("[%s] %s\n", text.slot.c_str(), text.text.c_str());
            }
            std::fflush(stdout);
            return;
        }
        if (text.slot == last_slot_ && !bare) {
            std::printf("\r\033[K[%s] %s", text.slot.c_str(), text.text.c_str());
        } else {
            if (!last_slot_.empty()) {
                std::printf("\r\n");
            }
            if (bare) {
                std::printf("%s", text.text.c_str());
            } else {
                std::printf("[%s] %s", text.slot.c_str(), text.text.c_str());
            }
            last_slot_ = text.slot;
        }
        std::fflush(stdout);
    }

    void on(const surface::PumpSurface&, loom::Mail& mail) { hello(mail); }

private:
    /// The hello is once per incarnation, on the first handled message —
    /// members (not state_) reset exactly when an incarnation does.
    void hello(loom::Mail& mail) {
        if (!said_hello_) {
            said_hello_ = true;
            mail.publish(surface::SurfaceReady{});
        }
    }

    bool said_hello_ = false;
    std::string last_slot_;
    const bool tty_ = ::isatty(1) != 0;
};

} // namespace workshop

ZEN_EXPORT_WEAVE(workshop::LogSkin)
