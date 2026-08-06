// ELSDON — seat five. "The observer was there and we were not."
//
// Elsdon starts from the position that a competent birdwatcher who took the
// trouble to write a submission probably saw what they said they saw, and looks
// for a reason to say no rather than a reason to say yes. There are two:
// a date that is simply wrong with nothing to back it up, and a bird that could
// still be something commoner with nothing to back that up either.
//
// A committee of five Brants would accept almost nothing and a committee of
// five Elsdons would accept almost everything. The published rules exist
// because neither of those is a county list.

#include "committee.hpp"

#include <zen/kernel/export.hpp>

namespace {

using namespace committee;

class Elsdon : public loom::WeaveBase<Elsdon, AssessorState, loom::Accept<Circulate>,
                                      loom::Emit<Vote>> {
public:
    void on(const Circulate& c, loom::Mail& mail) {
        ++state_.ballots;

        if (!has_media(c.record)) {
            if (!c.record.in_season) {
                cast(mail, false, "out of season on a description alone is asking too much");
                return;
            }
            const std::string standing = first_unaddressed(c.record);
            if (!standing.empty()) {
                cast(mail, false, "I would want " + standing + " dealt with");
                return;
            }
        }
        cast(mail, true, "I see no reason to doubt it");
    }

private:
    void cast(loom::Mail& mail, bool accept, const std::string& why) {
        accept ? ++state_.accepts : ++state_.rejects;
        mail.answer(Vote{accept, why});
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Elsdon)
