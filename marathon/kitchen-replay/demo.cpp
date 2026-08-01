// The usage example: a scripted, non-interactive kitchen on the REAL monotonic
// clock, so that at least one lane of this project feels actual time passing.
//
// It runs the same host the suite runs — `harness.hpp`, unchanged — with one
// argument different: the Timer artifact is `zengine-timer.so` (the shipped
// service, whose nap sleeps) instead of `zengine-timer-virtual.so` (whose nap
// books the duration and returns). That is the whole difference between this
// program and the suite, and saying it out loud is the point: if the demo needed
// a different HOST, the suite's host would be a fiction.
//
// Run:  ./kitchen-demo        (a couple of seconds; nothing is interactive)

#include "harness.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using marathon::testing::Kitchen;
namespace kitchen = marathon::kitchen;

std::size_t g_shown = 0;

/// Print whatever the diner has heard since the last time we looked. The diner's
/// transcript is the only vantage point that matters: it is what a consumer of
/// this kitchen actually experiences.
void diner_says(Kitchen& k) {
    const std::vector<std::string>& heard = k.book().heard;
    for (; g_shown < heard.size(); ++g_shown) {
        const std::string& line = heard[g_shown];
        const char* tag = line.rfind("receipt", 0) == 0   ? "receipt"
                          : line.rfind("served", 0) == 0  ? "SERVED "
                          : line.rfind("lost", 0) == 0    ? "LOST   "
                                                          : "note   ";
        std::printf("  %s | %s\n", tag, line.c_str());
    }
}

void heading(const char* text) { std::printf("\n-- %s %s\n", text, std::string(60, '-').c_str()); }

} // namespace

int main() {
    std::printf("the job kitchen, replayed against Loom 78d64ea / Zengine f6a4c69 (ABI v4)\n");
    std::printf("running on the REAL monotonic clock; in-process, single-threaded, abuse-tier.\n");

    Kitchen k;

    heading("1. open the kitchen");
    k.boot("kitchen-policy-house", "zengine-timer");
    for (const std::string& a : k.oplog().answers) {
        std::printf("  op      | %s\n", a.c_str());
    }

    heading("2. a preference the house can honour");
    k.order("a1", "steak", "grill", kitchen::kFallbackAnyStation);
    k.pump(20);
    diner_says(k);

    heading("3. swap the routing policy underneath a running kitchen");
    k.swap(kitchen::kPolicyRole, "kitchen-policy-rush", /*graceful=*/false);
    k.pump(10);
    std::printf("  op      | %s\n", k.oplog().answer_for("swap").c_str());

    heading("4. the same order, a different brain");
    k.order("a2", "fries", "grill", kitchen::kFallbackAnyStation);
    k.pump(30);
    diner_says(k);

    heading("5. replace a live station through loom::PreparedReplacement");
    std::printf("  ...a fourteen-pass brisket goes on the grill first, so the replacement\n");
    std::printf("     happens with real work in the incumbent's hands.\n");
    k.order("a3", "brisket", "grill", kitchen::kFallbackNone);
    k.pump(12);
    diner_says(k);

    // THE COMPOSITION. The preparation window is the one interval in which the
    // incumbent is alive AND the successor is reachable, so the owner asks the
    // incumbent to describe its work and hands that to the candidate in the ask.
    k.describe_work(kitchen::station_role("grill"));
    k.pump(4);
    const kitchen::WorkDescribed* work = k.desk().work_of("grill");
    std::printf("  owner   | the grill is holding %zu ticket(s)\n",
                work == nullptr ? 0u : work->tickets.size());

    const auto started = k.new_upgrade().start({
        .operator_id = k.owner(),
        .coordinator = k.owner(),
        .role = kitchen::station_role("grill"),
        .candidate_name = "kitchen-grill-2",
        .candidate_path = marathon::testing::weave_path("kitchen-grill-2"),
        .budget = 8,
    });
    std::printf("  owner   | start -> %s (candidate sealed: %s)\n", started.ok ? "ok" : "REFUSED",
                k.bus().sealed(k.upgrade().candidate()) ? "yes" : "no");
    k.upgrade().ask(kitchen::PrepareStation{
        "grill", work == nullptr ? std::vector<kitchen::StationTicket>{} : work->tickets, false});
    k.pump(6);
    for (const std::string& n : k.desk().notes) {
        std::printf("  owner   | %s\n", n.c_str());
    }
    std::printf("  owner   | state before commit: %s\n", loom::name_of(k.upgrade().state()));
    k.upgrade().commit(101);
    std::printf("  owner   | commit -> scheduled; state now: %s\n",
                loom::name_of(k.upgrade().state()));
    k.pump(60);
    if (const auto outcome = k.upgrade().take_outcome()) {
        std::printf("  owner   | outcome: %s (%s)\n", loom::name_of(outcome->state),
                    loom::name_of(outcome->reason));
    }
    diner_says(k);
    std::printf("  ...the brisket was finished by a weave that never received the Prep.\n");

    heading("6. ...and a station that simply walks out, saying nothing to anyone");
    k.order("a4", "brisket", "grill", kitchen::kFallbackNone);
    k.pump(10);
    diner_says(k);
    k.evict(kitchen::station_role("grill"));
    k.pump(200);
    std::printf("  op      | %s\n", k.oplog().answer_for("evict").c_str());
    diner_says(k);

    heading("7. the kitchen's own account of the evening");
    k.ask_status();
    k.pump(10);
    for (const std::string& s : k.status()) {
        std::printf("  status  | %s\n", s.c_str());
    }
    std::printf("\n  beats of virtual-or-real time spent: %lld\n",
                static_cast<long long>(k.beats()));
    return 0;
}
