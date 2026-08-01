#ifndef MARATHON_KITCHEN_HARNESS_HPP
#define MARATHON_KITCHEN_HARNESS_HPP

// The replay's kitchen: a REAL host, running REAL .so weaves through the REAL
// kernel, the REAL Weave Manager, REAL graceful swaps and REAL prepared
// replacements — with exactly one thing swapped out, and it is not part of the
// architecture under test.
//
// THE ONE SUBSTITUTION, LABELLED. The Timer service loaded here is
// `zengine-timer-virtual.so` — the Timer package's own suite artifact: the
// shipped `TimerServiceT` over a clock whose nap books the requested duration
// and returns instead of sleeping. Same protocol, same schedule table, same
// letter, same beat chain. It means every deadline in this lab is an exact
// integer nobody waited for.
//
// THE PUMP LEVER. `Switchboard::pump()` drains the queue and returns; a live
// Timer beat chain never quiesces, so an unbounded pump would never return. The
// harness therefore counts `Drive` deliveries on a tap and stops the bus after a
// budget. A stopped pump PARKS the queue mid-chain: the tail delivers on the
// next pump, nothing is lost, and the beat count is the clock.
//
// NO ROOT SENDS AFTER BOOT. Lifecycle commands go to the Weave Manager from an
// operator weave holding a narrow, target-scoped grant, sent with `send_as` —
// the host holds root but spends a real capability. Diner orders go out the same
// way, stamped as the diner.
//
// ---- WHAT IS NEW IN THE REPLAY ---------------------------------------------
//
// `loom::PreparedReplacement` is a HOST handle: it needs the `Switchboard&`
// itself, which no weave can hold. So the replacement is driven from here, and
// the OWNER — an ordinary weave — is the coordinator that actually talks to the
// candidate. The two are wired by a plain pointer on `OwnerDesk`, because the
// coordinator's handler is where `offer_current_answer` must be called: it
// offers THE DELIVERY IT IS HANDLING, and the Switchboard judges.
//
// That split is the shape of the sugar, and it is worth naming before the report
// does: the handle is the host's, the conversation is a weave's, and the only
// thing that crosses between them is "here is the delivery I am holding".

#include "diner.hpp"
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

