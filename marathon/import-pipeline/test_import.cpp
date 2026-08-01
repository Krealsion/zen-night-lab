// The import pipeline's suite.
//
// THE QUESTION UNDER TEST: the Timer package authored
//
//     request -> available choices -> selected choice -> resolved choice -> receipt
//
// and the kitchen used it unchanged. Two sightings is a candidate. This is the
// third, from a domain where the menu is DISCOVERED BY THE SERVICE rather than
// declared by the requester — and the point of the suite is to find out whether
// the shape survived that, or merely looked similar.
//
// It also contains the one wall three previous projects could not build: the
// second half of the consumer obligation, PERFORMABLE, because the counterparty
// here is a specific weave and not a role.
//
// The one substitution is the Timer's CLOCK, labelled in harness.hpp.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "harness.hpp"

#include <string>
#include <vector>

using marathon::import_testing::Pipeline;
namespace imp = marathon::importer;

namespace {

bool heard(const imp::RequesterBook& b, const std::vector<std::string>& fragments) {
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

std::size_t heard_count(const imp::RequesterBook& b, const std::vector<std::string>& fragments) {
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

std::string transcript(const imp::RequesterBook& b) {
    std::string out = "\n  requester heard:";
    for (const std::string& line : b.heard) {
        out += "\n    " + line;
    }
    if (b.heard.empty()) {
        out += " (nothing)";
    }
    return out;
}

std::string desk_notes(const marathon::import_testing::CuratorDesk& d) {
    std::string out = "\n  curator saw:";
    for (const std::string& n : d.notes) {
        out += "\n    " + n;
    }
    if (d.notes.empty()) {
        out += " (nothing)";
    }
    return out;
}

loom::PreparedReplacement::StartResult begin_upgrade(Pipeline& p, const std::string& stem,
                                                     std::uint32_t budget = 8) {
    return p.new_upgrade().start({
        .operator_id = p.curator(),
        .coordinator = p.curator(),
        .role = imp::kImporterRole,
        .candidate_name = stem,
        .candidate_path = marathon::import_testing::weave_path(stem),
        .budget = budget,
    });
}

} // namespace

// ---- 1. it works at all -----------------------------------------------------

TEST_CASE("the pipeline opens: both loads are answered and nothing is refused") {
    Pipeline p;
    p.boot();
    CHECK(p.oplog().pending.empty());
    REQUIRE(p.oplog().answers.size() == 2);
    for (const std::string& a : p.oplog().answers) {
        CHECK(a.find("refused") == std::string::npos);
    }
}

TEST_CASE("the pipeline answers a diagnostic about itself, authentically") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "note.txt");
    p.pump(4);
    p.choose("t1", "utf8", c);
    p.pump(20);
    p.ask_status();
    p.pump(4);

    REQUIRE(p.status().size() == 1);
    CHECK_MESSAGE(p.status()[0].find("offered=1") != std::string::npos, p.status()[0]);
    CHECK_MESSAGE(p.status()[0].find("resolved=1") != std::string::npos, p.status()[0]);
    CHECK_MESSAGE(p.status()[0].find("receipts=1") != std::string::npos, p.status()[0]);
}

// ---- 2. the positive vertical: the whole shape ------------------------------

TEST_CASE("THE SHAPE: request -> menu -> choice -> resolution -> receipt") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "holiday.mov");
    p.pump(4);

    // 1. THE ANSWER TO THE REQUEST IS THE MENU. Not an acknowledgement: nothing
    //    has been agreed, because the service does not yet know what is wanted.
    CHECK_MESSAGE(heard(p.book(), {"menu t1", "h264-1080", "h264-720", "prores-1080"}),
                  transcript(p.book()));
    CHECK(p.book().menus_attested == 1);
    CHECK(p.book().menus_unattested == 0);

    // 2. The choice, and 3. the resolution — a distinct step, authenticated.
    p.choose("t1", "h264-720", c);
    p.pump(4);
    CHECK_MESSAGE(heard(p.book(), {"resolved t1", "'h264-720' -> 'h264-720'",
                                   "named an interpretation exactly"}),
                  transcript(p.book()));
    CHECK(p.book().resolutions_attested == 1);

    // 4. The receipt, ORDINARY: the two answer rights this conversation earned
    //    were spent on the menu and the resolution, and there is not a third.
    p.pump(20);
    CHECK_MESSAGE(heard(p.book(), {"receipt t1", "holiday.mov#h264-720", "1280x720"}),
                  transcript(p.book()));
    CHECK(p.book().receipts_attested == 0);
    CHECK(p.book().receipts_unattested == 1);
    CHECK(p.book().outstanding.empty());
}

