#ifndef MARATHON_DOWNLOADS_HARNESS_HPP
#define MARATHON_DOWNLOADS_HARNESS_HPP

// The download manager's host: a REAL host running REAL .so weaves through the
// REAL kernel and REAL prepared replacements, with exactly one substitution.
//
// THE ONE SUBSTITUTION, LABELLED. The Timer service loaded here is
// `zengine-timer-virtual.so` — the Timer package's own suite artifact, the
// shipped service over a clock whose nap books the requested duration and
// returns instead of sleeping. Every byte moved in this package therefore moves
// on an exact integer nobody waited for.
//
// WHAT THIS FIXTURE HAS THAT THE KITCHEN'S DOES NOT, and it is here for one
// measurement: a BYSTANDER. `Switchboard::kMaxDeferredAnswers` is 64 and it
// belongs to one LOOM, not to one weave. A service that holds an answer right
// for the duration of a long operation is spending a globally shared resource,
// and the only way to show that honestly is to have somebody entirely unrelated
// try to hold a conversation at the same time and fail.

#include "client.hpp"
#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include <zen/host/prepared_replacement.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace marathon::dl_testing {

namespace dl = marathon::downloads;
namespace timer = zengine::timer;

#ifndef MARATHON_DOWNLOADS_RUNTIME_DIR
#error "MARATHON_DOWNLOADS_RUNTIME_DIR must be defined by the build"
#endif

inline std::string weave_path(const std::string& stem) {
    return std::string(MARATHON_DOWNLOADS_RUNTIME_DIR) + "/" + stem + ".so";
}

// ---- the operator: the one weave allowed to speak to the steward ------------

struct OperatorLog {
    std::map<std::uint64_t, std::string> pending;
    std::vector<std::string> answers;
    std::uint64_t next_correlation = 1;
};

struct OperatorState {
    std::int64_t answers = 0;
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

class Operator : public loom::WeaveBase<Operator, OperatorState,
                                        loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                        loom::Emit<loom::LoadWeave, loom::ListLoaded>> {
public:
    explicit Operator(OperatorLog& log) : log_(&log) {}
    void on(const loom::Result& r, loom::Mail& mail) { record(mail, "-> " + r.value); }
    void on(const loom::Ack&, loom::Mail& mail) { record(mail, "-> done"); }
    void on(const loom::Refused& r, loom::Mail& mail) { record(mail, "-> refused: " + r.reason); }

private:
    void record(loom::Mail& mail, const std::string& outcome) {
        ++state_.answers;
        const auto it = log_->pending.find(mail.correlation());
        if (it == log_->pending.end()) {
            log_->answers.push_back("(unsolicited) " + outcome);
            return;
        }
        log_->answers.push_back(it->second + " " + outcome);
        log_->pending.erase(it);
    }
    OperatorLog* log_;
};

// ---- ops: the coordinator of a replacement ---------------------------------

/// Everything the operations desk learned, plus the pointer that ties an
/// ordinary weave to a host-only handle.
///
/// THE SAME SHAPE THE KITCHEN NEEDED, arrived at independently: the handle is
/// the host's (it requires `Switchboard&`), the offer must happen inside the
/// coordinator's delivery (only a weave has one), so every prepared replacement
/// grows a raw pointer across that boundary. Two projects, two identical
/// pointers. Recorded in FRICTION.md as F2's second sighting.
struct OpsDesk {
    loom::PreparedReplacement* upgrade = nullptr;
    std::vector<loom::TxnResult> offers;
    std::vector<dl::Obligation> described;
    std::vector<std::string> notes;
    std::int64_t catalogue_answer = static_cast<std::int64_t>(dl::kCatalogueSize);
    std::int64_t offered_without_handle = 0;
    std::int64_t unattested = 0;
    bool described_arrived = false;
};

struct OpsState {
    std::int64_t descriptions = 0;
    std::int64_t ready = 0;
    std::int64_t not_ready = 0;
    std::int64_t consulted = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(OpsState, 1, ZEN_FIELD(descriptions), ZEN_FIELD(ready), ZEN_FIELD(not_ready),
              ZEN_FIELD(consulted));
};

class Ops : public loom::WeaveBase<Ops, OpsState,
                                   loom::Accept<dl::ObligationsDescribed, dl::ServiceReady,
                                                dl::ServiceNotReady, dl::AskCatalogueSize>,
                                   loom::Emit<dl::DescribeObligations, dl::PrepareService,
                                              dl::CatalogueSize>> {
public:
    explicit Ops(OpsDesk& desk) : desk_(&desk) {}

    void on(const dl::ObligationsDescribed& d, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ++desk_->unattested;
            return;
        }
        ++state_.descriptions;
        desk_->described = d.open;
        desk_->described_arrived = true;
        desk_->notes.push_back("the service still owes " + std::to_string(d.open.size()) +
                               " client(s)");
    }

