// Replay B — the build farm. Both shapes, or the design is incomplete:
// directed office truth (JobDone) AND published office truth (WorkerOpen).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "vocabulary.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace loom;

namespace {

const char* kWorkerRole = "farm.worker.a";

/// A strict receiver: records every delivery of its shapes with the office
/// verdict. Serves as the dispatcher (JobDone) and as observers (WorkerOpen).
class StrictReceiver : public Weave {
public:
    struct Seen {
        std::string schema;
        bool office = false;
    };
    std::vector<Seen> heard;
    std::int64_t accepted = 0;
    std::int64_t rejected = 0;

    std::vector<std::shared_ptr<const Schema>> accepted_schemas() const override {
        return {schema_of<farm::JobDone>(), schema_of<farm::WorkerOpen>()};
    }
    void handle(const Message& in, Bus& bus) override {
        Mail mail(bus, in, self_);
        const bool office = mail.authored_from_role(kWorkerRole);
        heard.push_back(Seen{std::string(in.payload.schema().name()), office});
        if (office) {
            ++accepted;
        } else {
            ++rejected;
        }
    }
    Value snapshot() const override {
        Value v(counter_schema());
        v.set("count", Cell::integer(accepted));
        return v;
    }
    void revive(const Value& v) override { accepted = v.get("count")->as_int(); }
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

StrictReceiver* mount_receiver(Switchboard& bus, const std::string& role, WeaveId& id_out) {
    auto r = std::make_unique<StrictReceiver>();
    StrictReceiver* raw = r.get();
    id_out = role.empty() ? bus.register_weave(std::move(r), Grant{}.allow_any())
                          : bus.register_weave(std::move(r), Grant{}.allow_any(), role);
    raw->set_self(id_out);
    return raw;
}

} // namespace

TEST_SUITE("farm-replay") {

TEST_CASE("real worker office statements are accepted — directed AND published — and the rogue's "
          "same-shaped statements verify as nothing") {
    Switchboard bus;
    Kernel kernel(bus);
    WeaveId dispatcher_id{};
    StrictReceiver* dispatcher = mount_receiver(bus, "farm.dispatcher", dispatcher_id);
    WeaveId observer_id{};
    StrictReceiver* observer = mount_receiver(bus, "", observer_id);

    LoadResult worker = kernel.load("worker", FARM_SO_WORKER, kWorkerRole);
    REQUIRE_MESSAGE(worker.ok, worker.error);
    LoadResult rogue = kernel.load("rogue", FARM_SO_WORKER); // same artifact, no office
    REQUIRE_MESSAGE(rogue.ok, rogue.error);

    std::size_t authorship_denied = 0;
    bus.add_observer([&](const BusEvent& ev) {
        if (ev.kind == EventKind::Refused &&
            ev.refusal.reason == RefusalReason::RoleAuthorshipDenied) {
            ++authorship_denied;
        }
    });

    // The real worker's office statements.
    farm::RunJob job;
    job.job = 7;
    bus.send(worker.id, Message(to_value(job)));
    bus.pump();

    // JobDone reached the dispatcher OFFICE, authored as the WORKER office —
    // two roles, preserved separately, neither mistaken for the other.
    REQUIRE(dispatcher->heard.size() >= 1);
    CHECK(dispatcher->heard[0].schema == "JobDone");
    CHECK(dispatcher->heard[0].office);
    // WorkerOpen reached every listener as verifiable office speech. (The
    // dispatcher hears the publication too — it accepts the shape.)
    REQUIRE(observer->heard.size() == 1);
    CHECK(observer->heard[0].schema == "WorkerOpen");
    CHECK(observer->heard[0].office);
    CHECK(dispatcher->accepted == 2); // its JobDone + its WorkerOpen copy

    // The rogue: its office attempts refuse at the seam (nothing arrives);
    // its personal same-shaped statements arrive and verify as NOTHING.
    farm::RunJob forged;
    forged.job = 8;
    bus.send(rogue.id, Message(to_value(forged)));
    bus.pump();
    CHECK(authorship_denied == 2); // the directed attempt and the publication
    CHECK(dispatcher->heard.size() == 2);
    CHECK(observer->heard.size() == 1); // no forged WorkerOpen fanned out

    forged.personal = true;
    bus.send(rogue.id, Message(to_value(forged)));
    bus.pump();
    REQUIRE(dispatcher->heard.size() == 4); // personal JobDone + personal WorkerOpen
    CHECK_FALSE(dispatcher->heard[2].office);
    CHECK_FALSE(dispatcher->heard[3].office);
    REQUIRE(observer->heard.size() == 2);
    CHECK_FALSE(observer->heard[1].office); // the announcement that is NOT evidence
    CHECK(dispatcher->rejected == 2);
    CHECK(observer->rejected == 1);
    // A healthy build survives: the strict farm acted only on office truth.
    CHECK(dispatcher->accepted == 2);
    CHECK(observer->accepted == 1);
}

} // TEST_SUITE
