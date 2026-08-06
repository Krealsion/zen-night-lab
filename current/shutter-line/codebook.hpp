// The codebook — the one thing on this line that knows what a code means.
//
// A vocabulary is a printed book. It is written by somebody else, it is issued
// in editions, and each office holds its own copy: that is why it is a shared
// library here and not a class in the tower. Two offices can be holding two
// different editions and neither of them can tell by looking at the other.
//
// THIS HEADER IS INCLUDED BY THE CODEBOOK ARTIFACTS AND BY NOTHING ELSE, and
// that is load-bearing rather than tidy. The tower must not be able to turn a
// code into a word on anybody's behalf, or the day's checks would be reading
// the tower's arithmetic instead of the line's traffic:
//
//     grep -l '#include "codebook.hpp"' *.cpp *.hpp
//         vocabulary_1805.cpp
//         vocabulary_1806.cpp

#ifndef SHUTTER_LINE_CODEBOOK_HPP
#define SHUTTER_LINE_CODEBOOK_HPP

#include "line.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace line {

/// One entry of a printed vocabulary: the number the shutters carry, and the
/// word it stands for in THIS edition.
struct Entry {
    std::int64_t code;
    const char* word;
};

/// A vocabulary, as a weave. It answers three questions and asks none: what
/// book are you, what is the code for this word, and what word is this code.
/// It cannot start a conversation — its grant lets it speak to the one office
/// whose desk it sits on, and only in answer.
template <class Self>
class CodebookWeave
    : public loom::WeaveBase<Self, BookState, loom::Accept<WhichBook, Coding, Decoding>> {
public:
    void on(const WhichBook&, loom::Mail& mail) {
        ++this->state_.questions_answered;
        mail.answer(ThisBook{this->state_.edition, this->state_.title});
    }

    void on(const Coding& q, loom::Mail& mail) {
        ++this->state_.questions_answered;
        for (const Entry& e : Self::pages()) {
            if (q.word == e.word) {
                mail.answer(Coded{q.word, e.code, this->state_.edition, true});
                return;
            }
        }
        // NOT IN THE BOOK IS AN ANSWER. A clerk who cannot find a word does not
        // guess a number; the word has to be spelled out or the message
        // rewritten, and either way somebody has to be told.
        mail.answer(Coded{q.word, 0, this->state_.edition, false});
    }

    void on(const Decoding& q, loom::Mail& mail) {
        ++this->state_.questions_answered;
        for (const Entry& e : Self::pages()) {
            if (q.code == e.code) {
                mail.answer(Decoded{q.code, e.word, this->state_.edition, true});
                return;
            }
        }
        mail.answer(Decoded{q.code, std::string{}, this->state_.edition, false});
    }

protected:
    void set_book(std::int64_t edition, std::string title) {
        this->state_.edition = edition;
        this->state_.title = std::move(title);
    }
};

} // namespace line

#endif // SHUTTER_LINE_CODEBOOK_HPP
