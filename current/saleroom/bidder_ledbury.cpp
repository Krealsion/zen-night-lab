// Mrs Ledbury, paddle 3. A private buyer, in for two things and nothing else.
//
// Her manner is the commonest one in a country saleroom and the reason an
// auctioneer works so hard at the start of a lot: SHE WILL NOT OPEN. Somebody
// else has to be seen to want it first. After that she goes one step at a time,
// and she stops on her figure without a flicker.
//
// Her figures are in this file and in no other. Nothing in the saleroom can
// read them while the sale is on; the house asks her what she was good for
// after the hammer has fallen on everything, which is when you may ask.

#include "saleroom.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>

namespace {

struct Figure {
    std::int64_t lot;
    std::int64_t limit;
};

// What Mrs Ledbury came for. The spaniels are for her hall; the watercolour is
// a punt, and she has priced it as one.
constexpr Figure kBook[] = {
    {11, 190},
    {13, 150},
    {15, 130},
};

class Ledbury : public loom::WeaveBase<Ledbury, saleroom::BidderState,
                                       loom::Accept<saleroom::LotUp, saleroom::Asking,
                                                    saleroom::KnockOut, saleroom::Knocked>,
                                       loom::Emit<saleroom::Bid>> {
public:
    Ledbury() {
        state_.name = "Mrs Ledbury";
        state_.paddle = 3;
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

    void on(const saleroom::KnockOut&, loom::Mail&) {
        // She is not in the trade and would be appalled. Counted so that the
        // absence of approaches is a measurement rather than a silence.
        ++state_.approaches_received;
    }

    void on(const saleroom::Asking& a, loom::Mail& mail) {
        saleroom::LotAccount* acct = account(a.lot);
        if (acct == nullptr || acct->limit == 0) {
            return;
        }
        if (a.standing_paddle == state_.paddle) {
            return; // never bid against yourself
        }
        // SHE WILL NOT OPEN. Until the room has shown it wants the thing, her
        // hand stays down — which is why an auctioneer's first thirty seconds
        // are spent trying to get anybody at all to start.
        if (a.standing == 0) {
            return;
        }
        if (a.asking > acct->limit) {
            return;
        }
        ++acct->bids_made;
        acct->highest_bid = a.asking;
        mail.as_role(role()).send_to_role("rostrum",
                                          saleroom::Bid{a.lot, a.asking, state_.paddle});
    }

private:
    static const char* role() { return "paddle.3"; }

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

ZEN_EXPORT_WEAVE(Ledbury)
