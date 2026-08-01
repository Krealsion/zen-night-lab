// The maintenance scheduler's suite.
//
// THE QUESTION: did the two sugars compose? A `timer::TimedWeave` with authored
// bindings, and `loom::PreparedReplacement` used twice — once on this package's
// own service and once on **the Timer service underneath the bindings**.
//
// The cases are grouped by what they are for:
//   1. the authored rhythm, and the hook Night One could not have
//   2. cancellation of an authored binding, and of a domain schedule
//   3. THE PROBE: what happens to a binding declared after construction
//   4. replacing the ordinary service
//   5. REPLACING THE CLOCK, through the Timer package's own vocabulary
//   6. hostile cases

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "harness.hpp"

#include <string>
#include <vector>

using marathon::maint_testing::Maintenance;
namespace maint = marathon::maint;
namespace timer = zengine::timer;

namespace {

std::string transcript(const marathon::maint_testing::SupervisorLog& l) {
    std::string out = "\n  supervisor heard:";
    for (const std::string& line : l.heard) {
        out += "\n    " + line;
    }
    if (l.heard.empty()) {
        out += " (nothing)";
    }
    return out;
}

std::string desk_notes(const marathon::maint_testing::EngineerDesk& d) {
    std::string out = "\n  engineer saw:";
    for (const std::string& n : d.notes) {
        out += "\n    " + n;
    }
    if (d.notes.empty()) {
        out += " (nothing)";
    }
    return out;
}

} // namespace

// ---- 1. the authored rhythm, and the hook ----------------------------------

TEST_CASE("the yard opens: three loads answered, and the scheduler announces itself FROM THE "
          "ACTIVATION HOOK") {
    Maintenance m;
    m.boot();
    CHECK(m.oplog().pending.empty());
    REQUIRE(m.oplog().answers.size() == 3);
    for (const std::string& a : m.oplog().answers) {
        CHECK(a.find("refused") == std::string::npos);
    }

    // NIGHT ONE'S FRICTION 4, CLOSED AND MEASURED. `on_timed_activation` ran, it
    // did real domain work (this announcement), and it reports THREE authored
    // bindings — so the bindings were reconciled before the hook, exactly as the
    // layer promises.
    REQUIRE_MESSAGE(m.sup().opens == 1, transcript(m.sup()));
    CHECK_MESSAGE(m.sup().saw("scheduler open: 3 binding(s)"), transcript(m.sup()));
}

TEST_CASE("A REPEATING BINDING drives a repeating domain schedule, and the reports arrive") {
    Maintenance m;
    m.boot();
    m.ask("check", "press-watch", "press-1", 2);
    m.pump(40);

    CHECK_MESSAGE(m.sup().saw("report press-watch press-1: ok"), transcript(m.sup()));
    // It repeats: several runs, and the run counter climbs.
    CHECK_MESSAGE(m.sup().runs_of["press-watch"] >= 3,
                  "runs: ", m.sup().runs_of["press-watch"]);
    CHECK(m.sup().unhealthy == 0);
}

TEST_CASE("A ONE-SHOT domain schedule runs exactly once and leaves the book") {
    Maintenance m;
    m.boot();
    m.ask("once", "kiln-vent", "kiln", 2, "vent");
    m.pump(60);

    CHECK_MESSAGE(m.sup().saw("report kiln-vent kiln: UNHEALTHY"), transcript(m.sup()));
    CHECK_MESSAGE(m.sup().count("report kiln-vent") == 1, transcript(m.sup()));
    CHECK(m.sup().unhealthy == 1);

    m.ask("status");
    m.pump(6);
    REQUIRE(m.status().size() == 1);
    CHECK_MESSAGE(m.status()[0].find("book=0") != std::string::npos, m.status()[0]);
}

