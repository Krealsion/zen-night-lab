// The build farm's suite.
//
// THE QUESTION UNDER TEST is not "does long-running responsibility work" — the
// download manager settled that. It is:
//
//     Two independent implementations of the same conversation shape. Did they
//     grow the same non-domain bookkeeping, or did the domain change the answer?
//
// So the cases below are chosen to make the COMPARISON sharp, and three of them
// are here specifically because the download manager could not have written
// them:
//
//   * a QUEUE — work that waits, and a promise made from behind other work;
//   * RESUMPTION — the opposite continuity contract, because a build is
//     re-derivable from its intent and a half-downloaded file is not;
//   * RECONCILIATION — an answer to absence that needs no clock, because a
//     replacement produces an announcement where a disappearance produces
//     nothing.
//
// The one substitution is the Timer's CLOCK, labelled in harness.hpp.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "harness.hpp"

#include <string>
#include <vector>

using marathon::farm_testing::Farm;
namespace farm = marathon::farm;

namespace {

bool heard(const farm::RequesterBook& b, const std::vector<std::string>& fragments) {
    for (const std::string& line : b.heard) {
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

std::size_t heard_count(const farm::RequesterBook& b, const std::vector<std::string>& fragments) {
    std::size_t n = 0;
    for (const std::string& line : b.heard) {
        bool all = true;
        for (const std::string& f : fragments) {
            all = all && line.find(f) != std::string::npos;
        }
        n += all ? 1u : 0u;
    }
    return n;
}

std::string transcript(const farm::RequesterBook& b) {
    std::string out = "\n  requester heard:";
    for (const std::string& line : b.heard) {
        out += "\n    " + line;
    }
    if (b.heard.empty()) {
        out += " (nothing)";
    }
    return out;
}

std::string desk_notes(const marathon::farm_testing::LeadDesk& d) {
    std::string out = "\n  shift lead saw:";
    for (const std::string& n : d.notes) {
        out += "\n    " + n;
    }
    if (d.notes.empty()) {
        out += " (nothing)";
    }
    return out;
}

loom::PreparedReplacement::StartResult begin_worker_upgrade(Farm& f, const std::string& worker,
                                                            const std::string& stem,
                                                            std::uint32_t budget = 8) {
    return f.new_upgrade().start({
        .operator_id = f.lead(),
        .coordinator = f.lead(),
        .role = farm::worker_role(worker),
        .candidate_name = stem,
        .candidate_path = marathon::farm_testing::weave_path(stem),
        .budget = budget,
    });
}

} // namespace

// ---- 1. the farm works at all ----------------------------------------------

TEST_CASE("the farm opens: every load is answered and nothing is refused") {
    Farm f;
    f.boot();
    CHECK(f.oplog().pending.empty());
    REQUIRE(f.oplog().answers.size() == 4);
    for (const std::string& a : f.oplog().answers) {
        CHECK(a.find("refused") == std::string::npos);
    }
}

TEST_CASE("the farm answers a diagnostic about itself, authentically") {
    Farm f;
    f.boot();
    f.submit("b1", "debug");
    f.pump(30);
    f.ask_status();
    f.pump(6);

    REQUIRE(f.status().size() == 1);
    const std::string& tally = f.status()[0];
    CHECK_MESSAGE(tally.find("accepted=1") != std::string::npos, tally);
    CHECK_MESSAGE(tally.find("succeeded=1") != std::string::npos, tally);
    // The roster is part of the diagnostic: presence is announced, never
    // discovered, so "who do you believe is here" is worth asking.
    CHECK_MESSAGE(tally.find("workers=a") != std::string::npos, tally);
}

// ---- 2. the positive vertical ----------------------------------------------

TEST_CASE("a build is accepted, moves through every stage, and succeeds") {
    Farm f;
    f.boot();
    f.submit("b1", "debug"); // five stages, one step each
    f.pump(30);

    CHECK_MESSAGE(heard(f.book(), {"accepted b1", "queued behind 0"}), transcript(f.book()));
    for (std::size_t i = 0; i < farm::kStageCount; ++i) {
        CHECK_MESSAGE(heard(f.book(), {"progress b1", farm::kStages[i]}), transcript(f.book()));
    }
    CHECK_MESSAGE(heard(f.book(), {"succeeded b1", "zen-r1-debug.out", "after 1 attempt"}),
                  transcript(f.book()));
    CHECK(f.book().outstanding.empty());
    // Within one attempt, progress is monotone. Between attempts it need not be —
    // see the resumption cases.
    CHECK_FALSE(f.book().backwards_within_an_attempt);
}

TEST_CASE("THE QUEUE: a second build waits for a free worker and is told so at accept time") {
    Farm f;
    f.boot(/*workers=*/1); // one worker, so the queue is real
    f.submit("q1", "release");
    f.pump(2);
    f.submit("q2", "debug");
    f.pump(60);

    // The farm told the truth about the queue rather than pretending both had
    // started. "Accepted" and "started" are different facts.
    CHECK_MESSAGE(heard(f.book(), {"accepted q1", "queued behind 0"}), transcript(f.book()));
    CHECK_MESSAGE(heard(f.book(), {"accepted q2", "queued behind 1"}), transcript(f.book()));
    CHECK_MESSAGE(heard(f.book(), {"succeeded q1"}), transcript(f.book()));
    CHECK_MESSAGE(heard(f.book(), {"succeeded q2"}), transcript(f.book()));
    CHECK(f.book().outstanding.empty());
}

TEST_CASE("two workers take two builds at once, and each build knows which one has it") {
    Farm f;
    f.boot();
    f.submit("p1", "release");
    f.submit("p2", "release");
    f.pump(60);

    CHECK_MESSAGE(heard(f.book(), {"succeeded p1"}), transcript(f.book()));
    CHECK_MESSAGE(heard(f.book(), {"succeeded p2"}), transcript(f.book()));
    // Different workers: the farm ran them in parallel rather than serialising.
    const bool p1_placed = heard(f.book(), {"progress p1", "on a"}) ||
                           heard(f.book(), {"progress p1", "on b"});
    CHECK_MESSAGE(p1_placed, transcript(f.book()));
    const bool both_workers = heard(f.book(), {"on a"}) && heard(f.book(), {"on b"});
    CHECK_MESSAGE(both_workers, transcript(f.book()));
}

TEST_CASE("two requesters naming a build the same way do not collide") {
    Farm f;
    f.boot();
    f.submit("same", "debug");
    f.submit_second("same", "debug");
    f.pump(60);

    CHECK_MESSAGE(heard(f.book(), {"succeeded same", "zen-r1-debug.out"}), transcript(f.book()));
    CHECK_MESSAGE(heard(f.second_book(), {"succeeded same", "other-r9-debug.out"}),
                  transcript(f.second_book()));
    CHECK(f.book().ignored == 0);
    CHECK(f.second_book().ignored == 0);
}

// ---- 3. endings that are not success ---------------------------------------

TEST_CASE("a target that dies in a named stage fails with that stage named") {
    Farm f;
    f.boot();
    f.submit("x1", "broken-link");
    f.pump(40);

    CHECK_MESSAGE(heard(f.book(), {"failed x1", "in link", "does not survive"}),
                  transcript(f.book()));
    CHECK_FALSE(heard(f.book(), {"succeeded x1"}));
    // ...and it is NOT retried. A target that cannot be built will not build on
    // the next attempt either.
    CHECK_MESSAGE(heard_count(f.book(), {"progress x1", "fetch"}) == 1, transcript(f.book()));
}

TEST_CASE("a target no worker has a recipe for is DECLINED, and a decline is not an absence") {
    Farm f;
    f.boot();
    f.submit("u1", "no-such-target");
    f.pump(40);

    CHECK_MESSAGE(heard(f.book(), {"failed u1", "worker declined", "no recipe for target"}),
                  transcript(f.book()));
    // THE DISTINCTION THIS CASE EXISTS FOR: a decline is a JUDGEMENT, and the
    // farm fails the build once instead of retrying it three times. Absence is
    // retried; refusal is not.
    CHECK_MESSAGE(heard_count(f.book(), {"failed u1"}) == 1, transcript(f.book()));
    CHECK_FALSE(heard(f.book(), {"gave up after"}));
}

TEST_CASE("a duplicate build id from the same requester is rejected") {
    Farm f;
    f.boot();
    f.submit("dup", "release");
    f.pump(2);
    f.submit("dup", "release");
    f.pump(10);
    CHECK_MESSAGE(heard(f.book(), {"rejected dup", "already have a build named"}),
                  transcript(f.book()));
}

TEST_CASE("a build that does not name all three of project, revision and target is rejected") {
    Farm f;
    f.boot();
    f.submit("bad", "debug", /*project=*/"", /*revision=*/"r1");
    f.pump(10);
    CHECK_MESSAGE(heard(f.book(), {"rejected bad", "needs a project, a revision and a target"}),
                  transcript(f.book()));
}

TEST_CASE("the farm's book is bounded, and a full book refuses visibly") {
    Farm f;
    f.boot(/*workers=*/1);
    for (std::size_t i = 0; i <= farm::kMaxOpenBuilds; ++i) {
        f.submit("n" + std::to_string(i), "release");
    }
    f.pump(8);
    CHECK_MESSAGE(heard_count(f.book(), {"accepted n"}) == farm::kMaxOpenBuilds,
                  "accepted: ", heard_count(f.book(), {"accepted n"}));
    CHECK_MESSAGE(heard(f.book(), {"rejected n", "maximum of 16 builds"}), transcript(f.book()));
}

TEST_CASE("a requester that withdraws a QUEUED build is acknowledged and still gets an ending") {
    Farm f;
    f.boot(/*workers=*/1);
    f.submit("w1", "release");
    const std::uint64_t corr = f.submit("w2", "release");
    f.pump(2);
    f.withdraw("w2", corr);
    f.pump(10);

    CHECK_MESSAGE(heard(f.book(), {"withdrawal acknowledged"}), transcript(f.book()));
    CHECK_MESSAGE(heard(f.book(), {"failed w2", "withdrawn by the requester"}),
                  transcript(f.book()));
    CHECK_FALSE(heard(f.book(), {"progress w2"}));
}

TEST_CASE("a requester that withdraws a RUNNING build has it stopped, and the worker freed") {
    Farm f;
    f.boot(/*workers=*/1);
    const std::uint64_t corr = f.submit("w1", "release");
    f.pump(10);
    REQUIRE_MESSAGE(heard(f.book(), {"progress w1"}), transcript(f.book()));

    f.withdraw("w1", corr);
    f.pump(6);
    CHECK_MESSAGE(heard(f.book(), {"failed w1", "withdrawn by the requester"}),
                  transcript(f.book()));

    // The worker is genuinely free: a new build starts at once rather than
    // waiting for the abandoned one to finish.
    const std::size_t before = heard_count(f.book(), {"progress w1"});
    f.submit("w3", "debug");
    f.pump(30);
    CHECK_MESSAGE(heard(f.book(), {"succeeded w3"}), transcript(f.book()));
    CHECK_MESSAGE(heard_count(f.book(), {"progress w1"}) == before, transcript(f.book()));
}

TEST_CASE("withdrawing a build nobody has is refused, in a shape a stranger can read") {
    Farm f;
    f.boot();
    f.withdraw("ghost", 999);
    f.pump(10);
    CHECK_MESSAGE(heard(f.book(), {"withdrawal refused", "no build named 'ghost'"}),
                  transcript(f.book()));
}

// ---- 4. replacement, and this project's opposite contract ------------------

TEST_CASE("THE REQUIRED CASE: a worker is replaced mid-build; the INTENT crosses and the build "
          "is resumed as a new attempt, while the farm stays available") {
    Farm f;
    f.boot();
    // `release` is ten beats of work, so there is a real window. `a` takes it
    // first because the roster is in announcement order.
    f.submit("r1", "release");
    f.pump(10);
    REQUIRE_MESSAGE(heard(f.book(), {"progress r1", "attempt 1", "on a"}), transcript(f.book()));

    // A second build on the OTHER worker, which must keep running throughout:
    // "the farm remains available" is a claim, and one worker cannot test it.
    f.submit("r2", "debug");
    f.pump(2);

    // 1. Ask the live incumbent what it is holding. INTENT ONLY — no stage, no
    //    step. A successor handed a stage cursor would be claiming to have
    //    compiled something it has never seen.
    f.describe_assignments("a");
    f.pump(3);
    REQUIRE_MESSAGE(f.desk().described_arrived, desk_notes(f.desk()));
    REQUIRE_MESSAGE(f.desk().described.size() == 1, desk_notes(f.desk()));
    CHECK(f.desk().described[0].target == "release");
    CHECK(f.desk().described[0].attempt == 1);

    // 2. Replace, through the handle and nothing else.
    REQUIRE(begin_worker_upgrade(f, "a", "farm-worker-a2").ok);
    std::vector<std::string>* seen = f.watch(f.upgrade().candidate());
    REQUIRE(f.upgrade().ask(farm::PrepareWorker{"a", f.desk().described, false}).ok);
    f.pump(4);
    REQUIRE_MESSAGE(f.desk().offers.size() == 1, desk_notes(f.desk()));
    CHECK(f.desk().offers[0].ok);
    REQUIRE(f.upgrade().state() == loom::TxnState::Ready);

    const std::size_t outside_the_world = seen->size();
    REQUIRE(f.upgrade().commit(31).ok);
    CHECK(f.upgrade().state() == loom::TxnState::AdmissionPending);
    f.pump(60);

    const std::optional<loom::TxnOutcome> outcome = f.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Committed);
    REQUIRE(seen->size() > outside_the_world);
    CHECK((*seen)[outside_the_world] == std::string(loom::Activated::zen_name));

    // 3. THE CONTRACT. The build was RESUMED, not failed and not continued: it
    //    starts again at `fetch` as attempt 2, and it finishes.
    CHECK_MESSAGE(heard(f.book(), {"progress r1", "attempt 2", "fetch"}), transcript(f.book()));
    CHECK_MESSAGE(heard(f.book(), {"succeeded r1", "after 2 attempt"}), transcript(f.book()));
    CHECK_FALSE(f.book().backwards_within_an_attempt);
    CHECK(f.book().highest_attempt["r1"] == 2);

    // 4. THE FARM STAYED AVAILABLE. The other worker's build ran to completion
    //    through the whole ceremony and never noticed it.
    CHECK_MESSAGE(heard(f.book(), {"succeeded r2"}), transcript(f.book()));
    CHECK(f.book().outstanding.empty());
}

TEST_CASE("RECONCILIATION: a worker that arrives holding nothing gets its work requeued at once, "
          "with no clock involved") {
    Farm f;
    f.boot(/*workers=*/1);
    f.submit("c1", "release");
    f.pump(10);
    REQUIRE_MESSAGE(heard(f.book(), {"progress c1", "on a"}), transcript(f.book()));

    // Replace `a` WITHOUT handing anything over. The successor announces itself
    // holding nothing, and that announcement is the evidence.
    REQUIRE(begin_worker_upgrade(f, "a", "farm-worker-a2").ok);
    REQUIRE(f.upgrade().ask(farm::PrepareWorker{"a", {}, false}).ok);
    f.pump(4);
    REQUIRE(f.upgrade().state() == loom::TxnState::Ready);
    REQUIRE(f.upgrade().commit(41).ok);
    f.pump(60);

    // The build was requeued and re-run — as attempt 2 — and the dispatcher knew
    // immediately rather than after `kAssignmentPatienceSweeps` sweeps.
    CHECK_MESSAGE(heard(f.book(), {"succeeded c1", "after 2 attempt"}), transcript(f.book()));
    f.ask_status();
    f.pump(6);
    REQUIRE(f.status().size() == 1);
    CHECK_MESSAGE(f.status()[0].find("requeued=1reconciled/0swept") != std::string::npos,
                  f.status()[0]);
}

TEST_CASE("THE SWEEP: a worker that vanishes with nothing taking its place needs a clock, and "
          "the farm has one") {
    Farm f;
    f.boot();
    f.submit("s1", "release");
    f.pump(10);
    REQUIRE_MESSAGE(heard(f.book(), {"progress s1", "on a"}), transcript(f.book()));

    // Nobody arrives. There is no announcement to reconcile against, so the only
    // evidence is silence — which is exactly the kitchen's situation, and exactly
    // why reconciliation cannot replace a watchdog.
    f.evict(farm::worker_role("a"));
    f.pump(300);

    // It lands on the OTHER worker: a requeued build prefers anyone but the place
    // it came back from.
    CHECK_MESSAGE(heard(f.book(), {"succeeded s1"}), transcript(f.book()));
    CHECK_MESSAGE(heard(f.book(), {"progress s1", "attempt 2", "on b"}), transcript(f.book()));
    CHECK(f.book().highest_attempt["s1"] >= 2);
    f.ask_status();
    f.pump(6);
    REQUIRE(f.status().size() == 1);
    const bool swept = f.status()[0].find("/1swept") != std::string::npos ||
                       f.status()[0].find("/2swept") != std::string::npos;
    CHECK_MESSAGE(swept, f.status()[0]);
}

TEST_CASE("attempts are BOUNDED: a build that keeps losing its worker ends in a word") {
    Farm f;
    f.boot(/*workers=*/1);
    f.submit("g1", "release");
    f.pump(10);
    REQUIRE_MESSAGE(heard(f.book(), {"progress g1"}), transcript(f.book()));

    // Take the only worker away and never give it back. The build is requeued
    // once, twice — and then the farm stops pretending.
    f.evict(farm::worker_role("a"));
    f.pump(400);

    CHECK_MESSAGE(heard(f.book(), {"failed g1", "gave up after 3 attempts"}),
                  transcript(f.book()));
    CHECK(f.book().outstanding.empty());
}

TEST_CASE("DEFERRED READINESS: the sealed candidate asks the shift lead before agreeing") {
    Farm f;
    f.boot();
    REQUIRE(begin_worker_upgrade(f, "a", "farm-worker-a2").ok);
    REQUIRE(f.upgrade().ask(farm::PrepareWorker{"a", {}, /*consult=*/true}).ok);
    f.pump(1);
    CHECK(f.upgrade().state() == loom::TxnState::Preparing);
    CHECK(f.desk().offers.empty());

    f.pump(8);
    CHECK_MESSAGE(f.upgrade().state() == loom::TxnState::Ready, desk_notes(f.desk()));
    REQUIRE(f.desk().offers.size() == 1);
    CHECK(f.desk().offers[0].ok);
}

TEST_CASE("AUTHENTIC REFUSAL: three different domain reasons a candidate says no") {
    SUBCASE("it is not the worker it was asked to be") {
        Farm f;
        // ONE worker: `farm-worker-b` must not already be loaded, or the start
        // fails on a NAME COLLISION and never reaches the candidate's own
        // judgement -- which is the thing under test.
        f.boot(/*workers=*/1);
        REQUIRE(begin_worker_upgrade(f, "a", "farm-worker-b").ok);
        REQUIRE(f.upgrade().ask(farm::PrepareWorker{"a", {}, false}).ok);
        f.pump(6);
        CHECK(f.upgrade().state() == loom::TxnState::Aborted);
        const auto outcome = f.upgrade().take_outcome();
        REQUIRE(outcome.has_value());
        CHECK(outcome->reason == loom::TxnReason::CandidateRefused);
    }

    SUBCASE("it is built for a toolchain the farm does not run") {
        Farm f;
        f.boot();
        REQUIRE(begin_worker_upgrade(f, "a", "farm-worker-a-clang").ok);
        REQUIRE(f.upgrade().ask(farm::PrepareWorker{"a", {}, /*consult=*/true}).ok);
        f.pump(10);
        CHECK(f.upgrade().state() == loom::TxnState::Aborted);
        const auto outcome = f.upgrade().take_outcome();
        REQUIRE(outcome.has_value());
        CHECK(outcome->reason == loom::TxnReason::CandidateRefused);
        CHECK_MESSAGE(f.desk().notes.size() >= 1, desk_notes(f.desk()));
    }

    SUBCASE("it is asked to resume a build that has already used up its attempts") {
        Farm f;
        f.boot();
        REQUIRE(begin_worker_upgrade(f, "a", "farm-worker-a2").ok);
        farm::AssignedJob exhausted{"9", "zen", "r1", "release", farm::kMaxAttempts};
        REQUIRE(f.upgrade().ask(farm::PrepareWorker{"a", {exhausted}, false}).ok);
        f.pump(6);
        CHECK(f.upgrade().state() == loom::TxnState::Aborted);
        const auto outcome = f.upgrade().take_outcome();
        REQUIRE(outcome.has_value());
        CHECK(outcome->reason == loom::TxnReason::CandidateRefused);
    }
}

TEST_CASE("the incumbent worker keeps building throughout a preparation, and never learns of it") {
    Farm f;
    f.boot(/*workers=*/1);
    REQUIRE(begin_worker_upgrade(f, "a", "farm-worker-a2").ok);
    f.submit("i1", "debug");
    f.pump(40);
    CHECK_MESSAGE(heard(f.book(), {"succeeded i1", "after 1 attempt"}), transcript(f.book()));
    CHECK(f.upgrade().state() == loom::TxnState::Preparing);
}

TEST_CASE("ABORTED OUTCOME: the shift lead changes its mind and the farm is as it was") {
    Farm f;
    f.boot();
    const loom::WeaveId incumbent = f.bus().role_holder(farm::worker_role("a"));
    REQUIRE(begin_worker_upgrade(f, "a", "farm-worker-a2").ok);
    REQUIRE(f.upgrade().ask(farm::PrepareWorker{"a", {}, false}).ok);
    f.pump(4);
    REQUIRE(f.upgrade().state() == loom::TxnState::Ready);

    REQUIRE(f.upgrade().abort().ok);
    const auto outcome = f.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::ExplicitAbort);
    CHECK(f.bus().role_holder(farm::worker_role("a")) == incumbent);

