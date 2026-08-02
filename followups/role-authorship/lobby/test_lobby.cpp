// Replay A — the lobby. The primary acceptance case.
//
// The marathon measured: a strict player under the PULL workaround refuses the
// forger and the honest successor for the same reason, spends Loom-wide answer
// capacity per waiting player, and leaves publications uncoverable. This
// replay measures the same lobby on role authorship: the push stays a push,
// the strict player checks the OFFICE, and the whole pull apparatus is gone.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "vocabulary.hpp"

#include <zen/host/lifecycle_wiring.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace loom;

namespace {

const char* kMatchmakerRole = "lobby.matchmaker";

/// What one delivered MatchCreated looked like to the player.
struct Seen {
    std::string server;
    std::uint64_t sender = 0;
    bool office = false;
};

/// THE STRICT PLAYER — the receiver the whole feature exists for. It acts on a
/// match iff the delivery was deliberately authored by the matchmaker OFFICE.
/// No Switchboard access, no role lookup, no payload field, no registry.
class StrictPlayer : public Weave {
public:
    std::vector<Seen> heard;   ///< every MatchCreated, verdict included
    std::int64_t joined = 0;   ///< acted on
    std::int64_t rejected = 0; ///< refused: personal chatter or a forger

    std::vector<std::shared_ptr<const Schema>> accepted_schemas() const override {
        return {schema_of<lobby::MatchCreated>()};
    }
    void handle(const Message& in, Bus& bus) override {
        Mail mail(bus, in, self_);
        const lobby::MatchCreated match = from_value<lobby::MatchCreated>(in.payload);
        const bool office = mail.authored_from_role(kMatchmakerRole);
        heard.push_back(Seen{match.server, in.sender.value, office});
        if (!office) {
            ++rejected;
            return;
        }
        ++joined;
    }
    Value snapshot() const override {
        Value v(counter_schema());
        v.set("count", Cell::integer(joined));
        return v;
    }
    void revive(const Value& v) override { joined = v.get("count")->as_int(); }
    Value policy() const override {
        Value v(lifecycle_policy_schema());
        v.set("max_reloads", Cell::integer(2));
        v.set("revive_from_last_good", Cell::boolean(true));
        return v;
    }
    void set_self(WeaveId id) { self_ = id; }

private:
    static std::shared_ptr<const Schema> counter_schema() {
        static const auto s = SchemaBuilder("Counter", 1).field("count", Kind::Int).build();
        return s;
    }
    WeaveId self_{};
};

struct Stage {
    Switchboard bus;
    Kernel kernel{bus};
    StrictPlayer* player = nullptr;
    WeaveId player_id{};

    Stage() {
        auto p = std::make_unique<StrictPlayer>();
        player = p.get();
        player_id = bus.register_weave(std::move(p), Grant{}.allow_any());
        player->set_self(player_id);
    }

    void command(WeaveId matchmaker, bool personal) {
        lobby::MakeMatch cmd;
        cmd.player = static_cast<std::int64_t>(player_id.value);
        cmd.personal = personal;
        bus.send(matchmaker, Message(to_value(cmd)));
        bus.pump();
    }
};

} // namespace

