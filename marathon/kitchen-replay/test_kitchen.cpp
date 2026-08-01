// The kitchen replay's suite.
//
// THE QUESTION UNDER TEST, restated so a reader of this file alone knows what is
// being proven: Loom ends a conversation when a participant dies, and tells the
// other party nothing. Can a kitchen therefore keep an honest promise for EVERY
// order it accepts — including orders whose cook disappears mid-dish — using
// only existing public Loom and Zengine behaviour?
//
// AND THE REPLAY'S OWN QUESTION, which is different: a year of substrate work
// happened between Night One and this file. What actually changed for an
// application that already existed?
//
// Every case below runs real .so weaves through the real kernel, the real Weave
// Manager, real graceful swaps and real prepared replacements. The one
// substitution is the Timer's CLOCK, and it is labelled in harness.hpp.
//
// The cases are grouped by what they are for:
//   1. the kitchen works at all
//   2. intent, preference, fallback, and the resolved receipt
//   3. absence and return
//   4. THE HEADLINE: a cook that vanishes mid-dish
//   5. continuity: the same moment, done gracefully
//   6. replaceable domain policy
//   7. the promiser itself being replaced
//   8. hostile arrivals — what the consumer obligation can and cannot catch
//   9. REPLAY: the prepared-replacement ceremony
//  10. REPLAY: what the rest of the world makes of it afterwards

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "harness.hpp"

#include <string>
#include <vector>

using marathon::testing::Kitchen;
namespace kitchen = marathon::kitchen;

namespace {

/// Did the diner hear a line containing all of these fragments?
bool heard(const kitchen::DinerBook& book, const std::vector<std::string>& fragments) {
    for (const std::string& line : book.heard) {
        bool all = true;
        for (const std::string& f : fragments) {
            all = all && line.find(f) != std::string::npos;
        }
        if (all) {
            return true;
        }
    }
    return false;
}

/// How many lines contain all of these fragments. Used where "exactly once"
/// is the claim — a dish served twice is a different bug from one never served.
std::size_t heard_count(const kitchen::DinerBook& book,
                        const std::vector<std::string>& fragments) {
    std::size_t n = 0;
    for (const std::string& line : book.heard) {
        bool all = true;
        for (const std::string& f : fragments) {
            all = all && line.find(f) != std::string::npos;
        }
        n += all ? 1u : 0u;
    }
    return n;
}

/// The whole transcript, for a failure message that says what actually happened.
std::string transcript(const kitchen::DinerBook& book) {
    std::string out = "\n  diner heard:";
    for (const std::string& line : book.heard) {
        out += "\n    " + line;
    }
    if (book.heard.empty()) {
        out += " (nothing)";
    }
    return out;
}

/// The owner's own trace, for the replacement cases.
std::string desk_notes(const marathon::testing::OwnerDesk& desk) {
    std::string out = "\n  owner saw:";
    for (const std::string& n : desk.notes) {
        out += "\n    " + n;
    }
    if (desk.notes.empty()) {
        out += " (nothing)";
    }
    return out;
}

} // namespace

// ---- 1. the kitchen works at all -------------------------------------------

TEST_CASE("the kitchen opens: every boot command is answered and both stations announce") {
    Kitchen k;
    k.boot();

    // Five loads, five answers, none refused. The steward answers every op — the
    // most dangerous surface in the system is not allowed to be the silent one.
    CHECK(k.oplog().pending.empty());
    REQUIRE(k.oplog().answers.size() == 5);
    for (const std::string& a : k.oplog().answers) {
        CHECK(a.find("refused") == std::string::npos);
    }
}

