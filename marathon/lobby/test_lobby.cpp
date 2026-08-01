// The lobby's suite — the truth table this project exists to produce.
//
// THE QUESTION: are SPEAKER IDENTITY and OFFICE AUTHORSHIP observably different
// security facts?
//
// The instrument is one policy knob on the player (`require_attestation`) and two
// builds of one matchmaker (push / pull). Every case below is a cell:
//
//                        | push lobby            | pull lobby
//   ---------------------+-----------------------+------------------------------
//   lax player           | joins real matches    | joins real matches
//                        | AND joins forgeries   | AND joins forgeries
//   strict player        | joins NOTHING --      | joins exactly the real ones
//                        | including real        | -- until the office is
//                        | matches               | replaced
//
// and the five-way comparison the brief asks for is a case of its own.
//
// The one substitution is the Timer's CLOCK; this project barely uses it.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "harness.hpp"

#include <string>
#include <vector>

using marathon::lobby_testing::Lobby;
using marathon::lobby_testing::kLax1;
using marathon::lobby_testing::kLax2;
using marathon::lobby_testing::kStrict1;
using marathon::lobby_testing::kStrict2;
namespace lob = marathon::lobby;

namespace {

std::string transcript(const lob::PlayerLog& l) {
    std::string out = "\n  " + l.name + " heard:";
    for (const std::string& line : l.heard) {
        out += "\n    " + line;
    }
    if (l.heard.empty()) {
        out += " (nothing)";
    }
    return out;
}

/// Get two players into the lobby, ready, and (in a pull lobby) seeking.
void two_ready(Lobby& l, std::size_t a, std::size_t b, bool pull) {
    l.join(a);
    l.join(b);
    l.pump(4);
    l.set_ready(a, true);
    l.set_ready(b, true);
    l.pump(4);
    if (pull) {
        l.seek(a);
        l.seek(b);
        l.pump(6);
    }
}

loom::PreparedReplacement::StartResult begin_upgrade(Lobby& l, const std::string& stem,
                                                     std::uint32_t budget = 8) {
    return l.new_upgrade().start({
        .operator_id = l.warden(),
        .coordinator = l.warden(),
        .role = lob::kMatchmakerRole,
        .candidate_name = stem,
        .candidate_path = marathon::lobby_testing::weave_path(stem),
        .budget = budget,
    });
}

} // namespace

// ---- 1. the lobby works at all ---------------------------------------------

TEST_CASE("the lobby opens: every load is answered and nothing is refused") {
    Lobby l;
    l.boot();
    CHECK(l.oplog().pending.empty());
    REQUIRE(l.oplog().answers.size() == 3);
    for (const std::string& a : l.oplog().answers) {
        CHECK(a.find("refused") == std::string::npos);
    }
}

TEST_CASE("membership is the registry's, and it says so authentically") {
    Lobby l;
    l.boot();
    l.join(kLax1);
    l.join(kLax2);
    l.pump(6);
    CHECK_MESSAGE(l.log(kLax1).saw("joined as alice"), transcript(l.log(kLax1)));
    CHECK(l.log(kLax1).in_lobby);
    // Everybody in the lobby learns about everybody, by publication.
    CHECK_MESSAGE(l.log(kLax1).saw("lobby now: [alice,bob]"), transcript(l.log(kLax1)));

    l.ask_lobby();
    l.pump(4);
    REQUIRE(l.status().size() == 1);
    CHECK_MESSAGE(l.status()[0].find("here=2") != std::string::npos, l.status()[0]);
}

TEST_CASE("the registry refuses a name already taken and a player already seated") {
    Lobby l;
    l.boot();
    l.join(kLax1);
    l.pump(4);
    l.join(kLax1);
    l.pump(4);
    // The NAME check answers first, which is the more specific complaint for this
    // input: somebody is sitting there under that name, and it happens to be you.
    CHECK_MESSAGE(l.log(kLax1).saw("somebody is already in this lobby as 'alice'"),
                  transcript(l.log(kLax1)));
}

// ---- 2. PUSH: the natural design, and what it cannot defend -----------------