TEST_SUITE("lobby-replay") {

TEST_CASE("the strict player joins the office's match, rejects the same holder's chatter, "
          "rejects every rogue, and follows the office through a real replacement") {
    Stage s;
    std::size_t authorship_denied = 0;
    s.bus.add_observer([&](const BusEvent& ev) {
        if (ev.kind == EventKind::Refused &&
            ev.refusal.reason == RefusalReason::RoleAuthorshipDenied) {
            ++authorship_denied;
        }
    });

    // v1 holds the office.
    LoadResult v1 = s.kernel.load("mm", LOBBY_SO_MATCHMAKER_V1, kMatchmakerRole);
    REQUIRE_MESSAGE(v1.ok, v1.error);

    // 1. The legitimate office-authored push: joined.
    s.command(v1.id, /*personal=*/false);
    REQUIRE(s.player->heard.size() == 1);
    CHECK(s.player->heard[0].office);
    CHECK(s.player->heard[0].server == "srv-v1");
    CHECK(s.player->joined == 1);

    // 2. The SAME holder, the SAME shape, spoken personally: rejected. This is
    //    the statement no identity check could ever separate.
    s.command(v1.id, /*personal=*/true);
    REQUIRE(s.player->heard.size() == 2);
    CHECK_FALSE(s.player->heard[1].office);
    CHECK(s.player->heard[1].sender == v1.id.value); // the very same weave
    CHECK(s.player->rejected == 1);

    // 3. An unrelated rogue — the same artifact, loaded WITHOUT the office.
    LoadResult rogue = s.kernel.load("rogue", LOBBY_SO_MATCHMAKER_V1);
    REQUIRE_MESSAGE(rogue.ok, rogue.error);
    //    Its office attempt refuses at the seam; NOTHING arrives.
    s.command(rogue.id, /*personal=*/false);
    CHECK(s.player->heard.size() == 2);
    CHECK(authorship_denied == 1);
    //    Its personal same-shaped forgery arrives — and is rejected.
    s.command(rogue.id, /*personal=*/true);
    REQUIRE(s.player->heard.size() == 3);
    CHECK_FALSE(s.player->heard[2].office);
    CHECK(s.player->rejected == 2);

    // 4. A real replacement: v2 is loaded SEALED, prepared outside the world,
    //    and admitted — the role moves, the incumbent retires sealed.
    auto coordinator = std::make_unique<StrictPlayer>(); // any weave can coordinate
    const WeaveId coord_id = s.bus.register_weave(std::move(coordinator), Grant{}.allow_any());
    LoadResult v2 = s.kernel.load_candidate("mm2", LOBBY_SO_MATCHMAKER_V2, coord_id);
    REQUIRE_MESSAGE(v2.ok, v2.error);
    const AdmitResult admitted = s.bus.admit_candidate(
        v2.id, v1.id, kMatchmakerRole, host_lifecycle_authority(s.bus),
        Message(to_value(loom::Activated{1})), 1);
    REQUIRE(admitted.scheduled);
    s.bus.pump();
    REQUIRE(s.bus.role_holder(kMatchmakerRole) == v2.id);

    // 5. The honest successor's office push: joined — the check the PULL
    //    workaround could never pass, passed with the SAME strict player code.
    s.command(v2.id, /*personal=*/false);
    REQUIRE(s.player->heard.size() == 4);
    CHECK(s.player->heard[3].office);
    CHECK(s.player->heard[3].server == "srv-v2");
    CHECK(s.player->heard[3].sender == v2.id.value);
    CHECK(s.player->heard[3].sender != s.player->heard[0].sender); // identity moved...
    CHECK(s.player->joined == 2);                                  // ...the office did not

    // 6. The retired predecessor's NEW office attempt: refused at authorship.
    //    (Sealed for retirement, it hears only its coordinator — the command
    //    must come from the admission's owner.)
    lobby::MakeMatch cmd;
    cmd.player = static_cast<std::int64_t>(s.player_id.value);
    cmd.personal = false;
    s.bus.send_as(coord_id, v1.id, Message(to_value(cmd)));
    s.bus.pump();
    CHECK(s.player->heard.size() == 4); // nothing arrived
    CHECK(authorship_denied == 2);      // and the refusal is precise, on the tap
}

TEST_CASE("what disappeared with the pull workaround, measured") {
    // The marathon's pull vertical needed: a SeekMatch request shape, a
    // deferred answer HELD PER WAITING PLAYER against the one Loom-wide budget
    // of 64, and a strict player that refused the honest successor. This
    // vertical's numbers, by construction and by assertion:
    Stage s;
    LoadResult v1 = s.kernel.load("mm", LOBBY_SO_MATCHMAKER_V1, kMatchmakerRole);
    REQUIRE_MESSAGE(v1.ok, v1.error);

    // Zero conversations are parked to make the push trustworthy: the player
    // never asks, the matchmaker never defers, and the full deferral budget is
    // free for actual conversations — proven by exhausting nothing after 100
    // strictly-verified pushes.
    for (int i = 0; i < 100; ++i) {
        s.command(v1.id, /*personal=*/false);
    }
    CHECK(s.player->joined == 100);
    CHECK(s.player->rejected == 0);
    // The vocabulary itself shrank: no SeekMatch shape exists in this replay's
    // vocabulary.hpp, and `defer` appears nowhere in matchmaker.cpp — the
    // REPORT records the grep. What replaced all of it is one line at each end.
}

} // TEST_SUITE
