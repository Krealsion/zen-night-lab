#ifndef NIGHT_LAB_TESTS_HARNESS_HPP
#define NIGHT_LAB_TESTS_HARNESS_HPP

// The suite's kitchen: a REAL host, running REAL .so weaves through the REAL
// kernel, the REAL Weave Manager and REAL graceful swaps — with exactly one
// thing swapped out, and it is not part of the architecture under test.
//
// THE ONE SUBSTITUTION, LABELLED. The Timer service loaded here is
// `zengine-timer-virtual.so` — the Timer package's own suite artifact: the
// shipped `TimerServiceT` over a clock whose nap books the requested duration
// and returns instead of sleeping. Same protocol, same schedule table, same
// letter, same beat chain. It means every deadline in this lab is an exact
// integer nobody waited for, and it means a test that says "twenty sweeps" is
// asserting twenty sweeps rather than hoping the machine was not busy.
//
// THE PUMP LEVER. `Switchboard::pump()` drains the queue and returns; a live
// Timer beat chain never quiesces, so an unbounded pump would never return. The
// harness therefore counts `Drive` deliveries on a tap and stops the bus after a
// budget — the same lever the Timer package's own suite uses. A stopped pump
// PARKS the queue mid-chain: the tail delivers on the next pump, nothing is
// lost, and the beat count is the clock.
//
// NO ROOT SENDS AFTER BOOT. Lifecycle commands go to the Weave Manager from an
// operator weave holding a narrow, target-scoped grant, sent with `send_as` —
// the host holds root but spends a real capability, exactly as Zengine's snake
// host does. Diner orders go out the same way, stamped as the diner and
// authorized against the diner's own grant.

#include "kitchen/diner.hpp"
#include "kitchen/vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace nightlab::testing {

namespace kitchen = nightlab::kitchen;
namespace timer = zengine::timer;

/// Where the weaves live. Set by the build (one runtime directory for the whole
/// lab), so nothing here guesses at a path.
#ifndef NIGHT_LAB_RUNTIME_DIR
#error "NIGHT_LAB_RUNTIME_DIR must be defined by the build"
#endif

inline std::string weave_path(const std::string& stem) {
    return std::string(NIGHT_LAB_RUNTIME_DIR) + "/" + stem + ".so";
}

// ---- the operator: the one weave allowed to speak to the steward ------------

struct OperatorLog {
    std::map<std::uint64_t, std::string> pending; ///< correlation -> label
    std::vector<std::string> answers;             ///< "label -> outcome", in arrival order
    std::uint64_t next_correlation = 1;

    /// The last answer whose label contains `needle`, or "" if none arrived.
    std::string answer_for(const std::string& needle) const {
        std::string found;
        for (const std::string& a : answers) {
            if (a.find(needle) != std::string::npos) {
                found = a;
            }
        }
        return found;
    }
};

