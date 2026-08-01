#ifndef MARATHON_IMPORT_REQUESTER_HPP
#define MARATHON_IMPORT_REQUESTER_HPP

// The example consumer — somebody importing a file, as a host-native weave.
//
// It records, for every arrival, whether Loom vouched for it. This project is
// the one where that measurement is most interesting, because a menu arrives
// TWICE in two different ways over the life of a replacement:
//
//   the first time   as the authenticated ANSWER to the import request
//   after a          as an ORDINARY directed message, because the answer right
//   replacement      belonged to a life that has ended
//
// A requester can tell those apart, and this one does. What it must NOT do is
// treat the unattested one as suspect: it is the honest successor doing the only
// thing it can. The right reading is *"the menu I am looking at is no longer
// something Loom will vouch for, and the menu identity is what I check instead"*.

#include "vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace marathon::importer {

struct RequesterBook {
    std::map<std::uint64_t, std::string> outstanding; ///< correlation -> ticket
    std::vector<std::string> heard;
    std::uint64_t next_correlation = 1;

    /// The menu identity currently believed open, per ticket. THE REQUESTER'S
    /// OWN BOOKKEEPING: it is what makes a stale choice the requester's mistake
    /// rather than the importer's problem.
    std::map<std::string, std::string> menu_of;
    std::map<std::string, std::vector<std::string>> options_of;

    std::int64_t menus_attested = 0;
    std::int64_t menus_unattested = 0;
    std::int64_t resolutions_attested = 0;
    std::int64_t resolutions_unattested = 0;
    std::int64_t receipts_attested = 0;
    std::int64_t receipts_unattested = 0;
    std::int64_t ignored = 0;

    std::uint64_t open(const std::string& ticket) {
        const std::uint64_t c = next_correlation++;
        outstanding[c] = ticket;
        return c;
    }
};

struct RequesterState {
    std::int64_t menus = 0;
    std::int64_t refusals = 0;
    std::int64_t resolutions = 0;
    std::int64_t receipts = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(RequesterState, 1, ZEN_FIELD(menus), ZEN_FIELD(refusals), ZEN_FIELD(resolutions),
              ZEN_FIELD(receipts));
};

class Requester
    : public loom::WeaveBase<Requester, RequesterState,
                             loom::Accept<ImportOptions, ImportRefused, ChoiceResolved,
                                          ChoiceRefused, ImportReceipt, ImportFailed, loom::Ack,
                                          loom::Refused>,
                             loom::Emit<ImportAsset, ChooseOption, AbandonImport>> {
public:
    explicit Requester(RequesterBook& book) : book_(&book) {}

    void on(const ImportOptions& o, loom::Mail& mail) {
        if (!mine(mail, o.ticket, "menu")) {
            return;
        }
        (mail.answers_ask() ? book_->menus_attested : book_->menus_unattested) += 1;
        ++state_.menus;
        book_->menu_of[o.ticket] = o.menu;
        std::vector<std::string> labels;
        std::string listed;
        for (const Interpretation& i : o.options) {
            labels.push_back(i.label);
            listed += (listed.empty() ? "" : ", ") + i.label;
        }
        book_->options_of[o.ticket] = std::move(labels);
        book_->heard.push_back("menu " + o.ticket + " [" + o.menu + "]: " + listed +
                               attested(mail));
    }

    void on(const ImportRefused& r, loom::Mail& mail) {
        if (!mine(mail, r.ticket, "refusal")) {
            return;
        }
        ++state_.refusals;
        book_->heard.push_back("refused " + r.ticket + ": " + r.reason + attested(mail));
        book_->outstanding.erase(mail.correlation());
    }

    void on(const ChoiceResolved& r, loom::Mail& mail) {
        if (!mine(mail, r.ticket, "resolution")) {
            return;
        }
        (mail.answers_ask() ? book_->resolutions_attested : book_->resolutions_unattested) += 1;
        ++state_.resolutions;
        book_->heard.push_back("resolved " + r.ticket + ": '" + r.chose + "' -> '" +
                               r.resolved_to + "' (" + r.why + ")" + attested(mail));
    }

    void on(const ChoiceRefused& r, loom::Mail& mail) {
        if (!mine(mail, r.ticket, "choice refusal")) {
            return;
        }
        book_->heard.push_back("choice refused " + r.ticket + " [" + r.menu + "]: " + r.reason +
                               attested(mail));
    }

    void on(const ImportReceipt& r, loom::Mail& mail) {
        if (!mine(mail, r.ticket, "receipt")) {
            return;
        }
        (mail.answers_ask() ? book_->receipts_attested : book_->receipts_unattested) += 1;
        ++state_.receipts;
        book_->heard.push_back("receipt " + r.ticket + ": " + r.asset + " as " +
                               r.interpretation + " " + std::to_string(r.width) + "x" +
                               std::to_string(r.height) + attested(mail));
        book_->outstanding.erase(mail.correlation());
    }

    void on(const ImportFailed& f, loom::Mail& mail) {
        if (!mine(mail, f.ticket, "failure")) {
            return;
        }
        book_->heard.push_back("failed " + f.ticket + ": " + f.reason + attested(mail));
        book_->outstanding.erase(mail.correlation());
    }

    void on(const loom::Ack&, loom::Mail&) { book_->heard.push_back("abandon acknowledged"); }
    void on(const loom::Refused& r, loom::Mail&) {
        book_->heard.push_back("abandon refused: " + r.reason);
    }

private:
    bool mine(const loom::Mail& mail, const std::string& ticket, const char* kind) {
        const auto it = book_->outstanding.find(mail.correlation());
        if (it == book_->outstanding.end() || it->second != ticket) {
            ++book_->ignored;
            book_->heard.push_back("IGNORED: " + std::string(kind) + " '" + ticket +
                                   "' matches no conversation I am in");
            return false;
        }
        return true;
    }

    static const char* attested(const loom::Mail& mail) {
        return mail.answers_ask() ? "  [attested]" : "  [unattested]";
    }

    RequesterBook* book_;
};

} // namespace marathon::importer

#endif // MARATHON_IMPORT_REQUESTER_HPP
