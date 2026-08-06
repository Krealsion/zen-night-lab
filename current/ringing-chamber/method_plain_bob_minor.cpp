// Plain Bob Minor.
//
// The same method one bell wider: six bells changing, so the tenor stops
// covering and has a line of its own. A lead is twelve changes instead of ten,
// a plain course is five leads instead of four, and every ringer has to learn
// it again -- which is why a band stands, gets the new method up, and only then
// goes.
//
//   &-.16.-.16.-.16,12    bob 14        single 1234
//
// A second artifact through the same door as the first, and deliberately not a
// variant of it: nothing in the tower knows there are two methods, and nothing
// in either method knows there is a tower.

#include "method.hpp"

#include <zen/kernel/export.hpp>

namespace {

tower::Method plain_bob_minor() {
    tower::Method m;
    m.name = "Plain Bob Minor";
    m.bells = 6;
    // - . 16 . - . 16 . - . 16 . - . 16 . - . 16 . -
    // An empty place-list is `x`: nothing stays, everything crosses in pairs.
    m.lead = {{}, {1, 6}, {}, {1, 6}, {}, {1, 6}, {}, {1, 6}, {}, {1, 6}, {}};
    m.plain = {1, 2};
    m.bob = {1, 4};
    m.single = {1, 2, 3, 4};
    return m;
}

class PlainBobMinor : public tower::MethodWeave {
public:
    PlainBobMinor() : tower::MethodWeave(plain_bob_minor()) {}
};

} // namespace

ZEN_EXPORT_WEAVE(PlainBobMinor)