TEST_CASE("the kitchen answers a diagnostic about itself, authentically") {
    Kitchen k;
    k.boot();
    k.order("s1", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(30);

    k.ask_status();
    k.pump(10);

    REQUIRE(k.status().size() == 1);
    const std::string& tally = k.status()[0];
    CHECK_MESSAGE(tally.find("placed=1") != std::string::npos, tally);
    CHECK_MESSAGE(tally.find("served=1") != std::string::npos, tally);
    CHECK_MESSAGE(tally.find("lost=0") != std::string::npos, tally);
    // The roster is part of the diagnostic: "who do you believe is here?" is the
    // question a kitchen with no way to detect absence most needs answered.
    CHECK_MESSAGE(tally.find("grill") != std::string::npos, tally);
    CHECK_MESSAGE(tally.find("fryer") != std::string::npos, tally);
}

// ---- 2. intent, preference, fallback, receipt -------------------------------

TEST_CASE("a preference that can be honoured is honoured, and the dish arrives") {
    Kitchen k;
    k.boot();
    k.order("s1", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(30);

    CHECK_MESSAGE(heard(k.book(), {"receipt s1", kitchen::kRoutedPreferred, "@grill"}),
                  transcript(k.book()));
    CHECK_MESSAGE(heard(k.book(), {"served s1", "steak", "grill"}), transcript(k.book()));
}

TEST_CASE("an unavailable preference falls back, and the receipt names both halves") {
    Kitchen k;
    k.boot();
    // The grill does not cook wings; the fryer does.
    k.order("w1", "wings", "grill", kitchen::kFallbackAnyStation);
    k.pump(30);

    CHECK_MESSAGE(heard(k.book(), {"receipt w1", kitchen::kRoutedFallback, "@fryer"}),
                  transcript(k.book()));
    // A stranger must be able to read WHY without holding the kitchen header.
    CHECK_MESSAGE(heard(k.book(), {"receipt w1", "does not cook 'wings'"}), transcript(k.book()));
    CHECK_MESSAGE(heard(k.book(), {"served w1", "wings", "fryer"}), transcript(k.book()));
}

TEST_CASE("a REQUIRED preference is refused rather than quietly re-routed") {
    Kitchen k;
    k.boot();
    k.order("w2", "wings", "grill", kitchen::kFallbackNone);
    k.pump(30);

    CHECK_MESSAGE(heard(k.book(), {"receipt w2", kitchen::kRoutedRefused}), transcript(k.book()));
    CHECK_MESSAGE(heard(k.book(), {"receipt w2", "no fallback is acceptable"}),
                  transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served w2"}));
    CHECK_FALSE(heard(k.book(), {"lost w2"}));
}

TEST_CASE("an unknown fallback spelling is refused, never guessed at") {
    Kitchen k;
    k.boot();
    k.order("x1", "steak", "grill", "whatever_you_think_best");
    k.pump(30);

    CHECK_MESSAGE(heard(k.book(), {"receipt x1", kitchen::kRoutedRefused,
                                   "whatever_you_think_best"}),
                  transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served x1"}));
}

TEST_CASE("a dish nobody cooks is refused with a reason, not dropped") {
    Kitchen k;
    k.boot();
    k.order("z1", "souffle", kitchen::kPreferAny, kitchen::kFallbackAnyStation);
    k.pump(30);

    CHECK_MESSAGE(heard(k.book(), {"receipt z1", kitchen::kRoutedRefused, "souffle"}),
                  transcript(k.book()));
}

TEST_CASE("two orders with the same name from the same diner: the second is refused") {
    Kitchen k;
    k.boot();
    k.order("dup", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(6);
    k.order("dup", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(30);

    CHECK_MESSAGE(heard(k.book(), {"receipt dup", "already have an order named"}),
                  transcript(k.book()));
}

// ---- 3. absence and return --------------------------------------------------

TEST_CASE("a kitchen with no stations refuses honestly instead of waiting") {
    Kitchen k;
    k.load("zengine-timer-virtual", zengine::timer::kTimerRole);
    k.load("kitchen-policy-house", kitchen::kPolicyRole);
    k.load("kitchen-expediter", kitchen::kExpediterRole);
    k.pump(20);

    k.order("c1", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(20);

    CHECK_MESSAGE(heard(k.book(), {"receipt c1", kitchen::kRoutedRefused, "kitchen is closed"}),
                  transcript(k.book()));
}

TEST_CASE("with no policy loaded the promise still ends in a word, not silence") {
    Kitchen k;
    k.load("zengine-timer-virtual", zengine::timer::kTimerRole);
    k.load("kitchen-expediter", kitchen::kExpediterRole);
    k.load("kitchen-grill", kitchen::station_role("grill"));
    k.pump(20);

    k.order("p1", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(140);

    CHECK_MESSAGE(heard(k.book(), {"receipt p1", kitchen::kRoutedRefused,
                                   "no kitchen policy answered"}),
                  transcript(k.book()));
}

TEST_CASE("a station that arrives late is usable as soon as it announces itself") {
    Kitchen k;
    k.load("zengine-timer-virtual", zengine::timer::kTimerRole);
    k.load("kitchen-policy-house", kitchen::kPolicyRole);
    k.load("kitchen-expediter", kitchen::kExpediterRole);
    k.pump(20);

    k.order("late1", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(20);
    CHECK_MESSAGE(heard(k.book(), {"receipt late1", "kitchen is closed"}), transcript(k.book()));

    k.load("kitchen-grill", kitchen::station_role("grill"));
    k.pump(20);
    k.order("late2", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(30);

    CHECK_MESSAGE(heard(k.book(), {"served late2", "grill"}), transcript(k.book()));
}

// ---- 4. THE HEADLINE --------------------------------------------------------

TEST_CASE("a cook that walks out mid-dish costs a word, never a silence") {
    Kitchen k;
    k.boot();

    // A slow dish: fourteen passes, so there is a real window in which the job is
    // held by a weave that is about to stop existing.
    k.order("f1", "brisket", "grill", kitchen::kFallbackNone);
    k.pump(10); // the receipt is issued and the Prep is on the griddle
    REQUIRE_MESSAGE(heard(k.book(), {"receipt f1", kitchen::kRoutedPreferred, "@grill"}),
                    transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served f1"}));

    // The grill walks out with nothing to replace it: the role is left UNHELD,
    // which is the Weave Manager's own documented outcome for a swap whose
    // successor cannot load. Loom tells the expediter NOTHING about this.
    k.evict(kitchen::station_role("grill"));
    k.pump(140);

    CHECK_MESSAGE(heard(k.book(), {"lost f1", "@grill", "never plated it"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served f1"}));
    // Every promise ends in a message: the diner is waiting on nothing.
    CHECK(k.book().outstanding.empty());
}

TEST_CASE("a station that lost a dish is struck from the roster until it returns") {
    Kitchen k;
    k.boot();
    k.order("f2", "brisket", "grill", kitchen::kFallbackNone);
    k.pump(10);
    k.evict(kitchen::station_role("grill"));
    k.pump(140);
    REQUIRE_MESSAGE(heard(k.book(), {"lost f2", "struck from the roster"}), transcript(k.book()));

    // The grill is gone from the roster, so a REQUIRED preference for it is now
    // refused as "not open" — the kitchen's belief tracks the evidence it has.
    k.order("f3", "steak", "grill", kitchen::kFallbackNone);
    k.pump(30);
    CHECK_MESSAGE(heard(k.book(), {"receipt f3", kitchen::kRoutedRefused, "is not open"}),
                  transcript(k.book()));

    // ...and it comes back the only way a service can: by announcing itself.
    k.load("kitchen-grill", kitchen::station_role("grill"));
    k.pump(20);
    k.order("f4", "steak", "grill", kitchen::kFallbackNone);
    k.pump(30);
    CHECK_MESSAGE(heard(k.book(), {"served f4", "grill"}), transcript(k.book()));
}

// ---- 5. continuity: the same moment, done gracefully ------------------------

TEST_CASE("a GRACEFUL replacement carries the dish across, and nothing is lost") {
    Kitchen k;
    k.boot();
    k.order("g1", "brisket", "grill", kitchen::kFallbackNone);
    k.pump(12); // several passes of a fourteen-pass dish are already done
    REQUIRE_MESSAGE(heard(k.book(), {"receipt g1", "@grill"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served g1"}));

    // Same moment as the eviction above — a different ceremony. The incumbent is
    // asked first, writes its letter, and the heir claims it by role.
    k.swap(kitchen::station_role("grill"), "kitchen-grill-2", /*graceful=*/true);
    k.pump(120);

    CHECK_MESSAGE(heard(k.book(), {"served g1", "brisket", "grill"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"lost g1"}));
}

TEST_CASE("a HARD replacement of the same station loses the dish, and says so") {
    Kitchen k;
    k.boot();
    k.order("h1", "brisket", "grill", kitchen::kFallbackNone);
    k.pump(6);
    REQUIRE_MESSAGE(heard(k.book(), {"receipt h1", "@grill"}), transcript(k.book()));

    // The successor loads fine — the ROLE is never empty for long — but nothing
    // was said, so the work in the predecessor's hands is simply gone.
    k.swap(kitchen::station_role("grill"), "kitchen-grill-2", /*graceful=*/false);
    k.pump(140);

    CHECK_MESSAGE(heard(k.book(), {"lost h1", "never plated it"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served h1"}));
}

// ---- 6. replaceable domain policy -------------------------------------------

TEST_CASE("swapping the policy changes where an identical order goes, and why") {
    Kitchen k;
    k.boot("kitchen-policy-house");
    k.order("r1", "fries", "grill", kitchen::kFallbackAnyStation);
    k.pump(40);
    CHECK_MESSAGE(heard(k.book(), {"receipt r1", kitchen::kRoutedPreferred, "@grill"}),
                  transcript(k.book()));

    k.swap(kitchen::kPolicyRole, "kitchen-policy-rush", /*graceful=*/false);
    k.pump(20);

    k.order("r2", "fries", "grill", kitchen::kFallbackAnyStation);
    k.pump(40);
    CHECK_MESSAGE(heard(k.book(), {"receipt r2", kitchen::kRoutedFallback, "@fryer"}),
                  transcript(k.book()));
    CHECK_MESSAGE(heard(k.book(), {"receipt r2", "[rush]", "specialist"}), transcript(k.book()));

    // Both dishes still arrive: the swap changed the decision, not the kitchen.
    CHECK_MESSAGE(heard(k.book(), {"served r1", "grill"}), transcript(k.book()));
    CHECK_MESSAGE(heard(k.book(), {"served r2", "fryer"}), transcript(k.book()));
}

TEST_CASE("a required preference binds every policy, including the one that disagrees") {
    Kitchen k;
    k.boot("kitchen-policy-rush");
    k.order("r3", "fries", "grill", kitchen::kFallbackNone);
    k.pump(120);

    CHECK_MESSAGE(heard(k.book(), {"receipt r3", kitchen::kRoutedPreferred, "@grill"}),
                  transcript(k.book()));
    CHECK_MESSAGE(heard(k.book(), {"served r3", "grill"}), transcript(k.book()));
}

// ---- 7. the promiser itself being replaced ----------------------------------

TEST_CASE("an expediter replaced gracefully keeps the promises it already made") {
    Kitchen k;
    k.boot();
    k.order("e1", "brisket", "grill", kitchen::kFallbackNone);
    k.pump(10);
    REQUIRE_MESSAGE(heard(k.book(), {"receipt e1", "@grill"}), transcript(k.book()));

    k.swap(kitchen::kExpediterRole, "kitchen-expediter", /*graceful=*/true);
    k.pump(120);

    // The dish is served by a DIFFERENT weave than the one that promised it —
    // which is exactly why the outcome could not have been an authenticated
    // answer, and exactly why the diner cannot check the sender.
    CHECK_MESSAGE(heard(k.book(), {"served e1", "brisket", "grill"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"lost e1"}));
}

TEST_CASE("a dish plated WHILE the kitchen is changing hands is not dropped") {
    Kitchen k;
    k.boot();
    // A six-pass dish and a handover started ten beats in: the plate lands inside
    // the ceremony. Night One's bus trace caught the `Plated` delivered to the
    // heir TWO TURNS BEFORE its own `zen.Activated`. The handover window is what
    // makes it survivable — and this case is what proves the race still exists.
    k.order("w9", "fries", "grill", kitchen::kFallbackNone);
    k.pump(10);
    REQUIRE_MESSAGE(heard(k.book(), {"receipt w9", "@grill"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served w9"}));

    k.swap(kitchen::kExpediterRole, "kitchen-expediter", /*graceful=*/true);
    k.pump(120);

    CHECK_MESSAGE(heard(k.book(), {"served w9", "fries", "grill"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"lost w9"}));
}

TEST_CASE("an expediter replaced MID-ROUTING closes the conversation it cannot bequeath") {
    Kitchen k;
    k.boot();
    // No policy answer can arrive before the swap: the policy is evicted first,
    // so the order is parked in routing with an unspent answer right.
    k.evict(kitchen::kPolicyRole);
    k.pump(10);
    k.order("e2", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(6);
    CHECK_FALSE_MESSAGE(heard(k.book(), {"receipt e2"}), transcript(k.book()));

    k.swap(kitchen::kExpediterRole, "kitchen-expediter", /*graceful=*/true);
    k.pump(40);

    // The receipt arrives — as a refusal, from the outgoing incarnation, at the
    // one moment it could still speak. Silence was the alternative.
    CHECK_MESSAGE(heard(k.book(), {"receipt e2", kitchen::kRoutedRefused,
                                   "replaced while this order was still being routed"}),
                  transcript(k.book()));
    CHECK(k.book().outstanding.empty());
}

// ---- 8. hostile arrivals ----------------------------------------------------

TEST_CASE("a receipt that Loom did not attest is ignored by the diner") {
    Kitchen k;
    k.boot();
    k.order("a1", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(30);
    CHECK_MESSAGE(heard(k.book(), {"receipt a1"}), transcript(k.book()));
    for (const std::string& line : k.book().heard) {
        CHECK(line.find("IGNORED") == std::string::npos);
    }
}

TEST_CASE("a forged routing answer is refused by Loom's attestation, not by luck") {
    Kitchen k;
    k.boot();
    // Park the order in routing with nobody to answer it honestly, so the only
    // RouteChoice that can arrive is the forged one.
    k.evict(kitchen::kPolicyRole);
    k.pump(10);
    k.order("q1", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(6);

    // Job numbers start at 1 and the correlation IS the job number: the rogue is
    // handed everything a bus-watcher could have worked out for itself.
    k.rogue_does(marathon::testing::ForgeRouteChoice{"q1", "fryer", 1});
    k.pump(10);

    // Nothing happened. `answers_ask()` is Loom's word that a delivery is the
    // authorized answer to a request THIS weave sent, and no weave can produce
    // it for a conversation it was not part of.
    CHECK_MESSAGE(k.book().heard.empty(), transcript(k.book()));

    // ...and the promise still ends in a word, from the watchdog, on time.
    k.pump(140);
    CHECK_MESSAGE(heard(k.book(), {"receipt q1", kitchen::kRoutedRefused,
                                   "no kitchen policy answered"}),
                  transcript(k.book()));
    CHECK(k.book().outstanding.empty());
}

TEST_CASE("a Plated for a job that is not open is ignored") {
    Kitchen k;
    k.boot();
    k.rogue_does(marathon::testing::ForgePlated{"999", "steak", "grill"});
    k.pump(10);
    CHECK_MESSAGE(k.book().heard.empty(), transcript(k.book()));
}

TEST_CASE("STILL MEASURED, STILL NOT WAVED AT: a forged Plated finishes someone else's dish") {
    Kitchen k;
    k.boot();
    k.order("v1", "brisket", "grill", kitchen::kFallbackNone); // fourteen passes: a long window
    k.pump(10);
    REQUIRE_MESSAGE(heard(k.book(), {"receipt v1", "@grill"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served v1"}));

    // The rogue holds nothing but an ordinary grant for an ordinary shape. The
    // job number is not a secret — it is on the wire, and it is sequential.
    k.rogue_does(marathon::testing::ForgePlated{"1", "brisket", "grill"});
    k.pump(6);

    // THIS IS THE SEAM, ASSERTED AS IT ACTUALLY BEHAVES — and Night One's finding
    // B REPRODUCES UNCHANGED on the current substrate. The expediter has no way
    // to ask Loom whether a sender holds the station role it claims, so the
    // second half of the standing consumer obligation is not performable and the
    // forgery lands. Loom attests ANSWERS and LIFECYCLE; it does not attest
    // role-holding, and this kitchen cannot invent what the substrate does not
    // offer.
    CHECK_MESSAGE(heard(k.book(), {"served v1", "brisket", "grill"}), transcript(k.book()));
}

TEST_CASE("a plate that names the wrong station is ignored") {
    Kitchen k;
    k.boot();
    k.order("v4", "brisket", "grill", kitchen::kFallbackNone);
    k.pump(10);
    REQUIRE_MESSAGE(heard(k.book(), {"receipt v4", "@grill"}), transcript(k.book()));

    // Everything about this forgery is right except the one thing the expediter
    // CAN check: the job went to the grill, and this claims to come from the
    // fryer. The sender is still unverifiable; the itinerary is not.
    k.rogue_does(marathon::testing::ForgePlated{"1", "brisket", "fryer"});
    k.pump(10);
    CHECK_FALSE_MESSAGE(heard(k.book(), {"served v4"}), transcript(k.book()));
}

TEST_CASE("a forged roster entry cannot make a station cook what it cannot cook") {
    Kitchen k;
    k.boot();
    // The rogue tells the kitchen the fryer does steak. The roster is assembled
    // from unauthenticated announcements, so the lie is believed.
    k.rogue_does(marathon::testing::ForgeStationOpen{"fryer", "steak"});
    k.pump(10);

    k.order("v2", "steak", "fryer", kitchen::kFallbackNone);
    k.pump(30);

    // ...and the failure is still LEGIBLE and IMMEDIATE, because the station
    // itself is the authority on its own menu and answers the Prep it will not
    // take. The lie costs a refusal, not a lost order and not a silence.
    CHECK_MESSAGE(heard(k.book(), {"lost v2", "declined the job", "does not cook 'steak'"}),
                  transcript(k.book()));
    CHECK(k.book().outstanding.empty());
}

TEST_CASE("a station named by nobody real: the order ends in a word, not a wait") {
    Kitchen k;
    k.boot();
    k.rogue_does(marathon::testing::ForgeStationOpen{"ghost", "steak"});
    k.pump(10);

    k.order("v3", "steak", "ghost", kitchen::kFallbackNone);
    k.pump(140);

    // The Prep is sent to an unheld role and refused by the bus — which the
    // expediter cannot see. The watchdog is what turns that invisible refusal
    // into an outcome the diner can act on.
    CHECK_MESSAGE(heard(k.book(), {"lost v3", "@ghost", "never plated it"}), transcript(k.book()));
    CHECK(k.book().outstanding.empty());
}

// ---- 9. REPLAY: the prepared-replacement ceremony ---------------------------
//
// Everything below this line is new. The kitchen's own logic is unchanged; what
// changed is that there is now a second way to put a different weave in a live
// role, and it has the opposite properties from the first one.

namespace {

/// Start a prepared replacement of the grill through the handle, with `ask` as
/// the preparation payload. Returns once the candidate has answered and the
/// owner has offered — the caller decides whether to commit.
///
/// NOTE WHAT IS NOT HERE: no transaction id is carried anywhere, no lifecycle
/// authority is minted by hand, no candidate is unloaded on a failed begin, and
/// no readiness is asserted. All of that is the handle's, and this is the whole
/// of what an application writes.
loom::PreparedReplacement::StartResult begin_grill_upgrade(Kitchen& k,
                                                           const std::string& candidate_stem,
                                                           std::uint32_t budget = 8) {
    return k.new_upgrade().start({
        .operator_id = k.owner(),
        .coordinator = k.owner(),
        .role = kitchen::station_role("grill"),
        .candidate_name = candidate_stem,
        .candidate_path = marathon::testing::weave_path(candidate_stem),
        .budget = budget,
    });
}

} // namespace

TEST_CASE("THE REQUIRED CASE: a station is replaced through PreparedReplacement, activation "
          "first, and the new station serves") {
    Kitchen k;
    k.boot();
    const loom::WeaveId incumbent = k.bus().role_holder(kitchen::station_role("grill"));
    REQUIRE(incumbent.valid());

    // 1. Start. The incumbent is resolved FROM THE ROLE by the handle and never
    //    supplied; the candidate is loaded SEALED.
    const auto started = begin_grill_upgrade(k, "kitchen-grill-2");
    REQUIRE_MESSAGE(started.ok, "stage=", static_cast<int>(started.stage), " ", started.error);
    CHECK(k.upgrade().incumbent() == incumbent);
    CHECK(k.bus().sealed(k.upgrade().candidate()));
    CHECK(k.upgrade().state() == loom::TxnState::Preparing);

    // The tap goes on BEFORE anything can reach the candidate, because "the
    // activation was its first live delivery" is a claim about delivery ORDER and
    // only a tap can see delivery order.
    //
    // ⚠ AND THE PHRASE HAS A TRAP IN IT, which this case found by getting it
    // wrong first. "Activation first" does NOT mean the activation is the
    // candidate's first delivery — the whole preparation conversation is
    // delivered to it before that, and must be. It means the activation is its
    // first delivery AS PART OF THE WORLD. The mark below is what makes that
    // distinction testable, and an application author reading only the phrase
    // would write the assertion this case originally had.
    std::vector<std::string>* seen = k.watch(k.upgrade().candidate());

    // 2. The incumbent is undisturbed and still serving throughout.
    k.order("u1", "steak", "grill", kitchen::kFallbackNone);
    k.pump(30);
    CHECK_MESSAGE(heard(k.book(), {"served u1", "grill"}), transcript(k.book()));

    // 3. Ask. The payload is domain vocabulary and carries NO transaction id.
    REQUIRE(k.upgrade().ask(kitchen::PrepareStation{"grill", {}, false}).ok);
    k.pump(6);

    // 4. The candidate answered for itself, the owner offered, the bus judged.
    REQUIRE_MESSAGE(k.desk().offers.size() == 1, desk_notes(k.desk()));
    CHECK(k.desk().offers[0].ok);
    CHECK(k.upgrade().state() == loom::TxnState::Ready);

    // The mark: everything before this point was said to a sealed candidate
    // outside the world. Everything after it is the world talking.
    const std::size_t outside_the_world = seen->size();
    REQUIRE(outside_the_world >= 1); // it really was spoken to while sealed

    // 5. Commit means SCHEDULED, not done. The incumbent is still the service.
    REQUIRE(k.upgrade().commit(41).ok);
    CHECK(k.upgrade().state() == loom::TxnState::AdmissionPending);
    CHECK(k.bus().role_holder(kitchen::station_role("grill")) == incumbent);
    CHECK_FALSE(k.upgrade().take_outcome().has_value());
    CHECK(seen->size() == outside_the_world); // scheduling delivers nothing

    // 6. Pump: one dispatch does the whole thing.
    k.pump(10);
    CHECK(k.bus().role_holder(kitchen::station_role("grill")) == k.upgrade().candidate());

    // ACTIVATION FIRST, measured: the candidate's first delivery as part of the
    // world is its own zen.Activated, ahead of any production traffic.
    REQUIRE_MESSAGE(seen->size() > outside_the_world,
                    "the candidate was never delivered anything after the commit");
    CHECK_MESSAGE((*seen)[outside_the_world] == std::string(loom::Activated::zen_name),
                  (*seen)[outside_the_world]);

    const std::optional<loom::TxnOutcome> outcome = k.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Committed);
    CHECK(outcome->id == k.upgrade().id());
    // Exactly one outcome exists, and it is consumed once.
    CHECK_FALSE(k.upgrade().take_outcome().has_value());

    // 7. The new station serves new orders, under the same role, and the diner
    //    never learned any of this happened.
    k.order("u2", "steak", "grill", kitchen::kFallbackNone);
    k.pump(40);
    CHECK_MESSAGE(heard(k.book(), {"served u2", "steak", "grill"}), transcript(k.book()));
}

TEST_CASE("the incumbent is NEVER TOLD, so its work is gone — and the promise still ends in a "
          "word") {
    Kitchen k;
    k.boot();
    // A fourteen-pass dish, held by the incumbent at the moment of admission.
    k.order("n1", "brisket", "grill", kitchen::kFallbackNone);
    k.pump(10);
    REQUIRE_MESSAGE(heard(k.book(), {"receipt n1", "@grill"}), transcript(k.book()));

    REQUIRE(begin_grill_upgrade(k, "kitchen-grill-2").ok);
    REQUIRE(k.upgrade().ask(kitchen::PrepareStation{"grill", {}, false}).ok);
    k.pump(6);
    REQUIRE(k.upgrade().state() == loom::TxnState::Ready);
    REQUIRE(k.upgrade().commit(7).ok);
    k.pump(160);

    // THE HONEST CONTRACT. A prepared replacement asks the INCOMING holder
    // everything and the OUTGOING holder nothing: there is no PrepareShutdown, no
    // letter, and no moment at which the incumbent could have spoken. Its dish is
    // simply gone — exactly the HARD-swap outcome — and the only thing standing
    // between the diner and silence is the kitchen's own watchdog.
    CHECK_MESSAGE(heard(k.book(), {"lost n1", "@grill", "never plated it"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served n1"}));
    CHECK(k.book().outstanding.empty());
}

TEST_CASE("THE COMPOSITION: the preparation window is where continuity can be arranged, and the "
          "dish crosses") {
    Kitchen k;
    k.boot();
    k.order("c9", "brisket", "grill", kitchen::kFallbackNone);
    k.pump(10);
    REQUIRE_MESSAGE(heard(k.book(), {"receipt c9", "@grill"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served c9"}));

    // THE ONE INTERVAL IN WHICH BOTH EXIST. The incumbent is alive and serving;
    // the candidate is reachable through the preparation conversation. So the
    // owner asks the incumbent to DESCRIBE its work — an ordinary question that
    // changes nothing — and hands the description to the candidate in the ask.
    k.describe_work(kitchen::station_role("grill"));
    k.pump(4);
    const kitchen::WorkDescribed* work = k.desk().work_of("grill");
    REQUIRE_MESSAGE(work != nullptr, desk_notes(k.desk()));
    REQUIRE_MESSAGE(work->tickets.size() == 1, desk_notes(k.desk()));

    REQUIRE(begin_grill_upgrade(k, "kitchen-grill-2").ok);
    REQUIRE(k.upgrade().ask(kitchen::PrepareStation{"grill", work->tickets, false}).ok);
    k.pump(6);
    REQUIRE_MESSAGE(k.upgrade().state() == loom::TxnState::Ready, desk_notes(k.desk()));
    REQUIRE(k.upgrade().commit(11).ok);
    k.pump(160);

    // The dish is finished by a weave that never received the Prep, and the diner
    // is served exactly once. Nothing in Loom did this: the substrate offers
    // verification OR continuity, and the composition is the application's.
    CHECK_MESSAGE(heard(k.book(), {"served c9", "brisket", "grill"}), transcript(k.book()));
    CHECK_MESSAGE(heard_count(k.book(), {"served c9"}) == 1, transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"lost c9"}));
}

TEST_CASE("DEFERRED READINESS: a sealed candidate asks its coordinator a question and answers "
          "afterwards") {
    Kitchen k;
    k.boot();

    REQUIRE(begin_grill_upgrade(k, "kitchen-grill-2").ok);
    // `consult` makes the candidate take its readiness answer away and ask the
    // one party a sealed weave may speak to.
    REQUIRE(k.upgrade().ask(kitchen::PrepareStation{"grill", {}, true}).ok);
    k.pump(2);

    // Mid-conversation: the ask has been answered by nobody, and the transaction
    // has NOT become ready by anyone's assertion.
    CHECK(k.upgrade().state() == loom::TxnState::Preparing);
    CHECK(k.desk().offers.empty());

    k.pump(8);
    CHECK_MESSAGE(k.upgrade().state() == loom::TxnState::Ready, desk_notes(k.desk()));
    REQUIRE_MESSAGE(k.desk().offers.size() == 1, desk_notes(k.desk()));
    CHECK(k.desk().offers[0].ok);

    REQUIRE(k.upgrade().commit(12).ok);
    k.pump(10);
    const std::optional<loom::TxnOutcome> outcome = k.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Committed);
}

TEST_CASE("AUTHENTIC REFUSAL: the fryer will not become the grill, and the incumbent simply "
          "continues") {
    Kitchen k;
    k.boot();
    const loom::WeaveId incumbent = k.bus().role_holder(kitchen::station_role("grill"));

    // The candidate is a real artifact that loads, seals and is asked — and then
    // says no, for a domain reason of its own.
    REQUIRE(begin_grill_upgrade(k, "kitchen-fryer-candidate").ok);
    REQUIRE(k.upgrade().ask(kitchen::PrepareStation{"grill", {}, false}).ok);
    k.pump(6);

    REQUIRE_MESSAGE(k.desk().offers.size() == 1, desk_notes(k.desk()));
    CHECK(k.desk().offers[0].ok); // the OFFER succeeded; the ANSWER was "no"
    CHECK(k.upgrade().state() == loom::TxnState::Aborted);

    const std::optional<loom::TxnOutcome> outcome = k.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Aborted);
    // The successor's OWN judgement, not a mechanism failure. Every other reason
    // would send an operator to the topology; this one sends them to the
    // candidate.
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);

    // The incumbent never learned any of this happened, and is still the service.
    CHECK(k.bus().role_holder(kitchen::station_role("grill")) == incumbent);
    k.order("nr1", "steak", "grill", kitchen::kFallbackNone);
    k.pump(40);
    CHECK_MESSAGE(heard(k.book(), {"served nr1", "grill"}), transcript(k.book()));
}

TEST_CASE("a candidate that cannot cook the carried work refuses rather than accepting a promise "
          "it must break") {
    Kitchen k;
    k.boot();

    REQUIRE(begin_grill_upgrade(k, "kitchen-grill-2").ok);
    // "wings" is on the fryer's menu and not the grill's.
    REQUIRE(k.upgrade()
                .ask(kitchen::PrepareStation{
                    "grill", {kitchen::StationTicket{"77", "wings", 3}}, false})
                .ok);
    k.pump(6);

    CHECK(k.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = k.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);
    CHECK_MESSAGE(k.desk().notes.size() >= 1, desk_notes(k.desk()));
}

TEST_CASE("EXACT ERROR INSPECTION: every refusal keeps the substrate's own words") {
    Kitchen k;
    k.boot();

    SUBCASE("nobody holds the role: checked BEFORE anything loads") {
        const auto r = k.new_upgrade().start({
            .operator_id = k.owner(),
            .coordinator = k.owner(),
            .role = "kitchen.station.nobody",
            .candidate_name = "kitchen-grill-2",
            .candidate_path = marathon::testing::weave_path("kitchen-grill-2"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::NoRoleHolder);
        CHECK_FALSE(r.cleanup_failed);
        CHECK_FALSE(k.upgrade().started());
    }

    SUBCASE("the artifact refuses to load, and the loader's own words survive") {
        const auto r = k.new_upgrade().start({
            .operator_id = k.owner(),
            .coordinator = k.owner(),
            .role = kitchen::station_role("grill"),
            .candidate_name = "kitchen-nowhere",
            .candidate_path = marathon::testing::weave_path("kitchen-nowhere"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::CandidateLoad);
        CHECK_MESSAGE(r.error.find("kitchen-nowhere") != std::string::npos, r.error);
    }

    SUBCASE("a second replacement of the same incumbent is IncumbentBusy") {
        REQUIRE(begin_grill_upgrade(k, "kitchen-grill-2").ok);
        loom::PreparedReplacement second(k.bus(), k.kernel());
        const auto r = second.start({
            .operator_id = k.owner(),
            .coordinator = k.owner(),
            .role = kitchen::station_role("grill"),
            .candidate_name = "kitchen-grill-3",
            .candidate_path = marathon::testing::weave_path("kitchen-grill-3"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::BeginTransaction);
        CHECK(r.begin_reason == loom::TxnReason::IncumbentBusy);
        // ATOMIC ON FAILURE: the candidate this call loaded was removed again, so
        // the name is free and nothing leaked.
        CHECK_FALSE(r.cleanup_failed);
    }

    SUBCASE("starting a handle twice is refused without touching anything") {
        REQUIRE(begin_grill_upgrade(k, "kitchen-grill-2").ok);
        const loom::TxnId first = k.upgrade().id();
        const auto r = k.upgrade().start({
            .operator_id = k.owner(),
            .coordinator = k.owner(),
            .role = kitchen::station_role("grill"),
            .candidate_name = "kitchen-grill-3",
            .candidate_path = marathon::testing::weave_path("kitchen-grill-3"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::AlreadyStarted);
        CHECK(k.upgrade().id() == first);
    }
}

TEST_CASE("ABORTED OUTCOME: the owner changes its mind, and the incumbent never noticed") {
    Kitchen k;
    k.boot();
    const loom::WeaveId incumbent = k.bus().role_holder(kitchen::station_role("grill"));

    REQUIRE(begin_grill_upgrade(k, "kitchen-grill-2").ok);
    REQUIRE(k.upgrade().ask(kitchen::PrepareStation{"grill", {}, false}).ok);
    k.pump(6);
    REQUIRE(k.upgrade().state() == loom::TxnState::Ready);

    // Ready is not committed. Nothing happens because a candidate is willing.
    REQUIRE(k.upgrade().abort().ok);
    CHECK(k.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = k.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::ExplicitAbort);

    CHECK(k.bus().role_holder(kitchen::station_role("grill")) == incumbent);
    k.order("ab1", "steak", "grill", kitchen::kFallbackNone);
    k.pump(40);
    CHECK_MESSAGE(heard(k.book(), {"served ab1", "grill"}), transcript(k.book()));
}

TEST_CASE("A FORGED READINESS CANNOT MAKE A TRANSACTION READY") {
    Kitchen k;
    k.boot();
    REQUIRE(begin_grill_upgrade(k, "kitchen-grill-2").ok);

    // The forgery is a perfectly-shaped StationReady from a weave holding an
    // ordinary grant for the shape, aimed straight at the owner. It has
    // everything except Loom's word that it answers this transaction's own ask —
    // which no weave can manufacture.
    //
    // The owner DOES offer it. The owner is not the wall and does not pretend to
    // be: it offers the delivery it is holding and the Switchboard judges. That
    // division is the whole design, and a coordinator that tried to pre-screen
    // forgeries would be a second, weaker copy of the real check.

    SUBCASE("before any conversation is open — nothing can move the transaction") {
        k.rogue_does(marathon::testing::ForgeStationReady{"grill"});
        k.pump(6);

        REQUIRE_MESSAGE(k.desk().offers.size() == 1, desk_notes(k.desk()));
        CHECK_FALSE(k.desk().offers[0].ok);
        // ONE reason for every way a forgery can be wrong, deliberately: telling
        // a forger which term it failed is telling it what to fix next.
        CHECK(k.desk().offers[0].why == loom::TxnReason::InvalidReadiness);
        // Deterministic, because nothing else in this world can move the state:
        // the transaction's one conversation was never opened.
        CHECK(k.upgrade().state() == loom::TxnState::Preparing);
    }

    SUBCASE("while the real conversation is in flight — the forgery loses, the truth wins") {
        REQUIRE(k.upgrade().ask(kitchen::PrepareStation{"grill", {}, true}).ok);
        k.pump(1); // the candidate has the ask and is consulting the owner
        k.rogue_does(marathon::testing::ForgeStationReady{"grill"});
        k.pump(10);

        // The forged offer is queued ahead of the honest conversation's remaining
        // hops, so it is genuinely first, and it is refused.
        REQUIRE_MESSAGE(k.desk().offers.size() == 2, desk_notes(k.desk()));
        CHECK_FALSE(k.desk().offers[0].ok);
        CHECK(k.desk().offers[0].why == loom::TxnReason::InvalidReadiness);
        // ...and the candidate's own answer still lands. Hostile traffic does not
        // get to end a legitimate transaction.
        CHECK(k.desk().offers[1].ok);
        CHECK_MESSAGE(k.upgrade().state() == loom::TxnState::Ready, desk_notes(k.desk()));
    }
}

TEST_CASE("an offer with no transaction in flight is a nothing, not a crash") {
    Kitchen k;
    k.boot();
    k.forget_upgrade();
    k.rogue_does(marathon::testing::ForgeStationReady{"grill"});
    k.pump(4);
    CHECK(k.desk().offered_without_handle == 1);
    CHECK(k.desk().offers.empty());
}

TEST_CASE("DROPPING THE HANDLE CHANGES NOTHING: the transaction is the Switchboard's") {
    Kitchen k;
    k.boot();
    const loom::WeaveId incumbent = k.bus().role_holder(kitchen::station_role("grill"));
    loom::TxnId id{};
    loom::WeaveId candidate{};
    {
        loom::PreparedReplacement scoped(k.bus(), k.kernel());
        REQUIRE(scoped
                    .start({
                        .operator_id = k.owner(),
                        .coordinator = k.owner(),
                        .role = kitchen::station_role("grill"),
                        .candidate_name = "kitchen-grill-2",
                        .candidate_path = marathon::testing::weave_path("kitchen-grill-2"),
                        .budget = 8,
                    })
                    .ok);
        id = scoped.id();
        candidate = scoped.candidate();
    } // <- scope ends. No abort, no unload, no pump.

    CHECK(k.bus().transaction_state(id) == loom::TxnState::Preparing);
    CHECK(k.bus().sealed(candidate));
    CHECK(k.bus().role_holder(kitchen::station_role("grill")) == incumbent);
}

// ---- 10. REPLAY: what the rest of the world makes of it afterwards ----------

TEST_CASE("after a prepared replacement the steward's account of the world is still true") {
    Kitchen k;
    k.boot();

    REQUIRE(begin_grill_upgrade(k, "kitchen-grill-2").ok);
    REQUIRE(k.upgrade().ask(kitchen::PrepareStation{"grill", {}, false}).ok);
    k.pump(6);
    REQUIRE(k.upgrade().commit(21).ok);
    k.pump(10);
    REQUIRE(k.bus().role_holder(kitchen::station_role("grill")) == k.upgrade().candidate());

    // THE COMPOSITION QUESTION. The Weave Manager loaded `kitchen-grill` and bound
    // it to the role. A prepared replacement then moved that role with NO steward
    // call at all. A steward that had cached "who holds what" at load time would
    // now be lying; this asks it out loud.
    k.list_loaded();
    k.pump(10);
    const std::string listing = k.oplog().answer_for("list");
    REQUIRE_MESSAGE(!listing.empty(), "the steward never answered ListLoaded");
    // The successor is a real loaded artifact and the listing knows it.
    CHECK_MESSAGE(listing.find("kitchen-grill-2") != std::string::npos, listing);
}
