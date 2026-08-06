// CORVE — seat three. The one who does homework.
//
// Corve holds that the standard of proof depends on the claim. A second county
// record of something that has occurred here before is an ordinary bird in an
// ordinary place; a FIRST is an extraordinary claim, and Corve will not vote
// for one without a photograph or a recording AND a description that leaves
// nothing standing.
//
// Which means Corve cannot vote out of its own head. Whether this species has
// occurred in Marchfield before is not in the submission and is not Corve's to
// know — it is in the recorder's file, a file older than Corve's membership.
// So Corve takes the ballot away, asks, and votes when it has the answer.
//
// That is why this assessor holds a DeferredAnswer and the other four do not:
// one member of this committee has a question that only the durable record can
// answer, and the ballot has to survive being carried across that question.
//
// Corve also reads the other members' comments when a record is recirculated.
// If somebody has named a confusion species the description never addressed,
// and there is no image to fall back on, Corve changes its vote. A second
// round in which nobody can be moved is a second round for nothing.

#include "committee.hpp"

#include <zen/kernel/export.hpp>

#include <utility>
#include <vector>

namespace {

using namespace committee;

class Corve : public loom::WeaveBase<Corve, AssessorState, loom::Accept<Circulate, OnTheList>,
                                     loom::Emit<Vote, IsItOnTheList>> {
public:
    void on(const Circulate& c, loom::Mail& mail) {
        ++state_.ballots;

        // TAKE THE BALLOT WITH YOU. The vote is still one vote — deferring
        // converts the answer, it does not add a second one — and it is still
        // this exact delivery's answer when it is finally cast.
        const std::uint64_t enquiry = next_enquiry_++;
        waiting_.emplace_back(enquiry, Pending{c.record, c.round, c.dissent, mail.defer_answer()});
        mail.send_to_role(kOfficeArchive, IsItOnTheList{c.record.species}, enquiry);
    }

    void on(const OnTheList& answer, loom::Mail& mail) {
        for (std::size_t i = 0; i < waiting_.size(); ++i) {
            if (waiting_[i].first != mail.correlation()) {
                continue;
            }
            Pending pending = std::move(waiting_[i].second);
            waiting_.erase(waiting_.begin() + static_cast<std::ptrdiff_t>(i));

            bool accept = false;
            std::string why;
            decide(pending, answer, accept, why);

            accept ? ++state_.accepts : ++state_.rejects;
            loom::answer_deferred(pending.ticket, mail, Vote{accept, why});
            return;
        }
        // An answer to an enquiry this incarnation never made. Nothing to do
        // with it, and nothing to be gained by pretending otherwise.
        ++stray_;
    }

    std::int64_t stray() const { return stray_; }

private:
    struct Pending {
        Submission record;
        std::int64_t round = 1;
        std::vector<std::string> dissent;
        loom::DeferredAnswer ticket;
    };

    void decide(const Pending& p, const OnTheList& answer, bool& accept, std::string& why) const {
        // Recirculated, and somebody has named something the description never
        // dealt with. Without media there is nothing to check it against.
        if (p.round > 1 && !has_media(p.record)) {
            const std::string named = named_in_dissent(p);
            if (!named.empty()) {
                accept = false;
                why = "on reflection: " + named + " was raised and the description does not "
                                                 "address it";
                return;
            }
        }

        if (answer.listed) {
            accept = has_media(p.record) || ruled_out_any(p.record);
            why = accept ? "the county has had " + p.record.species + " before (" +
                               answer.first_record + "); this stands"
                         : "not a first, but nothing here separates it from anything";
            return;
        }

        accept = has_media(p.record) && ruled_out_all(p.record);
        why = accept ? "a first for the county, and the evidence carries it"
                     : "a first for the county needs media and a complete elimination";
    }

    // Reading the other members' words as prose, which is what a member
    // actually does with a recirculation packet.
    std::string named_in_dissent(const Pending& p) const {
        for (const std::string& c : p.record.confusion) {
            if (eliminated(p.record, c)) {
                continue;
            }
            for (const std::string& said : p.dissent) {
                if (said.find(c) != std::string::npos) {
                    return c;
                }
            }
        }
        return "";
    }

    std::vector<std::pair<std::uint64_t, Pending>> waiting_;
    std::uint64_t next_enquiry_ = 1;
    std::int64_t stray_ = 0;
};

} // namespace

ZEN_EXPORT_WEAVE(Corve)