TEST_CASE("THE AUTHORED ONE-SHOT fires once and becomes Spent, and no lifecycle event revives it") {
    Maintenance m;
    m.boot();
    m.ask("check", "w", "press-1", 8);
    m.pump(60);

    m.ask("status");
    m.pump(6);
    REQUIRE(m.status().size() == 1);
    // `warmup` is the requester-addressed one-shot: it fired, and the binding is
    // spent rather than waiting.
    CHECK_MESSAGE(m.status()[0].find("warmups=1") != std::string::npos, m.status()[0]);
    CHECK_MESSAGE(m.status()[0].find("warmup=spent") != std::string::npos, m.status()[0]);
    // ...and the repeating one is still waiting, which is what `repeat` means.
    CHECK_MESSAGE(m.status()[0].find("sweep=waiting") != std::string::npos, m.status()[0]);
}

// ---- 2. cancellation --------------------------------------------------------

TEST_CASE("CANCELLING AN AUTHORED BINDING is a different act from cancelling a schedule, and "
          "both work") {
    Maintenance m;
    m.boot();
    m.ask("check", "press-watch", "press-1", 2);
    m.pump(20);
    REQUIRE_MESSAGE(m.sup().saw("report press-watch"), transcript(m.sup()));

    // (a) the AUTHORED binding: a `TimerHandle::cancel`, both halves — local
    //     (a later TimerReady will not re-establish it) and remote (the service
    //     stops holding it).
    m.ask("audit");
    m.pump(6);
    m.ask("status");
    m.pump(6);
    REQUIRE(m.status().size() == 1);
    CHECK_MESSAGE(m.status()[0].find("audit=canceled") != std::string::npos, m.status()[0]);

    // (b) the DOMAIN schedule: a book entry, nothing to do with the Timer.
    //     A check already in flight when the cancel lands still answers -- the
    //     raw contract's honest edge, inherited rather than smoothed over -- so
    //     what is asserted is that it STOPS, not that it stopped instantly.
    m.ask("cancel", "press-watch");
    m.pump(10);
    const std::int64_t settled = m.sup().runs_of["press-watch"];
    m.pump(60);
    CHECK_MESSAGE(m.sup().runs_of["press-watch"] == settled, transcript(m.sup()));

    // ...and the AUDIT never fires, however long we wait.
    m.pump(300);
    m.ask("status");
    m.pump(6);
    REQUIRE(m.status().size() == 2);
    CHECK_MESSAGE(m.status()[1].find("audits=0") != std::string::npos, m.status()[1]);
}

TEST_CASE("cancelling a schedule nobody has is refused, in a shape a stranger can read") {
    Maintenance m;
    m.boot();
    m.ask("cancel", "ghost");
    m.pump(6);
    CHECK_MESSAGE(m.sup().saw("no schedule named 'ghost'"), transcript(m.sup()));
}

TEST_CASE("the book is bounded and a period of zero is refused") {
    Maintenance m;
    m.boot();
    m.ask("check", "bad", "press-1", 0);
    m.pump(6);
    CHECK_MESSAGE(m.sup().saw("needs a period of at least one sweep"), transcript(m.sup()));

    for (std::size_t i = 0; i < maint::kMaxSchedules + 1; ++i) {
        m.ask("check", "s" + std::to_string(i), "press-1", 5);
    }
    m.pump(10);
    CHECK_MESSAGE(m.sup().saw("maximum of 12 schedules"), transcript(m.sup()));
}

// ---- 3. THE PROBE ----------------------------------------------------------

TEST_CASE("THE BINDING TABLE IS AUTHORED, NOT DYNAMIC -- measured, not assumed") {
    Maintenance m;
    m.boot();
    m.pump(20);

    // Declare a repeating binding long AFTER construction and activation.
    m.ask("late");
    m.pump(60);

    m.ask("status");
    m.pump(6);
    REQUIRE(m.status().size() == 1);
    // It never fires. `reconcile` belongs to the binding layer — correctly, an
    // author reconciling would be a second scheduler — and the layer reconciles
    // on an accepted activation or a `TimerReady`, neither of which has happened
    // since. The binding sits `Waiting` and the Timer service has never heard of
    // it.
    CHECK_MESSAGE(m.status()[0].find("late_fires=0") != std::string::npos, m.status()[0]);
}

// ---- 4. replacing the ordinary service -------------------------------------

