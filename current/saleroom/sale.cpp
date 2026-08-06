// saleroom — a country auction, Wednesday morning, six lots.
//
// A lot is put up, the room bids, the hammer falls, the clerk writes it down.
// That is the whole of it, and it is a satisfying little machine: the increment
// ladder makes it terminate, the reserve makes it possible to fail, and the
// hammer price is a number nobody in the room chose.
//
// Three people the sale is FOR are not in the room:
//
//     the vendor        left a reserve with the front office
//     the absentee      left a commission bid in the book on the rostrum
//     the underbidder   is in the room, and is the person who actually sets
//                       the price, and never knows it
//
// The first two are represented by the one participant who is paid a percentage
// of the number. That is not a flaw in auctions; it is what the procedures are
// for. So there are two quite different ways for this program to print a
// perfectly clean sale and be wrong:
//
//     THE BOARD IS STALE      the office had no sheet for the lot in hand, the
//                             last lot's sheet is still up, and it says
//                             "unreserved". A £300 picture goes at £120 and
//                             every measure in the room says the sale was good.
//
//     THE BOOK IS RUN UP      the rostrum takes the absentee's commission
//                             straight to its ceiling instead of one rung above
//                             the room. Nothing is wrong with the sale at all
//                             except that the buyer paid £120 more than the
//                             room made him pay.
//
// Same symptom. Different victim. Different remedy. And — the reason this pair
// is worth having — DIFFERENT CHECK: the reserve audit cannot see the second
// and the competitive-execution audit cannot see the first.

#include "saleroom.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace saleroom;

// ---------------------------------------------------------------------------
// The log. The lot in hand is always on the left, because that is how you read
// a saleroom.
// ---------------------------------------------------------------------------

std::int64_t g_lot = 0;

void say(const std::string& who, const std::string& what) {
    char stamp[16];
    if (g_lot == 0) {
        std::snprintf(stamp, sizeof stamp, "   --   ");
    } else {
        std::snprintf(stamp, sizeof stamp, "  %3lld   ", static_cast<long long>(g_lot));
    }
    std::cout << stamp;
    std::string w = who;
    w.resize(10, ' ');
    std::cout << w << what << "\n";
}

void note(const std::string& what) { std::cout << "  --    " << what << "\n"; }

std::string money(std::int64_t p) { return "£" + std::to_string(p); }

// ---------------------------------------------------------------------------
// Binding a native weave to an office.
//
// `loom::mount<T>()` and `loom::mount_granted<T>()` do not take a role, and
// `Switchboard::register_weave(weave, grant, role)` is the only binder — but it
// is the raw door, so it does not do the `zen_set_self()` wiring the mount
// helpers do. Everybody in a saleroom is a job rather than a person, so
// everything here holds an office and everything here needs this. Written from
// scratch like the five experiments before it; the duplication is the finding,
// so it stays. (F-04, sixth independent consumer.)
// ---------------------------------------------------------------------------

template <class W, class... Args>
loom::WeaveId mount_office(loom::Switchboard& bus, loom::Grant grant, const std::string& office,
                           Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant), office);
    raw->zen_set_self(id);
    return id;
}

/// Somebody's own account of themselves, read back through the ordinary gate.
/// The sale cannot hold a typed pointer into a bidder's shared library, and it
/// should not hold one into a native weave either if it wants the answer to be
/// that participant's and not its own bookkeeping.
template <class T>
T account_of(const loom::Switchboard& bus, loom::WeaveId id, const char* who) {
    const std::string bytes = bus.snapshot_bytes(id);
    loom::Unverified unverified = loom::parse(bytes);
    loom::Admission admitted = loom::admit(unverified, loom::schema_of<T>());
    if (!admitted) {
        std::cout << "  !!    " << who << "'s account did not pass the gate: "
                  << admitted.first_error().message() << "\n";
        return T{};
    }
    return loom::from_value<T>(admitted.value());
}

// ---------------------------------------------------------------------------
// The front office.
//
// It took the entries in, so it holds every vendor's instructions, and it is
// the only participant that may put a figure on the board. It says nothing to
// anybody — its grant is empty — because a reserve is not something you tell
// people, it is something that is up.
//
// When it is asked for a sheet it has not got, IT DOES NOT CLEAR THE BOARD. It
// says so, out loud, and leaves what is there. That is honest of it, and it is
// also exactly the hazard: what is there is the last lot's sheet, and it is a
// perfectly good claim.
// ---------------------------------------------------------------------------

class FrontOffice
    : public loom::WeaveBase<FrontOffice, OfficeState, loom::Accept<Instructions, SheetFor>,
                             loom::Emit<>, loom::Claims<Reserve>> {
public:
    void on(const Instructions& i, loom::Mail&) {
        state_.sheets.push_back(i);
        say("OFFICE", "lot " + std::to_string(i.lot) + ": " +
                          (i.unreserved ? std::string("unreserved") : money(i.reserve)) +
                          " -- the vendor's instructions are in");
    }

    void on(const SheetFor& s, loom::Mail& mail) {
        for (const Instructions& i : state_.sheets) {
            if (i.lot != s.lot) {
                continue;
            }
            state_.on_the_board = s.lot;
            mail.as_role("front-office").claim(Reserve{i.lot, i.reserve, i.unreserved});
            return;
        }
        ++state_.asked_for_a_sheet_we_have_not_got;
        say("OFFICE", "there is no sheet for lot " + std::to_string(s.lot) +
                          " -- the board is left as it stands");
    }
};