TEST_CASE("AN UNDERSPECIFIED CHOICE IS RESOLVED IN THE OPEN, and the requester is told why") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "holiday.mov");
    p.pump(4);
    // "h264" names a CODEC, and the file admits two of them.
    p.choose("t1", "h264", c);
    p.pump(4);

    CHECK_MESSAGE(heard(p.book(), {"resolved t1", "'h264' -> 'h264-1080'", "names a codec"}),
                  transcript(p.book()));
    p.pump(20);
    CHECK_MESSAGE(heard(p.book(), {"receipt t1", "1920x1080"}), transcript(p.book()));
}

TEST_CASE("a file with ONE interpretation still gets a menu: the importer will not decide") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "note.txt");
    p.pump(4);
    CHECK_MESSAGE(heard(p.book(), {"menu t1", "utf8"}), transcript(p.book()));
    // Nothing has been imported yet. The requester must still say so.
    CHECK_FALSE(heard(p.book(), {"receipt t1"}));
    p.pump(20);
    CHECK_FALSE(heard(p.book(), {"receipt t1"}));

    p.choose("t1", "utf8", c);
    p.pump(20);
    CHECK_MESSAGE(heard(p.book(), {"receipt t1", "note.txt#utf8"}), transcript(p.book()));
}

TEST_CASE("two requesters naming a conversation the same way do not collide") {
    Pipeline p;
    p.boot();
    const std::uint64_t a = p.ask("same", "note.txt");
    p.ask_second("same", "scan.tif");
    p.pump(6);
    CHECK_MESSAGE(heard(p.book(), {"menu same", "utf8"}), transcript(p.book()));
    CHECK_MESSAGE(heard(p.second_book(), {"menu same", "rgb-600"}),
                  transcript(p.second_book()));
    p.choose("same", "utf8", a);
    p.pump(20);
    CHECK_MESSAGE(heard(p.book(), {"receipt same", "note.txt#utf8"}), transcript(p.book()));
    CHECK(p.book().ignored == 0);
    CHECK(p.second_book().ignored == 0);
}

// ---- 3. refusals ------------------------------------------------------------

TEST_CASE("a file the catalogue cannot read is refused, naming the catalogue") {
    Pipeline p;
    p.boot();
    p.ask("t1", "mystery.xyz");
    p.pump(6);
    CHECK_MESSAGE(heard(p.book(), {"refused t1", "'house' catalogue does not know how to read"}),
                  transcript(p.book()));
    CHECK(p.book().outstanding.empty());
}

TEST_CASE("REFUSAL IS AN OUTCOME AND NEVER AN EMPTY MENU") {
    Pipeline p;
    p.boot();
    p.ask("t1", "corrupt.bin"); // readable, admits nothing
    p.pump(6);

    // The distinction this case exists for: an empty menu would be the service
    // asking the requester to choose from nothing, which is not a question.
    CHECK_MESSAGE(heard(p.book(), {"refused t1", "admits no interpretation"}),
                  transcript(p.book()));
    CHECK_FALSE(heard(p.book(), {"menu t1"}));
}

TEST_CASE("a duplicate ticket from the same requester is refused") {
    Pipeline p;
    p.boot();
    p.ask("dup", "note.txt");
    p.pump(4);
    p.ask("dup", "note.txt");
    p.pump(6);
    CHECK_MESSAGE(heard(p.book(), {"refused dup", "already have a conversation named"}),
                  transcript(p.book()));
}