    f.submit("a1", "debug");
    f.pump(40);
    CHECK_MESSAGE(heard(f.book(), {"succeeded a1"}), transcript(f.book()));
}

TEST_CASE("EXACT ERROR INSPECTION: refusals keep the substrate's own words") {
    Farm f;
    f.boot();

    SUBCASE("nobody holds that worker slot") {
        const auto r = f.new_upgrade().start({
            .operator_id = f.lead(),
            .coordinator = f.lead(),
            .role = farm::worker_role("zzz"),
            .candidate_name = "farm-worker-a2",
            .candidate_path = marathon::farm_testing::weave_path("farm-worker-a2"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::NoRoleHolder);
    }

    SUBCASE("the artifact refuses to load, and the loader's own words survive") {
        const auto r = begin_worker_upgrade(f, "a", "farm-worker-imaginary");
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::CandidateLoad);
        CHECK_MESSAGE(r.error.find("farm-worker-imaginary") != std::string::npos, r.error);
    }

    SUBCASE("two replacements of the same incumbent: IncumbentBusy, atomically") {
        REQUIRE(begin_worker_upgrade(f, "a", "farm-worker-a2").ok);
        loom::PreparedReplacement second(f.bus(), f.kernel());
        const auto r = second.start({
            .operator_id = f.lead(),
            .coordinator = f.lead(),
            .role = farm::worker_role("a"),
            .candidate_name = "farm-worker-a-clang",
            .candidate_path = marathon::farm_testing::weave_path("farm-worker-a-clang"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.begin_reason == loom::TxnReason::IncumbentBusy);
        CHECK_FALSE(r.cleanup_failed);
    }
}

// ---- 5. hostile arrivals ---------------------------------------------------

TEST_CASE("A FORGED READINESS CANNOT MAKE A TRANSACTION READY") {
    Farm f;
    f.boot();
    REQUIRE(begin_worker_upgrade(f, "a", "farm-worker-a2").ok);
    f.rogue_does(marathon::farm_testing::ForgeWorkerReady{"a"});
    f.pump(6);

    REQUIRE_MESSAGE(f.desk().offers.size() == 1, desk_notes(f.desk()));
    CHECK_FALSE(f.desk().offers[0].ok);
    CHECK(f.desk().offers[0].why == loom::TxnReason::InvalidReadiness);
    CHECK(f.upgrade().state() == loom::TxnState::Preparing);
}

TEST_CASE("an offer with no transaction in flight is a nothing, not a crash") {
    Farm f;
    f.boot();
    f.forget_upgrade();
    f.rogue_does(marathon::farm_testing::ForgeWorkerReady{"a"});
    f.pump(4);
    CHECK(f.desk().offered_without_handle == 1);
    CHECK(f.desk().offers.empty());
}

TEST_CASE("MEASURED, NOT WAVED AT: a forged JobDone ends somebody else's build") {
    Farm f;
    f.boot();
    f.submit("v1", "release");
    f.pump(10);
    REQUIRE_MESSAGE(heard(f.book(), {"progress v1", "on a"}), transcript(f.book()));

    // The rogue holds an ordinary grant for an ordinary shape. Job numbers are on
    // the wire and start at 1. The dispatcher checks everything it CAN — the job
    // must be open and the claimed worker must be the one it went to — and there
    // is no way to ask Loom whether the sender holds `buildfarm.worker.a`.
    f.rogue_does(marathon::farm_testing::ForgeJobDone{"1", "a", true, "not-a-real-artifact.out"});
    f.pump(6);

    // FOURTH INDEPENDENT SIGHTING of the same gap: Loom attests answers and
    // lifecycle, not role-holding.
    CHECK_MESSAGE(heard(f.book(), {"succeeded v1", "not-a-real-artifact.out"}),
                  transcript(f.book()));
}

TEST_CASE("THE NEW ATTACK SURFACE: a forged arrival DESTROYS a healthy build") {
    Farm f;
    f.boot(/*workers=*/1);
    f.submit("z1", "release");
    f.pump(10);
    REQUIRE_MESSAGE(heard(f.book(), {"progress z1", "attempt 1"}), transcript(f.book()));

    // RECONCILIATION IS FASTER THAN A WATCHDOG AND TRUSTS MORE. A `WorkerOpen` is
    // an unauthenticated publication. The kitchen's roster had the same weakness
    // and the worst a forgery could do there was invent a station. HERE AN
    // ANNOUNCEMENT IS EVIDENCE THAT WORK WAS LOST, so a forged one is a claim
    // that somebody else's build is dead.
    f.rogue_does(marathon::farm_testing::ForgeWorkerOpen{"a"});
    f.pump(60);

    // ASSERTED AS IT ACTUALLY BEHAVES, and it is worse than "the build restarts".
    // The dispatcher requeues and re-offers the build to worker `a` — which never
    // lost it and is still building it — so `a` DECLINES as busy, and a decline
    // is a JUDGEMENT rather than an absence, so the build is failed outright.
    //
    // One unauthenticated publication, one destroyed build, and a reason the
    // requester cannot make sense of. The mechanism is the application's; the
    // fact that an announcement cannot be attributed is the substrate's.
    CHECK_MESSAGE(heard(f.book(), {"failed z1", "worker declined", "already building"}),
                  transcript(f.book()));
    CHECK_FALSE(heard(f.book(), {"succeeded z1"}));
    // The promise still ENDS in a word, which is the difference between this and
    // a lost order — but the word is wrong, and nothing at this tier can fix it.
    CHECK(f.book().outstanding.empty());
}

TEST_CASE("a JobDone naming a worker the job never went to is ignored") {
    Farm f;
    f.boot();
    f.submit("v4", "release");
    f.pump(10);
    REQUIRE_MESSAGE(heard(f.book(), {"progress v4", "on a"}), transcript(f.book()));

    // Everything about this is right except the one thing the dispatcher CAN
    // check: the itinerary.
    f.rogue_does(marathon::farm_testing::ForgeJobDone{"1", "b", true, "wrong-worker.out"});
    f.pump(6);
    CHECK_FALSE_MESSAGE(heard(f.book(), {"succeeded v4", "wrong-worker.out"}),
                        transcript(f.book()));
}