TEST_CASE("THE ORDINARY REPLACEMENT: the maintenance worker is swapped underneath a running "
          "schedule, and the rhythm never notices") {
    Maintenance m;
    m.boot();
    m.ask("check", "press-watch", "press-1", 2);
    m.pump(30);
    REQUIRE_MESSAGE(m.sup().saw("(v1)"), transcript(m.sup()));
    const std::int64_t before = m.sup().reports;

    m.describe_fleet();
    m.pump(4);
    REQUIRE_MESSAGE(m.desk().fleet_described, desk_notes(m.desk()));
    REQUIRE(m.desk().fleet.size() == maint::kFleetSize);

    REQUIRE(m.begin(maint::kWorkerRole, "maint-worker-v2").ok);
    std::vector<std::string>* seen = m.watch(m.upgrade().candidate());
    REQUIRE(m.upgrade()
                .ask(maint::PrepareMaintWorker{m.desk().fleet, m.desk().fleet_checks})
                .ok);
    m.pump(6);
    REQUIRE_MESSAGE(m.desk().offers.size() == 1, desk_notes(m.desk()));
    CHECK(m.desk().offers[0].ok);
    REQUIRE(m.upgrade().state() == loom::TxnState::Ready);

    const std::size_t outside_the_world = seen->size();
    REQUIRE(m.upgrade().commit(11).ok);
    CHECK(m.upgrade().state() == loom::TxnState::AdmissionPending);
    m.pump(40);

    const std::optional<loom::TxnOutcome> outcome = m.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Committed);
    REQUIRE(seen->size() > outside_the_world);
    CHECK((*seen)[outside_the_world] == std::string(loom::Activated::zen_name));

    // THE SCHEDULER NEVER LEARNED. Its beat is its own, its book is its own, and
    // the reports simply start saying v2.
    CHECK_MESSAGE(m.sup().saw("(v2)"), transcript(m.sup()));
    CHECK_MESSAGE(m.sup().reports > before, transcript(m.sup()));
}

TEST_CASE("AUTHENTIC REFUSAL: a worker that does not service the whole fleet says no") {
    Maintenance m;
    m.boot();
    m.describe_fleet();
    m.pump(4);
    REQUIRE(m.desk().fleet_described);

    REQUIRE(m.begin(maint::kWorkerRole, "maint-worker-narrow").ok);
    REQUIRE(m.upgrade().ask(maint::PrepareMaintWorker{m.desk().fleet, 0}).ok);
    m.pump(8);

    REQUIRE_MESSAGE(m.desk().offers.size() == 1, desk_notes(m.desk()));
    CHECK(m.desk().offers[0].ok); // the OFFER worked; the ANSWER was "no"
    CHECK(m.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = m.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);

    // The incumbent never learned, and the fleet is still served.
    m.ask("check", "kiln-watch", "kiln", 2);
    m.pump(40);
    CHECK_MESSAGE(m.sup().saw("report kiln-watch kiln: UNHEALTHY"), transcript(m.sup()));
}

// ---- 5. REPLACING THE CLOCK ------------------------------------------------

TEST_CASE("THE COMPOSITION: the TIMER SERVICE is replaced underneath the bindings, through the "
          "Timer package's OWN preparation vocabulary, and the rhythm resumes by itself") {
    Maintenance m;
    m.boot();
    m.ask("check", "press-watch", "press-1", 2);
    m.pump(30);
    const std::int64_t before = m.sup().reports;
    REQUIRE(before > 0);

    // The SAME handle, the SAME five calls. The only thing that differs from the
    // worker replacement above is which domain shape goes into `ask` and which
    // handler offers the answer — and both of those are the Timer package's, not
    // this application's.
    REQUIRE(m.begin(timer::kTimerRole, "zengine-timer-virtual-v2").ok);
    std::vector<std::string>* seen = m.watch(m.upgrade().candidate());

    timer::PrepareTimerHandover ask;
    // ⚠ SUGAR AUDIT, NONZERO AND EXPLAINED: this third-party vocabulary carries a
    // transaction id for wire legibility (its own header says it is NOT
    // authority), so driving it through the handle means reaching for
    // `upgrade().id()` — which the handle documents as diagnostics. Classified in
    // REPORT.md as "third-party vocabulary predating the handle", not as missing
    // sugar.
    ask.transaction = static_cast<std::int64_t>(m.upgrade().id().value);
    ask.continuity = timer::kStartFresh;
    REQUIRE(m.upgrade().ask(ask).ok);
    m.pump(6);

    REQUIRE_MESSAGE(m.desk().offers.size() == 1, desk_notes(m.desk()));
    CHECK(m.desk().offers[0].ok);
    REQUIRE_MESSAGE(m.upgrade().state() == loom::TxnState::Ready, desk_notes(m.desk()));

    const std::size_t outside_the_world = seen->size();
    REQUIRE(m.upgrade().commit(21).ok);
    m.pump(60);
    const std::optional<loom::TxnOutcome> outcome = m.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Committed);
    REQUIRE(seen->size() > outside_the_world);
    CHECK((*seen)[outside_the_world] == std::string(loom::Activated::zen_name));

    // THE NICEST THING IN THIS PROJECT, AND IT REQUIRED NOTHING. The new clock
    // publishes `TimerReady`; the binding layer reconciles; every authored
    // binding is re-established. There is not one line of application code for
    // this, and the domain schedule simply keeps running.
    m.pump(80);
    CHECK_MESSAGE(m.sup().reports > before, transcript(m.sup()));
    CHECK_MESSAGE(m.sup().runs_of["press-watch"] >= 3, transcript(m.sup()));
}

