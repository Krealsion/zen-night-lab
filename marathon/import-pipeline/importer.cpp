// The import pipeline — a service that CANNOT do what it is asked until the
// asker chooses, and therefore a service whose whole shape is a conversation.
//
// One source, several libraries. `MARATHON_IMPORT_CATALOGUE_NAME` names what
// this artifact knows how to read, so two generations can honestly disagree
// during a preparation.
//
// ---- WHAT THIS WEAVE PROVES THAT THE OTHERS COULD NOT ----------------------
//
// THE SECOND HALF OF THE CONSUMER OBLIGATION IS PERFORMABLE HERE. Three projects
// have now had to write "and there is no way to ask Loom whether the sender
// holds the role it claims". This one does not, and the reason is worth stating
// exactly: **its counterparty is a specific weave, not a role.** The importer
// offered a menu to a particular requester, recorded that requester's id at the
// moment of the ask, and a choice arrives with a bus-stamped sender it can
// compare. So a forged choice from a weave that holds a perfectly ordinary grant
// for `ChooseOption` is refused — by an ordinary equality, not by cleverness.
//
// That is not a hole closing. It is the same hole seen from the one angle where
// it does not matter: identity works when you are talking to somebody, and fails
// when you are talking to whoever currently holds an office.
//
// ---- WHAT SURVIVES A REPLACEMENT -------------------------------------------
//
//   the ticket, the requester, the correlation, the file    words -> they cross
//   a RESOLVED choice (a label)                             a word -> it crosses
//   the MENU IDENTITY                                       a promise about a
//                                                           conversation -> it
//                                                           does NOT
//   the answer right                                        a capability -> no
//
// So a successor re-derives the options (they are a function of the file), mints
// a NEW menu, and RE-OFFERS — as an ordinary message, because the answer right
// died. A conversation that had already resolved is simply finished. The
// conversation is neither continued nor ended: it is REOPENED.

#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using namespace marathon::importer;
namespace timer = zengine::timer;

constexpr const char* kCatalogueName = MARATHON_IMPORT_CATALOGUE_NAME;

#if defined(MARATHON_IMPORT_LABEL)
constexpr const char* kLabel = MARATHON_IMPORT_LABEL;
#else
constexpr const char* kLabel = "v1";
#endif

/// One conversation in flight.
struct Conversation {
    std::string ticket;
    std::string requester;
    std::int64_t correlation = 0;
    std::string file;
    std::string menu;        ///< the identity of the offer currently open; empty once resolved
    std::string resolved_to; ///< empty until a choice is resolved
    std::int64_t work_left = 0;
    ZEN_SHAPE(Conversation, 1, ZEN_FIELD(ticket), ZEN_FIELD(requester), ZEN_FIELD(correlation),
              ZEN_FIELD(file), ZEN_FIELD(menu), ZEN_FIELD(resolved_to), ZEN_FIELD(work_left));
};

struct ImporterState {
    std::vector<Conversation> open;
    /// Conversations taken over during preparation, held until this incarnation
    /// is activated and can actually speak to the requesters they name.
    std::vector<PendingImport> adopted;
    std::int64_t next_menu = 1;
    std::int64_t offered = 0;
    std::int64_t reoffered = 0;
    std::int64_t refused = 0;
    std::int64_t resolved = 0;
    std::int64_t choice_refused = 0;
    std::int64_t forged_choices = 0; ///< choices from somebody who was not the asker
    std::int64_t receipts = 0;
    std::int64_t abandoned = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ImporterState, 1, ZEN_FIELD(open), ZEN_FIELD(adopted), ZEN_FIELD(next_menu),
              ZEN_FIELD(offered), ZEN_FIELD(reoffered), ZEN_FIELD(refused), ZEN_FIELD(resolved),
              ZEN_FIELD(choice_refused), ZEN_FIELD(forged_choices), ZEN_FIELD(receipts),
              ZEN_FIELD(abandoned));
};

