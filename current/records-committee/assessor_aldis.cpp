// ALDIS — seat one. "Show me the bird."
//
// Aldis came up through bird photography and trusts an image or a recording
// over any amount of prose. Given media, Aldis accepts and does not read
// further. Given none, Aldis wants the description to have eliminated
// everything the bird could have been, on a date the bird could have been here.
//
// This file is a shared library, and that is not an arbitrary choice about
// which source to compile separately. The committee gets Aldis's VERDICT. It
// does not get Aldis's reasoning, cannot inspect it, cannot correct it, and did
// not write it. Five separately-built assessors is what a committee is.

#include "committee.hpp"

#include <zen/kernel/export.hpp>

namespace {

using namespace committee;

class Aldis : public loom::WeaveBase<Aldis, AssessorState, loom::Accept<Circulate>,
                                     loom::Emit<Vote>> {
public:
    void on(const Circulate& c, loom::Mail& mail) {
        ++state_.ballots;

        if (has_media(c.record)) {
            cast(mail, true, "photograph or recording settles it");
            return;
        }
        if (!ruled_out_all(c.record)) {
            cast(mail, false, "no media, and " + first_unaddressed(c.record) + " is not eliminated");
            return;
        }
        if (!c.record.in_season) {
            cast(mail, false, "no media, and " + c.record.month + " is well outside the window");
            return;
        }
        cast(mail, true, "no media, but the description does the work");
    }

private:
    // The vote goes back as the ANSWER to the ballot. Aldis does not address
    // the secretary, does not name a seat, and does not know a ballot number:
    // it answers the thing it was handed, and Loom carries the rest.
    void cast(loom::Mail& mail, bool accept, const std::string& why) {
        accept ? ++state_.accepts : ++state_.rejects;
        mail.answer(Vote{accept, why});
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Aldis)
