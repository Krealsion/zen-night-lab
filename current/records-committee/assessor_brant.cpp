// BRANT — seat two. "A photograph of a bird you have not identified is a
// photograph of a bird you have not identified."
//
// Brant has exactly one question and asks it of every record: does the
// description eliminate everything this bird could have been? Media does not
// enter into it, the observer's reputation does not enter into it, and the
// date does not enter into it. Brant is why this committee's minutes are worth
// reading in forty years, and Brant is also why some perfectly good records
// take two rounds.
//
// Brant's dissent names the species that was left standing. That naming is
// load-bearing: it is what another assessor reconsiders on.

#include "committee.hpp"

#include <zen/kernel/export.hpp>

namespace {

using namespace committee;

class Brant : public loom::WeaveBase<Brant, AssessorState, loom::Accept<Circulate>,
                                     loom::Emit<Vote>> {
public:
    void on(const Circulate& c, loom::Mail& mail) {
        ++state_.ballots;

        const std::string standing = first_unaddressed(c.record);
        if (!standing.empty()) {
            ++state_.rejects;
            mail.answer(Vote{false, standing + " is not eliminated"});
            return;
        }
        ++state_.accepts;
        mail.answer(Vote{true, "every confusion species is addressed"});
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Brant)