    void on(const dl::ServiceReady& r, loom::Mail&) {
        ++state_.ready;
        desk_->notes.push_back("candidate says READY (took " +
                               std::to_string(r.obligations_taken) + " obligation(s))");
        offer(loom::PreparationAnswer::Ready);
    }

    void on(const dl::ServiceNotReady& r, loom::Mail&) {
        ++state_.not_ready;
        desk_->notes.push_back("candidate says NOT READY: " + r.reason);
        offer(loom::PreparationAnswer::Refused);
    }

    /// A sealed candidate asked the one party it may speak to.
    void on(const dl::AskCatalogueSize&, loom::Mail& mail) {
        ++state_.consulted;
        (void)mail.answer(dl::CatalogueSize{desk_->catalogue_answer});
    }

private:
    void offer(loom::PreparationAnswer answer) {
        if (desk_->upgrade == nullptr) {
            ++desk_->offered_without_handle;
            return;
        }
        desk_->offers.push_back(desk_->upgrade->offer_current_answer(answer));
    }
    OpsDesk* desk_;
};

// ---- the bystander: proof that the answer capacity is one LOOM's ------------

struct AskBystander {
    ZEN_SHAPE(AskBystander, 1);
};

struct BystanderState {
    std::int64_t asked = 0;
    std::int64_t could_defer = 0;
    std::int64_t could_not_defer = 0;
    ZEN_SHAPE(BystanderState, 1, ZEN_FIELD(asked), ZEN_FIELD(could_defer),
              ZEN_FIELD(could_not_defer));
};

/// A weave with nothing whatever to do with downloads. It is asked a question
/// and tries to take the answer away — which is the ordinary thing any service
/// does when it cannot answer inside one delivery.
///
/// If a download service holding sixty-four answer rights makes THIS fail, then
/// "hold the answer for the whole operation" is not a local design choice: it is
/// a service quietly spending the whole Loom's conversation budget.
class Bystander : public loom::WeaveBase<Bystander, BystanderState,
                                         loom::Accept<AskBystander>, loom::Emit<loom::Ack>> {
public:
    Bystander(std::int64_t& asked, std::int64_t& denied) : asked_(&asked), denied_(&denied) {}

    void on(const AskBystander&, loom::Mail& mail) {
        ++state_.asked;
        ++*asked_; // so a test can tell "not denied" from "never ran"
        loom::DeferredAnswer right = mail.defer_answer();
        if (!right.valid()) {
            ++state_.could_not_defer;
            ++*denied_;
            return;
        }
        ++state_.could_defer;
        // Give it straight back: this weave is measuring availability, not
        // hoarding. A released right returns its slot immediately.
        loom::release_deferred(right, mail);
    }

private:
    std::int64_t* asked_;
    std::int64_t* denied_;
};

// ---- a rogue: the threat model, as one ordinary weave -----------------------

struct RogueState {
    std::int64_t sent = 0;
    ZEN_SHAPE(RogueState, 1, ZEN_FIELD(sent));
};