TEST_CASE("the pipeline's book is bounded, and a full book refuses visibly") {
    Pipeline p;
    p.boot();
    for (std::size_t i = 0; i <= imp::kMaxOpenConversations; ++i) {
        p.ask("n" + std::to_string(i), "holiday.mov");
    }
    p.pump(10);
    CHECK_MESSAGE(heard_count(p.book(), {"menu n"}) == imp::kMaxOpenConversations,
                  "menus offered: ", heard_count(p.book(), {"menu n"}));
    CHECK_MESSAGE(heard(p.book(), {"refused n", "maximum of 12 conversations"}),
                  transcript(p.book()));
}

TEST_CASE("a requester may leave, and leaving an unknown conversation is refused") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "holiday.mov");
    p.pump(4);
    p.abandon("t1", c);
    p.pump(4);
    CHECK_MESSAGE(heard(p.book(), {"abandon acknowledged"}), transcript(p.book()));

    p.abandon("ghost", 999);
    p.pump(4);
    CHECK_MESSAGE(heard(p.book(), {"abandon refused", "no conversation named 'ghost'"}),
                  transcript(p.book()));
}

// ---- 4. the hostile cases the brief asks for --------------------------------

TEST_CASE("HOSTILE: a choice that is not on the menu is refused, never guessed at") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "holiday.mov");
    p.pump(4);
    p.choose("t1", "av1-4k", c);
    p.pump(4);

    CHECK_MESSAGE(heard(p.book(), {"choice refused t1", "'av1-4k' is not one of the 3"}),
                  transcript(p.book()));
    p.pump(20);
    CHECK_FALSE(heard(p.book(), {"receipt t1"}));
}

TEST_CASE("HOSTILE: a STALE choice naming a closed menu is refused, and the refusal names the "
          "open one") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "holiday.mov");
    p.pump(4);
    const std::string real_menu = p.book().menu_of["t1"];
    REQUIRE_FALSE(real_menu.empty());

    p.choose("t1", "h264-720", c, /*menu=*/"m99");
    p.pump(4);
    CHECK_MESSAGE(heard(p.book(), {"choice refused t1", "menu 'm99' is closed",
                                   "'" + real_menu + "'"}),
                  transcript(p.book()));
    // ...and the honest choice still works afterwards. A stale choice does not
    // end the conversation.
    p.choose("t1", "h264-720", c);
    p.pump(20);
    CHECK_MESSAGE(heard(p.book(), {"receipt t1"}), transcript(p.book()));
}

TEST_CASE("HOSTILE: a DUPLICATE choice is refused, naming what was already decided") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "holiday.mov");
    p.pump(4);
    const std::string menu = p.book().menu_of["t1"];
    // Both choices go out with NO pump between them, so the second is genuinely
    // racing the first rather than arriving after a comfortable gap.
    p.choose("t1", "h264-720", c);
    p.choose("t1", "prores-1080", c, menu);
    p.pump(6);
    REQUIRE_MESSAGE(heard(p.book(), {"resolved t1"}), transcript(p.book()));
    CHECK_MESSAGE(heard(p.book(), {"choice refused t1", "already resolved to 'h264-720'"}),
                  transcript(p.book()));
    p.pump(20);
    // The first decision stands.
    CHECK_MESSAGE(heard(p.book(), {"receipt t1", "1280x720"}), transcript(p.book()));
}

TEST_CASE("HOSTILE: A FORGED CHOICE FROM THE WRONG PARTICIPANT IS REFUSED -- and this is the "
          "wall three previous projects could not build") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "holiday.mov");
    p.pump(4);
    const std::string menu = p.book().menu_of["t1"];
    REQUIRE_FALSE(menu.empty());

    // The rogue is handed EVERYTHING a bus-watcher could have collected: the
    // ticket, the menu identity, a valid option label, and the correlation. None
    // of it is a secret. The only thing it does not have is being the requester,
    // and that is the one thing no weave can manufacture.
    p.rogue_does(marathon::import_testing::ForgeChoice{"t1", menu, "prores-1080",
                                                       static_cast<std::int64_t>(c)});
    p.pump(6);

    // The importer found no conversation between ITSELF AND THAT SENDER. Not a
    // heuristic, not a payload field — an equality against the bus-stamped
    // sender, available because the counterparty here is a specific weave rather
    // than whoever currently holds an office.
    CHECK_FALSE_MESSAGE(heard(p.book(), {"resolved t1", "prores"}), transcript(p.book()));

    // ...and the real requester's conversation is untouched.
    p.choose("t1", "h264-720", c);
    p.pump(20);
    CHECK_MESSAGE(heard(p.book(), {"receipt t1", "1280x720"}), transcript(p.book()));
}