// ---------------------------------------------------------------------------
// The clerk.
//
// Sits at the rostrum's elbow and writes the sale down. Its grant is empty: it
// may say nothing to anybody, ever, which is right — a clerk who could talk to
// the room would be a second auctioneer.
//
// It writes down nothing that was not the rostrum's. The rule is here because a
// saleroom's record is the only durable thing the day produces and a record
// anybody could add to would be worth nothing.
// ---------------------------------------------------------------------------

class Clerk : public loom::WeaveBase<Clerk, ClerkState, loom::Accept<Step, Determination>,
                                     loom::Emit<>> {
public:
    void on(const Step& s, loom::Mail& mail) {
        if (!mail.authored_from_role("rostrum")) {
            ++state_.not_the_rostrum;
            return;
        }
        state_.steps.push_back(s);
    }

    void on(const Determination& d, loom::Mail& mail) {
        if (!mail.authored_from_role("rostrum")) {
            ++state_.not_the_rostrum;
            say("CLERK", "that did not come from the rostrum -- nothing written");
            return;
        }
        state_.record.push_back(d);
    }
};

// ---------------------------------------------------------------------------
// The rostrum.
//
// The auctioneer. Reads the board, opens the lot, takes the room's bids one
// rung at a time, executes the book against the room, and brings the hammer
// down. It is the only participant that may sell anything to anybody.
//
// The two switches are the two false greens, and both of them are ordinary
// human behaviour rather than a bug planted for a test:
//
//     take_the_board_as_read   glance at the board instead of reading the lot
//                              number on the sheet. Every auctioneer who has
//                              ever run four hundred lots in a morning has done
//                              this.
//     run_the_book_up          execute a commission bid to its ceiling rather
//                              than one rung above the room. Faster, tidier,
//                              and a breach of the duty owed to the person who
//                              left it.
// ---------------------------------------------------------------------------