TEST_CASE("PUSH: a real match reaches the players, and carries no attestation whatsoever") {
    Lobby l;
    l.boot("lobby-matchmaker-push");
    two_ready(l, kLax1, kLax2, /*pull=*/false);
    l.pump(6);

    CHECK_MESSAGE(l.log(kLax1).saw("ACTED ON match match-1"), transcript(l.log(kLax1)));
    CHECK_MESSAGE(l.log(kLax2).saw("ACTED ON match match-1"), transcript(l.log(kLax2)));
    // THE MEASUREMENT. The real service's real statement is unattested.
    CHECK(l.log(kLax1).joined_attested == 0);
    CHECK(l.log(kLax1).joined_unattested == 1);
}

TEST_CASE("PUSH: AN UNRELATED WEAVE CAN CREATE A MATCH, and the players go") {
    Lobby l;
    l.boot("lobby-matchmaker-push");
    l.join(kLax1);
    l.join(kLax2);
    l.pump(6);
    // Nobody is ready, so the office has created nothing.
    REQUIRE_FALSE(l.log(kLax1).saw("ACTED ON"));

    // The rogue holds an ordinary grant for an ordinary shape, and every fact it
    // needs was published: names and addresses are in `LobbyChanged`.
    l.rogue_does(marathon::lobby_testing::ForgeMatch{
        "match-ghost", {"alice", "bob"}, {l.player_weave(kLax1), l.player_weave(kLax2)}});
    l.pump(6);

    CHECK_MESSAGE(l.log(kLax1).saw("ACTED ON match match-ghost"), transcript(l.log(kLax1)));
    CHECK(l.log(kLax1).joined_unattested == 1);
    CHECK_FALSE(l.log(kLax1).in_lobby);
}

TEST_CASE("PUSH: A STRICT PLAYER IS UNUSABLE -- it refuses the REAL match too") {
    Lobby l;
    l.boot("lobby-matchmaker-push");
    two_ready(l, kStrict1, kStrict2, /*pull=*/false);
    l.pump(6);

    // The office really did create a match and really did tell them. There is
    // simply nothing for a careful receiver to check, so a policy of "only act on
    // what Loom vouched for" makes the lobby not work at all.
    CHECK_MESSAGE(l.log(kStrict1).saw("REFUSED match match-1"), transcript(l.log(kStrict1)));
    CHECK(l.log(kStrict1).joined_attested == 0);
    CHECK(l.log(kStrict1).joined_unattested == 0);
    CHECK(l.log(kStrict1).refused_unattested == 1);
}

// ---- 3. PULL: the only construction that attests an office's statement ------

TEST_CASE("PULL: the match a player acts on is Loom's answer to that player's own request") {
    Lobby l;
    l.boot("lobby-matchmaker-pull");
    two_ready(l, kStrict1, kStrict2, /*pull=*/true);
    l.pump(6);

    CHECK_MESSAGE(l.log(kStrict1).saw("ACTED ON match match-1"), transcript(l.log(kStrict1)));
    // THE MEASUREMENT. Attested, and a strict player is therefore usable.
    CHECK(l.log(kStrict1).joined_attested == 1);
    CHECK(l.log(kStrict1).joined_unattested == 0);
    CHECK(l.log(kStrict1).refused_unattested == 0);
    CHECK(l.log(kStrict2).joined_attested == 1);
}

TEST_CASE("PULL: the same forgery is refused, and the wall is Loom's word") {
    Lobby l;
    l.boot("lobby-matchmaker-pull");
    l.join(kStrict1);
    l.join(kStrict2);
    l.pump(6);

    l.rogue_does(marathon::lobby_testing::ForgeMatch{
        "match-ghost", {"carol", "dave"}, {l.player_weave(kStrict1), l.player_weave(kStrict2)}});
    l.pump(6);

    CHECK_MESSAGE(l.log(kStrict1).saw("REFUSED match match-ghost"),
                  transcript(l.log(kStrict1)));
    CHECK(l.log(kStrict1).joined_unattested == 0);
    CHECK(l.log(kStrict1).in_lobby);
}

