// saleroom — the vocabulary of a country auction.
//
// Everything in this file is what a person in the room can hear or say. The
// three things that decide whether the sale was honest are NOT in it:
//
//     a bidder's limit          lives in that bidder's own .so, and is only
//                               readable afterwards, out of its own account
//     a vendor's reserve        lives on the office's board as a claim, and
//                               is confidential to the office and the rostrum
//     a commission bid          lives in the book on the rostrum, left by
//                               somebody who is not here
//
// The room can hear the bidding and nothing else. That is not a simplification
// of an auction; it is what an auction IS.

#ifndef SALEROOM_SALEROOM_HPP
#define SALEROOM_SALEROOM_HPP

#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace saleroom {

// ---------------------------------------------------------------------------
// The increment ladder.
//
// A house rule, printed in the front of every catalogue, and the reason an
// auction terminates at all. It is the same arithmetic for everybody in the
// room, which is why it is here and not inside anybody.
// ---------------------------------------------------------------------------

/// The step in force at a standing bid of `at`.
inline std::int64_t step_at(std::int64_t at) {
    if (at < 100) return 5;
    if (at < 200) return 10;
    if (at < 500) return 20;
    if (at < 1000) return 50;
    if (at < 2000) return 100;
    return 200;
}

/// The next rung above `at`. The whole room can compute this and does.
inline std::int64_t next_rung(std::int64_t at) { return at + step_at(at); }

/// The highest rung at or below `at`, walking up from nothing. Used only to
/// pick an opening ask, which is a round number a person would say out loud.
inline std::int64_t rung_at_or_below(std::int64_t at) {
    std::int64_t r = 5;
    while (next_rung(r) <= at) {
        r = next_rung(r);
    }
    return r > at ? 0 : r;
}

/// Where the auctioneer opens: comfortably under the low estimate, so the room
/// has somewhere to come in.
inline std::int64_t opening_ask(std::int64_t estimate_low) {
    const std::int64_t want = (estimate_low * 3) / 5;
    const std::int64_t r = rung_at_or_below(want);
    return r > 0 ? r : 5;
}

/// The paddle number that means "the book" — a bid the rostrum makes on behalf
/// of somebody who is not in the room. Zero is not a paddle anybody holds.
inline constexpr std::int64_t kTheBook = 0;

// ---------------------------------------------------------------------------
// What the house says to the saleroom's own people.
// ---------------------------------------------------------------------------

/// The catalogue entry, handed to the rostrum as the lot comes up. The
/// commission is the highest bid left with the saleroom by an absentee; zero
/// means there is nobody on the book for this lot.
struct PutUpLot {
    std::int64_t lot = 0;
    std::string description{};
    std::int64_t estimate_low = 0;
    std::int64_t estimate_high = 0;
    std::int64_t commission = 0;
    ZEN_SHAPE(PutUpLot, 1, ZEN_FIELD(lot), ZEN_FIELD(description), ZEN_FIELD(estimate_low),
              ZEN_FIELD(estimate_high), ZEN_FIELD(commission));
};

/// A vendor's instructions, arriving at the front office before the sale. The
/// office puts them on the board; the rostrum never sees this message.
struct Instructions {
    std::int64_t lot = 0;
    std::int64_t reserve = 0;
    bool unreserved = false;
    ZEN_SHAPE(Instructions, 1, ZEN_FIELD(lot), ZEN_FIELD(reserve), ZEN_FIELD(unreserved));
};

/// "Lot fifteen, please." The office puts that lot's sheet on the board.
struct SheetFor {
    std::int64_t lot = 0;
    ZEN_SHAPE(SheetFor, 1, ZEN_FIELD(lot));
};

/// The auctioneer's beat. A real saleroom's rhythm is the auctioneer's own; the
/// house owns it here for the same reason every experiment in this laboratory
/// owns its clock.
struct Beat {
    std::int64_t at = 0;
    ZEN_SHAPE(Beat, 1, ZEN_FIELD(at));
};

// ---------------------------------------------------------------------------
// The board.
//
// A RESERVE IS TRUE IN THE PRESENT TENSE — it is what the vendor's instructions
// say right now, not something that happened — so it is a claim and not a
// message, and the office claims it as the office. It carries its own lot
// number, and THAT FIELD IS THE WHOLE OF THIS APPLICATION'S PRIMARY FALSE
// GREEN: a board that still shows the last lot's sheet is a perfectly good
// claim, honestly stamped, answering a question nobody asked.
// ---------------------------------------------------------------------------

