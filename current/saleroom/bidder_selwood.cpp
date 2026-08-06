// Mr Selwood, paddle 11. A jump bidder, and the vendor of lot 12.
//
// He bids two rungs at a time, which is a real manner and a deliberate one: it
// is meant to tell the room that going on will be expensive and tiring. It
// costs him money on the lots he wins and saves him time on the lots he loses,
// and he considers that a fair trade.
//
// He is also selling the bureau. He is therefore the one person in the room who
// must not bid on lot 12, and — because he is here anyway and can see the
// board — the one person in the room with a reason to wish he could put a
// figure on it himself. He may leave a reserve with the front office like
// anybody else. He may not put it up.

#include "saleroom.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>

namespace {

struct Figure {
    std::int64_t lot;
    std::int64_t limit;
};

// Note lot 12 is absent, and that is not an oversight in the table. He is
// selling it.
constexpr Figure kBook[] = {
    {13, 210},
    {16, 160},
};

class Selwood : public loom::WeaveBase<Selwood, saleroom::BidderState,
                                       loom::Accept<saleroom::LotUp, saleroom::Asking,
                                                    saleroom::KnockOut, saleroom::Knocked>,
                                       loom::Emit<saleroom::Bid>> {
public:
    Selwood() {
        state_.name = "Mr Selwood";
        state_.paddle = 11;
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
        if (a.asking > acct->limit) {
            return;
        }
        // TWO RUNGS AT ONCE, unless two would carry him past his figure, in
        // which case he takes the one and says nothing about it.
        std::int64_t say = saleroom::next_rung(a.asking);
        if (say > acct->limit) {
            say = a.asking;
        }
        ++acct->bids_made;
        acct->highest_bid = say;
        mail.as_role(role()).send_to_role("rostrum", saleroom::Bid{a.lot, say, state_.paddle});
    }

private:
    static const char* role() { return "paddle.11"; }

    saleroom::LotAccount* account(std::int64_t lot) {
        for (saleroom::LotAccount& a : state_.lots) {
            if (a.lot == lot) {
                return &a;
            }
        }
        return nullptr;
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Selwood)