// ---- 5. replacement: the third distinct answer ------------------------------

TEST_CASE("THE REQUIRED CASE: replaced AFTER the menu and BEFORE the choice, the conversation is "
          "REOPENED") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "holiday.mov");
    p.pump(4);
    const std::string first_menu = p.book().menu_of["t1"];
    REQUIRE_FALSE(first_menu.empty());
    REQUIRE(p.book().menus_attested == 1);

    // 1. Ask the live incumbent what conversations it is in.
    p.describe_conversations();
    p.pump(4);
    REQUIRE_MESSAGE(p.desk().described_arrived, desk_notes(p.desk()));
    REQUIRE_MESSAGE(p.desk().described.size() == 1, desk_notes(p.desk()));
    CHECK(p.desk().described[0].ticket == "t1");
    CHECK(p.desk().described[0].file == "holiday.mov");
    CHECK(p.desk().described[0].resolved_to.empty());

    // 2. Replace, through the handle and nothing else.
    REQUIRE(begin_upgrade(p, "import-pipeline-v2").ok);
    std::vector<std::string>* seen = p.watch(p.upgrade().candidate());
    REQUIRE(p.upgrade()
                .ask(imp::PrepareImporter{p.desk().described, false, p.desk().next_menu})
                .ok);
    p.pump(4);
    REQUIRE_MESSAGE(p.desk().offers.size() == 1, desk_notes(p.desk()));
    CHECK(p.desk().offers[0].ok);
    REQUIRE(p.upgrade().state() == loom::TxnState::Ready);

    const std::size_t outside_the_world = seen->size();
    REQUIRE(p.upgrade().commit(17).ok);
    CHECK(p.upgrade().state() == loom::TxnState::AdmissionPending);
    p.pump(10);
    const std::optional<loom::TxnOutcome> outcome = p.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Committed);
    REQUIRE(seen->size() > outside_the_world);
    CHECK((*seen)[outside_the_world] == std::string(loom::Activated::zen_name));

    // 3. THE THIRD ANSWER. Not continued (the kitchen), not ended (the download
    //    manager): REOPENED. A fresh menu, with a NEW identity, sent as an
    //    ORDINARY message because the answer right died with the life that
    //    earned it.
    const std::string second_menu = p.book().menu_of["t1"];
    CHECK_MESSAGE(second_menu != first_menu, "the menu identity crossed the replacement");
    CHECK(p.book().menus_attested == 1);   // still just the first one
    CHECK(p.book().menus_unattested == 1); // and the re-offer, honestly unattested

    // 4. THE MENU IDENTITY IS WHAT MAKES THE RACE SAFE. A requester that chose
    //    against the dead menu is refused and told which menu is open.
    p.choose("t1", "h264-720", c, first_menu);
    p.pump(4);
    CHECK_MESSAGE(heard(p.book(), {"choice refused t1", "is closed",
                                   "'" + second_menu + "'"}),
                  transcript(p.book()));

    // 5. ...and the conversation completes against the new menu.
    p.choose("t1", "h264-720", c);
    p.pump(20);
    CHECK_MESSAGE(heard(p.book(), {"receipt t1", "1280x720"}), transcript(p.book()));
    CHECK(p.book().outstanding.empty());
}

