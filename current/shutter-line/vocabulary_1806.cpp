// The Telegraph Vocabulary of 1806 — the revision that comes into force at
// midday.
//
// It is a genuine revision, which is to say the numbers moved. Every code in
// the 1805 book still means something in this one, and eight of them mean
// something else. That is not carelessness by the Admiralty: a vocabulary is
// re-cut when the war moves, and the numbers are re-used because there are
// only forty-eight of them.
//
// It is also the whole reason a message announces which book it was written
// with. A hoist sequence coded out of the 1805 book and read back through this
// one comes out as perfectly good English, correctly spelled, addressed to the
// right admiral, and about a different navy.

#include "codebook.hpp"

#include <zen/kernel/export.hpp>

#include <array>

namespace {

class Vocabulary1806 : public line::CodebookWeave<Vocabulary1806> {
public:
    Vocabulary1806() { set_book(2, "the Telegraph Vocabulary of 1806"); }

    static const std::array<line::Entry, 13>& pages() {
        static const std::array<line::Entry, 13> kPages{{
            {16, "ADMIRAL"},
            {17, "SPANISH"},  // was FRENCH
            {18, "SQUADRON"}, // was FLEET
            {19, "IS"},
            {20, "OFF"},      // was AT
            {21, "ANCHOR"},
            {22, "USHANT"},   // was SEA
            {23, "RETURN"},   // was SAIL
            {24, "PLYMOUTH"}, // was SPITHEAD
            {25, "TOMORROW"},
            {26, "ENEMY"},
            {27, "CHANNEL"},
            {28, "CONVOY"},   // new
        }};
        return kPages;
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Vocabulary1806)