TEST_CASE("PULL: a lax player is still fooled -- the wall is the RECEIVER's policy") {
    Lobby l;
    l.boot("lobby-matchmaker-pull");
    l.join(kLax1);
    l.join(kLax2);
    l.pump(6);
    l.rogue_does(marathon::lobby_testing::ForgeMatch{
        "match-ghost", {"alice", "bob"}, {l.player_weave(kLax1), l.player_weave(kLax2)}});
    l.pump(6);

    // Worth pinning: the pull style does not make forgeries impossible. It makes
    // them REFUSABLE. A receiver that does not check is exactly as exposed as
    // before, in either lobby.
    CHECK_MESSAGE(l.log(kLax1).saw("ACTED ON match match-ghost"), transcript(l.log(kLax1)));
}

// ---- 4. THE FIVE-WAY COMPARISON the brief asks for -------------------------

TEST_CASE("THE FIVE SPEAKERS: which of them can a receiver tell apart?") {
    // 1. THE CURRENT ROLE HOLDER, speaking as the office.
    // 2. THE SAME WEAVE, speaking personally.
    // 3. THE PREDECESSOR, after being replaced.
    // 4. AN UNRELATED WEAVE.
    // 5. THE SUCCESSOR, after a replacement.

    SUBCASE("push: one and two are the SAME WeaveId and neither is attested") {
        Lobby l;
        l.boot("lobby-matchmaker-push");
        two_ready(l, kLax1, kLax2, /*pull=*/false);
        l.pump(6);
        REQUIRE_MESSAGE(l.log(kLax1).saw("ACTED ON match match-1"), transcript(l.log(kLax1)));

        // (2) The office speaks PERSONALLY: same weave, same shape, no office
        // behind it. It is indistinguishable from (1) by construction.
        l.office_speaks_personally("match-personal", {kLax1, kLax2});
        l.pump(6);
        CHECK_MESSAGE(l.log(kLax1).saw("ACTED ON match match-personal"),
                      transcript(l.log(kLax1)));
        CHECK(l.log(kLax1).joined_unattested == 2);
        CHECK(l.log(kLax1).joined_attested == 0);
    }

    SUBCASE("pull: two is refused while one is accepted -- the ONLY separation available") {
        Lobby l;
        l.boot("lobby-matchmaker-pull");
        two_ready(l, kStrict1, kStrict2, /*pull=*/true);
        l.pump(6);
        REQUIRE(l.log(kStrict1).joined_attested == 1);

        l.office_speaks_personally("match-personal", {kStrict1, kStrict2});
        l.pump(6);
        // THE WHOLE POINT: the same WeaveId, and one statement lands while the
        // other does not, because one answered a question and the other did not.
        CHECK_MESSAGE(l.log(kStrict1).saw("REFUSED match match-personal"),
                      transcript(l.log(kStrict1)));
        CHECK(l.log(kStrict1).joined_attested == 1);
        CHECK(l.log(kStrict1).refused_unattested == 1);
    }

    SUBCASE("the PREDECESSOR cannot speak at all -- and that one IS the substrate's") {
        Lobby l;
        l.boot("lobby-matchmaker-push");
        l.join(kLax1);
        l.join(kLax2);
        l.pump(6);
        const loom::WeaveId predecessor = l.bus().role_holder(lob::kMatchmakerRole);

        REQUIRE(begin_upgrade(l, "lobby-matchmaker-push-v2").ok);
        REQUIRE(l.upgrade()
                    .ask(lob::PrepareMatchmaker{{}, static_cast<std::int64_t>(lob::kMatchSize)})
                    .ok);
        l.pump(4);
        REQUIRE(l.upgrade().state() == loom::TxnState::Ready);
        REQUIRE(l.upgrade().commit(5).ok);
        l.pump(10);
        REQUIRE(l.bus().role_holder(lob::kMatchmakerRole) == l.upgrade().candidate());

        // The predecessor is SEALED by the admission: it is outside the world and
        // may speak only to the coordinator that sealed it. So of the five
        // speakers, this is the one the substrate already handles completely —
        // worth saying, because the other four are the application's problem.
        CHECK(l.bus().sealed(predecessor));
        l.bus().send_as(predecessor, l.player(kLax1),
                        loom::Message(loom::to_value(lob::MatchCreated{
                                          "match-zombie", {"alice", "bob"}, "server-zombie"}),
                                      predecessor, predecessor, 0));
        l.pump(6);
        CHECK_MESSAGE(!l.log(kLax1).saw("match-zombie"), transcript(l.log(kLax1)));
    }
}

