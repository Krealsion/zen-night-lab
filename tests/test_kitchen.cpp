// The job kitchen's suite.
//
// THE QUESTION UNDER TEST, restated so a reader of this file alone knows what is
// being proven: Loom ends a conversation when a participant dies, and tells the
// other party nothing. Can a kitchen therefore keep an honest promise for EVERY
// order it accepts — including orders whose cook disappears mid-dish — using
// only existing public Loom and Zengine behaviour?
//
// Every case below runs real .so weaves through the real kernel, the real Weave
// Manager and real graceful swaps. The one substitution is the Timer's CLOCK,
// and it is labelled in harness.hpp.
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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "third_party/doctest.h"

#include "harness.hpp"

#include <string>
#include <vector>

using nightlab::testing::Kitchen;
namespace kitchen = nightlab::kitchen;

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
    // Nothing was started: no dish, no loss, just the refusal.
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
    // The routing watchdog: kOrderPatienceSweeps sweeps at kSweepMs, and a beat
    // is kBeatCapMs of virtual time, so the bound is a countable number of beats.
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

    // A slow dish: the grill is bad at fries (six passes), so there is a real
    // window in which the job is held by a weave that is about to stop existing.
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
    k.pump(12); // several passes of a six-pass dish are already done
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
    // was said, so the work in the predecessor's hands is simply gone. The
    // distinction between this case and the one above is the entire value of the
    // letter, and it is visible from the diner's chair.
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
    // the ceremony. This is the exact scenario a bus trace caught during this
    // experiment — the `Plated` was delivered to the heir TWO TURNS BEFORE its own
    // `zen.Activated`, the heir had no such job, and the inherited ticket then
    // timed out. The handover window is what makes it survivable.
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
    // The honest path produced exactly one attested receipt and no ignores.
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
    k.rogue_does(nightlab::testing::ForgeRouteChoice{"q1", "fryer", 1});
    k.pump(10);

    // Nothing happened. `answers_ask()` is Loom's word that a delivery is the
    // authorized answer to a request THIS weave sent, and no weave can produce
    // it for a conversation it was not part of — a correct shape and a correct
    // correlation are not, and were never, enough.
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
    k.rogue_does(nightlab::testing::ForgePlated{"999", "steak", "grill"});
    k.pump(10);
    // Nothing reached the diner at all: there is no ticket to reach them about.
    CHECK_MESSAGE(k.book().heard.empty(), transcript(k.book()));
}

TEST_CASE("MEASURED, NOT WAVED AT: a forged Plated finishes someone else's dish") {
    Kitchen k;
    k.boot();
    k.order("v1", "brisket", "grill", kitchen::kFallbackNone); // fourteen passes: a long window
    k.pump(10);
    REQUIRE_MESSAGE(heard(k.book(), {"receipt v1", "@grill"}), transcript(k.book()));
    CHECK_FALSE(heard(k.book(), {"served v1"}));

    // The rogue holds nothing but an ordinary grant for an ordinary shape. The
    // job number is not a secret — it is on the wire, and it is sequential.
    k.rogue_does(nightlab::testing::ForgePlated{"1", "brisket", "grill"});
    k.pump(6);

    // THIS IS THE SEAM, ASSERTED AS IT ACTUALLY BEHAVES. The expediter has no way
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
    k.rogue_does(nightlab::testing::ForgePlated{"1", "brisket", "fryer"});
    k.pump(10);
    CHECK_FALSE_MESSAGE(heard(k.book(), {"served v4"}), transcript(k.book()));
}

TEST_CASE("a forged roster entry cannot make a station cook what it cannot cook") {
    Kitchen k;
    k.boot();
    // The rogue tells the kitchen the fryer does steak. The roster is assembled
    // from unauthenticated announcements, so the lie is believed.
    k.rogue_does(nightlab::testing::ForgeStationOpen{"fryer", "steak"});
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
    k.rogue_does(nightlab::testing::ForgeStationOpen{"ghost", "steak"});
    k.pump(10);

    k.order("v3", "steak", "ghost", kitchen::kFallbackNone);
    k.pump(140);

    // The Prep is sent to an unheld role and refused by the bus — which the
    // expediter cannot see. The watchdog is what turns that invisible refusal
    // into an outcome the diner can act on.
    CHECK_MESSAGE(heard(k.book(), {"lost v3", "@ghost", "never plated it"}), transcript(k.book()));
    CHECK(k.book().outstanding.empty());
}