TEST_CASE("replaced AFTER the choice was resolved: the DECISION crosses, and the successor just "
          "finishes the work") {
    Pipeline p;
    p.boot();
    const std::uint64_t c = p.ask("t1", "holiday.mov");
    p.pump(4);
    p.choose("t1", "prores-1080", c);
    p.pump(4);
    REQUIRE_MESSAGE(heard(p.book(), {"resolved t1", "prores-1080"}), transcript(p.book()));
    CHECK_FALSE(heard(p.book(), {"receipt t1"}));

    p.describe_conversations();
    p.pump(4);
    REQUIRE(p.desk().described.size() == 1);
    // A RESOLVED CHOICE IS A LABEL, which is a word, which crosses.
    CHECK(p.desk().described[0].resolved_to == "prores-1080");

    REQUIRE(begin_upgrade(p, "import-pipeline-v2").ok);
    REQUIRE(p.upgrade()
                .ask(imp::PrepareImporter{p.desk().described, false, p.desk().next_menu})
                .ok);
    p.pump(4);
    REQUIRE(p.upgrade().state() == loom::TxnState::Ready);
    REQUIRE(p.upgrade().commit(18).ok);
    p.pump(30);

    // No second menu was offered: there was nothing left to ask.
    CHECK(p.book().menus_unattested == 0);
    CHECK_MESSAGE(heard(p.book(), {"receipt t1", "holiday.mov#prores-1080", "1920x1080"}),
                  transcript(p.book()));
    CHECK(p.book().outstanding.empty());
}

TEST_CASE("the incumbent keeps serving throughout a preparation, and never learns of it") {
    Pipeline p;
    p.boot();
    REQUIRE(begin_upgrade(p, "import-pipeline-v2").ok);
    const std::uint64_t c = p.ask("t1", "note.txt");
    p.pump(4);
    p.choose("t1", "utf8", c);
    p.pump(20);
    CHECK_MESSAGE(heard(p.book(), {"receipt t1"}), transcript(p.book()));
    CHECK(p.upgrade().state() == loom::TxnState::Preparing);
}

TEST_CASE("DEFERRED READINESS: the sealed candidate asks the curator before agreeing") {
    Pipeline p;
    p.boot();
    REQUIRE(begin_upgrade(p, "import-pipeline-v2").ok);
    REQUIRE(p.upgrade().ask(imp::PrepareImporter{{}, /*verify_files=*/true}).ok);
    p.pump(1);
    CHECK(p.upgrade().state() == loom::TxnState::Preparing);
    CHECK(p.desk().offers.empty());

    p.pump(8);
    CHECK_MESSAGE(p.upgrade().state() == loom::TxnState::Ready, desk_notes(p.desk()));
    REQUIRE(p.desk().offers.size() == 1);
    CHECK(p.desk().offers[0].ok);
}

TEST_CASE("AUTHENTIC REFUSAL: a candidate that ships a different catalogue says no") {
    Pipeline p;
    p.boot();
    REQUIRE(begin_upgrade(p, "import-pipeline-narrow").ok);
    REQUIRE(p.upgrade().ask(imp::PrepareImporter{{}, /*verify_files=*/true}).ok);
    p.pump(10);

    REQUIRE_MESSAGE(p.desk().offers.size() == 1, desk_notes(p.desk()));
    CHECK(p.desk().offers[0].ok); // the OFFER succeeded; the ANSWER was "no"
    CHECK(p.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = p.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);

    // The incumbent never learned any of it.
    const std::uint64_t c = p.ask("s1", "note.txt");
    p.pump(4);
    p.choose("s1", "utf8", c);
    p.pump(20);
    CHECK_MESSAGE(heard(p.book(), {"receipt s1"}), transcript(p.book()));
}

TEST_CASE("a candidate refuses a conversation about a file it cannot read") {
    Pipeline p;
    p.boot();
    REQUIRE(begin_upgrade(p, "import-pipeline-v2").ok);
    imp::PendingImport impossible;
    impossible.ticket = "x1";
    impossible.requester = "9";
    impossible.correlation = 1;
    impossible.file = "a-file-nobody-ships.raw";
    REQUIRE(p.upgrade().ask(imp::PrepareImporter{{impossible}, /*verify_files=*/true}).ok);
    p.pump(8);

    CHECK(p.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = p.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);
}

