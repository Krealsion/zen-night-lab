// kitchen-demo — the usage example: a scripted, non-interactive service run.
//
// It boots a real kitchen on the REAL monotonic clock (the shipped
// `zengine-timer`, not the suite's virtual one), places a handful of orders,
// makes the grill walk out mid-dish, swaps the routing policy underneath a
// running kitchen, and prints exactly what the diner heard. It ends on its own.
//
// THE HOST IS THIN, and deliberately: it owns the boot list and the script, and
// nothing else. It holds no clock, no recipes, no roster, no ticket book, and no
// opinion about routing. Every step is a message.
//
// THE SCRIPT IS EVENT-DRIVEN, NOT TIMED. Each step declares what it is waiting
// for — operator answers, diner outcomes — and the next step runs when the last
// one is finished. Nothing here sleeps or counts milliseconds; the demo advances
// because the kitchen said something.
//
// Run it:  ./kitchen-demo

#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace nightlab::kitchen;
namespace timer = zengine::timer;

std::string exe_dir() {
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return ".";
    }
    buf[n] = '\0';
    const std::string path(buf, static_cast<std::size_t>(n));
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

void say(const std::string& line) {
    std::printf("%s\n", line.c_str());
    std::fflush(stdout);
}

/// What the host remembers between steps. Owned by the host, handed to the
/// maitre by reference — the snake OperatorContext pattern.
struct Script {
    std::string dir;
    loom::WeaveId manager{};
    std::function<void()> request_stop;

    int step = -1;
    int waiting_answers = 0;  ///< lifecycle answers still owed to us
    int waiting_outcomes = 0; ///< diner outcomes still owed to us
    bool evicted = false;

    std::map<std::uint64_t, std::string> pending;   ///< correlation -> command label
    std::map<std::uint64_t, std::string> orders;    ///< correlation -> order id
    std::uint64_t next_command = 1;
    std::uint64_t next_order = 1000; ///< a separate range, so a glance tells them apart

    std::string so(const std::string& stem) const { return dir + "/" + stem + ".so"; }
};

struct MaitreState {
    std::int64_t heard = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(MaitreState, 1, ZEN_FIELD(heard));
};

/// The host's one weave: it operates the kitchen (as a Manager client) and eats
/// in it (as a diner). Two jobs in one weave because it is a demo script, not a
/// component — a real diner would be its own weave with no lifecycle reach at
/// all, which is exactly what the suite's diner is.
/// The one message the host sends to start the script — an ordinary shape, so
/// even the kick-off is a message rather than a call into a weave.
struct Open {
    ZEN_SHAPE(Open, 1);
};