// ---- 5. replacement, and the pull style's fatal cost ------------------------

TEST_CASE("THE COST: replacing a PULL matchmaker strands every waiting player, and the honest "
          "successor is refused for the same reason a forger is") {
    Lobby l;
    l.boot("lobby-matchmaker-pull");
    // ONE seeker: not enough for a match, so it is still waiting when the office
    // changes hands.
    l.join(kStrict1);
    l.pump(4);
    l.set_ready(kStrict1, true);
    l.pump(4);
    l.seek(kStrict1);
    l.pump(4);
    REQUIRE_FALSE(l.log(kStrict1).saw("ACTED ON"));

    // The warden asks the office what it owes. The answer names the number that
    // matters: how many of those players were promised an ATTESTED answer.
    l.describe_waiting();
    l.pump(4);
    REQUIRE(l.desk().described_arrived);
    REQUIRE(l.desk().waiting.size() == 1);
    CHECK_MESSAGE(l.desk().held_answer_rights == 1,
                  "the office did not report the rights it was holding");

    REQUIRE(begin_upgrade(l, "lobby-matchmaker-pull-v2").ok);
    std::vector<std::string>* seen = l.watch(l.upgrade().candidate());
    REQUIRE(l.upgrade()
                .ask(lob::PrepareMatchmaker{l.desk().waiting,
                                            static_cast<std::int64_t>(lob::kMatchSize)})
                .ok);
    l.pump(4);
    REQUIRE_MESSAGE(l.upgrade().state() == loom::TxnState::Ready, l.desk().notes.size());
    const std::size_t outside_the_world = seen->size();
    REQUIRE(l.upgrade().commit(9).ok);
    l.pump(10);
    const std::optional<loom::TxnOutcome> outcome = l.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Committed);
    REQUIRE(seen->size() > outside_the_world);
    CHECK((*seen)[outside_the_world] == std::string(loom::Activated::zen_name));

    // A second player arrives and seeks, so the successor CAN form a match.
    l.join(kStrict2);
    l.pump(4);
    l.set_ready(kStrict2, true);
    l.pump(4);
    l.seek(kStrict2);
    l.pump(10);

    // THE FINDING. The successor is honest, holds the office, and formed a real
    // match. The player it INHERITED gets an unattested statement — the right
    // that would have attested it belonged to a life that has ended — and a
    // strict player refuses it. The one that asked the SUCCESSOR gets an attested
    // one and acts on it.
    CHECK_MESSAGE(l.log(kStrict1).saw("REFUSED match"), transcript(l.log(kStrict1)));
    CHECK(l.log(kStrict1).joined_attested == 0);
    CHECK(l.log(kStrict2).joined_attested == 1);
}

TEST_CASE("a PUSH matchmaker replaces cleanly, because it never promised anything it could not "
          "hand over") {
    Lobby l;
    l.boot("lobby-matchmaker-push");
    l.join(kLax1);
    l.pump(4);
    l.set_ready(kLax1, true);
    l.pump(4);

    REQUIRE(begin_upgrade(l, "lobby-matchmaker-push-v2").ok);
    REQUIRE(l.upgrade()
                .ask(lob::PrepareMatchmaker{{}, static_cast<std::int64_t>(lob::kMatchSize)})
                .ok);
    l.pump(4);
    REQUIRE(l.upgrade().state() == loom::TxnState::Ready);
    REQUIRE(l.upgrade().commit(3).ok);
    l.pump(10);

    // The successor serves: a second ready player completes a match.
    l.join(kLax2);
    l.pump(4);
    l.set_ready(kLax2, true);
    l.pump(10);
    CHECK_MESSAGE(l.log(kLax1).saw("ACTED ON match"), transcript(l.log(kLax1)));
    CHECK_MESSAGE(l.log(kLax2).saw("ACTED ON match"), transcript(l.log(kLax2)));

    // ...and the trade is stated plainly: nothing was stranded because nothing
    // was ever attested.
    CHECK(l.log(kLax1).joined_attested == 0);
    CHECK(l.log(kLax1).joined_unattested == 1);
}