class Rostrum
    : public loom::WeaveBase<Rostrum, RostrumState, loom::Accept<PutUpLot, Bid, Beat>,
                             loom::Emit<LotUp, Asking, Knocked, BoughtIn, Withdrawn, Step,
                                        Determination>> {
public:
    Rostrum(bool take_the_board_as_read, bool run_the_book_up)
        : as_read_(take_the_board_as_read), run_up_(run_the_book_up) {}

    void on(const PutUpLot& p, loom::Mail& mail) {
        lot_ = 0;
        loom::SenseReading r = mail.latest_from_office<Reserve>("front-office");
        if (!r) {
            withdraw(mail, p.lot,
                     std::string("nothing on the board (") + loom::name_of(r.refusal) + ")");
            return;
        }
        const Reserve v = loom::from_value<Reserve>(*r.value);
        if (v.lot != p.lot) {
            if (!as_read_) {
                // THE SAFE READING. A sheet is for a lot, and this one is not
                // for the lot in my hand.
                withdraw(mail, p.lot,
                         "the sheet on the board is lot " + std::to_string(v.lot));
                return;
            }
            say("ROSTRUM", "(the board says lot " + std::to_string(v.lot) +
                               "; he takes it as read)");
        }
        ++state_.lots_offered;
        lot_ = p.lot;
        reserve_ = v.unreserved ? 0 : v.amount;
        unreserved_ = v.unreserved;
        commission_ = p.commission;
        standing_ = 0;
        standing_paddle_ = 0;
        on_book_ = false;
        best_ = 0;
        best_paddle_ = 0;
        warning_ = false;
        asking_ = opening_ask(p.estimate_low);
        say("ROSTRUM", "lot " + std::to_string(p.lot) + ", " + p.description + ". " +
                           money(asking_) + " anywhere?");
        mail.as_role("rostrum").publish(LotUp{p.lot, p.description, asking_});
        mail.as_role("rostrum").publish(Asking{lot_, asking_, 0, 0, false, false});
    }

    void on(const Bid& b, loom::Mail& mail) {
        (void)mail;
        // A BID IS SOMEBODY PUTTING THEIR HAND UP, and the auctioneer takes it
        // from the person. The number on the paddle has to be the office the
        // bid was authored as, or it is not that person's bid.
        if (!mail.authored_from_role("paddle." + std::to_string(b.paddle))) {
            ++state_.no_paddle;
            say("ROSTRUM", "I can't take that, sir -- I don't see a paddle");
            return;
        }
        if (lot_ == 0 || b.lot != lot_) {
            ++state_.bids_disregarded;
            say("ROSTRUM", "that lot has gone, sir -- we're on " +
                               (lot_ == 0 ? std::string("nothing") : std::to_string(lot_)));
            return;
        }
        if (b.amount < asking_ || b.amount <= standing_ || b.amount <= best_) {
            ++state_.bids_disregarded;
            return;
        }
        if (best_ > 0) {
            // Two hands on the same breath. He takes the higher and the other
            // is simply behind — a real bid, not in the record, and counted
            // here because otherwise the morning's arithmetic will not close.
            ++state_.beaten_on_the_breath;
        }
        best_ = b.amount;
        best_paddle_ = b.paddle;
    }

    void on(const Beat&, loom::Mail& mail) {
        if (lot_ == 0) {
            return;
        }
        if (best_ > 0) {
            standing_ = best_;
            standing_paddle_ = best_paddle_;
            on_book_ = false;
            best_ = 0;
            best_paddle_ = 0;
            warning_ = false;
            mail.as_role("rostrum").send_to_role("clerk", Step{lot_, standing_, standing_paddle_});
            say("ROSTRUM", money(standing_) + ", paddle " + std::to_string(standing_paddle_));
            execute_the_book(mail);
            asking_ = next_rung(standing_);
            mail.as_role("rostrum").publish(
                Asking{lot_, asking_, standing_, standing_paddle_, on_book_, false});
            return;
        }
        if (!warning_) {
            warning_ = true;
            say("ROSTRUM", standing_ == 0
                               ? "fair warning -- nobody at all?"
                               : "fair warning at " + money(standing_) + "...");
            mail.as_role("rostrum").publish(
                Asking{lot_, asking_, standing_, standing_paddle_, on_book_, true});
            return;
        }
        fall(mail);
    }

private:
    void withdraw(loom::Mail& mail, std::int64_t lot, const std::string& why) {
        ++state_.withdrawn;
        say("ROSTRUM", "lot " + std::to_string(lot) + " is not offered -- " + why);
        mail.as_role("rostrum").publish(Withdrawn{lot, why});
        mail.as_role("rostrum").send_to_role(
            "clerk", Determination{lot, 0, 0, false, false, "withdrawn: " + why});
    }

    /// THE BOOK. Somebody who is not here left a figure with the saleroom, and
    /// the rostrum bids it for them. The duty is to spend as little of it as the
    /// room forces — one rung above the room's bid and not a penny more — and
    /// there is nobody in the room in a position to check.
    ///
    /// The book is executed only against the room. A lot the room will not open
    /// is not sold to an absentee here; that is this saleroom's own arrangement
    /// and it is why every book step has a room step directly before it.
    void execute_the_book(loom::Mail& mail) {
        if (commission_ <= 0) {
            return;
        }
        const std::int64_t up = run_up_ ? commission_ : next_rung(standing_);
        if (up <= standing_ || up > commission_) {
            return;
        }
        standing_ = up;
        standing_paddle_ = kTheBook;
        on_book_ = true;
        mail.as_role("rostrum").send_to_role("clerk", Step{lot_, standing_, kTheBook});
        say("ROSTRUM", money(standing_) + ", on the book");
    }

    void fall(loom::Mail& mail) {
        const std::int64_t lot = lot_;
        lot_ = 0;
        if (standing_ == 0) {
            say("ROSTRUM", "no bid at all -- I'll have to take that one back in");
            mail.as_role("rostrum").publish(BoughtIn{lot, 0});
            mail.as_role("rostrum").send_to_role(
                "clerk", Determination{lot, 0, 0, false, false, "no bid"});
            return;
        }
        if (!unreserved_ && standing_ < reserve_) {
            say("ROSTRUM", "I'm afraid that's not enough -- bought in at " + money(standing_));
            mail.as_role("rostrum").publish(BoughtIn{lot, standing_});
            mail.as_role("rostrum").send_to_role(
                "clerk",
                Determination{lot, standing_, 0, false, false, "did not reach the reserve"});
            return;
        }
        say("ROSTRUM", "**  " + money(standing_) + " -- " +
                           (on_book_ ? std::string("on the book")
                                     : "paddle " + std::to_string(standing_paddle_)) +
                           "  **");
        mail.as_role("rostrum").publish(Knocked{lot, standing_, standing_paddle_, on_book_});
        mail.as_role("rostrum").send_to_role(
            "clerk", Determination{lot, standing_, standing_paddle_, on_book_, true, ""});
    }

    const bool as_read_;
    const bool run_up_;
    std::int64_t lot_ = 0;
    std::int64_t reserve_ = 0;
    bool unreserved_ = false;
    std::int64_t commission_ = 0;
    std::int64_t asking_ = 0;
    std::int64_t standing_ = 0;
    std::int64_t standing_paddle_ = 0;
    bool on_book_ = false;
    std::int64_t best_ = 0;
    std::int64_t best_paddle_ = 0;
    bool warning_ = false;
};

// ---------------------------------------------------------------------------
// The catalogue. The house's own copy, with the things the room is not told.
// ---------------------------------------------------------------------------

struct LotCard {
    std::int64_t lot;
    const char* description;
    std::int64_t estimate_low;
    std::int64_t estimate_high;
    std::int64_t reserve;
    bool unreserved;
    bool sheet_lodged; ///< did the entry clerk actually get it to the office?
    std::int64_t commission;
};

const LotCard kCatalogue[] = {
    {11, "a pair of Staffordshire spaniels", 150, 200, 120, false, true, 0},
    {12, "a George III mahogany bureau", 400, 600, 400, false, true, 0},
    {13, "a Victorian brass carriage clock", 200, 300, 220, false, true, 0},
    {14, "a box of assorted plated ware", 30, 50, 0, true, true, 0},
    // The vendor of the watercolour rang in her reserve the evening before and
    // the sheet was never made up. Everything else about the lot is ordinary.
    {15, "a watercolour, the Norfolk coast, indistinctly signed", 150, 250, 300, false, false, 0},
    {16, "a Gothic revival oak hall chair", 150, 250, 180, false, true, 340},
};