TEST_CASE("a candidate refuses more conversations than an honest predecessor could have held") {
    Pipeline p;
    p.boot();
    REQUIRE(begin_upgrade(p, "import-pipeline-v2").ok);

    std::vector<imp::PendingImport> too_many;
    for (std::size_t i = 0; i <= imp::kMaxAdoptedConversations; ++i) {
        imp::PendingImport c;
        c.ticket = "o" + std::to_string(i);
        c.requester = "7";
        c.correlation = static_cast<std::int64_t>(i);
        c.file = "note.txt";
        too_many.push_back(c);
    }
    REQUIRE(p.upgrade().ask(imp::PrepareImporter{too_many, false, 1}).ok);
    p.pump(8);

    CHECK(p.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = p.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);
}

TEST_CASE("ABORTED OUTCOME: the curator changes its mind and the world is as it was") {
    Pipeline p;
    p.boot();
    const loom::WeaveId incumbent = p.bus().role_holder(imp::kImporterRole);
    REQUIRE(begin_upgrade(p, "import-pipeline-v2").ok);
    REQUIRE(p.upgrade().ask(imp::PrepareImporter{{}, false}).ok);
    p.pump(4);
    REQUIRE(p.upgrade().state() == loom::TxnState::Ready);

    REQUIRE(p.upgrade().abort().ok);
    const std::optional<loom::TxnOutcome> outcome = p.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::ExplicitAbort);
    CHECK(p.bus().role_holder(imp::kImporterRole) == incumbent);
}

TEST_CASE("EXACT ERROR INSPECTION: refusals keep the substrate's own words") {
    Pipeline p;
    p.boot();

    SUBCASE("nobody holds the role") {
        const auto r = p.new_upgrade().start({
            .operator_id = p.curator(),
            .coordinator = p.curator(),
            .role = "import.nobody",
            .candidate_name = "import-pipeline-v2",
            .candidate_path = marathon::import_testing::weave_path("import-pipeline-v2"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::NoRoleHolder);
    }

    SUBCASE("the artifact refuses to load") {
        const auto r = begin_upgrade(p, "import-pipeline-imaginary");
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::CandidateLoad);
        CHECK_MESSAGE(r.error.find("import-pipeline-imaginary") != std::string::npos, r.error);
    }

    SUBCASE("two replacements of the same incumbent: IncumbentBusy, atomically") {
        REQUIRE(begin_upgrade(p, "import-pipeline-v2").ok);
        loom::PreparedReplacement second(p.bus(), p.kernel());
        const auto r = second.start({
            .operator_id = p.curator(),
            .coordinator = p.curator(),
            .role = imp::kImporterRole,
            .candidate_name = "import-pipeline-narrow",
            .candidate_path = marathon::import_testing::weave_path("import-pipeline-narrow"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.begin_reason == loom::TxnReason::IncumbentBusy);
        CHECK_FALSE(r.cleanup_failed);
    }
}

TEST_CASE("A FORGED READINESS CANNOT MAKE A TRANSACTION READY") {
    Pipeline p;
    p.boot();
    REQUIRE(begin_upgrade(p, "import-pipeline-v2").ok);
    p.rogue_does(marathon::import_testing::ForgeImporterReady{});
    p.pump(6);

    REQUIRE_MESSAGE(p.desk().offers.size() == 1, desk_notes(p.desk()));
    CHECK_FALSE(p.desk().offers[0].ok);
    CHECK(p.desk().offers[0].why == loom::TxnReason::InvalidReadiness);
    CHECK(p.upgrade().state() == loom::TxnState::Preparing);
}

TEST_CASE("an offer with no transaction in flight is a nothing, not a crash") {
    Pipeline p;
    p.boot();
    p.forget_upgrade();
    p.rogue_does(marathon::import_testing::ForgeImporterReady{});
    p.pump(4);
    CHECK(p.desk().offered_without_handle == 1);
    CHECK(p.desk().offers.empty());
}