TEST_CASE("AUTHENTIC REFUSAL: a candidate that plays a different game size says no") {
    Lobby l;
    l.boot("lobby-matchmaker-push");
    l.desk().match_size = 5;
    REQUIRE(begin_upgrade(l, "lobby-matchmaker-push-v2").ok);
    REQUIRE(l.upgrade().ask(lob::PrepareMatchmaker{{}, 5}).ok);
    l.pump(6);

    REQUIRE(l.desk().offers.size() == 1);
    CHECK(l.desk().offers[0].ok);
    CHECK(l.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = l.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);
}

TEST_CASE("ABORTED OUTCOME: the warden changes its mind and the office is unchanged") {
    Lobby l;
    l.boot("lobby-matchmaker-push");
    const loom::WeaveId incumbent = l.bus().role_holder(lob::kMatchmakerRole);
    REQUIRE(begin_upgrade(l, "lobby-matchmaker-push-v2").ok);
    REQUIRE(l.upgrade()
                .ask(lob::PrepareMatchmaker{{}, static_cast<std::int64_t>(lob::kMatchSize)})
                .ok);
    l.pump(4);
    REQUIRE(l.upgrade().state() == loom::TxnState::Ready);
    REQUIRE(l.upgrade().abort().ok);
    const std::optional<loom::TxnOutcome> outcome = l.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::ExplicitAbort);
    CHECK(l.bus().role_holder(lob::kMatchmakerRole) == incumbent);
}

TEST_CASE("EXACT ERROR INSPECTION: refusals keep the substrate's own words") {
    Lobby l;
    l.boot("lobby-matchmaker-push");

    SUBCASE("nobody holds the role") {
        const auto r = l.new_upgrade().start({
            .operator_id = l.warden(),
            .coordinator = l.warden(),
            .role = "lobby.nobody",
            .candidate_name = "lobby-matchmaker-push-v2",
            .candidate_path = marathon::lobby_testing::weave_path("lobby-matchmaker-push-v2"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::NoRoleHolder);
    }

    SUBCASE("the artifact refuses to load") {
        const auto r = begin_upgrade(l, "lobby-matchmaker-imaginary");
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::CandidateLoad);
    }
}

// ---- 6. the registry has the same problem ----------------------------------

TEST_CASE("THE REGISTRY IS A RECEIVER TOO: a forged match empties a lobby of people still in it") {
    Lobby l;
    l.boot("lobby-matchmaker-push");
    l.join(kStrict1);
    l.join(kStrict2);
    l.pump(6);
    l.ask_lobby();
    l.pump(4);
    REQUIRE(l.status().size() == 1);
    REQUIRE_MESSAGE(l.status()[0].find("here=2") != std::string::npos, l.status()[0]);

    // The strict players refuse the forgery and stay put. The REGISTRY has no
    // such option: it is not a PARTY to any match conversation, so what reaches it
    // is a PUBLICATION -- and a publication can never be attested to anybody, in
    // either matchmaker style. The pull workaround protects the players and does
    // exactly nothing here.
    l.rogue_does(marathon::lobby_testing::ForgeMatch{
        "match-ghost", {"carol", "dave"}, {l.player_weave(kStrict1), l.player_weave(kStrict2)}});
    l.rogue_does(marathon::lobby_testing::ForgeMatchStarted{"match-ghost", {"carol", "dave"}});
    l.pump(6);

    CHECK_MESSAGE(l.log(kStrict1).saw("REFUSED match match-ghost"),
                  transcript(l.log(kStrict1)));
    CHECK(l.log(kStrict1).in_lobby); // the player thinks it is still here...

    l.ask_lobby();
    l.pump(4);
    REQUIRE(l.status().size() == 2);
    // ...and the registry does not. One forged publication, and the two halves of
    // the system now disagree about who is in the room.
    CHECK_MESSAGE(l.status()[1].find("here=0") != std::string::npos, l.status()[1]);
    CHECK_MESSAGE(l.status()[1].find("matched_away=2") != std::string::npos, l.status()[1]);
}