/// "Tell that client its download finished." The forgery this domain invites: a
/// terminal message carries no attestation in the acknowledge-at-once build, so
/// a weave holding an ordinary grant for the shape and a guessed correlation can
/// end somebody else's operation.
struct ForgeCompleted {
    std::int64_t client = 0;
    std::string ticket;
    std::int64_t correlation = 0;
    std::int64_t bytes = 0;
    ZEN_SHAPE(ForgeCompleted, 1, ZEN_FIELD(client), ZEN_FIELD(ticket), ZEN_FIELD(correlation),
              ZEN_FIELD(bytes));
};

/// "Tell the operations desk the candidate is ready." The forgery aimed at the
/// new ceremony.
struct ForgeServiceReady {
    ZEN_SHAPE(ForgeServiceReady, 1);
};

class Rogue : public loom::WeaveBase<Rogue, RogueState,
                                     loom::Accept<ForgeCompleted, ForgeServiceReady>,
                                     loom::Emit<dl::DownloadCompleted, dl::ServiceReady>> {
public:
    explicit Rogue(loom::WeaveId ops) : ops_(ops) {}

    void on(const ForgeCompleted& f, loom::Mail& mail) {
        ++state_.sent;
        mail.send(loom::WeaveId{static_cast<std::uint64_t>(f.client)},
                  dl::DownloadCompleted{f.ticket, f.bytes, 0},
                  static_cast<std::uint64_t>(f.correlation));
    }

    void on(const ForgeServiceReady&, loom::Mail& mail) {
        ++state_.sent;
        mail.send(ops_, dl::ServiceReady{0});
    }

private:
    loom::WeaveId ops_;
};

// ---- the inspector ---------------------------------------------------------

struct InspectorState {
    std::int64_t results = 0;
    std::int64_t ignored = 0;
    ZEN_SHAPE(InspectorState, 1, ZEN_FIELD(results), ZEN_FIELD(ignored));
};

struct AskStatus {
    ZEN_SHAPE(AskStatus, 1);
};

class Inspector : public loom::WeaveBase<Inspector, InspectorState,
                                         loom::Accept<AskStatus, loom::Result>,
                                         loom::Emit<dl::ServiceStatus>> {
public:
    explicit Inspector(std::vector<std::string>& seen) : seen_(&seen) {}
    void on(const AskStatus&, loom::Mail& mail) {
        mail.send_to_role(dl::kServiceRole, dl::ServiceStatus{});
    }
    void on(const loom::Result& r, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ++state_.ignored;
            return;
        }
        ++state_.results;
        seen_->push_back(r.value);
    }

private:
    std::vector<std::string>* seen_;
};

// ---- the fixture ------------------------------------------------------------

class Downloads {
public:
    Downloads() {
        control_ = loom::mount_control(kernel_, bus_);
        manager_ = loom::mount_manager(control_, bus_);

        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager_);
        reach.allow(loom::ListLoaded::zen_name, loom::ListLoaded::zen_version, manager_);
        op_ = loom::mount_granted<Operator>(bus_, std::move(reach), oplog_);

        ops_ = loom::mount<Ops>(bus_, desk_);
        client_ = loom::mount<dl::Client>(bus_, ledger_);
        second_client_ = loom::mount<dl::Client>(bus_, second_ledger_);
        // ⚠ THE GRANT IS WRITTEN OUT, and Night One's friction 5 is why. `mount()`
        // derives a grant from the weave's declared `Emit<...>`, which is the
        // weave's honest outbound silhouette — and `AskBystander` is not part of
        // it: it is the HARNESS poking this weave into speaking, not something
        // this weave emits. Mounting it with the derived grant made every
        // `send_as(bystander_, ...)` CapabilityDenied at delivery, where no
        // participant could see it, and the case failed as "the bystander was
        // never denied a slot" rather than as "the bystander never ran".
        // SECOND SIGHTING of that exact failure, in a second project.
        loom::Grant bystander_grant;
        bystander_grant.allow_to_any(AskBystander::zen_name, AskBystander::zen_version);
        bystander_ = loom::mount_granted<Bystander>(bus_, std::move(bystander_grant),
                                                    bystander_asked_, bystander_denied_);

