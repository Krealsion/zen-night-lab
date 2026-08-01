// The download manager's suite.
//
// THE QUESTION UNDER TEST: a long-running operation has an acknowledgment, a
// stream of progress, and exactly one ending. Loom grants exactly ONE
// authenticated answer per request. So:
//
//     Is the original answer capability the right thing to hold for the entire
//     operation?
//
// The suite answers by running the SAME client against two services built from
// one source that differ in nothing but that decision, and reading back which
// of the client's attestation counters moved. Nothing here argues; everything
// here counts.
//
// The one substitution is the Timer's CLOCK, labelled in harness.hpp.
//
// The cases are grouped by what they are for:
//   1. the service works at all
//   2. the positive vertical: accepted -> progress -> completed
//   3. refusals and endings that are not success
//   4. THE MEASUREMENT: which half of an operation can be attested, and what
//      holding an answer for the whole operation actually costs
//   5. replacement: what crosses, what does not, and who says so
//   6. hostile arrivals

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "harness.hpp"

#include <string>
#include <vector>

using marathon::dl_testing::Downloads;
namespace dl = marathon::downloads;

namespace {

bool heard(const dl::ClientLedger& l, const std::vector<std::string>& fragments) {
    for (const std::string& line : l.heard) {
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

std::size_t heard_count(const dl::ClientLedger& l, const std::vector<std::string>& fragments) {
    std::size_t n = 0;
    for (const std::string& line : l.heard) {
        bool all = true;
        for (const std::string& f : fragments) {
            all = all && line.find(f) != std::string::npos;
        }
        n += all ? 1u : 0u;
    }
    return n;
}

std::string transcript(const dl::ClientLedger& l) {
    std::string out = "\n  client heard:";
    for (const std::string& line : l.heard) {
        out += "\n    " + line;
    }
    if (l.heard.empty()) {
        out += " (nothing)";
    }
    return out;
}

std::string desk_notes(const marathon::dl_testing::OpsDesk& d) {
    std::string out = "\n  ops saw:";
    for (const std::string& n : d.notes) {
        out += "\n    " + n;
    }
    if (d.notes.empty()) {
        out += " (nothing)";
    }
    return out;
}

/// How many progress reports about `ticket`, and did any of them go backwards?
struct ProgressShape {
    std::size_t count = 0;
    bool monotonic = true;
    std::int64_t last = -1;
};

ProgressShape progress_of(const dl::ClientLedger& l, const std::string& ticket) {
    ProgressShape s;
    for (const auto& p : l.progress) {
        if (p.first != ticket) {
            continue;
        }
        ++s.count;
        s.monotonic = s.monotonic && p.second > s.last;
        s.last = p.second;
    }
    return s;
}

} // namespace

// ---- 1. the service works at all -------------------------------------------

TEST_CASE("the service opens: both loads are answered and nothing is refused") {
    Downloads d;
    d.boot();
    CHECK(d.oplog().pending.empty());
    REQUIRE(d.oplog().answers.size() == 2);
    for (const std::string& a : d.oplog().answers) {
        CHECK(a.find("refused") == std::string::npos);
    }
}

TEST_CASE("the service answers a diagnostic about itself, authentically") {
    Downloads d;
    d.boot();
    d.start("t1", "manifest.json");
    d.pump(10);
    d.ask_status();
    d.pump(4);

    REQUIRE(d.status().size() == 1);
    const std::string& tally = d.status()[0];
    CHECK_MESSAGE(tally.find("acknowledges-at-once") != std::string::npos, tally);
    CHECK_MESSAGE(tally.find("accepted=1") != std::string::npos, tally);
    CHECK_MESSAGE(tally.find("completed=1") != std::string::npos, tally);
}

// ---- 2. the positive vertical ----------------------------------------------

TEST_CASE("a download is accepted, reports progress, and completes with bytes that check out") {
    Downloads d;
    d.boot();
    d.start("t1", "index.db"); // 640 bytes, 64 per beat: ten beats
    d.pump(30);

    CHECK_MESSAGE(heard(d.ledger(), {"accepted t1", "index.db", "640 bytes"}),
                  transcript(d.ledger()));
    CHECK_MESSAGE(heard(d.ledger(), {"completed t1", "640 bytes", "checks out"}),
                  transcript(d.ledger()));
    CHECK(d.ledger().outstanding.empty());

    // Progress is a real channel and not decoration: it arrives many times and it
    // never goes backwards.
    const ProgressShape p = progress_of(d.ledger(), "t1");
    CHECK_MESSAGE(p.count >= 9, "progress reports: ", p.count);
    CHECK(p.monotonic);
    CHECK(p.last == 640);
}

TEST_CASE("two clients naming a transfer the same way do not collide") {
    Downloads d;
    d.boot();
    d.start("same", "manifest.json");
    d.start_second("same", "index.db");
    d.pump(30);

    CHECK_MESSAGE(heard(d.ledger(), {"completed same", "192 bytes"}), transcript(d.ledger()));
    CHECK_MESSAGE(heard(d.second_ledger(), {"completed same", "640 bytes"}),
                  transcript(d.second_ledger()));
    CHECK(d.ledger().ignored == 0);
    CHECK(d.second_ledger().ignored == 0);
}

TEST_CASE("many simultaneous transfers all reach an ending") {
    Downloads d;
    d.boot();
    for (int i = 0; i < 12; ++i) {
        d.start("m" + std::to_string(i), i % 2 == 0 ? "manifest.json" : "index.db");
    }
    d.pump(40);

    CHECK(d.ledger().outstanding.empty());
    CHECK_MESSAGE(heard_count(d.ledger(), {"completed m"}) == 12, transcript(d.ledger()));
    CHECK(d.ledger().ignored == 0);
}

// ---- 3. refusals and endings that are not success ---------------------------

TEST_CASE("an unknown source is refused with a reason, not dropped") {
    Downloads d;
    d.boot();
    d.start("t1", "nowhere.bin");
    d.pump(6);

    CHECK_MESSAGE(heard(d.ledger(), {"refused t1", "no source named 'nowhere.bin'"}),
                  transcript(d.ledger()));
    CHECK(d.ledger().outstanding.empty());
}

TEST_CASE("a duplicate ticket from the same client is refused") {
    Downloads d;
    d.boot();
    d.start("dup", "kernel.img");
    d.pump(2);
    d.start("dup", "kernel.img");
    d.pump(6);

    CHECK_MESSAGE(heard(d.ledger(), {"refused dup", "already have a transfer named"}),
                  transcript(d.ledger()));
}

TEST_CASE("a transfer with no destination is refused: the service will not guess one") {
    Downloads d;
    d.boot();
    d.start("t1", "manifest.json", "");
    d.pump(6);
    CHECK_MESSAGE(heard(d.ledger(), {"refused t1", "needs a destination"}),
                  transcript(d.ledger()));
}

TEST_CASE("a source that goes bad partway fails, and says how much was discarded") {
    Downloads d;
    d.boot();
    d.start("bad", "truncated.iso"); // 896 bytes, breaks at 448
    d.pump(30);

    CHECK_MESSAGE(heard(d.ledger(), {"failed bad", "448 bytes", "went bad at byte 448"}),
                  transcript(d.ledger()));
    CHECK_FALSE(heard(d.ledger(), {"completed bad"}));
    CHECK(d.ledger().outstanding.empty());
}

TEST_CASE("the book is bounded, and a full book refuses visibly") {
    Downloads d;
    d.boot();
    // kernel.img is 1536 bytes at 64 per beat: nothing completes inside this run,
    // so every accepted transfer is still occupying a slot when the last starts.
    for (std::size_t i = 0; i <= dl::kMaxOpenTransfers; ++i) {
        d.start("b" + std::to_string(i), "kernel.img");
    }
    d.pump(3);

    CHECK_MESSAGE(heard_count(d.ledger(), {"accepted b"}) == dl::kMaxOpenTransfers,
                  "accepted: ", heard_count(d.ledger(), {"accepted b"}));
    CHECK_MESSAGE(heard(d.ledger(), {"refused b", "maximum of 80 transfers"}),
                  "the 81st transfer was not refused");
}

TEST_CASE("a client that withdraws is acknowledged, and still gets the ending it was owed") {
    Downloads d;
    d.boot();
    const std::uint64_t corr = d.start("c1", "kernel.img");
    d.pump(4);
    REQUIRE_MESSAGE(heard(d.ledger(), {"accepted c1"}), transcript(d.ledger()));

    d.cancel("c1", corr);
    d.pump(6);

    CHECK_MESSAGE(heard(d.ledger(), {"failed c1", "cancelled by the client"}),
                  transcript(d.ledger()));
    CHECK_MESSAGE(heard(d.ledger(), {"cancel acknowledged"}), transcript(d.ledger()));
    CHECK(d.ledger().outstanding.empty());
    // ...and nothing keeps moving afterwards.
    const std::size_t before = d.ledger().progress.size();
    d.pump(10);
    CHECK(d.ledger().progress.size() == before);
}

TEST_CASE("cancelling a transfer nobody has is refused, in the shape a stranger can read") {
    Downloads d;
    d.boot();
    d.cancel("ghost", 999);
    d.pump(6);
    CHECK_MESSAGE(heard(d.ledger(), {"cancel refused", "no transfer named 'ghost'"}),
                  transcript(d.ledger()));
}

// ---- 4. THE MEASUREMENT -----------------------------------------------------
//
// The same client, the same transfer, two services. What differs is which single
// message Loom vouched for.

TEST_CASE("ACKNOWLEDGE-AT-ONCE: the promise is attested and the ENDING is not") {
    Downloads d;
    d.boot("download-service");
    d.start("t1", "index.db");
    d.pump(30);
    REQUIRE_MESSAGE(heard(d.ledger(), {"completed t1"}), transcript(d.ledger()));

    CHECK(d.ledger().accepts_attested == 1);
    CHECK(d.ledger().accepts_unattested == 0);
    CHECK(d.ledger().terminals_attested == 0);
    CHECK(d.ledger().terminals_unattested == 1);
    // PROGRESS CAN NEVER BE ATTESTED, in either build, because there is one
    // authenticated answer per request and progress is many messages.
    CHECK(d.ledger().progress_attested == 0);
    CHECK(d.ledger().progress_unattested > 0);
}

TEST_CASE("HOLDS-THE-ANSWER: the ENDING is attested and the promise is not") {
    Downloads d;
    d.boot("download-service-holds");
    d.start("t1", "index.db");
    d.pump(30);
    REQUIRE_MESSAGE(heard(d.ledger(), {"completed t1"}), transcript(d.ledger()));

    CHECK(d.ledger().accepts_attested == 0);
    CHECK(d.ledger().accepts_unattested == 1);
    CHECK(d.ledger().terminals_attested == 1);
    CHECK(d.ledger().terminals_unattested == 0);
    CHECK(d.ledger().progress_attested == 0);
}

TEST_CASE("YOU GET ONE. Nothing available makes both the promise and the ending attested") {
    // Stated as a case rather than as prose, because it is the whole answer to
    // this project's question and it should go red if the substrate ever changes.
    Downloads at_once;
    at_once.boot("download-service");
    at_once.start("t1", "manifest.json");
    at_once.pump(20);

    Downloads holds;
    holds.boot("download-service-holds");
    holds.start("t1", "manifest.json");
    holds.pump(20);

    const std::int64_t a = at_once.ledger().accepts_attested +
                           at_once.ledger().terminals_attested;
    const std::int64_t h = holds.ledger().accepts_attested + holds.ledger().terminals_attested;
    CHECK(a == 1);
    CHECK(h == 1);
    // ...and they attested DIFFERENT halves.
    CHECK(at_once.ledger().accepts_attested != holds.ledger().accepts_attested);
}

TEST_CASE("HOLDING AN ANSWER SPENDS ONE LOOM'S CAPACITY, not one weave's") {
    Downloads d;
    d.boot("download-service-holds");

    // Before: an unrelated weave can hold a conversation without trouble. The
    // "was it asked at all" counter exists because "not denied" and "never ran"
    // look identical from the outside, and one of them is a green that lies.
    d.ask_bystander();
    d.pump(4);
    REQUIRE(d.bystander_asked() == 1);
    REQUIRE(d.bystander_denied() == 0);

    // kernel.img is 24 beats long, so every one of these is still open at the end
    // of this run. `kMaxOpenTransfers` is 80, deliberately larger than Loom's 64,
    // so the limit that binds here is the SUBSTRATE's.
    for (int i = 0; i < 70; ++i) {
        d.start("h" + std::to_string(i), "kernel.img");
    }
    d.pump(5);

    // Some transfers were refused, and the reason names the real limit.
    CHECK_MESSAGE(heard(d.ledger(), {"refused h", "no unfinished-conversation slots left"}),
                  "no transfer was refused for want of an answer slot");

    // ...AND SO IS EVERYBODY ELSE. This is the finding: a service that holds an
    // answer right for the duration of a long operation is not making a local
    // design choice, it is spending a resource that belongs to the whole Loom.
    d.ask_bystander();
    d.pump(6);
    REQUIRE_MESSAGE(d.bystander_asked() == 2, "the bystander was never asked a second time");
    CHECK_MESSAGE(d.bystander_denied() >= 1,
                  "a weave with nothing to do with downloads could still defer");
}

TEST_CASE("ACKNOWLEDGE-AT-ONCE costs the Loom nothing while the operation runs") {
    Downloads d;
    d.boot("download-service");
    for (int i = 0; i < 70; ++i) {
        d.start("h" + std::to_string(i), "kernel.img");
    }
    d.pump(5);

    // The control for the case above: the same seventy operations, in flight at
    // the same time, and the Loom's conversation capacity is untouched.
    d.ask_bystander();
    d.pump(2);
    CHECK(d.bystander_denied() == 0);
    CHECK_FALSE(heard(d.ledger(), {"no unfinished-conversation slots left"}));
}

// ---- 5. replacement ---------------------------------------------------------

namespace {

loom::PreparedReplacement::StartResult begin_upgrade(Downloads& d, const std::string& stem,
                                                     std::uint32_t budget = 8) {
    return d.new_upgrade().start({
        .operator_id = d.ops(),
        .coordinator = d.ops(),
        .role = dl::kServiceRole,
        .candidate_name = stem,
        .candidate_path = marathon::dl_testing::weave_path(stem),
        .budget = budget,
    });
}

} // namespace

TEST_CASE("THE REQUIRED CASE: the service is replaced mid-transfer, and the OBLIGATION crosses "
          "even though the work does not") {
    Downloads d;
    d.boot("download-service");
    d.start("r1", "kernel.img"); // 1536 bytes: long enough to still be running
    d.pump(6);
    REQUIRE_MESSAGE(heard(d.ledger(), {"accepted r1"}), transcript(d.ledger()));
    const std::int64_t reached = progress_of(d.ledger(), "r1").last;
    REQUIRE(reached > 0);

    // 1. The preparation window is the one interval in which the incumbent is
    //    alive AND the successor is reachable. Ask the incumbent what it owes.
    d.describe_obligations();
    d.pump(3);
    REQUIRE_MESSAGE(d.desk().described_arrived, desk_notes(d.desk()));
    REQUIRE_MESSAGE(d.desk().described.size() == 1, desk_notes(d.desk()));
    CHECK(d.desk().described[0].ticket == "r1");
    CHECK(d.desk().described[0].bytes_done >= reached);

    // 2. Start, ask, offer, commit — nothing but the handle.
    REQUIRE(begin_upgrade(d, "download-service-v2").ok);
    CHECK(d.bus().sealed(d.upgrade().candidate()));
    std::vector<std::string>* seen = d.watch(d.upgrade().candidate());

    REQUIRE(d.upgrade().ask(dl::PrepareService{d.desk().described, false}).ok);
    d.pump(4);
    REQUIRE_MESSAGE(d.desk().offers.size() == 1, desk_notes(d.desk()));
    CHECK(d.desk().offers[0].ok);
    REQUIRE(d.upgrade().state() == loom::TxnState::Ready);

    const std::size_t outside_the_world = seen->size();
    REQUIRE(d.upgrade().commit(9).ok);
    CHECK(d.upgrade().state() == loom::TxnState::AdmissionPending);
    d.pump(10);

    const std::optional<loom::TxnOutcome> outcome = d.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Committed);
    REQUIRE(seen->size() > outside_the_world);
    CHECK((*seen)[outside_the_world] == std::string(loom::Activated::zen_name));

    // 3. THE CONTRACT, HONOURED. The successor does not continue the transfer and
    //    does not pretend to. It reports the failure, names the bytes discarded,
    //    and the client's conversation is closed.
    CHECK_MESSAGE(heard(d.ledger(), {"failed r1", "service was replaced", "were discarded"}),
                  transcript(d.ledger()));
    CHECK_FALSE(heard(d.ledger(), {"completed r1"}));
    CHECK(d.ledger().outstanding.empty());

    // 4. THE BYTES DID NOT CROSS, and nothing claims they did: the number in the
    //    failure is exactly the number that crossed in the OBLIGATION, which is
    //    a count and not a payload. (Not the number from step 1 — the incumbent
    //    kept transferring while the description was in flight, which is itself
    //    the proof that asking it to describe its work changed nothing.)
    const std::int64_t crossed = d.desk().described[0].bytes_done;
    const std::string* line = nullptr;
    for (const std::string& l : d.ledger().heard) {
        if (l.find("failed r1") != std::string::npos) {
            line = &l;
        }
    }
    REQUIRE(line != nullptr);
    CHECK_MESSAGE(line->find("after " + std::to_string(crossed) + " bytes") != std::string::npos,
                  *line);
    CHECK_MESSAGE(line->find(std::to_string(crossed) + " of 1536 bytes were discarded") !=
                      std::string::npos,
                  *line);

    // 5. The new service serves new transfers.
    d.start("r2", "manifest.json");
    d.pump(20);
    CHECK_MESSAGE(heard(d.ledger(), {"completed r2", "checks out"}), transcript(d.ledger()));
}

TEST_CASE("the incumbent keeps serving throughout the preparation, and never learns of it") {
    Downloads d;
    d.boot("download-service");
    REQUIRE(begin_upgrade(d, "download-service-v2").ok);

    // A whole transfer runs to completion while a replacement is prepared behind
    // it. The service being replaced is not told, not paused, and not degraded.
    d.start("p1", "index.db");
    d.pump(30);
    CHECK_MESSAGE(heard(d.ledger(), {"completed p1", "checks out"}), transcript(d.ledger()));
    CHECK(d.upgrade().state() == loom::TxnState::Preparing);
}

TEST_CASE("DEFERRED READINESS: the sealed candidate asks the operations desk before agreeing") {
    Downloads d;
    d.boot("download-service");
    REQUIRE(begin_upgrade(d, "download-service-v2").ok);
    REQUIRE(d.upgrade().ask(dl::PrepareService{{}, /*verify_sources=*/true}).ok);
    d.pump(1);
    CHECK(d.upgrade().state() == loom::TxnState::Preparing);
    CHECK(d.desk().offers.empty());

    d.pump(8);
    CHECK_MESSAGE(d.upgrade().state() == loom::TxnState::Ready, desk_notes(d.desk()));
    REQUIRE(d.desk().offers.size() == 1);
    CHECK(d.desk().offers[0].ok);
}

TEST_CASE("AUTHENTIC REFUSAL: a candidate that disagrees about what it must serve says no") {
    Downloads d;
    d.boot("download-service");
    // The desk answers the candidate's question with a catalogue size this
    // artifact does not ship. The candidate refuses — for a domain reason, on its
    // own authority.
    d.desk().catalogue_answer = 99;
    REQUIRE(begin_upgrade(d, "download-service-v2").ok);
    REQUIRE(d.upgrade().ask(dl::PrepareService{{}, /*verify_sources=*/true}).ok);
    d.pump(10);

    REQUIRE_MESSAGE(d.desk().offers.size() == 1, desk_notes(d.desk()));
    CHECK(d.desk().offers[0].ok); // the OFFER succeeded; the ANSWER was "no"
    CHECK(d.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = d.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);

    // The incumbent never learned any of this, and is still the service.
    d.start("s1", "manifest.json");
    d.pump(20);
    CHECK_MESSAGE(heard(d.ledger(), {"completed s1"}), transcript(d.ledger()));
}

TEST_CASE("a candidate refuses debt it could never discharge") {
    Downloads d;
    d.boot("download-service");
    REQUIRE(begin_upgrade(d, "download-service-v2").ok);

    dl::Obligation impossible;
    impossible.ticket = "x1";
    impossible.client = "12345";
    impossible.correlation = 1;
    impossible.source = "a-source-that-does-not-exist";
    impossible.bytes_done = 10;
    impossible.total_bytes = 100;
    REQUIRE(d.upgrade().ask(dl::PrepareService{{impossible}, /*verify_sources=*/true}).ok);
    d.pump(6);

    CHECK(d.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = d.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);
}

TEST_CASE("a candidate refuses more debt than an honest predecessor could have owed") {
    Downloads d;
    d.boot("download-service");
    REQUIRE(begin_upgrade(d, "download-service-v2").ok);

    std::vector<dl::Obligation> too_many;
    for (std::size_t i = 0; i <= dl::kMaxInheritedObligations; ++i) {
        dl::Obligation o;
        o.ticket = "o" + std::to_string(i);
        o.client = "7";
        o.source = "manifest.json";
        too_many.push_back(o);
    }
    REQUIRE(d.upgrade().ask(dl::PrepareService{too_many, false}).ok);
    d.pump(6);

    CHECK(d.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = d.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);
}

TEST_CASE("ABORTED OUTCOME: the desk changes its mind and the world is exactly as it was") {
    Downloads d;
    d.boot("download-service");
    const loom::WeaveId incumbent = d.bus().role_holder(dl::kServiceRole);
    REQUIRE(begin_upgrade(d, "download-service-v2").ok);
    REQUIRE(d.upgrade().ask(dl::PrepareService{{}, false}).ok);
    d.pump(4);
    REQUIRE(d.upgrade().state() == loom::TxnState::Ready);

    REQUIRE(d.upgrade().abort().ok);
    const std::optional<loom::TxnOutcome> outcome = d.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::ExplicitAbort);
    CHECK(d.bus().role_holder(dl::kServiceRole) == incumbent);