class Maitre : public loom::WeaveBase<
                   Maitre, MaitreState,
                   loom::Accept<Open, loom::Result, loom::Ack, loom::Refused, OrderReceipt, Served,
                                OrderLost>,
                   loom::Emit<loom::LoadWeave, loom::SwapWeave, PlaceOrder, KitchenStatus>> {
public:
    explicit Maitre(Script& s) : s_(&s) {}

    /// The host opens the doors; everything after this is the kitchen talking.
    void on(const Open&, loom::Mail& mail) { run_step(mail); }

    /// zen.Result is two different things arriving through one door, and the
    /// difference is Loom's word: a lifecycle answer is RELAYED by the steward
    /// and matched by our own correlation, while the kitchen's diagnostic is an
    /// AUTHENTICATED answer to a question this weave asked. Checking
    /// `answers_ask()` first is what keeps them apart without guessing.
    void on(const loom::Result& r, loom::Mail& mail) {
        ++state_.heard;
        if (mail.answers_ask() && s_->pending.find(mail.correlation()) == s_->pending.end()) {
            say("  status | " + r.value);
            step_done(mail);
            return;
        }
        answered(mail, "-> " + r.value);
    }
    void on(const loom::Ack&, loom::Mail& mail) { answered(mail, "-> done"); }
    void on(const loom::Refused& r, loom::Mail& mail) { answered(mail, "-> REFUSED: " + r.reason); }

    void on(const OrderReceipt& r, loom::Mail& mail) {
        if (!mail.answers_ask() || !ours(mail, r.order_id)) {
            say("  IGNORED an unattested or unmatched receipt for '" + r.order_id + "'");
            return;
        }
        say("  receipt | " + r.order_id + ": " + r.resolved +
            (r.station.empty() ? "" : " @" + r.station) + " -- " + r.reason);
        if (r.resolved == kRoutedRefused) {
            s_->orders.erase(mail.correlation());
            outcome_done(mail);
            return;
        }
        // The dramatic moment: once step 4's order is on the griddle, the grill
        // walks out. A swap whose successor cannot load leaves the role UNHELD —
        // the Weave Manager's own documented outcome, and the honest way to make
        // a service vanish mid-job without reaching around the architecture.
        if (s_->step == 4 && !s_->evicted) {
            s_->evicted = true;
            say("  ...the grill walks out, mid-dish, saying nothing to anyone.");
            command(mail, "evict the grill",
                    loom::SwapWeave{station_role("grill"), "no-such-weave",
                                    s_->so("no-such-weave"), /*graceful=*/false});
        }
    }

    void on(const Served& v, loom::Mail& mail) {
        if (!ours(mail, v.order_id)) {
            return;
        }
        say("  SERVED  | " + v.order_id + ": " + v.dish + " from " + v.station);
        s_->orders.erase(mail.correlation());
        outcome_done(mail);
    }

    void on(const OrderLost& l, loom::Mail& mail) {
        if (!ours(mail, l.order_id)) {
            return;
        }
        say("  LOST    | " + l.order_id + ": " + l.reason);
        s_->orders.erase(mail.correlation());
        outcome_done(mail);
    }

private:
    bool ours(const loom::Mail& mail, const std::string& order_id) const {
        const auto it = s_->orders.find(mail.correlation());
        return it != s_->orders.end() && it->second == order_id;
    }

    void answered(loom::Mail& mail, const std::string& outcome) {
        ++state_.heard;
        const auto it = s_->pending.find(mail.correlation());
        if (it == s_->pending.end()) {
            say("  (unsolicited answer " + outcome + ")");
            return;
        }
        say("  op      | " + it->second + " " + outcome);
        s_->pending.erase(it);
        step_done(mail);
    }

    void step_done(loom::Mail& mail) {
        if (s_->waiting_answers > 0) {
            --s_->waiting_answers;
        }
        maybe_advance(mail);
    }

    void outcome_done(loom::Mail& mail) {
        if (s_->waiting_outcomes > 0) {
            --s_->waiting_outcomes;
        }
        maybe_advance(mail);
    }

    void maybe_advance(loom::Mail& mail) {
        if (s_->waiting_answers == 0 && s_->waiting_outcomes == 0) {
            run_step(mail);
        }
    }

    template <class Cmd>
    void command(loom::Mail& mail, std::string label, const Cmd& cmd) {
        const std::uint64_t corr = s_->next_command++;
        s_->pending[corr] = std::move(label);
        ++s_->waiting_answers;
        mail.send(s_->manager, cmd, corr);
    }

    void place(loom::Mail& mail, const std::string& id, const std::string& dish,
               const std::string& prefer, const std::string& fallback) {
        const std::uint64_t corr = s_->next_order++;
        s_->orders[corr] = id;
        ++s_->waiting_outcomes;
        say("  order   | " + id + ": " + dish + " (prefer '" + prefer + "', fallback '" +
            fallback + "')");
        mail.send_to_role(kExpediterRole, PlaceOrder{id, dish, prefer, fallback}, corr);
    }

    void run_step(loom::Mail& mail) {
        ++s_->step;
        switch (s_->step) {
        case 0:
            say("\n-- 0. open the kitchen -------------------------------------------");
            command(mail, "load the timer service",
                    loom::LoadWeave{"zengine-timer", s_->so("zengine-timer"), timer::kTimerRole});
            command(mail, "load the house policy",
                    loom::LoadWeave{"kitchen-policy-house", s_->so("kitchen-policy-house"),
                                    kPolicyRole});
            command(mail, "load the expediter",
                    loom::LoadWeave{"kitchen-expediter", s_->so("kitchen-expediter"),
                                    kExpediterRole});
            command(mail, "open the grill",
                    loom::LoadWeave{"kitchen-grill", s_->so("kitchen-grill"),
                                    station_role("grill")});
            command(mail, "open the fryer",
                    loom::LoadWeave{"kitchen-fryer", s_->so("kitchen-fryer"),
                                    station_role("fryer")});
            break;
        case 1:
            say("\n-- 1. a preference that can be honoured --------------------------");
            place(mail, "a1", "steak", "grill", kFallbackAnyStation);
            break;
        case 2:
            say("\n-- 2. a preference that cannot: the fallback, and the reason -----");
            place(mail, "a2", "wings", "grill", kFallbackAnyStation);
            break;
        case 3:
            say("\n-- 3. swap the routing policy underneath a running kitchen -------");
            command(mail, "swap policy -> rush",
                    loom::SwapWeave{kPolicyRole, "kitchen-policy-rush",
                                    s_->so("kitchen-policy-rush"), /*graceful=*/false});
            break;
        case 4:
            say("\n-- 4. the same order, a different brain: 'fries' now goes to the");
            say("      specialist -- and then the grill walks out mid-dish ---------");
            place(mail, "a3", "fries", "grill", kFallbackNone); // required: stays on the grill
            break;
        case 5:
            say("\n-- 5. the grill is struck from the roster until it says otherwise -");
            place(mail, "a4", "steak", "grill", kFallbackNone);
            break;
        case 6:
            say("\n-- 6. a service returns the only way a service can: by saying so --");
            command(mail, "re-open the grill",
                    loom::LoadWeave{"kitchen-grill", s_->so("kitchen-grill"),
                                    station_role("grill")});
            break;
        case 7:
            place(mail, "a5", "steak", "grill", kFallbackNone);
            break;
        case 8:
            say("\n-- 7. what does the kitchen believe about itself? ----------------");
            ++s_->waiting_answers;
            mail.send_to_role(kExpediterRole, KitchenStatus{});
            break;
        default:
            say("\n-- done ----------------------------------------------------------");
            if (s_->request_stop) {
                s_->request_stop();
            }
            break;
        }
    }

    Script* s_;
};

} // namespace