namespace marathon::testing {

namespace kitchen = marathon::kitchen;
namespace timer = zengine::timer;

/// Where the weaves live. Set by the build, so nothing here guesses at a path.
#ifndef MARATHON_KITCHEN_RUNTIME_DIR
#error "MARATHON_KITCHEN_RUNTIME_DIR must be defined by the build"
#endif

inline std::string weave_path(const std::string& stem) {
    return std::string(MARATHON_KITCHEN_RUNTIME_DIR) + "/" + stem + ".so";
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

// ---- REPLAY: the owner, who replaces stations -------------------------------

/// Everything the owner learned, and the one pointer that ties an ordinary weave
/// to a host-only handle.
struct OwnerDesk {
    /// The LIVE handle. Host-owned; the owner weave only ever offers it a
    /// delivery. Null when no replacement is in flight, and the owner says so
    /// rather than assuming.
    loom::PreparedReplacement* upgrade = nullptr;

    std::vector<loom::TxnResult> offers;            ///< every offer, in order
    std::vector<kitchen::WorkDescribed> described;  ///< what incumbents said they held
    std::vector<std::string> notes;                 ///< a readable trace for failures
    std::int64_t consult_answer = kitchen::kPassMs; ///< what a consulting candidate is told
    std::int64_t offered_without_handle = 0;        ///< answers that arrived with no transaction
    std::int64_t unattested = 0;                    ///< arrivals that failed answers_ask()

    const kitchen::WorkDescribed* work_of(const std::string& station) const {
        const kitchen::WorkDescribed* found = nullptr;
        for (const kitchen::WorkDescribed& w : described) {
            if (w.station == station) {
                found = &w;
            }
        }
        return found;
    }
};

struct OwnerState {
    std::int64_t descriptions = 0;
    std::int64_t ready = 0;
    std::int64_t not_ready = 0;
    std::int64_t consulted = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(OwnerState, 1, ZEN_FIELD(descriptions), ZEN_FIELD(ready), ZEN_FIELD(not_ready),
              ZEN_FIELD(consulted));
};

/// The coordinator. It does exactly three things and none of them is a decision
/// about readiness: it records what an incumbent described, it answers a sealed
/// candidate's own question, and it OFFERS the delivery it is holding to the
/// transaction's gate.
///
/// Note what is absent. There is no readiness flag here, no "the candidate said
/// yes so I will commit", no transaction id read out of a payload. The owner
/// cannot make a candidate ready by believing it is; the Switchboard proves the
/// answer is authentic, from the exact candidate, to the exact ask, heard by the
/// exact coordinator — and refuses otherwise.
class Owner : public loom::WeaveBase<
                  Owner, OwnerState,
                  loom::Accept<kitchen::WorkDescribed, kitchen::StationReady,
                               kitchen::StationNotReady, kitchen::AskHousePassRate>,
                  loom::Emit<kitchen::DescribeWork, kitchen::PrepareStation,
                             kitchen::HousePassRate>> {
public:
    explicit Owner(OwnerDesk& desk) : desk_(&desk) {}

    /// The incumbent's authenticated answer to DescribeWork. Both halves of the
    /// consumer obligation are available here — Loom's word, and the owner's own
    /// knowledge that it asked — so both are done.
    void on(const kitchen::WorkDescribed& w, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ++desk_->unattested;
            return;
        }
        ++state_.descriptions;
        desk_->described.push_back(w);
        desk_->notes.push_back("described " + w.station + ": " +
                               std::to_string(w.tickets.size()) + " ticket(s)");
    }

    void on(const kitchen::StationReady& r, loom::Mail&) {
        ++state_.ready;
        desk_->notes.push_back("candidate says READY as '" + r.station + "' (adopted " +
                               std::to_string(r.adopted) + ")");
        offer(loom::PreparationAnswer::Ready);
    }

    void on(const kitchen::StationNotReady& r, loom::Mail&) {
        ++state_.not_ready;
        desk_->notes.push_back("candidate says NOT READY: " + r.reason);
        offer(loom::PreparationAnswer::Refused);
    }

    /// A sealed candidate asked the one party it may speak to. Answering is an
    /// ordinary authenticated answer — the seal restricts the candidate's reach,
    /// not the coordinator's ability to reply to it.
    void on(const kitchen::AskHousePassRate&, loom::Mail& mail) {
        ++state_.consulted;
        (void)mail.answer(kitchen::HousePassRate{desk_->consult_answer});
    }

private:
    void offer(loom::PreparationAnswer answer) {
        if (desk_->upgrade == nullptr) {
            ++desk_->offered_without_handle;
            return;
        }
        desk_->offers.push_back(desk_->upgrade->offer_current_answer(answer));
    }

    OwnerDesk* desk_;
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

/// "Answer a routing question that was never asked of you." The correlation is
/// GIVEN to the rogue because a correlation is not a secret — it is a
/// conversation label, visible on the bus to anyone watching. If knowing it were
/// enough, the expediter's wall would be made of nothing.
struct ForgeRouteChoice {
    std::string order_id;
    std::string station;
    std::int64_t correlation = 0;
    ZEN_SHAPE(ForgeRouteChoice, 1, ZEN_FIELD(order_id), ZEN_FIELD(station),
              ZEN_FIELD(correlation));
};

/// REPLAY: "tell the owner a candidate is ready." The forgery aimed at the new
/// ceremony. It is a perfectly-shaped `StationReady` from a weave holding an
/// ordinary grant for the shape — everything except the one thing no weave can
/// manufacture, which is Loom's word that this is an answer to the transaction's
/// own ask.
struct ForgeStationReady {
    std::string station;
    ZEN_SHAPE(ForgeStationReady, 1, ZEN_FIELD(station));
};

class Rogue
    : public loom::WeaveBase<Rogue, RogueState,
                             loom::Accept<ForgePlated, ForgeStationOpen, ForgeRouteChoice,
                                          ForgeStationReady>,
                             loom::Emit<kitchen::Plated, kitchen::StationOpen,
                                        kitchen::RouteChoice, kitchen::StationReady>> {
public:
    explicit Rogue(loom::WeaveId owner) : owner_(owner) {}

    void on(const ForgePlated& f, loom::Mail& mail) {
        ++state_.sent;
        mail.send_to_role(kitchen::kExpediterRole, kitchen::Plated{f.job, f.dish, f.station});
    }

    /// A perfectly-shaped RouteChoice with the right correlation, sent by a weave
    /// that holds an ordinary grant for the shape — and no attestation, because
    /// no weave can manufacture one.
    void on(const ForgeRouteChoice& f, loom::Mail& mail) {
        ++state_.sent;
        mail.send_to_role(kitchen::kExpediterRole,
                          kitchen::RouteChoice{f.order_id, f.station, kitchen::kRoutedPreferred,
                                               "[rogue] trust me"},
                          static_cast<std::uint64_t>(f.correlation));
    }

    void on(const ForgeStationOpen& f, loom::Mail& mail) {
        ++state_.sent;
        kitchen::StationOpen hello;
        hello.station = f.station;
        hello.dishes.push_back(f.dish);
        hello.pass_ms = 20;
        mail.publish(hello);
    }

    void on(const ForgeStationReady& f, loom::Mail& mail) {
        ++state_.sent;
        mail.send(owner_, kitchen::StationReady{f.station, 0});
    }

private:
    loom::WeaveId owner_;
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

    /// The consumer obligation, and here BOTH halves are available.
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

        owner_ = loom::mount<Owner>(bus_, desk_);
        diner_ = loom::mount<kitchen::Diner>(bus_, book_);

        // The rogue's grant is written out rather than derived, because the
        // Forge* shapes are the HARNESS talking to it and have no business in a
        // weave's honest Emit silhouette. Everything it may say about the kitchen
        // is a shape any weave may legitimately hold: that is the threat model,
        // not a hole punched for the test.
        loom::Grant rogue_grant;
        rogue_grant.allow_to_any(ForgePlated::zen_name, ForgePlated::zen_version);
        rogue_grant.allow_to_any(ForgeStationOpen::zen_name, ForgeStationOpen::zen_version);
        rogue_grant.allow_to_any(ForgeRouteChoice::zen_name, ForgeRouteChoice::zen_version);
        rogue_grant.allow_to_any(ForgeStationReady::zen_name, ForgeStationReady::zen_version);
        rogue_grant.allow_to_any(kitchen::Plated::zen_name, kitchen::Plated::zen_version);
        rogue_grant.allow_to_any(kitchen::StationOpen::zen_name, kitchen::StationOpen::zen_version);
        rogue_grant.allow_to_any(kitchen::RouteChoice::zen_name, kitchen::RouteChoice::zen_version);
        rogue_grant.allow_to_any(kitchen::StationReady::zen_name,
                                 kitchen::StationReady::zen_version);
        rogue_ = loom::mount_granted<Rogue>(bus_, std::move(rogue_grant), owner_);

        loom::Grant inspector_grant;
        inspector_grant.allow_to_any(AskStatus::zen_name, AskStatus::zen_version);
        inspector_grant.allow_to_role(kitchen::KitchenStatus::zen_name,
                                      kitchen::KitchenStatus::zen_version,
                                      kitchen::kExpediterRole);
        inspector_ = loom::mount_granted<Inspector>(bus_, std::move(inspector_grant), status_);

        // MARATHON_TRACE=1 prints every delivery and every refusal the bus sees.
        // It exists because the single hardest thing to debug in Night One was a
        // message refused where NO PARTICIPANT COULD SEE IT.
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
    loom::Kernel& kernel() { return kernel_; }
    kitchen::DinerBook& book() { return book_; }
    OperatorLog& oplog() { return oplog_; }
    OwnerDesk& desk() { return desk_; }
    loom::WeaveId diner() const { return diner_; }
    loom::WeaveId owner() const { return owner_; }
    loom::WeaveId manager() const { return manager_; }
    std::int64_t beats() const { return beats_; }

    // ---- watching a weave's first live deliveries ---------------------------

    /// Record, in order, every shape delivered to `who` from now on. This is how
    /// "the activation was its FIRST live delivery" is proven rather than
    /// asserted: it is a fact about the delivery order, and only a tap can see
    /// delivery order.
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

    // ---- driving the steward ------------------------------------------------

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

    std::uint64_t list_loaded() { return command("list", loom::ListLoaded{}); }

    /// Make a role's holder walk out with nothing to replace it. A swap whose
    /// successor cannot load leaves the role UNHELD, which is the honest way to
    /// make a service vanish mid-job without reaching around the architecture.
    std::uint64_t evict(const std::string& role) {
        return command("evict " + role,
                       loom::SwapWeave{role, "no-such-weave",
                                       std::string(MARATHON_KITCHEN_RUNTIME_DIR) +
                                           "/no-such-weave.so",
                                       /*graceful=*/false});
    }

    // ---- driving the owner --------------------------------------------------

    /// Ask whoever holds `role` right now to describe its work, AS the owner.
    void describe_work(const std::string& role) {
        bus_.send_as_to_role(owner_, role,
                             loom::Message(loom::to_value(kitchen::DescribeWork{}), owner_,
                                           owner_, 0));
    }

    /// A fresh handle for a fresh replacement. The desk's pointer follows it, so
    /// the owner weave always offers to the transaction that is actually live.
    loom::PreparedReplacement& new_upgrade() {
        upgrade_ = loom::PreparedReplacement(bus_, kernel_);
        desk_.upgrade = &upgrade_;
        return upgrade_;
    }
    loom::PreparedReplacement& upgrade() { return upgrade_; }
    /// Point the desk at nothing — for proving the owner does not invent a
    /// transaction when an answer arrives with none in flight.
    void forget_upgrade() { desk_.upgrade = nullptr; }

    // ---- driving the diner --------------------------------------------------

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

    /// Ask the kitchen for its tally, as the inspector.
    void ask_status() {
        bus_.send_as(inspector_, inspector_,
                     loom::Message(loom::to_value(AskStatus{}), inspector_, inspector_, 0));
    }

    const std::vector<std::string>& status() const { return status_; }

    /// Boot a working kitchen: time, routing, front of house, two stations.
    /// Ordered deliberately so nothing depends on load order — every weave
    /// arranges its own timers on its own activation, and anything loaded before
    /// the Timer retries on its TimerReady.
    ///
    /// `clock` is the one labelled substitution: the suite passes the virtual
    /// clock, the demo passes the shipped one. Same service, same protocol, same
    /// beat chain — only the nap differs.
    void boot(const std::string& policy = "kitchen-policy-house",
              const std::string& clock = "zengine-timer-virtual") {
        load(clock, timer::kTimerRole);
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
    loom::WeaveId owner_{};
    loom::WeaveId diner_{};
    loom::WeaveId rogue_{};
    loom::WeaveId inspector_{};
    OperatorLog oplog_;
    OwnerDesk desk_;
    kitchen::DinerBook book_;
    std::vector<std::string> status_;
    loom::PreparedReplacement upgrade_{bus_, kernel_};
    /// Stable storage for tap sinks. A `std::deque` and not a `std::vector`
    /// because the observers capture pointers INTO it: a vector that reallocated
    /// would leave every earlier tap writing through a dangling pointer, and the
    /// crash would land in the middle of an unrelated case.
    std::deque<std::vector<std::string>> watched_;
    std::int64_t budget_ = 0;
    std::int64_t beats_ = 0;
};

} // namespace marathon::testing

#endif // MARATHON_KITCHEN_HARNESS_HPP