TEST_CASE("...and the late-declared binding is established by exactly that TimerReady") {
    Maintenance m;
    m.boot();
    m.ask("late");
    m.pump(40);
    m.ask("status");
    m.pump(6);
    REQUIRE(m.status().size() == 1);
    REQUIRE_MESSAGE(m.status()[0].find("late_fires=0") != std::string::npos, m.status()[0]);

    // Replace the clock. Nothing about this is aimed at the late binding.
    REQUIRE(m.begin(timer::kTimerRole, "zengine-timer-virtual-v2").ok);
    timer::PrepareTimerHandover ask;
    ask.transaction = static_cast<std::int64_t>(m.upgrade().id().value);
    ask.continuity = timer::kStartFresh;
    REQUIRE(m.upgrade().ask(ask).ok);
    m.pump(6);
    REQUIRE(m.upgrade().state() == loom::TxnState::Ready);
    REQUIRE(m.upgrade().commit(31).ok);
    m.pump(80);

    m.ask("status");
    m.pump(6);
    REQUIRE(m.status().size() == 2);
    // DELIGHTFUL, HONEST, AND NOT SOMETHING ANYONE WOULD GUESS: the binding
    // declared at run time starts firing the moment the Timer service is
    // replaced, because that is when the layer next reconciles.
    CHECK_MESSAGE(m.status()[1].find("late_fires=0") == std::string::npos, m.status()[1]);
}

TEST_CASE("AUTHENTIC REFUSAL FROM SOMEBODY ELSE'S SERVICE: a clock candidate that declines") {
    Maintenance m;
    m.boot();
    m.ask("check", "press-watch", "press-1", 2);
    m.pump(30);

    REQUIRE(m.begin(timer::kTimerRole, "zengine-timer-declines").ok);
    timer::PrepareTimerHandover ask;
    ask.transaction = static_cast<std::int64_t>(m.upgrade().id().value);
    ask.continuity = timer::kStartFresh;
    REQUIRE(m.upgrade().ask(ask).ok);
    m.pump(8);

    REQUIRE_MESSAGE(m.desk().offers.size() == 1, desk_notes(m.desk()));
    CHECK(m.desk().offers[0].ok);
    CHECK(m.upgrade().state() == loom::TxnState::Aborted);
    const std::optional<loom::TxnOutcome> outcome = m.upgrade().take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->reason == loom::TxnReason::CandidateRefused);

    // The incumbent clock never learned, and the rhythm never stuttered.
    const std::int64_t before = m.sup().reports;
    m.pump(40);
    CHECK_MESSAGE(m.sup().reports > before, transcript(m.sup()));
}

TEST_CASE("the incumbent clock keeps ticking throughout a preparation") {
    Maintenance m;
    m.boot();
    m.ask("check", "press-watch", "press-1", 2);
    m.pump(20);
    REQUIRE(m.begin(timer::kTimerRole, "zengine-timer-virtual-v2").ok);
    const std::int64_t before = m.sup().reports;
    m.pump(40);
    CHECK_MESSAGE(m.sup().reports > before, transcript(m.sup()));
    CHECK(m.upgrade().state() == loom::TxnState::Preparing);
}

