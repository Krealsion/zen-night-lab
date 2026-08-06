// Plain Bob Doubles.
//
// Five bells changing; on six, the tenor covers. The first method most ringers
// learn after plain hunt, and the one on which the first 120 is almost always
// rung.
//
//   &5.1.5.1.5,125        bob 145        single 123
//
// which is to say: nine changes in which fifths and lead alternate as the
// stationary place, and then a lead end at which seconds is made -- or, if the
// conductor calls, fourths (a bob) or seconds-and-thirds (a single).
//
// This library knows the notation and nothing else. It has never heard of a
// tower, a band, a conductor or a touch, and the only authority it is given is
// permission to answer the one question it is asked.

#include "method.hpp"

#include <zen/kernel/export.hpp>

namespace {

tower::Method plain_bob_doubles() {
    tower::Method m;
    m.name = "Plain Bob Doubles";
    m.bells = 5;
    // 5 . 1 . 5 . 1 . 5 . 1 . 5 . 1 . 5
    m.lead = {{5}, {1}, {5}, {1}, {5}, {1}, {5}, {1}, {5}};
    m.plain = {1, 2, 5};
    m.bob = {1, 4, 5};
    m.single = {1, 2, 3};
    return m;
}

class PlainBobDoubles : public tower::MethodWeave {
public:
    PlainBobDoubles() : tower::MethodWeave(plain_bob_doubles()) {}
};

} // namespace

ZEN_EXPORT_WEAVE(PlainBobDoubles)