struct OperatorState {
    std::int64_t answers = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

/// The host's hand on the steward. It matches every answer against its own
/// outstanding correlations — the standing consumer obligation — and records
/// anything unsolicited as noise rather than acting on it.
class Operator : public loom::WeaveBase<Operator, OperatorState,
                                        loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                        loom::Emit<loom::LoadWeave, loom::SwapWeave,
                                                   loom::ReloadWeave, loom::ListLoaded>> {
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

// ---- a rogue: the whole threat model, as one ordinary weave -----------------

/// It holds nothing but an ordinary grant for shapes anyone may hold, and it
/// speaks them. That is the entire attack surface being measured: an honest API
/// that could not express the attack would make the pin worthless.
struct RogueState {
    std::int64_t sent = 0;
    ZEN_SHAPE(RogueState, 1, ZEN_FIELD(sent));
};

/// "Plate this job, from this station." A test-only order to the rogue, so the
/// forgery is issued by a weave with a stamped identity rather than by a root.
struct ForgePlated {
    std::string job;
    std::string dish;
    std::string station;
    ZEN_SHAPE(ForgePlated, 1, ZEN_FIELD(job), ZEN_FIELD(dish), ZEN_FIELD(station));
};

/// "Say a station is open." The other half of the roster's threat model.
struct ForgeStationOpen {
    std::string station;
    std::string dish;
    ZEN_SHAPE(ForgeStationOpen, 1, ZEN_FIELD(station), ZEN_FIELD(dish));
};

class Rogue : public loom::WeaveBase<Rogue, RogueState,
                                     loom::Accept<ForgePlated, ForgeStationOpen>,
                                     loom::Emit<kitchen::Plated, kitchen::StationOpen>> {
public:
    void on(const ForgePlated& f, loom::Mail& mail) {
        ++state_.sent;
        mail.send_to_role(kitchen::kExpediterRole, kitchen::Plated{f.job, f.dish, f.station});
    }
    void on(const ForgeStationOpen& f, loom::Mail& mail) {
        ++state_.sent;
        kitchen::StationOpen hello;
        hello.station = f.station;
        hello.dishes.push_back(f.dish);
        hello.pass_ms = 20;
        mail.publish(hello);
    }
};

// ---- the inspector: somebody has to ask the diagnostic ----------------------

struct InspectorState {
    std::int64_t results = 0;
    std::int64_t ignored = 0;
    ZEN_SHAPE(InspectorState, 1, ZEN_FIELD(results), ZEN_FIELD(ignored));
};

/// "Ask the kitchen how it is doing." A test-only nudge, so the question is
/// asked by a weave with a stamped identity rather than by a root.
struct AskStatus {
    ZEN_SHAPE(AskStatus, 1);
};

class Inspector : public loom::WeaveBase<Inspector, InspectorState,
                                         loom::Accept<AskStatus, loom::Result>,
                                         loom::Emit<kitchen::KitchenStatus>> {
public:
    explicit Inspector(std::vector<std::string>& seen) : seen_(&seen) {}

    void on(const AskStatus&, loom::Mail& mail) {
        mail.send_to_role(kitchen::kExpediterRole, kitchen::KitchenStatus{});
    }

    /// The consumer obligation, and here BOTH halves are available: the kitchen
    /// answers authentically, so Loom's word is what makes this a reply to our
    /// question rather than a stranger's `zen.Result`.
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

class Kitchen {
public:
    Kitchen() {
        control_ = loom::mount_control(kernel_, bus_);
        manager_ = loom::mount_manager(control_, bus_);

        // The dangerous grant, target-scoped to the steward — never allow_any.
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager_);
        reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager_);
        reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager_);
        reach.allow(loom::ListLoaded::zen_name, loom::ListLoaded::zen_version, manager_);
        op_ = loom::mount_granted<Operator>(bus_, std::move(reach), oplog_);

        diner_ = loom::mount<kitchen::Diner>(bus_, book_);
        // The rogue's grant is written out rather than derived, because the two
        // Forge* shapes are the HARNESS talking to it and have no business in a
        // weave's honest Emit silhouette. Everything it may say about the kitchen
        // — Plated, StationOpen — is a shape any weave may legitimately hold: that
        // is the threat model, not a hole punched for the test.
        loom::Grant rogue_grant;
        rogue_grant.allow_to_any(ForgePlated::zen_name, ForgePlated::zen_version);
        rogue_grant.allow_to_any(ForgeStationOpen::zen_name, ForgeStationOpen::zen_version);
        rogue_grant.allow_to_any(kitchen::Plated::zen_name, kitchen::Plated::zen_version);
        rogue_grant.allow_to_any(kitchen::StationOpen::zen_name, kitchen::StationOpen::zen_version);
        rogue_ = loom::mount_granted<Rogue>(bus_, std::move(rogue_grant));
        // Same shape as the rogue's grant, same reason: `AskStatus` is the
        // harness nudging this weave, not part of the conversation it has with
        // the kitchen, so it is granted explicitly rather than declared as an
        // emission the weave does not really make.
        loom::Grant inspector_grant;
        inspector_grant.allow_to_any(AskStatus::zen_name, AskStatus::zen_version);
        inspector_grant.allow_to_role(kitchen::KitchenStatus::zen_name,
                                      kitchen::KitchenStatus::zen_version,
                                      kitchen::kExpediterRole);
        inspector_ = loom::mount_granted<Inspector>(bus_, std::move(inspector_grant), status_);

        // The lab's own diagnostic: NIGHT_LAB_TRACE=1 prints every delivery and
        // every refusal the bus sees. It exists because the single hardest thing
        // to debug in this experiment was a message that was refused where NO
        // PARTICIPANT COULD SEE IT — the host's tap is the only vantage point
        // from which that is visible at all, which is itself part of the finding.
        if (const char* on = std::getenv("NIGHT_LAB_TRACE"); on != nullptr && *on == '1') {
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

        // The pump lever: count the Timer's own beats and stop on budget.
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
    kitchen::DinerBook& book() { return book_; }
    OperatorLog& oplog() { return oplog_; }
    loom::WeaveId diner() const { return diner_; }
    loom::WeaveId manager() const { return manager_; }
    std::int64_t beats() const { return beats_; }

    // ---- driving ------------------------------------------------------------

    /// Send one lifecycle command AS the operator, with the bookkeeping its
    /// answer will be matched against.
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

    std::uint64_t swap(const std::string& role, const std::string& stem, bool graceful) {
        return command((graceful ? "graceful swap " : "hard swap ") + role + " -> " + stem,
                       loom::SwapWeave{role, stem, weave_path(stem), graceful});
    }

    /// Make a role's holder walk out with nothing to replace it. A swap whose
    /// successor cannot load leaves the role UNHELD — the Weave Manager says so
    /// in its own header, and it is the honest way to make a service vanish
    /// mid-job without reaching around the architecture.
    std::uint64_t evict(const std::string& role) {
        return command("evict " + role,
                       loom::SwapWeave{role, "no-such-weave",
                                       std::string(NIGHT_LAB_RUNTIME_DIR) + "/no-such-weave.so",
                                       /*graceful=*/false});
    }

    /// Place an order AS the diner: stamped as the diner, authorized against the
    /// diner's own grant. Returns the correlation the outcome will echo.
    std::uint64_t order(const std::string& order_id, const std::string& dish,
                        const std::string& prefer, const std::string& fallback) {
        const std::uint64_t corr = book_.open(order_id);
        bus_.send_as_to_role(
            diner_, kitchen::kExpediterRole,
            loom::Message(loom::to_value(kitchen::PlaceOrder{order_id, dish, prefer, fallback}),
                          diner_, diner_, corr));
        return corr;
    }

    template <class T>
    void rogue_does(const T& order_) {
        bus_.send_as(rogue_, rogue_, loom::Message(loom::to_value(order_), rogue_, rogue_, 0));
    }

    /// Ask the kitchen for its tally, as the inspector. The answers land in
    /// `status()` in arrival order.
    void ask_status() {
        bus_.send_as(inspector_, inspector_,
                     loom::Message(loom::to_value(AskStatus{}), inspector_, inspector_, 0));
    }

    const std::vector<std::string>& status() const { return status_; }

    /// Boot a working kitchen: time, routing, front of house, two stations.
    /// Ordered deliberately so nothing depends on load order — every weave
    /// arranges its own timers on its own activation, and anything loaded before
    /// the Timer retries on its TimerReady.
    void boot(const std::string& policy = "kitchen-policy-house") {
        load("zengine-timer-virtual", timer::kTimerRole);
        load(policy, kitchen::kPolicyRole);
        load("kitchen-expediter", kitchen::kExpediterRole);
        load("kitchen-grill", kitchen::station_role("grill"));
        load("kitchen-fryer", kitchen::station_role("fryer"));
        pump(40);
    }

    /// Pump until `beats` more Timer beats have been delivered, or the bus goes
    /// quiet. A quiet bus is a real outcome here, not a hang: it means nothing in
    /// this process will ever speak again.
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
    loom::WeaveId diner_{};
    loom::WeaveId rogue_{};
    loom::WeaveId inspector_{};
    OperatorLog oplog_;
    kitchen::DinerBook book_;
    std::vector<std::string> status_;
    std::int64_t budget_ = 0;
    std::int64_t beats_ = 0;
};

} // namespace nightlab::testing

#endif // NIGHT_LAB_TESTS_HARNESS_HPP