std::uint64_t parse_u64(const std::string& text) {
    std::uint64_t v = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const std::from_chars_result r = std::from_chars(first, last, v);
    if (r.ec != std::errc{} || r.ptr != last) {
        return 0;
    }
    return v;
}

class Importer
    : public loom::WeaveBase<
          Importer, ImporterState,
          loom::Accept<ImportAsset, ChooseOption, AbandonImport, ImporterStatus,
                       DescribeConversations, PrepareImporter, CatalogueNameIs, loom::Activated,
                       timer::TimerReady, timer::TimerFired>,
          loom::Emit<ImportOptions, ImportRefused, ChoiceResolved, ChoiceRefused, ImportReceipt,
                     ImportFailed, ConversationsDescribed, ImporterReady, ImporterNotReady,
                     AskCatalogueName, timer::StartRoleTimer, loom::Result, loom::Ack,
                     loom::Refused>> {
public:
    // ---- arrival ------------------------------------------------------------

    /// This incarnation is live. Adopted conversations are picked up HERE and
    /// nowhere else: a sealed candidate can speak to nobody but its coordinator,
    /// so this is the first moment the requesters are reachable — and the right
    /// one, since a candidate that re-offered and was then never admitted would
    /// have made an offer on behalf of a service still running perfectly.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        take_over(mail);
        ask_for_the_pulse(mail);
    }

    void on(const timer::TimerReady&, loom::Mail& mail) { ask_for_the_pulse(mail); }

    // ---- request -> menu ----------------------------------------------------

    /// THE ANSWER TO A REQUEST IS THE MENU. Not a receipt and not an
    /// acknowledgement: the service has agreed to nothing yet, because it does
    /// not know what the requester wants the file to become.
    void on(const ImportAsset& r, loom::Mail& mail) {
        const std::string who = std::to_string(mail.sender().value);
        if (state_.open.size() >= kMaxOpenConversations) {
            refuse(mail, r.ticket, "this importer is holding its maximum of " +
                                       std::to_string(kMaxOpenConversations) +
                                       " conversations");
            return;
        }
        for (const Conversation& c : state_.open) {
            if (c.ticket == r.ticket && c.requester == who) {
                refuse(mail, r.ticket,
                       "you already have a conversation named '" + r.ticket + "' open here");
                return;
            }
        }
        const FileKind* kind = find_file(r.file);
        if (kind == nullptr) {
            refuse(mail, r.ticket, "the '" + std::string(kCatalogueName) +
                                       "' catalogue does not know how to read '" + r.file + "'");
            return;
        }
        if (kind->count == 0) {
            // A FILE THAT ADMITS NOTHING IS A REFUSAL, NOT AN EMPTY MENU. An empty
            // menu would be the service asking the requester to choose from
            // nothing, which is not a question.
            refuse(mail, r.ticket,
                   "'" + r.file + "' is readable but admits no interpretation this importer "
                   "can produce");
            return;
        }

        Conversation c;
        c.ticket = r.ticket;
        c.requester = who;
        c.correlation = static_cast<std::int64_t>(mail.correlation());
        c.file = r.file;
        c.menu = mint_menu();
        state_.open.push_back(c);
        ++state_.offered;
        (void)mail.answer(menu_for(c));
    }

    // ---- selected choice -> resolved choice ---------------------------------

    /// THE WALL THIS PROJECT CAN ACTUALLY BUILD.
    ///
    /// The conversation is found by (ticket, BUS-STAMPED SENDER). A weave that
    /// is not the one this menu was offered to does not find a conversation at
    /// all, however right its ticket and menu are — and the correlation and the
    /// menu identity are both public, so the only thing standing there is the
    /// sender stamp, which no weave can forge.
    void on(const ChooseOption& c, loom::Mail& mail) {
        const std::string who = std::to_string(mail.sender().value);
        Conversation* conv = nullptr;
        bool ticket_exists = false;
        for (Conversation& x : state_.open) {
            if (x.ticket != c.ticket) {
                continue;
            }
            ticket_exists = true;
            if (x.requester == who) {
                conv = &x;
            }
        }
        if (conv == nullptr) {
            if (ticket_exists) {
                ++state_.forged_choices; // somebody chose for a conversation not theirs
            }
            ++state_.choice_refused;
            (void)mail.answer(ChoiceRefused{c.ticket, c.menu,
                                            "no conversation named '" + c.ticket +
                                                "' is open between you and this importer"});
            return;
        }
        if (!conv->resolved_to.empty()) {
            // THE DUPLICATE CHOICE. A resolved conversation has no open menu, and
            // saying so names what was already decided rather than pretending the
            // second choice never happened.
            ++state_.choice_refused;
            (void)mail.answer(ChoiceRefused{c.ticket, c.menu,
                                            "this conversation was already resolved to '" +
                                                conv->resolved_to + "'"});
            return;
        }
        if (c.menu != conv->menu) {
            // THE STALE CHOICE. One field answers this, the duplicate above, and
            // the post-replacement race below.
            ++state_.choice_refused;
            (void)mail.answer(ChoiceRefused{c.ticket, c.menu,
                                            "menu '" + c.menu + "' is closed; the menu open for "
                                            "this conversation is '" + conv->menu + "'"});
            return;
        }

        const FileKind* kind = find_file(conv->file);
        if (kind == nullptr) {
            ++state_.choice_refused;
            (void)mail.answer(ChoiceRefused{c.ticket, c.menu,
                                            "this importer can no longer read '" + conv->file +
                                                "'"});
            return;
        }
        const Interpretation* exact = nullptr;
        const Interpretation* by_codec = nullptr;
        for (std::size_t i = 0; i < kind->count; ++i) {
            const Interpretation& option = kind->options[i];
            if (option.label == c.choice) {
                exact = &option;
                break;
            }
            if (by_codec == nullptr && option.codec == c.choice) {
                by_codec = &option; // the FIRST is the best available, by construction
            }
        }
        if (exact == nullptr && by_codec == nullptr) {
            // AN UNKNOWN SPELLING IS REFUSED, NEVER GUESSED AT. Picking the
            // nearest option would be the importer deciding something the
            // requester did not say.
            ++state_.choice_refused;
            (void)mail.answer(ChoiceRefused{c.ticket, c.menu,
                                            "'" + c.choice + "' is not one of the " +
                                                std::to_string(kind->count) +
                                                " interpretations offered in menu '" +
                                                conv->menu + "'"});
            return;
        }

        const Interpretation& chosen = exact != nullptr ? *exact : *by_codec;
        const std::string why =
            exact != nullptr
                ? "you named an interpretation exactly"
                : "'" + c.choice + "' names a codec rather than an interpretation, and '" +
                      chosen.label + "' is the best this file admits for it";
        conv->resolved_to = chosen.label;
        conv->menu.clear(); // the offer is consumed
        conv->work_left = kWorkBeats;
        ++state_.resolved;
        (void)mail.answer(ChoiceResolved{c.ticket, c.menu, c.choice, chosen.label, why});
    }

    /// "Never mind." The requester leaves; nothing more is owed.
    void on(const AbandonImport& a, loom::Mail& mail) {
        const std::string who = std::to_string(mail.sender().value);
        for (std::size_t i = 0; i < state_.open.size(); ++i) {
            if (state_.open[i].ticket == a.ticket && state_.open[i].requester == who) {
                state_.open.erase(state_.open.begin() + static_cast<std::ptrdiff_t>(i));
                ++state_.abandoned;
                (void)mail.answer(loom::Ack{});
                return;
            }
        }
        (void)mail.answer(
            loom::Refused{"no conversation named '" + a.ticket + "' is open for you here"});
    }

    void on(const ImporterStatus&, loom::Mail& mail) {
        std::int64_t awaiting = 0;
        std::int64_t working = 0;
        for (const Conversation& c : state_.open) {
            (c.resolved_to.empty() ? awaiting : working) += 1;
        }
        (void)mail.answer(loom::Result{
            "importer[" + std::string(kLabel) + "/" + std::string(kCatalogueName) +
            "]: offered=" + std::to_string(state_.offered) + " reoffered=" +
            std::to_string(state_.reoffered) + " refused=" + std::to_string(state_.refused) +
            " resolved=" + std::to_string(state_.resolved) + " choice_refused=" +
            std::to_string(state_.choice_refused) + " forged=" +
            std::to_string(state_.forged_choices) + " receipts=" +
            std::to_string(state_.receipts) + " open=" + std::to_string(awaiting) +
            "awaiting/" + std::to_string(working) + "working"});
    }

    // ---- resolved choice -> receipt -----------------------------------------

    void on(const timer::TimerFired& f, loom::Mail& mail) {
        if (f.id != kTickTimerId) {
            return;
        }
        std::size_t at = 0;
        while (at < state_.open.size()) {
            Conversation& c = state_.open[at];
            if (c.work_left <= 0 || --c.work_left > 0) {
                ++at;
                continue;
            }
            const Conversation done = c;
            const FileKind* kind = find_file(done.file);
            const Interpretation* what = nullptr;
            for (std::size_t i = 0; kind != nullptr && i < kind->count; ++i) {
                if (kind->options[i].label == done.resolved_to) {
                    what = &kind->options[i];
                }
            }
            state_.open.erase(state_.open.begin() + static_cast<std::ptrdiff_t>(at));
            const loom::WeaveId who{parse_u64(done.requester)};
            if (!who.valid()) {
                continue;
            }
            if (what == nullptr) {
                mail.send(who,
                          ImportFailed{done.ticket, "the interpretation '" + done.resolved_to +
                                                        "' is no longer producible"},
                          static_cast<std::uint64_t>(done.correlation));
                continue;
            }
            ++state_.receipts;
            mail.send(who,
                      ImportReceipt{done.ticket, done.file + "#" + done.resolved_to,
                                    done.resolved_to, what->width, what->height, what->bytes},
                      static_cast<std::uint64_t>(done.correlation));
        }
    }

    // ---- the replacement conversation ---------------------------------------

    void on(const DescribeConversations&, loom::Mail& mail) {
        ConversationsDescribed described;
        for (const Conversation& c : state_.open) {
            if (described.open.size() >= kMaxAdoptedConversations) {
                break;
            }
            // THE MENU IDENTITY IS NOT IN HERE, and its absence is the contract.
            described.open.push_back(
                PendingImport{c.ticket, c.requester, c.correlation, c.file, c.resolved_to});
        }
        described.next_menu = state_.next_menu;
        (void)mail.answer(described);
    }

    void on(const PrepareImporter& p, loom::Mail& mail) {
        if (p.adopt.size() > kMaxAdoptedConversations) {
            refuse_prep(mail, "a preparation may hand over at most " +
                                  std::to_string(kMaxAdoptedConversations) +
                                  " conversations, and this one carries " +
                                  std::to_string(p.adopt.size()));
            return;
        }
        for (const PendingImport& c : p.adopt) {
            if (c.requester.empty() || parse_u64(c.requester) == 0) {
                refuse_prep(mail, "conversation '" + c.ticket +
                                      "' names no requester this importer could ever answer");
                return;
            }
            if (p.verify_files && find_file(c.file) == nullptr) {
                refuse_prep(mail, "conversation '" + c.ticket + "' is about '" + c.file +
                                      "', which the '" + std::string(kCatalogueName) +
                                      "' catalogue cannot read");
                return;
            }
        }
        if (p.verify_files) {
            pending_ = mail.defer_answer();
            if (!pending_.valid()) {
                refuse_prep(mail, "this preparation carried no answer authority to hold");
                return;
            }
            staged_ = p.adopt;
            staged_next_menu_ = p.next_menu;
            mail.send(mail.sender(), AskCatalogueName{});
            return;
        }
        state_.adopted = p.adopt;
        adopt_numbering(p.next_menu);
        (void)mail.answer(ImporterReady{static_cast<std::int64_t>(state_.adopted.size())});
    }

    void on(const CatalogueNameIs& n, loom::Mail& mail) {
        if (!mail.answers_ask() || !pending_.valid()) {
            return;
        }
        std::vector<PendingImport> staged;
        staged.swap(staged_);
        if (n.name != kCatalogueName) {
            (void)loom::answer_deferred(
                pending_, mail,
                ImporterNotReady{"the pipeline runs the '" + n.name +
                                 "' catalogue and this artifact ships '" +
                                 std::string(kCatalogueName) + "'"});
            pending_ = loom::DeferredAnswer{};
            return;
        }
        state_.adopted = std::move(staged);
        adopt_numbering(staged_next_menu_);
        (void)loom::answer_deferred(
            pending_, mail, ImporterReady{static_cast<std::int64_t>(state_.adopted.size())});
        pending_ = loom::DeferredAnswer{};
    }