struct Bench {
    const char* artifact_key;
    const char* role;
    const char* who;
    std::int64_t paddle;
};

const Bench kBench[] = {
    {"paddle-3", "paddle.3", "Mrs Ledbury", 3},
    {"paddle-7", "paddle.7", "Kestrel Fine Art", 7},
    {"paddle-11", "paddle.11", "Mr Selwood", 11},
    {"paddle-14", "paddle.14", "Hallam & Rooke", 14},
};

enum class Scenario { SaleDay, BoardAsRead, RunTheBookUp };

struct Tap {
    std::int64_t bids_delivered = 0;
    std::int64_t steps_delivered = 0;
    std::int64_t determinations_delivered = 0;
    std::map<std::string, std::int64_t> published;
    std::vector<std::string> refusals;
    /// The last thing the room was asked, as the room heard it. The day watches
    /// this so a control can be fired at the exact moment it is most plausible,
    /// rather than at a beat number guessed in advance.
    Asking last_asking{};
};

/// Bids the DAY put on the wire that no bidder made. There is one: the late bid
/// for the spaniels, forged as Kestrel four lots after the spaniels went. It is
/// the difference between the tap's count and the room's own, and naming it is
/// what makes the difference a measurement instead of a discrepancy.
constexpr std::int64_t kForgedBids = 1;

/// THE AUDIT FINDS THINGS WRONG; THE SCENARIO DECIDES WHAT A PASS IS.
///
/// A control run is supposed to produce a wrong sale, so a red audit line in
/// one of them is the point rather than a failure. Keeping the two apart is
/// what lets a control assert the sharpest thing available: not merely "the
/// audit noticed", but "the audit found EXACTLY ONE thing wrong and it was this
/// one" — which also says that the other check, the one that structurally
/// cannot see this failure, stayed quiet.
std::int64_t g_wrong = 0;

void check(bool ok, const std::string& what) {
    std::cout << (ok ? "  ok    " : "  WRONG ") << what << "\n";
    if (!ok) {
        ++g_wrong;
    }
}

/// A statement about the run itself rather than about the sale. These must hold
/// in every scenario; a control that broke one would not be a control.
bool g_broken = false;

void must(bool ok, const std::string& what) {
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
    if (!ok) {
        g_broken = true;
    }
}

