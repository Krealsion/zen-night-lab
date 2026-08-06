// Place notation, and how a method turns into lines.
//
// INCLUDED ONLY BY THE METHOD LIBRARIES. The host does not include this file
// and cannot: `practice.cpp` names `tower.hpp` and nothing else, so the program
// that runs the practice night has no code anywhere in it that could work out
// what a row ought to be. That is not a promise about restraint -- it is a
// property of the translation units, and `grep -c method.hpp practice.cpp` is
// the whole audit.
//
// ---------------------------------------------------------------------------
// What place notation is
//
// A change is a permutation of the row in which every bell moves at most one
// place. Write it by naming the positions that DO NOT move; everything else
// swaps with its neighbour, in pairs, from the front.
//
//   x  or  -    nothing stays: 1<->2, 3<->4, 5<->6      (even numbers of bells)
//   16          1 and 6 stay:  2<->3, 4<->5
//   125         1, 2 and 5 stay: 3<->4
//
// A method is a short list of changes that repeats -- one LEAD -- plus what the
// last change of the lead becomes when the conductor calls something. Plain Bob
// Doubles is nine changes and a lead end:
//
//   5.1.5.1.5.1.5.1.5   then   125 plain / 145 bob / 123 single
//
// ---------------------------------------------------------------------------
// What a LINE is
//
// A ringer does not hold the method. A ringer holds a line: the positions their
// own bell occupies, blow by blow, through one lead -- and which place bell
// they become at the lead end, which depends on what was called. Learning a
// method means learning every place bell, because over a course your bell rings
// each of them in turn.
//
// So this file derives, from the notation alone, exactly what one ringer needs
// and nothing else. It is the book on the shelf, not the band.

#ifndef RINGING_CHAMBER_METHOD_HPP
#define RINGING_CHAMBER_METHOD_HPP

#include "tower.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace tower {

/// One change: the positions (1-based) that stay put. Everything else swaps
/// with its neighbour in pairs, from the front.
using Places = std::vector<int>;

inline bool stays(const Places& places, int position) {
    for (int p : places) {
        if (p == position) {
            return true;
        }
    }
    return false;
}

/// Apply one change to a row. `row[i]` is the bell in position i+1.
inline std::vector<int> change(const std::vector<int>& row, const Places& places) {
    std::vector<int> out = row;
    const int n = static_cast<int>(row.size());
    int i = 1;
    while (i <= n) {
        if (stays(places, i)) {
            ++i;
            continue;
        }
        // i and i+1 swap. A well-formed notation never leaves an odd tail: if
        // one turns up, the last bell simply stays, which is what the arithmetic
        // does anyway.
        if (i + 1 <= n) {
            out[i - 1] = row[i];
            out[i] = row[i - 1];
            i += 2;
        } else {
            ++i;
        }
    }
    return out;
}

/// A method: its name, how many bells it changes, the notation for all but the
/// last change of a lead, and the three things the last change can be.
struct Method {
    std::string name;
    int bells = 0;
    std::vector<Places> lead;  ///< changes 1 .. lead_length-1
    Places plain;
    Places bob;
    Places single;

    int lead_length() const { return static_cast<int>(lead.size()) + 1; }

    /// The line for one place bell, derived by ringing a lead from rounds and
    /// watching where that one bell goes. `place_bell` outside the method's
    /// bells is not an error and not a refusal -- it is the tenor behind, and
    /// the honest answer is "you cover".
    YourLine line_for(std::int64_t place_bell) const {
        YourLine out;
        out.method = name;
        out.bells = bells;
        out.lead = lead_length();
        out.place_bell = place_bell;

        if (place_bell < 1 || place_bell > bells) {
            out.covering = true;
            return out;
        }

        std::vector<int> row(static_cast<std::size_t>(bells));
        for (int i = 0; i < bells; ++i) {
            row[static_cast<std::size_t>(i)] = i + 1;
        }

        const int me = static_cast<int>(place_bell);
        std::vector<std::vector<int>> rows;
        for (const Places& pn : lead) {
            row = change(row, pn);
            rows.push_back(row);
        }
        for (const std::vector<int>& r : rows) {
            out.path.push_back(where(r, me));
        }
        const std::vector<int>& last = rows.back();
        out.plain_end = where(change(last, plain), me);
        out.bob_end = where(change(last, bob), me);
        out.single_end = where(change(last, single), me);
        return out;
    }

private:
    static std::int64_t where(const std::vector<int>& row, int bell) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (row[i] == bell) {
                return static_cast<std::int64_t>(i) + 1;
            }
        }
        return 0;
    }
};

/// The weave a method library exports. It answers one question, for one place
/// bell, and it is never asked anything else -- during the ringing it is not
/// asked anything at all, because a band that has to look at the book mid-touch
/// has already lost the method.
class MethodWeave
    : public loom::WeaveBase<MethodWeave, MethodState, loom::Accept<WhatIsMyLine>,
                             loom::Emit<YourLine>> {
public:
    explicit MethodWeave(Method m) : method_(std::move(m)) {
        state_.name = method_.name;
        state_.bells = method_.bells;
        state_.lead = method_.lead_length();
    }

    void on(const WhatIsMyLine& q, loom::Mail& mail) {
        ++state_.lines_given;
        mail.answer(method_.line_for(q.place_bell));
    }

private:
    Method method_;
};

} // namespace tower

#endif // RINGING_CHAMBER_METHOD_HPP