struct Reserve {
    std::int64_t lot = 0;
    std::int64_t amount = 0;
    bool unreserved = false;
    ZEN_SHAPE(Reserve, 1, ZEN_FIELD(lot), ZEN_FIELD(amount), ZEN_FIELD(unreserved));
};

// ---------------------------------------------------------------------------
// What the room hears. All of it is published by the rostrum, as the rostrum,
// to everybody, because that is what saying something out loud in a saleroom
// is.
// ---------------------------------------------------------------------------

/// "Lot eleven. A pair of Staffordshire spaniels. Ninety pounds anywhere?"
struct LotUp {
    std::int64_t lot = 0;
    std::string description{};
    std::int64_t asking = 0;
    ZEN_SHAPE(LotUp, 1, ZEN_FIELD(lot), ZEN_FIELD(description), ZEN_FIELD(asking));
};

/// "I'm bid a hundred and thirty. Forty anywhere?" — and, when the room has
/// gone quiet, the same sentence with `warning` set, which is the auctioneer
/// telling the room that the hammer is coming.
struct Asking {
    std::int64_t lot = 0;
    std::int64_t asking = 0;
    std::int64_t standing = 0;
    std::int64_t standing_paddle = 0;
    bool on_the_book = false;
    bool warning = false;
    ZEN_SHAPE(Asking, 1, ZEN_FIELD(lot), ZEN_FIELD(asking), ZEN_FIELD(standing),
              ZEN_FIELD(standing_paddle), ZEN_FIELD(on_the_book), ZEN_FIELD(warning));
};

/// The hammer. ONLY THE ROSTRUM MAY SAY THIS, and it is the only thing in the
/// room that transfers anything to anybody.
struct Knocked {
    std::int64_t lot = 0;
    std::int64_t hammer = 0;
    std::int64_t paddle = 0;
    bool on_the_book = false;
    ZEN_SHAPE(Knocked, 1, ZEN_FIELD(lot), ZEN_FIELD(hammer), ZEN_FIELD(paddle),
              ZEN_FIELD(on_the_book));
};

/// "I'm afraid I shall have to buy that one in." The lot did not reach its
/// reserve, or nobody bid at all.
struct BoughtIn {
    std::int64_t lot = 0;
    std::int64_t at = 0;
    ZEN_SHAPE(BoughtIn, 1, ZEN_FIELD(lot), ZEN_FIELD(at));
};

/// The lot is not offered at all. In this saleroom that happens for exactly one
/// reason: the rostrum has no instructions it can trust for the lot in hand.
struct Withdrawn {
    std::int64_t lot = 0;
    std::string why{};
    ZEN_SHAPE(Withdrawn, 1, ZEN_FIELD(lot), ZEN_FIELD(why));
};

// ---------------------------------------------------------------------------
// What a bidder says. One shape, to one office, and nothing else, ever.
// ---------------------------------------------------------------------------

struct Bid {
    std::int64_t lot = 0;
    std::int64_t amount = 0;
    std::int64_t paddle = 0;
    ZEN_SHAPE(Bid, 1, ZEN_FIELD(lot), ZEN_FIELD(amount), ZEN_FIELD(paddle));
};

/// THE SENTENCE A BIDDER MAY NOT SAY.
///
/// A ring is two dealers agreeing not to bid against each other and settling up
/// afterwards, and it is the oldest crime in the auction world — the Auctions
/// (Bidding Agreements) Acts exist for nothing else. The saleroom's structural
/// defence against it is not vigilance: it is that a bidder addresses the
/// rostrum and has no way of addressing another bidder.
///
/// The shape exists because one of the four bidders would send it if it could,
/// and one of the other four would honour it. Neither of those is a device: a
/// trade buyer who deals with the same faces every month is exactly who a ring
/// is made of.
struct KnockOut {
    std::int64_t lot = 0;
    std::int64_t from_paddle = 0;
    std::int64_t settle = 0; ///< what the approach is worth to you afterwards
    ZEN_SHAPE(KnockOut, 1, ZEN_FIELD(lot), ZEN_FIELD(from_paddle), ZEN_FIELD(settle));
};

