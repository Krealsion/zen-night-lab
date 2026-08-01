#ifndef NIGHT_LAB_KITCHEN_DINER_HPP
#define NIGHT_LAB_KITCHEN_DINER_HPP

// The example consumer — a diner, as a host-native weave.
//
// It is the OTHER END of every promise the kitchen makes, and it is written to
// be pedantic about the consumer obligation rather than convenient, because the
// interesting thing about this experiment is exactly what a careful consumer CAN
// and CANNOT check.
//
//   * An OrderReceipt is Loom's authenticated answer. The diner checks
//     `answers_ask()` — Loom's word that this is the answer to a request this
//     weave sent — AND matches the correlation against its own book. Both halves
//     are available, so both are done.
//   * A Served or an OrderLost is an ordinary directed message. The diner can
//     match the correlation, and that is ALL: it cannot check the bus-stamped
//     sender, because the expediter that serves is legitimately allowed to be a
//     different incarnation from the one that promised. Half the standing
//     obligation is simply not performable here. The diner says so in its own
//     transcript rather than pretending otherwise.
//
// It is host-native on purpose. A native weave is never `zen.Activated` (the
// kernel's control door only sees dynamic load/reload), so this weave has no
// activation ceremony at all — which keeps the example about ordering food.

#include "vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace nightlab::kitchen {

/// The diner's outstanding orders and its transcript. Owned by the HOST (the
/// snake OperatorContext pattern), so a test or a demo can place orders as this
/// weave and read back exactly what it heard.
struct DinerBook {
    std::map<std::uint64_t, std::string> outstanding; ///< correlation -> order_id
    std::vector<std::string> heard;                   ///< the transcript, in arrival order
    std::uint64_t next_correlation = 1;

    /// Record an order about to be placed and return its correlation. The host
    /// then sends the PlaceOrder AS this weave (`send_as_to_role`), which stamps
    /// the diner and authorizes against the DINER's grant — a real capability
    /// spend, not a root send.
    std::uint64_t open(const std::string& order_id) {
        const std::uint64_t c = next_correlation++;
        outstanding[c] = order_id;
        return c;
    }

    /// Every line the diner heard that mentions `needle`. The suite's read-back.
    std::vector<std::string> matching(const std::string& needle) const {
        std::vector<std::string> out;
        for (const std::string& line : heard) {
            if (line.find(needle) != std::string::npos) {
                out.push_back(line);
            }
        }
        return out;
    }
};

struct DinerState {
    std::int64_t receipts = 0;
    std::int64_t served = 0;
    std::int64_t lost = 0;
    std::int64_t ignored = 0; ///< arrivals that failed the consumer obligation
    ZEN_EXPOSE();
    ZEN_SHAPE(DinerState, 1, ZEN_FIELD(receipts), ZEN_FIELD(served), ZEN_FIELD(lost),
              ZEN_FIELD(ignored));
};

class Diner : public loom::WeaveBase<Diner, DinerState,
                                     loom::Accept<OrderReceipt, Served, OrderLost>,
                                     loom::Emit<PlaceOrder>> {
public:
    explicit Diner(DinerBook& book) : book_(&book) {}

    /// BOTH halves of the obligation, because both are available here.
    void on(const OrderReceipt& r, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ignore("receipt for '" + r.order_id + "' arrived without Loom's attestation");
            return;
        }
        const std::string* mine = lookup(mail.correlation());
        if (mine == nullptr || *mine != r.order_id) {
            ignore("receipt for '" + r.order_id + "' matches no order I placed");
            return;
        }
        ++state_.receipts;
        book_->heard.push_back("receipt " + r.order_id + ": " + r.resolved +
                               (r.station.empty() ? "" : " @" + r.station) + " -- " + r.reason);
        if (r.resolved == kRoutedRefused) {
            // A refused order is finished: nothing was started, so no outcome is
            // owed and the correlation is closed here.
            book_->outstanding.erase(mail.correlation());
        }
    }

    /// HALF the obligation, and the half that is missing is named out loud.
    void on(const Served& s, loom::Mail& mail) {
        if (!accept_outcome(mail, s.order_id, "served")) {
            return;
        }
        ++state_.served;
        book_->heard.push_back("served " + s.order_id + ": " + s.dish + " from " + s.station);
    }

    void on(const OrderLost& l, loom::Mail& mail) {
        if (!accept_outcome(mail, l.order_id, "lost")) {
            return;
        }
        ++state_.lost;
        book_->heard.push_back("lost " + l.order_id + " @" +
                               (l.station.empty() ? "(unrouted)" : l.station) + ": " + l.reason);
    }

private:
    const std::string* lookup(std::uint64_t correlation) const {
        const auto it = book_->outstanding.find(correlation);
        return it == book_->outstanding.end() ? nullptr : &it->second;
    }

    /// An outcome carries no attestation — Loom grants ONE authenticated answer
    /// per request and the receipt spent it. So the correlation is the only wall
    /// available, and it is a real one: the kitchen echoes the diner's own
    /// number, which nothing outside this conversation was told.
    ///
    /// What it does NOT establish, recorded here rather than assumed away: that
    /// the sender is the kitchen. A weave holding a grant for `Served` can send
    /// one, and if it guessed a live correlation this diner would believe it.
    bool accept_outcome(const loom::Mail& mail, const std::string& order_id, const char* kind) {
        const std::string* mine = lookup(mail.correlation());
        if (mine == nullptr || *mine != order_id) {
            ignore(std::string(kind) + " '" + order_id + "' matches no order I am waiting on");
            return false;
        }
        book_->outstanding.erase(mail.correlation());
        return true;
    }

    void ignore(std::string why) {
        ++state_.ignored;
        book_->heard.push_back("IGNORED: " + std::move(why));
    }

    DinerBook* book_;
};

} // namespace nightlab::kitchen

#endif // NIGHT_LAB_KITCHEN_DINER_HPP
