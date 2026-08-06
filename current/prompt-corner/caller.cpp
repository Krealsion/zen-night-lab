// The DSM at the prompt corner: the person calling the show.
//
// This is the one participant in the theatre that is loaded from a shared
// library, and the reason is not "something had to be dynamic". It is that this
// is the only job in the building that can change hands while the show is
// running. The board operator, the sound op and the fly floor are where they
// are for the whole act. The book can be taken by somebody else at the top of a
// page, and the audience must not be able to tell.
//
// One source, both incarnations, because the relief has to be the same DSM the
// production rehearsed -- a relief assembled specially for the handover would
// prove nothing about whether the handover works.
//
// What it may do is decided by the house at load: give standbys, say GO as the
// office, and answer the company stage manager. It cannot address the house, it
// cannot speak to the deck, and it does not ask to.

#include "prompt_book.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <string>

namespace {

using namespace promptcorner;

bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

void erase_one(std::vector<std::string>& v, const std::string& s) {
    const auto it = std::find(v.begin(), v.end(), s);
    if (it != v.end()) {
        v.erase(it);
    }
}

class Caller : public loom::WeaveBase<
                   Caller, CallerState,
                   loom::Accept<Moment, StandingBy, StandDown, CarryOn, BriefTheRelief,
                                loom::Activated>,
                   loom::Emit<Standby, Go, TheBook, HaveTheBook, CannotTakeTheBook>> {
public:
    Caller() { state_.edition = kEdition; }

    // ---- calling the show --------------------------------------------------

    void on(const Moment& m, loom::Mail& mail) {
        // POST-BOUNDARY POLICY, and it is this application's, not Loom's. A DSM
        // who has stood down does not call cues; the beat was delivered, the
        // office was still held, the shape was accepted, and this weave simply
        // declined to act. Counted, so declining is visible rather than quiet.
        if (!state_.calling) {
            ++state_.after_boundary;
            return;
        }

        give_standbys(m.at, mail);
        call_cues(m.at, mail);
    }

    void on(const StandingBy& s, loom::Mail& mail) {
        // Loom's own word that this is the answer to THIS DSM's ask. A
        // lookalike from anywhere else is not a department standing by.
        if (!mail.answers_ask()) {
            return;
        }
        if (!contains(state_.confirmed, s.cue)) {
            state_.confirmed.push_back(s.cue);
        }
    }

    // ---- the handover ------------------------------------------------------

    void on(const StandDown&, loom::Mail& mail) {
        // THE BOUNDARY. Stop calling first, then author the position -- in that
        // order, because a book position read while the DSM is still working is
        // a guess, and this one has to be exact. Nothing after this line can
        // change what is being reported.
        state_.calling = false;
        TheBook book;
        book.edition = state_.edition;
        book.next_index = state_.index;
        book.given = state_.given;
        book.exact = true;
        mail.answer(book);
    }

    void on(const CarryOn&, loom::Mail&) {
        // The relief could not take it. As you were.
        state_.calling = true;
    }

    void on(const BriefTheRelief& brief, loom::Mail& mail) {
        // Sealed in the wings. The one conversation this incarnation will have
        // before it is anybody, and the only chance it gets to say no.
        if (brief.edition != state_.edition) {
            CannotTakeTheBook no;
            no.why = "this is the '" + state_.edition + "' book; the brief says '" +
                     brief.edition + "'";
            mail.answer(no);
            return;
        }
        state_.index = brief.next_index;
        state_.given = brief.given;
        state_.confirmed.clear();
        state_.calling = false; // not until somebody says so
        mail.answer(HaveTheBook{brief.next_index});
    }

    void on(const loom::Activated& a, loom::Mail& mail) {
        // "You have the book." Trusted because Loom attests it, never because
        // the shape arrived: an ordinary zen.Activated is a costume.
        if (!mail.lifecycle_attested() || mail.attested_sequence() != a.sequence) {
            return;
        }
        state_.calling = true;

        // RE-GIVE EVERY STANDBY. The departments may well still be standing by,
        // but the ANSWERS were owed to the DSM who has just left the corner --
        // a conversation belongs to the life that opened it, and this is a new
        // life. Re-giving is what a relief does out loud anyway, and here it is
        // also the only way this incarnation can ever hear "standing by".
        for (const std::string& cue : state_.given) {
            const Cue* c = find_cue(cue);
            if (c == nullptr) {
                continue;
            }
            mail.send_to_role(c->dept, Standby{c->id, c->effect});
            ++state_.re_given;
        }
        state_.confirmed.clear();
    }

private:
    static const Cue* find_cue(const std::string& id) {
        for (const Cue& c : the_book()) {
            if (id == c.id) {
                return &c;
            }
        }
        return nullptr;
    }

    /// Warn every department whose cue has come within reach and has not been
    /// warned yet. `<=` rather than `==` on purpose: a DSM who has just picked
    /// the book up part-way through the act catches up rather than silently
    /// skipping the warnings that are already due.
    void give_standbys(std::int64_t at, loom::Mail& mail) {
        const std::vector<Cue>& book = the_book();
        for (std::size_t i = static_cast<std::size_t>(state_.index); i < book.size(); ++i) {
            const Cue& c = book[i];
            if (c.standby > at || contains(state_.given, c.id)) {
                continue;
            }
            mail.send_to_role(c.dept, Standby{c.id, c.effect});
            state_.given.push_back(c.id);
            ++state_.standbys;
        }
    }

    /// Cues go in book order, one at a time, and the show waits for a standby it
    /// has not heard rather than jumping the one in front of it. A cue called
    /// out of order is not a late cue; it is a different show.
    void call_cues(std::int64_t at, loom::Mail& mail) {
        const std::vector<Cue>& book = the_book();
        while (static_cast<std::size_t>(state_.index) < book.size()) {
            const Cue& c = book[static_cast<std::size_t>(state_.index)];
            if (c.go > at) {
                return;
            }
            if (!contains(state_.confirmed, c.id)) {
                ++state_.held; // due, and nobody is standing by. Hold the show.
                return;
            }
            // AS THE OFFICE. This is the whole difference between a cue and a
            // voice in the dark, and it is a deliberate act per statement --
            // holding the book is not calling the show.
            mail.as_role(kOfficeCaller).send_to_role(c.dept, Go{c.id, c.effect, at});
            erase_one(state_.given, c.id);
            erase_one(state_.confirmed, c.id);
            ++state_.called;
            ++state_.index;
        }
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Caller)