int main() {
    std::printf("kitchen-demo - containment: %s\n", loom::Kernel::containment_note());
    std::fflush(stdout);

    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    Script script;
    script.dir = exe_dir();
    script.manager = manager;
    script.request_stop = [&bus] { bus.stop(); };

    // The maitre's reach, assembled by the host and target-scoped where it is
    // dangerous. The steward grant IS kernel reach, transitively; the kitchen
    // grants are ordinary. Nothing here is allow_any.
    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
    reach.allow_to_role(PlaceOrder::zen_name, PlaceOrder::zen_version, kExpediterRole);
    reach.allow_to_role(KitchenStatus::zen_name, KitchenStatus::zen_version, kExpediterRole);
    // The kick-off, addressed to nobody in particular because only this weave
    // accepts the shape. Narrow by declaration rather than by target — the honest
    // spelling for a shape with exactly one accepter, and not a wildcard.
    reach.allow_to_any(Open::zen_name, Open::zen_version);
    auto maitre = std::make_unique<Maitre>(script);
    Maitre* maitre_raw = maitre.get();
    const loom::WeaveId maitre_id = bus.register_weave(std::move(maitre), std::move(reach));
    maitre_raw->zen_set_self(maitre_id);

    // The host holds root but spends a real capability: the message is stamped as
    // the maitre and authorized against the MAITRE's grant.
    bus.send_as(maitre_id, maitre_id,
                loom::Message(loom::to_value(Open{}), maitre_id, maitre_id, 0));

    // The whole demo runs inside pump(): the Timer's beat chain keeps the queue
    // alive and its nap paces it, and the script's last step stops the bus. A
    // pump that returns QUIESCENT instead means nothing in this process will ever
    // speak again — say so honestly and leave rather than spin on a dead bus.
    bus.pump();
    if (bus.pending() == 0 && script.step < 9) {
        say("\n(the bus went quiet before the script finished: no timer service deployed?)");
        return 1;
    }
    return 0;
}
