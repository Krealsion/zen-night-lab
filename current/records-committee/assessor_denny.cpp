// DENNY — seat four. The one who thinks about how likely any of this was.
//
// Denny's question is not "could this be something else" but "what would it
// take for this to be true". In the window, a bird seen by more than one pair
// of eyes is ordinary and Denny accepts it; outside the window the claim is
// doing much more work, and Denny wants an image and a description that closes
// every door.
//
// Denny is the reason a single observer with a good description still gets
// challenged, and Denny is also the reason a scruffy record with three
// independent observers gets through.

#include "committee.hpp"

#include <zen/kernel/export.hpp>

namespace {

using namespace committee;

class Denny : public loom::WeaveBase<Denny, AssessorState, loom::Accept<Circulate>,
                                     loom::Emit<Vote>> {
public:
    void on(const Circulate& c, loom::Mail& mail) {
        ++state_.ballots;

        if (!c.record.in_season) {
            const bool ok = has_media(c.record) && ruled_out_all(c.record);
            cast(mail, ok,
                 ok ? c.record.month + " is wrong for it, but the evidence is complete"
                    : c.record.month + " is wrong for it, and the evidence is not complete");
            return;
        }
        if (has_media(c.record)) {
            cast(mail, true, "in the window, with an image");
            return;
        }
        if (c.record.observers >= 2) {
            cast(mail, true, "in the window, and more than one observer");
            return;
        }
        cast(mail, false, "one observer, no image, nothing to check it against");
    }

private:
    void cast(loom::Mail& mail, bool accept, const std::string& why) {
        accept ? ++state_.accepts : ++state_.rejects;
        mail.answer(Vote{accept, why});
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Denny)