    d.start("a1", "manifest.json");
    d.pump(20);
    CHECK_MESSAGE(heard(d.ledger(), {"completed a1"}), transcript(d.ledger()));
}

TEST_CASE("EXACT ERROR INSPECTION: refusals keep the substrate's own words") {
    Downloads d;
    d.boot("download-service");

    SUBCASE("nobody holds the role") {
        const auto r = d.new_upgrade().start({
            .operator_id = d.ops(),
            .coordinator = d.ops(),
            .role = "download.nobody",
            .candidate_name = "download-service-v2",
            .candidate_path = marathon::dl_testing::weave_path("download-service-v2"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::NoRoleHolder);
    }

    SUBCASE("the artifact refuses to load") {
        const auto r = begin_upgrade(d, "download-service-imaginary");
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::CandidateLoad);
        CHECK_MESSAGE(r.error.find("download-service-imaginary") != std::string::npos, r.error);
    }

    SUBCASE("a second replacement of the same incumbent is IncumbentBusy, atomically") {
        REQUIRE(begin_upgrade(d, "download-service-v2").ok);
        loom::PreparedReplacement second(d.bus(), d.kernel());
        const auto r = second.start({
            .operator_id = d.ops(),
            .coordinator = d.ops(),
            .role = dl::kServiceRole,
            .candidate_name = "download-service-holds-v2",
            .candidate_path = marathon::dl_testing::weave_path("download-service-holds-v2"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.begin_reason == loom::TxnReason::IncumbentBusy);
        CHECK_FALSE(r.cleanup_failed);
    }
}

TEST_CASE("A FORGED READINESS CANNOT MAKE A TRANSACTION READY") {
    Downloads d;
    d.boot("download-service");
    REQUIRE(begin_upgrade(d, "download-service-v2").ok);

    d.rogue_does(marathon::dl_testing::ForgeServiceReady{});
    d.pump(6);

    REQUIRE_MESSAGE(d.desk().offers.size() == 1, desk_notes(d.desk()));
    CHECK_FALSE(d.desk().offers[0].ok);
    CHECK(d.desk().offers[0].why == loom::TxnReason::InvalidReadiness);
    CHECK(d.upgrade().state() == loom::TxnState::Preparing);
}

TEST_CASE("HOLDS-THE-ANSWER ACROSS A REPLACEMENT: the attested ending was the one thing that "
          "could not be inherited") {
    Downloads d;
    d.boot("download-service-holds");
    d.start("k1", "kernel.img");
    d.pump(6);
    REQUIRE_MESSAGE(heard(d.ledger(), {"accepted k1"}), transcript(d.ledger()));
    // In this build the acknowledgment was ordinary and the ENDING was going to
    // be the attested one.
    REQUIRE(d.ledger().accepts_attested == 0);
    REQUIRE(d.ledger().terminals_attested == 0);

    d.describe_obligations();
    d.pump(3);
    REQUIRE(d.desk().described.size() == 1);

    REQUIRE(begin_upgrade(d, "download-service-holds-v2").ok);
    REQUIRE(d.upgrade().ask(dl::PrepareService{d.desk().described, false}).ok);
    d.pump(4);
    REQUIRE(d.upgrade().state() == loom::TxnState::Ready);
    REQUIRE(d.upgrade().commit(3).ok);
    d.pump(20);

    // The client is told. But the promise that its ending would be attested could
    // not be kept by anybody: the answer right belonged to a life that has ended,
    // and there is no representation of one that a successor could be handed.
    CHECK_MESSAGE(heard(d.ledger(), {"failed k1", "service was replaced"}),
                  transcript(d.ledger()));
    CHECK(d.ledger().terminals_attested == 0);
    CHECK(d.ledger().terminals_unattested == 1);
    CHECK(d.ledger().outstanding.empty());
}

// ---- 6. hostile arrivals ----------------------------------------------------

TEST_CASE("MEASURED, NOT WAVED AT: a forged ending closes somebody else's operation") {
    Downloads d;
    d.boot("download-service");
    const std::uint64_t corr = d.start("v1", "kernel.img");
    d.pump(4);
    REQUIRE_MESSAGE(heard(d.ledger(), {"accepted v1"}), transcript(d.ledger()));

    // The rogue holds an ordinary grant for an ordinary shape, and the
    // correlation is not a secret — it is on the bus for anyone watching.
    d.rogue_does(marathon::dl_testing::ForgeCompleted{
        static_cast<std::int64_t>(d.client().value), "v1", static_cast<std::int64_t>(corr), 1536});
    d.pump(4);

    // THE SAME SEAM THE KITCHEN FOUND, from a different domain: in the
    // acknowledge-at-once build the ENDING carries no attestation, so a weave
    // that can guess a correlation can end an operation. The digest check is what
    // catches it here — and a digest is domain cleverness, not a substrate fact.
    CHECK_MESSAGE(heard(d.ledger(), {"completed v1", "IS WRONG"}), transcript(d.ledger()));
    CHECK(d.ledger().outstanding.empty());
}

TEST_CASE("a forged ending for a correlation nobody is waiting on is ignored") {
    Downloads d;
    d.boot("download-service");
    d.rogue_does(marathon::dl_testing::ForgeCompleted{
        static_cast<std::int64_t>(d.client().value), "ghost", 4242, 10});
    d.pump(4);
    CHECK_MESSAGE(d.ledger().ignored == 1, transcript(d.ledger()));
    CHECK_FALSE(heard(d.ledger(), {"completed ghost"}));
}

TEST_CASE("an offer with no transaction in flight is a nothing, not a crash") {
    Downloads d;
    d.boot("download-service");
    d.forget_upgrade();
    d.rogue_does(marathon::dl_testing::ForgeServiceReady{});
    d.pump(4);
    CHECK(d.desk().offered_without_handle == 1);
    CHECK(d.desk().offers.empty());
}