// ---- 6. hostile / negative --------------------------------------------------

TEST_CASE("ABORTED OUTCOME: on either service, and the world is exactly as it was") {
    SUBCASE("the worker") {
        Maintenance m;
        m.boot();
        const loom::WeaveId incumbent = m.bus().role_holder(maint::kWorkerRole);
        REQUIRE(m.begin(maint::kWorkerRole, "maint-worker-v2").ok);
        REQUIRE(m.upgrade().ask(maint::PrepareMaintWorker{{"press-1"}, 0}).ok);
        m.pump(6);
        REQUIRE(m.upgrade().state() == loom::TxnState::Ready);
        REQUIRE(m.upgrade().abort().ok);
        const auto outcome = m.upgrade().take_outcome();
        REQUIRE(outcome.has_value());
        CHECK(outcome->reason == loom::TxnReason::ExplicitAbort);
        CHECK(m.bus().role_holder(maint::kWorkerRole) == incumbent);
    }

    SUBCASE("the clock") {
        Maintenance m;
        m.boot();
        const loom::WeaveId incumbent = m.bus().role_holder(timer::kTimerRole);
        REQUIRE(m.begin(timer::kTimerRole, "zengine-timer-virtual-v2").ok);
        REQUIRE(m.upgrade().abort().ok);
        const auto outcome = m.upgrade().take_outcome();
        REQUIRE(outcome.has_value());
        CHECK(outcome->reason == loom::TxnReason::ExplicitAbort);
        CHECK(m.bus().role_holder(timer::kTimerRole) == incumbent);

        // The rhythm never stopped.
        m.ask("check", "press-watch", "press-1", 2);
        m.pump(40);
        CHECK_MESSAGE(m.sup().saw("report press-watch"), transcript(m.sup()));
    }
}

TEST_CASE("EXACT ERROR INSPECTION: refusals keep the substrate's own words") {
    Maintenance m;
    m.boot();

    SUBCASE("nobody holds the role") {
        const auto r = m.begin("maint.nobody", "maint-worker-v2");
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::NoRoleHolder);
    }

    SUBCASE("the artifact refuses to load") {
        const auto r = m.begin(maint::kWorkerRole, "maint-worker-imaginary");
        CHECK_FALSE(r.ok);
        CHECK(r.stage == loom::PreparedReplacement::StartStage::CandidateLoad);
    }

    SUBCASE("two replacements of the same incumbent: IncumbentBusy, atomically") {
        REQUIRE(m.begin(maint::kWorkerRole, "maint-worker-v2").ok);
        loom::PreparedReplacement second(m.bus(), m.kernel());
        const auto r = second.start({
            .operator_id = m.engineer(),
            .coordinator = m.engineer(),
            .role = maint::kWorkerRole,
            .candidate_name = "maint-worker-narrow",
            .candidate_path = marathon::maint_testing::weave_path("maint-worker-narrow"),
            .budget = 8,
        });
        CHECK_FALSE(r.ok);
        CHECK(r.begin_reason == loom::TxnReason::IncumbentBusy);
        CHECK_FALSE(r.cleanup_failed);
    }
}

TEST_CASE("a machine no worker services is reported UNHEALTHY with a reason, not dropped") {
    Maintenance m;
    m.boot();
    m.ask("check", "ghost-watch", "not-a-machine", 2);
    m.pump(40);
    CHECK_MESSAGE(m.sup().saw("does not service 'not-a-machine'"), transcript(m.sup()));
}

TEST_CASE("an offer with no transaction in flight is a nothing, not a crash") {
    Maintenance m;
    m.boot();
    REQUIRE(m.begin(maint::kWorkerRole, "maint-worker-v2").ok);
    m.forget_upgrade();
    REQUIRE(m.upgrade().ask(maint::PrepareMaintWorker{{"press-1"}, 0}).ok);
    m.pump(6);
    CHECK(m.desk().offered_without_handle == 1);
    CHECK(m.desk().offers.empty());
}
