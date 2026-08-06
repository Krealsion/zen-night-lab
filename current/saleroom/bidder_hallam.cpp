// Hallam & Rooke, paddle 14. Trade, and local — they are at this sale every
// month and they know everybody in the front three rows.
//
// Their manner is patience: they do not bid until the auctioneer gives fair
// warning, on the theory that a lot which has stopped is a lot that can be had.
// Once they are in they bid like anybody else.
//
// AND THEY WOULD PUT A RING TOGETHER IF THE ROOM LET THEM. Before the bureau
// goes they would like a word with Kestrel — stand off this one, and there will
// be something in it for you afterwards. There is no such word to be had: a
// bidder in this saleroom may speak to the rostrum and to nobody else, so the
// sentence has no way out of this weave and the approach has to be forged on
// their behalf by the house (see the controls in sale.cpp).

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

// The bureau is worth real money to them; the rest is trade.
constexpr Figure kBook[] = {
    {11, 160},
    {12, 620},
    {14, 55},
    {15, 110},
};

class Hallam : public loom::WeaveBase<Hallam, saleroom::BidderState,
                                      loom::Accept<saleroom::LotUp, saleroom::Asking,
                                                   saleroom::KnockOut, saleroom::Knocked>,
                                      loom::Emit<saleroom::Bid>> {
public:
    Hallam() {
        state_.name = "Hallam & Rooke";
        state_.paddle = 14;
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

    void on(const saleroom::KnockOut&, loom::Mail&) { ++state_.approaches_received; }

    void on(const saleroom::Asking& a, loom::Mail& mail) {
        saleroom::LotAccount* acct = account(a.lot);
        if (acct == nullptr || acct->limit == 0) {
            return;
        }
        if (a.standing_paddle == state_.paddle) {
            return;
        }
        // NOT UNTIL IT HAS STOPPED. Fair warning is the signal that the room is
        // out; after that they behave like everybody else for the rest of the
        // lot.
        if (!a.warning && in_.count(a.lot) == 0) {
            return;
        }
        if (a.asking > acct->limit) {
            return;
        }
        in_.insert(a.lot);
        ++acct->bids_made;
        acct->highest_bid = a.asking;
        mail.as_role(role()).send_to_role("rostrum",
                                          saleroom::Bid{a.lot, a.asking, state_.paddle});
    }

private:
    static const char* role() { return "paddle.14"; }

    saleroom::LotAccount* account(std::int64_t lot) {
        for (saleroom::LotAccount& a : state_.lots) {
            if (a.lot == lot) {
                return &a;
            }
        }
        return nullptr;
    }

    std::set<std::int64_t> in_{};
};

} // namespace

ZEN_EXPORT_WEAVE(Hallam)