// ---------------------------------------------------------------------------
// What the rostrum tells the clerk. Nothing else in the room may say either of
// these, and the clerk writes down nothing that is not one of them.
// ---------------------------------------------------------------------------

/// One rung of the ladder, as it was taken. `who` is a paddle number, or
/// `kTheBook` when the rostrum bid on behalf of an absentee.
struct Step {
    std::int64_t lot = 0;
    std::int64_t amount = 0;
    std::int64_t who = 0;
    ZEN_SHAPE(Step, 1, ZEN_FIELD(lot), ZEN_FIELD(amount), ZEN_FIELD(who));
};

/// How the lot ended.
struct Determination {
    std::int64_t lot = 0;
    std::int64_t hammer = 0;
    std::int64_t buyer = 0;
    bool on_the_book = false;
    bool sold = false;
    std::string why{};
    ZEN_SHAPE(Determination, 1, ZEN_FIELD(lot), ZEN_FIELD(hammer), ZEN_FIELD(buyer),
              ZEN_FIELD(on_the_book), ZEN_FIELD(sold), ZEN_FIELD(why));
};

// ---------------------------------------------------------------------------
// What each participant can be asked to show of itself afterwards.
//
// These are read back through the ordinary gate at the end of the sale. The
// bidders' accounts are the only place a limit is ever written down, and they
// are read AFTER the hammer has fallen on everything — which is precisely when
// an auctioneer may ask you what you were good for, and not a moment before.
// ---------------------------------------------------------------------------

/// One bidder's account of one lot.
struct LotAccount {
    std::int64_t lot = 0;
    std::int64_t limit = 0;      ///< what I was prepared to go to; 0 = not interested
    std::int64_t bids_made = 0;  ///< how many times I put my hand up
    std::int64_t highest_bid = 0;
    bool stood_off = false;      ///< I was good for it and did not bid
    bool bought = false;         ///< I heard the hammer and it was mine
    ZEN_SHAPE(LotAccount, 1, ZEN_FIELD(lot), ZEN_FIELD(limit), ZEN_FIELD(bids_made),
              ZEN_FIELD(highest_bid), ZEN_FIELD(stood_off), ZEN_FIELD(bought));
};

struct BidderState {
    std::string name{};
    std::int64_t paddle = 0;
    std::vector<LotAccount> lots{};
    std::int64_t approaches_received = 0; ///< how many times another dealer got to me
    ZEN_SHAPE(BidderState, 1, ZEN_FIELD(name), ZEN_FIELD(paddle), ZEN_FIELD(lots),
              ZEN_FIELD(approaches_received));
};

struct ClerkState {
    std::vector<Step> steps{};
    std::vector<Determination> record{};
    std::int64_t not_the_rostrum = 0; ///< things offered to the book that were not the rostrum's
    ZEN_SHAPE(ClerkState, 1, ZEN_FIELD(steps), ZEN_FIELD(record), ZEN_FIELD(not_the_rostrum));
};

struct OfficeState {
    std::vector<Instructions> sheets{};
    std::int64_t on_the_board = 0; ///< the lot whose sheet is up right now
    std::int64_t asked_for_a_sheet_we_have_not_got = 0;
    ZEN_SHAPE(OfficeState, 1, ZEN_FIELD(sheets), ZEN_FIELD(on_the_board),
              ZEN_FIELD(asked_for_a_sheet_we_have_not_got));
};

struct RostrumState {
    std::int64_t lots_offered = 0;
    std::int64_t bids_disregarded = 0; ///< heard, and not a bid: wrong lot, too late, too low
    std::int64_t no_paddle = 0;        ///< somebody at the back without a paddle
    /// Two hands went up on the same breath and the auctioneer took the higher.
    /// The lower one was a perfectly good bid and is not in the record, which is
    /// why it has to be counted somewhere or the arithmetic will not close.
    std::int64_t beaten_on_the_breath = 0;
    std::int64_t withdrawn = 0;
    ZEN_SHAPE(RostrumState, 1, ZEN_FIELD(lots_offered), ZEN_FIELD(bids_disregarded),
              ZEN_FIELD(no_paddle), ZEN_FIELD(beaten_on_the_breath), ZEN_FIELD(withdrawn));
};

} // namespace saleroom

#endif // SALEROOM_SALEROOM_HPP
