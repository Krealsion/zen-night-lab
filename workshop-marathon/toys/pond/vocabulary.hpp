#ifndef POND_VOCABULARY_HPP
#define POND_VOCABULARY_HPP

// The pond's message vocabulary. One shape: a firefly announcing its flash.
// Every firefly hears every other's flash (publish fan-out IS the pond), and
// the canvas hears them all to paint. Nobody knows how many fireflies exist —
// the canvas discovers them by who flashes.

#include <zen/weave/shape.hpp>

#include <cstdint>

namespace pond {

/// "I flashed." `who` is the firefly's declared index (set by the project
/// description at launch — the same artifact is every firefly).
struct FireflyFlash {
    std::int64_t who = 0;
    ZEN_SHAPE(FireflyFlash, 1, ZEN_FIELD(who));
};

inline constexpr const char* kCanvasSlot = "pond";

} // namespace pond

#endif // POND_VOCABULARY_HPP