private:
    std::string mint_menu() { return "m" + std::to_string(state_.next_menu++); }

    /// Never go BACKWARDS. A successor told a smaller number than it has already
    /// used keeps its own, so this is safe to call from either preparation path.
    void adopt_numbering(std::int64_t from) {
        if (from > state_.next_menu) {
            state_.next_menu = from;
        }
    }

    ImportOptions menu_for(const Conversation& c) const {
        ImportOptions offer;
        offer.ticket = c.ticket;
        offer.menu = c.menu;
        const FileKind* kind = find_file(c.file);
        for (std::size_t i = 0; kind != nullptr && i < kind->count; ++i) {
            offer.options.push_back(kind->options[i]);
        }
        return offer;
    }

    /// THE THIRD ANSWER TO REPLACEMENT: reopen.
    ///
    /// A conversation that had already resolved is finished — a resolved choice
    /// is a LABEL, which is a word, which crosses. One still awaiting a choice
    /// gets a fresh menu with a NEW identity, sent as an ORDINARY message,
    /// because the answer right that would have carried it belonged to a life
    /// that has ended.
    void take_over(loom::Mail& mail) {
        std::vector<PendingImport> adopted;
        adopted.swap(state_.adopted);
        for (const PendingImport& p : adopted) {
            const loom::WeaveId who{parse_u64(p.requester)};
            if (!who.valid()) {
                continue;
            }
            const FileKind* kind = find_file(p.file);
            if (kind == nullptr || kind->count == 0) {
                mail.send(who,
                          ImportFailed{p.ticket, "the importer was replaced and its successor "
                                                 "cannot read '" + p.file + "'"},
                          static_cast<std::uint64_t>(p.correlation));
                continue;
            }
            Conversation c;
            c.ticket = p.ticket;
            c.requester = p.requester;
            c.correlation = p.correlation;
            c.file = p.file;
            c.resolved_to = p.resolved_to;
            if (!p.resolved_to.empty()) {
                c.work_left = kWorkBeats; // the decision crossed; just do the work
                state_.open.push_back(c);
                continue;
            }
            c.menu = mint_menu();
            state_.open.push_back(c);
            ++state_.reoffered;
            mail.send(who, menu_for(c), static_cast<std::uint64_t>(p.correlation));
        }
    }

    void refuse(loom::Mail& mail, const std::string& ticket, std::string reason) {
        ++state_.refused;
        (void)mail.answer(ImportRefused{ticket, std::move(reason)});
    }

    void refuse_prep(loom::Mail& mail, std::string reason) {
        (void)mail.answer(ImporterNotReady{std::move(reason)});
    }

    void ask_for_the_pulse(loom::Mail& mail) {
        mail.send_to_role(timer::kTimerRole,
                          timer::StartRoleTimer{kTickTimerId, kTickMs, /*repeat=*/true,
                                                kImporterRole});
    }

    zengine::ActivationCursor activation_;
    loom::DeferredAnswer pending_;
    std::vector<PendingImport> staged_;
    std::int64_t staged_next_menu_ = 1;
};

} // namespace

static_assert(kLabel[0] != '\0', "an importer generation needs a label");

ZEN_EXPORT_WEAVE(Importer)
