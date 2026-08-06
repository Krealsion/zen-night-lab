// Kestrel Fine Art, paddle 7. Trade.
//
// A dealer's limit is not what a thing is worth; it is what they can sell it
// for, less what they need to make. So Kestrel is quick at the bottom of a lot
// and stops without sentiment, and it stops early — a private buyer will always
// beat a dealer on something they actually want.
//
// AND KESTREL DEALS WITH THE SAME FACES EVERY MONTH. If another member of the
// trade gets to it before the hammer and offers to settle up afterwards, it
// will stand off the lot. That is not a device put here for an experiment; it
// is what a ring is made of, and it is the reason the room is arranged so that
// nobody can get to it.

#include "saleroom.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <set>

namespace {

struct Figure {
    std::int64_t lot;
    std::int64_t limit;
};

// The bureau is the one Kestrel came for; the rest is stock.
constexpr Figure kBook[] = {
    {11, 140},
    {12, 520},
    {13, 190},
    {14, 45},
    {15, 95},
    {16, 200},
};

class Kestrel : public loom::WeaveBase<Kestrel, saleroom::BidderState,
                                       loom::Accept<saleroom::LotUp, saleroom::Asking,
                                                    saleroom::KnockOut, saleroom::Knocked>,
                                       loom::Emit<saleroom::Bid>> {
public:
    Kestrel() {
        state_.name = "Kestrel Fine Art";
        state_.paddle = 7;
        for (const Figure& f : kBook) {
            saleroom::LotAccount a{};
            a.lot = f.lot;
            a.limit = f.limit;
            state_.lots.push_back(a);
        }
    }

    void on(const saleroom::LotUp&, loom::Mail&) {}

    void on(const saleroom::Knocked& k, loom::Mail&) {
        if (saleroom::LotAccount* acct = account(k.lot)) {
            acct->bought = !k.on_the_book && k.paddle == state_.paddle;
        }
    }

    void on(const saleroom::KnockOut& k, loom::Mail&) {
        // Somebody in the trade has got to us before the hammer. We take the
        // settlement and leave the lot alone.
        ++state_.approaches_received;
        standing_off_.insert(k.lot);
        if (saleroom::LotAccount* acct = account(k.lot)) {
            acct->stood_off = true;
        }
    }

    void on(const saleroom::Asking& a, loom::Mail& mail) {
        if (standing_off_.count(a.lot) != 0) {
            return;
        }
        saleroom::LotAccount* acct = account(a.lot);
        if (acct == nullptr || acct->limit == 0) {
            return;
        }
        if (a.standing_paddle == state_.paddle) {
            return;
        }
        if (a.asking > acct->limit) {
            return; // past the trade price; there is nothing in it
        }
        ++acct->bids_made;
        acct->highest_bid = a.asking;
        mail.as_role(role()).send_to_role("rostrum",
                                          saleroom::Bid{a.lot, a.asking, state_.paddle});
    }

private:
    static const char* role() { return "paddle.7"; }

    saleroom::LotAccount* account(std::int64_t lot) {
        for (saleroom::LotAccount& a : state_.lots) {
            if (a.lot == lot) {
                return &a;
            }
        }
        return nullptr;
    }

    std::set<std::int64_t> standing_off_{};
};

} // namespace

ZEN_EXPORT_WEAVE(Kestrel)
