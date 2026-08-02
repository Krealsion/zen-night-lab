// Replay C — the download manager. The architecture consequence, measured:
// can the client verify BOTH halves of a long-lived operation without anybody
// holding the original answer capability for the duration?

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

/// The client: asks once, then verifies each delivery for what it CLAIMS to be.
class Client : public Weave {
public:
    struct Seen {
        std::string schema;
        bool answers_ask = false;
        bool office = false;
    };
    std::vector<Seen> heard;

    std::vector<std::shared_ptr<const Schema>> accepted_schemas() const override {
        return {schema_of<dl::DownloadAccepted>(), schema_of<dl::DownloadDone>()};
    }
    void handle(const Message& in, Bus& bus) override {
        Mail mail(bus, in, self_);
        heard.push_back(Seen{std::string(in.payload.schema().name()), mail.answers_ask(),
                             mail.authored_from_role("download.service")});
    }
    Value snapshot() const override {
        Value v(counter_schema());
        v.set("count", Cell::integer(static_cast<std::int64_t>(heard.size())));
        return v;
    }
    void revive(const Value&) override {}
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

} // namespace

TEST_SUITE("download-replay") {

TEST_CASE("the acceptance is THE answer, the terminal truth is the office's — two different "
          "proofs, both verified, zero capabilities parked") {
    Switchboard bus;
    Kernel kernel(bus);
    auto c = std::make_unique<Client>();
    Client* client = c.get();
    const WeaveId client_id = bus.register_weave(std::move(c), Grant{}.allow_any());
    client->set_self(client_id);

    LoadResult svc = kernel.load("dl", DL_SO_SERVICE, "download.service");
    REQUIRE_MESSAGE(svc.ok, svc.error);

    // The ask — the client's own, so the answer can be bound to it.
    dl::StartDownload ask;
    ask.url = "zen://big-file";
    bus.send_as(client_id, svc.id, Message(to_value(ask)));
    bus.pump();

    // Half one: the acceptance arrived as Loom's word — THIS request was
    // accepted by its respondent. It is NOT office speech; it does not need to
    // be. (The answer door never inherits or invents an office.)
    REQUIRE(client->heard.size() == 1);
    CHECK(client->heard[0].schema == "DownloadAccepted");
    CHECK(client->heard[0].answers_ask);
    CHECK_FALSE(client->heard[0].office);

    // ...time passes; the download runs; NOBODY is holding an answer right.
    // The one-per-operation attestation was already spent, and that is fine.

    bus.send(svc.id, Message(to_value(dl::FinishDownload{})));
    bus.pump();

    // Half two: the terminal truth arrived as the OFFICE's deliberate speech —
    // a different proof, for a different claim. It is NOT an answer; it does
    // not need to be.
    REQUIRE(client->heard.size() == 2);
    CHECK(client->heard[1].schema == "DownloadDone");
    CHECK(client->heard[1].office);
    CHECK_FALSE(client->heard[1].answers_ask);

    // The measurement the replay exists for: the deferred-answer registry was
    // never touched. A hundred concurrent downloads would park a hundred slots
    // of the Loom-wide 64 under the marathon's workaround; here they park zero.
    // (The service source contains no defer; this suite calls none; and the
    // whole ceremony above consumed the ask's ONE answer at acceptance time.)
    // "Which half do you attest?" stopped being a choice: attest both, with
    // the proof each half actually means.
}

} // TEST_SUITE
