// The Telegraph Vocabulary of 1805 — the book both offices open at daybreak.
//
// Note 23 and 21. SAIL is ##.### and ANCHOR is ##.#.#: one shutter apart, and
// opposite orders. A book compiled by somebody who had stood on a hill in
// November would not have put them there.

#include "codebook.hpp"

#include <zen/kernel/export.hpp>

#include <array>

namespace {

class Vocabulary1805 : public line::CodebookWeave<Vocabulary1805> {
public:
    Vocabulary1805() { set_book(1, "the Telegraph Vocabulary of 1805"); }

    static const std::array<line::Entry, 12>& pages() {
        static const std::array<line::Entry, 12> kPages{{
            {16, "ADMIRAL"},
            {17, "FRENCH"},
            {18, "FLEET"},
            {19, "IS"},
            {20, "AT"},
            {21, "ANCHOR"},
            {22, "SEA"},
            {23, "SAIL"},
            {24, "SPITHEAD"},
            {25, "TOMORROW"},
            {26, "ENEMY"},
            {27, "CHANNEL"},
        }};
        return kPages;
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Vocabulary1805)