        loom::Grant rogue_grant;
        rogue_grant.allow_to_any(ForgeCompleted::zen_name, ForgeCompleted::zen_version);
        rogue_grant.allow_to_any(ForgeServiceReady::zen_name, ForgeServiceReady::zen_version);
        rogue_grant.allow_to_any(dl::DownloadCompleted::zen_name,
                                 dl::DownloadCompleted::zen_version);
        rogue_grant.allow_to_any(dl::ServiceReady::zen_name, dl::ServiceReady::zen_version);
        rogue_ = loom::mount_granted<Rogue>(bus_, std::move(rogue_grant), ops_);

        loom::Grant inspector_grant;
        inspector_grant.allow_to_any(AskStatus::zen_name, AskStatus::zen_version);
        inspector_grant.allow_to_role(dl::ServiceStatus::zen_name, dl::ServiceStatus::zen_version,
                                      dl::kServiceRole);
        inspector_ = loom::mount_granted<Inspector>(bus_, std::move(inspector_grant), status_);

        if (const char* on = std::getenv("MARATHON_TRACE"); on != nullptr && *on == '1') {
            bus_.add_observer([](const loom::BusEvent& ev) {
                const char* kind = ev.kind == loom::EventKind::Delivered  ? "deliver"
                                   : ev.kind == loom::EventKind::Refused  ? "REFUSED"
                                   : ev.kind == loom::EventKind::Died     ? "died"
                                                                          : "revived";
                std::fprintf(stderr, "[trace] %-7s %s v%u  %llu -> %llu  %s\n", kind,
                             ev.schema_name.c_str(), ev.schema_version,
                             static_cast<unsigned long long>(ev.sender.value),
                             static_cast<unsigned long long>(ev.target.value),
                             ev.kind == loom::EventKind::Refused ? ev.refusal.message().c_str()
                                                                 : "");
            });
        }