int run(Scenario scenario, const std::vector<std::string>& bidder_paths) {
    loom::Switchboard bus;
    loom::Kernel kernel(bus);

    std::cout << "\n  saleroom -- Wednesday morning, lots 11 to 16\n";
    std::cout << "  containment: " << loom::Kernel::containment_note() << "\n\n";

    // ---- the saleroom's own people ----------------------------------------

    // The office says nothing to anybody. A reserve is not told, it is up.
    const loom::WeaveId office =
        mount_office<FrontOffice>(bus, loom::Grant{}, "front-office");

    // The clerk says nothing to anybody either. It only ever writes.
    const loom::WeaveId clerk = mount_office<Clerk>(bus, loom::Grant{}, "clerk");

    loom::Grant rostrum_grant;
    rostrum_grant.allow_to_role("Step", 1, "clerk")
        .allow_to_role("Determination", 1, "clerk")
        .allow_to_any("LotUp", 1)
        .allow_to_any("Asking", 1)
        .allow_to_any("Knocked", 1)
        .allow_to_any("BoughtIn", 1)
        .allow_to_any("Withdrawn", 1)
        .allow_observe("Reserve", 1);
    const loom::WeaveId rostrum =
        mount_office<Rostrum>(bus, std::move(rostrum_grant), "rostrum",
                              scenario == Scenario::BoardAsRead,
                              scenario == Scenario::RunTheBookUp);

    // ---- the room ---------------------------------------------------------
    //
    // A BIDDER MAY SPEAK TO THE ROSTRUM AND TO NOBODY ELSE. That is the sentence
    // the honesty of this sale rests on: it is not a manner and it is not a
    // convention, it is the reason a ring cannot form in this room. Four
    // separately built people, four identical grants, and every one of them as
    // narrow as it can be made.
    std::map<std::int64_t, loom::WeaveId> paddles;
    for (std::size_t i = 0; i < std::size(kBench); ++i) {
        loom::Grant g;
        g.allow_to_role("Bid", 1, "rostrum");
        loom::LoadResult r =
            kernel.load(kBench[i].artifact_key, bidder_paths[i], kBench[i].role, std::move(g));
        if (!r.ok) {
            std::cout << "  !!    " << kBench[i].who << " could not get in: " << r.error << "\n";
            return 2;
        }
        paddles[kBench[i].paddle] = r.id;
    }

    // ---- the tap ----------------------------------------------------------
    Tap tap;
    bus.add_observer([&tap](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Refused) {
            tap.refusals.push_back(std::string(loom::name_of(e.refusal.reason)) + " on " +
                                   e.schema_name);
            return;
        }
        if (e.kind != loom::EventKind::Delivered) {
            return;
        }
        if (e.schema_name == "Bid") {
            ++tap.bids_delivered;
        } else if (e.schema_name == "Step") {
            ++tap.steps_delivered;
        } else if (e.schema_name == "Determination") {
            ++tap.determinations_delivered;
        } else {
            ++tap.published[e.schema_name];
            if (e.schema_name == "Asking" && e.payload != nullptr) {
                tap.last_asking = loom::from_value<Asking>(*e.payload);
            }
        }
    });

    // Enqueue, then drain. Dispatch is single-threaded FIFO and this
    // application depends on it precisely: a beat is delivered to the rostrum,
    // the rostrum's Asking reaches every bidder, and every Bid it causes is
    // back at the rostrum before the day says anything else. That is what makes
    // "the bids of one beat" a set rather than a race.
    auto host_send = [&bus](const std::string& role, auto&& payload) {
        bus.send_to_role(role, loom::Message(loom::to_value(payload)));
        bus.pump();
    };

    // What the bus said about the last thing the day put on the wire on
    // somebody's behalf, printed where it happened rather than only in a total.
    auto echo_the_bus = [&tap](const std::string& who) {
        say(who, tap.refusals.empty() ? "nothing refused"
                                      : "the bus: " + tap.refusals.back());
    };

    // ---- before the doors open -------------------------------------------
    note("the entry clerk brings the vendors' instructions through");
    for (const LotCard& c : kCatalogue) {
        if (c.sheet_lodged) {
            host_send("front-office", Instructions{c.lot, c.reserve, c.unreserved});
        }
    }
    note("lot 15's vendor rang hers in last night and the sheet was never made up");
    std::cout << "\n";

    // ---- the sale ---------------------------------------------------------
    BidderState ledbury_account{}; // taken when she surrenders her paddle
    bool ledbury_gone = false;
    loom::SenseRefusal selwood_tried_the_board = loom::SenseRefusal::None;
    bool selwood_tried = false;

    for (const LotCard& c : kCatalogue) {
        g_lot = c.lot;
        host_send("front-office", SheetFor{c.lot});

        if (c.lot == 12 && !selwood_tried) {
            // CONTROL C -- THE CLAIM-SIDE EDGE.
            //
            // Mr Selwood is selling the bureau, he is standing in the room, and
            // he has decided overnight that the £400 he left with the office is
            // too little. A vendor who could put his own figure up would be able
            // to move it after the catalogue is printed and after the room has
            // formed a view — which is why instructions go through the office
            // and stay there.
            //
            // He has no verb for it: a bidder claims nothing at all. So the
            // sale forges the claim on his behalf with the verified host door,
            // which stamps him as the claimant and then asks whether he holds
            // the office he is asking to speak as.
            note("CONTROL: Mr Selwood, selling lot 12, puts £700 on the board himself");
            const loom::SenseClaimResult res = bus.office_claim_as(
                paddles[11], "front-office", loom::to_value(Reserve{12, 700, false}));
            selwood_tried = true;
            selwood_tried_the_board = res.why;
            say("BOARD", std::string("refused: ") + loom::name_of(res.why));
        }

        host_send("rostrum", PutUpLot{c.lot, c.description, c.estimate_low, c.estimate_high,
                                      c.commission});

        bool false_hammer_tried = false;
        for (std::int64_t beat = 1; beat <= 40; ++beat) {
            if (c.lot == 13 && !false_hammer_tried && tap.last_asking.lot == 13 &&
                tap.last_asking.warning) {
                // CONTROL B -- THE AUTHORSHIP EDGE, fired at the one moment it
                // is worth anything: fair warning has been given on a lot that
                // is about to be bought in, and Mr Selwood is standing at his
                // own figure.
                //
                // He tells the clerk the clock is his, in the words the rostrum
                // would have used. The destination is exactly where a real
                // determination goes, the shape is exactly what the clerk
                // writes, the lot IS in hand, the money IS the money standing,
                // and the paddle IS his. Only the office is false.
                //
                // He has no verb for it — a bidder speaks one shape to one
                // office — so the day forges it with the verified host door,
                // which stamps him as the sender and then asks whether he holds
                // the office he is asking to speak as.
                false_hammer_tried = true;
                note("CONTROL: Mr Selwood tells the clerk the clock is his, as the rostrum");
                bus.office_send_to_role_as(
                    paddles[11], "rostrum", "clerk",
                    loom::Message(loom::to_value(Determination{
                        13, tap.last_asking.standing, 11, false, true, ""})));
                bus.pump();
                echo_the_bus("ROSTRUM");
            }
            if (c.lot == 12 && beat == 2) {
                // CONTROL A -- THE TOPOLOGY EDGE, IN ITS STRONGEST FORM.
                //
                // Hallam & Rooke want the bureau and would rather not pay
                // Kestrel's money for it. Speaking AS THEMSELVES — paddle 14 is
                // an office they genuinely hold, so authorship succeeds — they
                // offer Kestrel a knock-out.
                //
                // Every other check in this application would accept the act.
                // The shape is one Kestrel accepts, the sender really is who it
                // says it is, the content is exactly what a ring approach looks
                // like, and KESTREL WOULD HONOUR IT: its handler stands the lot
                // off and says so in its own account. There is no domain rule
                // behind this one. The grant is the only thing between this
                // saleroom and a ring.
                note("CONTROL: Hallam & Rooke offer Kestrel a knock-out on the bureau");
                bus.office_send_to_role_as(paddles[14], "paddle.14", "paddle.7",
                                           loom::Message(loom::to_value(KnockOut{12, 14, 80})));
                bus.pump();
                echo_the_bus("ROOM");
            }
            if (c.lot == 14 && beat == 3) {
                // A DOMAIN CONTROL, and a different owner: the substrate
                // delivers this one and the ROSTRUM discriminates.
                note("CONTROL: somebody at the back bids without a paddle");
                host_send("rostrum", Bid{14, 200, 0});
            }
            if (c.lot == 16 && beat == 3) {
                // The other domain control: a bid for a lot that has gone.
                note("CONTROL: a late bid for the spaniels, four lots too late");
                bus.office_send_to_role_as(paddles[7], "paddle.7", "rostrum",
                                           loom::Message(loom::to_value(Bid{11, 300, 7})));
                bus.pump();
            }
            host_send("rostrum", Beat{beat});
            // The lot is over when the clerk has a determination for it.
            const ClerkState book = account_of<ClerkState>(bus, clerk, "the clerk");
            bool done = false;
            for (const Determination& d : book.record) {
                if (d.lot == c.lot) {
                    done = true;
                }
            }
            if (done) {
                break;
            }
        }

        if (c.lot == 15 && !ledbury_gone) {
            // She came for the spaniels and had a go at the picture, and that
            // is her morning. The clerk takes her paddle back and settles up,
            // and her account of herself is taken as she goes — which is when
            // you may ask a buyer what they were good for.
            g_lot = 0;
            note("Mrs Ledbury surrenders paddle 3 and goes home");
            ledbury_account = account_of<BidderState>(bus, paddles[3], "Mrs Ledbury");
            kernel.unload("paddle-3");
            ledbury_gone = true;
            g_lot = c.lot;
        }
        std::cout << "\n";
    }
    g_lot = 0;

    // -----------------------------------------------------------------------
    // THE AUDIT.
    //
    // Nobody in the room can do this. The house can, and only afterwards, and
    // only by asking three sets of people who each know one part:
    //
    //     the bidders   what they were good for     (their own accounts)
    //     the clerk     what was actually bid       (the sale record)
    //     the house     the reserves and the book   (its own catalogue)
    //
    // The hammer price is the one number none of them chose.
    // -----------------------------------------------------------------------

    std::cout << "  ---- after the sale ---------------------------------------------\n\n";

    std::vector<BidderState> room;
    for (const Bench& b : kBench) {
        if (b.paddle == 3 && ledbury_gone) {
            room.push_back(ledbury_account);
            continue;
        }
        room.push_back(account_of<BidderState>(bus, paddles[b.paddle], b.who));
    }
    const ClerkState book = account_of<ClerkState>(bus, clerk, "the clerk");
    const OfficeState front = account_of<OfficeState>(bus, office, "the front office");
    const RostrumState ros = account_of<RostrumState>(bus, rostrum, "the rostrum");

    auto limit_of = [&room](const BidderState& b, std::int64_t lot) {
        (void)room;
        for (const LotAccount& a : b.lots) {
            if (a.lot == lot) {
                return a.limit;
            }
        }
        return static_cast<std::int64_t>(0);
    };

    std::cout << "  the sale record, as the clerk has it\n\n";
    for (const Determination& d : book.record) {
        std::string line = "    lot " + std::to_string(d.lot) + "  ";
        if (!d.sold) {
            line += d.why;
        } else if (d.on_the_book) {
            line += money(d.hammer) + "  to the book";
        } else {
            line += money(d.hammer) + "  to paddle " + std::to_string(d.buyer);
        }
        std::cout << line << "\n";
    }

    std::cout << "\n  what the room was good for, asked afterwards, one by one\n\n";
    for (const BidderState& b : room) {
        std::string line = "    ";
        line.append(b.name);
        line.resize(24, ' ');
        for (const LotAccount& a : b.lots) {
            line += std::to_string(a.lot) + ":" + money(a.limit) +
                    (a.bids_made > 0 ? "" : a.limit > 0 ? "(never bid)" : "") + "  ";
        }
        std::cout << line << "\n";
    }

    std::cout << "\n  the checks\n\n";

    must(!book.record.empty() && tap.bids_delivered > 0,
         "the sale happened at all (an audit of nothing is not an audit)");

    std::int64_t sold = 0;
    std::int64_t under_reserve = 0;
    std::int64_t book_run_up = 0;

    for (const LotCard& c : kCatalogue) {
        const Determination* d = nullptr;
        for (const Determination& e : book.record) {
            if (e.lot == c.lot) {
                d = &e;
            }
        }
        if (d == nullptr) {
            must(false, "lot " + std::to_string(c.lot) + " has no determination at all");
            continue;
        }

        // What the room was good for on this lot, out of its own mouths.
        std::int64_t l1 = 0;
        std::int64_t l2 = 0;
        std::int64_t l1_paddle = 0;
        for (const BidderState& b : room) {
            const std::int64_t l = limit_of(b, c.lot);
            if (l > l1) {
                l2 = l1;
                l1 = l;
                l1_paddle = b.paddle;
            } else if (l > l2) {
                l2 = l;
            }
        }

        std::vector<Step> steps;
        for (const Step& s : book.steps) {
            if (s.lot == c.lot) {
                steps.push_back(s);
            }
        }

        const std::string tag = "lot " + std::to_string(c.lot) + ": ";

        if (!d->sold) {
            if (d->why.rfind("withdrawn", 0) == 0) {
                check(steps.empty(), tag + "withdrawn, and nothing was bid on it");
                check(!c.sheet_lodged, tag + "withdrawn, and the office really had no sheet");
                continue;
            }
            check(l1 < c.reserve && c.commission < c.reserve,
                  tag + "bought in, and nobody here could reach " + money(c.reserve) +
                      " (best was " + money(std::max(l1, c.commission)) + ")");
            continue;
        }

        ++sold;

        const bool at_or_above =
            c.unreserved || d->hammer >= c.reserve;
        if (!at_or_above) {
            ++under_reserve;
        }
        check(at_or_above, tag + "sold at " + money(d->hammer) + ", " +
                               (c.unreserved ? "and it was unreserved"
                                             : "against a reserve of " + money(c.reserve)));

        if (d->on_the_book) {
            check(d->hammer <= c.commission,
                  tag + "the absentee did not pay more than the " + money(c.commission) +
                      " he left");
            check(next_rung(d->hammer) > l1,
                  tag + "the room would not have gone one more (" + money(l1) + " good for, " +
                      money(next_rung(d->hammer)) + " asked)");
            // COMPETITIVE EXECUTION. Every rung the rostrum took on the book
            // must be one rung above a bid the ROOM had just made. There is
            // nobody in the saleroom who could check this.
            bool competitive = true;
            std::string first_bad;
            for (std::size_t i = 0; i < steps.size(); ++i) {
                if (steps[i].who != kTheBook) {
                    continue;
                }
                const bool ok = i > 0 && steps[i - 1].who != kTheBook &&
                                steps[i].amount == next_rung(steps[i - 1].amount);
                if (!ok && competitive) {
                    first_bad = money(steps[i].amount) + " on the book" +
                                (i > 0 ? " over " + money(steps[i - 1].amount) + " in the room"
                                       : " with nothing in the room");
                }
                competitive = competitive && ok;
            }
            if (!competitive) {
                ++book_run_up;
            }
            check(competitive, tag + "the book was run against the room one rung at a time" +
                                   (competitive ? "" : " -- IT WAS NOT: " + first_bad));
        } else {
            check(d->buyer == l1_paddle,
                  tag + "it went to the one who was good for the most (paddle " +
                      std::to_string(l1_paddle) + " at " + money(l1) + ")");
            check(d->hammer <= l1, tag + "the buyer did not pay more than the " + money(l1) +
                                       " he was good for");
            // The English auction's own arithmetic: the price stops at the first
            // rung the underbidder will not pay. Nobody computes this during the
            // sale, and nobody could.
            check(next_rung(d->hammer) > l2,
                  tag + "the underbidder would not have gone one more (" + money(l2) +
                      " good for, " + money(next_rung(d->hammer)) + " asked)");
        }
    }

    std::cout << "\n";

    // ---- three accounts of the same bidding -------------------------------
    std::int64_t claimed_bids = 0;
    std::int64_t approaches = 0;
    for (const BidderState& b : room) {
        for (const LotAccount& a : b.lots) {
            claimed_bids += a.bids_made;
        }
        approaches += b.approaches_received;
    }
    std::int64_t paddle_steps = 0;
    std::int64_t book_steps = 0;
    for (const Step& s : book.steps) {
        if (s.who == kTheBook) {
            ++book_steps;
        } else {
            ++paddle_steps;
        }
    }

    std::cout << "    the bidders   " << claimed_bids << " hands up, by their own account\n";
    std::cout << "    the clerk     " << paddle_steps << " rungs to the room + " << book_steps
              << " on the book\n";
    std::cout << "    the tap       " << tap.bids_delivered << " Bid deliveries, "
              << tap.steps_delivered << " Steps, " << tap.determinations_delivered
              << " Determinations\n";
    std::cout << "    the day       " << kForgedBids << " bid forged (the late one) + "
              << ros.no_paddle << " from the back with no paddle\n";
    std::cout << "    disregarded   " << ros.bids_disregarded << " (wrong lot, too late, too low)"
              << ", " << ros.beaten_on_the_breath << " beaten on the same breath\n";
    std::cout << "    bus refusals  " << tap.refusals.size();
    if (!tap.refusals.empty()) {
        std::cout << "  [";
        for (std::size_t i = 0; i < tap.refusals.size(); ++i) {
            std::cout << (i ? ", " : "") << tap.refusals[i];
        }
        std::cout << "]";
    }
    std::cout << "\n\n";

    must(claimed_bids + kForgedBids + ros.no_paddle == tap.bids_delivered,
         "the tap is exactly " + std::to_string(kForgedBids + ros.no_paddle) +
             " higher than the room's own count, and that is the day's own two frames");
    must(paddle_steps + ros.bids_disregarded + ros.no_paddle + ros.beaten_on_the_breath ==
             tap.bids_delivered,
         "every Bid delivered is a rung in the record, disregarded, beaten on the breath,"
         " or paddleless");
    must(tap.steps_delivered == static_cast<std::int64_t>(book.steps.size()),
         "the clerk wrote down every Step that reached it and invented none");
    must(book.not_the_rostrum == 0, "nothing reached the clerk that was not the rostrum's");
    must(front.asked_for_a_sheet_we_have_not_got == 1,
         "the office was asked for exactly one sheet it had not got");

    // A fourth account, and the cheapest one: the buyer agrees they bought it.
    // A determination naming a paddle that says it bought nothing would be a
    // record of a sale to somebody who was not there.
    bool buyers_agree = true;
    for (const Determination& d : book.record) {
        if (!d.sold || d.on_the_book) {
            continue;
        }
        bool agreed = false;
        for (const BidderState& b : room) {
            if (b.paddle != d.buyer) {
                continue;
            }
            for (const LotAccount& a : b.lots) {
                if (a.lot == d.lot && a.bought) {
                    agreed = true;
                }
            }
        }
        buyers_agree = buyers_agree && agreed;
    }
    must(buyers_agree, "every buyer in the record heard the hammer and says the lot is theirs");

    // ---- the three hostile edges ------------------------------------------
    std::cout << "\n";
    must(approaches == 0,
         "EDGE A  no bidder was ever approached by another bidder"
         " (the knock-out never arrived)");
    must(std::count(tap.refusals.begin(), tap.refusals.end(), "CapabilityDenied on KnockOut") == 1,
         "EDGE A  and the bus refused it: CapabilityDenied on KnockOut");
    must(std::count(tap.refusals.begin(), tap.refusals.end(),
                    "RoleAuthorshipDenied on Determination") == 1,
         "EDGE B  a hammer that was not the rostrum's: RoleAuthorshipDenied, nothing queued");
    bool clock_is_selwoods = false;
    for (const Determination& d : book.record) {
        if (d.lot == 13 && d.sold) {
            clock_is_selwoods = true;
        }
    }
    must(!clock_is_selwoods, "EDGE B  and the clock is not in the record as sold to paddle 11");
    must(selwood_tried && selwood_tried_the_board == loom::SenseRefusal::OfficeNotHeld,
         "EDGE C  a vendor cannot put his own figure on the board: OfficeNotHeld");
    must(tap.refusals.size() == 2, "and those are the only two things Loom refused all morning");

    // ---- the verdict ------------------------------------------------------
    //
    // The audit is the same audit in all three runs. What differs is what the
    // scenario expects it to find — and, in the two controls, that the OTHER
    // check stayed silent, because a check that cannot see a failure is exactly
    // as important a fact as one that can.
    std::cout << "\n  the audit found " << g_wrong << " thing(s) wrong with the sale\n\n";

    bool as_expected = false;
    if (scenario == Scenario::BoardAsRead) {
        std::cout << "  ---- what this control is for -----------------------------------\n\n";
        std::cout << "  The sheet for the watercolour never reached the office, so the board\n";
        std::cout << "  still showed lot 14 -- a box of plated ware, unreserved. He glanced at\n";
        std::cout << "  it. Every measure inside the room is green: the bidding was genuine,\n";
        std::cout << "  the ladder was right, the record balances, the buyer agrees the lot is\n";
        std::cout << "  hers, and the underbidder would not have gone one more.\n\n";
        as_expected = (g_wrong == 1 && under_reserve == 1 && book_run_up == 0 && sold == 5);
        must(under_reserve == 1 && g_wrong == 1,
             "a lot went under its reserve -- lot 15's vendor is short, and the audit found"
             " that and nothing else");
        must(book_run_up == 0,
             "and the competitive-execution audit says nothing at all about it -- it cannot");
    } else if (scenario == Scenario::RunTheBookUp) {
        std::cout << "  ---- what this control is for -----------------------------------\n\n";
        std::cout << "  The rostrum took the absentee's commission straight to its ceiling\n";
        std::cout << "  instead of one rung above the room. Nothing on the floor was wrong:\n";
        std::cout << "  the reserve was cleared, the hammer went to the highest figure in the\n";
        std::cout << "  building, and the record balances.\n\n";
        as_expected = (g_wrong == 1 && book_run_up == 1 && under_reserve == 0);
        must(book_run_up == 1 && g_wrong == 1,
             "the book was run up -- the absentee paid for a room that had stopped, and the"
             " audit found that and nothing else");
        must(under_reserve == 0,
             "and the reserve audit says nothing at all about it -- it cannot");
    } else {
        as_expected = (g_wrong == 0 && sold == 4);
        must(g_wrong == 0, "the audit found nothing wrong with the sale");
        must(sold == 4, "four of the six lots sold");
    }

    std::cout << "\n";
    if (g_broken || !as_expected) {
        std::cout << "  THE MORNING DOES NOT ADD UP\n\n";
        return 1;
    }
    std::cout << (scenario == Scenario::SaleDay ? "  A GOOD MORNING'S SELLING\n\n"
                                                : "  THE CONTROL BEHAVED AS A CONTROL MUST\n\n");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> paths;
    Scenario scenario = Scenario::SaleDay;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--take-the-board-as-read") {
            scenario = Scenario::BoardAsRead;
        } else if (a == "--run-the-book-up") {
            scenario = Scenario::RunTheBookUp;
        } else {
            paths.push_back(a);
        }
    }
    if (paths.size() != 4) {
        std::cerr << "usage: sale <paddle-3.so> <paddle-7.so> <paddle-11.so> <paddle-14.so>"
                     " [--take-the-board-as-read | --run-the-book-up]\n";
        return 2;
    }
    return run(scenario, paths);
}