        bus_.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered ||
                ev.schema_name != timer::Drive::zen_name) {
                return;
            }
            ++beats_;
            if (budget_ > 0 && --budget_ == 0) {
                bus_.stop();
            }
        });
    }

    loom::Switchboard& bus() { return bus_; }
    loom::Kernel& kernel() { return kernel_; }
    dl::ClientLedger& ledger() { return ledger_; }
    dl::ClientLedger& second_ledger() { return second_ledger_; }
    OperatorLog& oplog() { return oplog_; }
    OpsDesk& desk() { return desk_; }
    loom::WeaveId ops() const { return ops_; }
    loom::WeaveId client() const { return client_; }
    std::int64_t bystander_denied() const { return bystander_denied_; }
    std::int64_t bystander_asked() const { return bystander_asked_; }
    const std::vector<std::string>& status() const { return status_; }
    std::int64_t beats() const { return beats_; }

    std::vector<std::string>* watch(loom::WeaveId who) {
        watched_.push_back(std::vector<std::string>{});
        std::vector<std::string>* sink = &watched_.back();
        bus_.add_observer([sink, who](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Delivered && ev.target == who) {
                sink->push_back(ev.schema_name);
            }
        });
        return sink;
    }

    // ---- driving ------------------------------------------------------------

    template <class Cmd>
    std::uint64_t command(std::string label, const Cmd& cmd) {
        const std::uint64_t corr = oplog_.next_correlation++;
        oplog_.pending[corr] = std::move(label);
        bus_.send_as(op_, manager_, loom::Message(loom::to_value(cmd), op_, op_, corr));
        return corr;
    }

    std::uint64_t load(const std::string& stem, const std::string& role = "") {
        return command("load " + stem, loom::LoadWeave{stem, weave_path(stem), role});
    }

    /// Start a transfer AS the client: stamped as the client, authorized against
    /// the client's own grant. Returns the correlation everything about this
    /// operation will echo.
    std::uint64_t start(const std::string& ticket, const std::string& source,
                        const std::string& destination = "/tmp/out") {
        const std::uint64_t corr = ledger_.open(ticket);
        ledger_.sources[ticket] = source;
        bus_.send_as_to_role(client_, dl::kServiceRole,
                             loom::Message(loom::to_value(dl::StartDownload{ticket, source,
                                                                            destination}),
                                           client_, client_, corr));
        return corr;
    }

    /// The same, as a SECOND client — because "two clients naming a transfer '1'
    /// must not collide" is a claim, and one client cannot test it.
    std::uint64_t start_second(const std::string& ticket, const std::string& source,
                               const std::string& destination = "/tmp/out2") {
        const std::uint64_t corr = second_ledger_.open(ticket);
        bus_.send_as_to_role(second_client_, dl::kServiceRole,
                             loom::Message(loom::to_value(dl::StartDownload{ticket, source,
                                                                            destination}),
                                           second_client_, second_client_, corr));
        return corr;
    }

    void cancel(const std::string& ticket, std::uint64_t correlation) {
        bus_.send_as_to_role(client_, dl::kServiceRole,
                             loom::Message(loom::to_value(dl::CancelDownload{ticket}), client_,
                                           client_, correlation));
    }

    /// Ask whoever holds the service role right now what it still owes.
    void describe_obligations() {
        desk_.described_arrived = false;
        bus_.send_as_to_role(ops_, dl::kServiceRole,
                             loom::Message(loom::to_value(dl::DescribeObligations{}), ops_, ops_,
                                           0));
    }

    /// Poke the bystander into trying to hold a conversation.
    void ask_bystander() {
        bus_.send_as(bystander_, bystander_,
                     loom::Message(loom::to_value(AskBystander{}), bystander_, bystander_, 0));
    }

    template <class T>
    void rogue_does(const T& order_) {
        bus_.send_as(rogue_, rogue_, loom::Message(loom::to_value(order_), rogue_, rogue_, 0));
    }

    void ask_status() {
        bus_.send_as(inspector_, inspector_,
                     loom::Message(loom::to_value(AskStatus{}), inspector_, inspector_, 0));
    }

    loom::PreparedReplacement& new_upgrade() {
        upgrade_ = loom::PreparedReplacement(bus_, kernel_);
        desk_.upgrade = &upgrade_;
        return upgrade_;
    }
    loom::PreparedReplacement& upgrade() { return upgrade_; }
    void forget_upgrade() { desk_.upgrade = nullptr; }

    /// Boot: a clock and a service.
    void boot(const std::string& service = "download-service") {
        load("zengine-timer-virtual", timer::kTimerRole);
        load(service, dl::kServiceRole);
        pump(20);
    }

    void pump(std::int64_t beats) {
        budget_ = beats;
        bus_.pump();
        budget_ = 0;
    }

private:
    loom::Switchboard bus_;
    loom::Kernel kernel_{bus_};
    loom::WeaveId control_{};
    loom::WeaveId manager_{};
    loom::WeaveId op_{};
    loom::WeaveId ops_{};
    loom::WeaveId client_{};
    loom::WeaveId second_client_{};
    loom::WeaveId bystander_{};
    loom::WeaveId rogue_{};
    loom::WeaveId inspector_{};
    OperatorLog oplog_;
    OpsDesk desk_;
    dl::ClientLedger ledger_;
    dl::ClientLedger second_ledger_;
    std::int64_t bystander_asked_ = 0;
    std::int64_t bystander_denied_ = 0;
    std::vector<std::string> status_;
    loom::PreparedReplacement upgrade_{bus_, kernel_};
    std::deque<std::vector<std::string>> watched_;
    std::int64_t budget_ = 0;
    std::int64_t beats_ = 0;
};

} // namespace marathon::dl_testing

#endif // MARATHON_DOWNLOADS_HARNESS_HPP
