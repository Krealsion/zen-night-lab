#ifndef WORKSHOP_BRIDGE_HPP
#define WORKSHOP_BRIDGE_HPP

// The tap bridge — SPECIAL MACHINERY, recorded as S-3.
//
// `Switchboard::add_observer` is host-tier: only something holding the
// Switchboard can watch the bus. This bridge is the one privileged hand that
// does, and everything it sees it immediately RE-PUBLISHES into the world as
// ordinary gated `BusFact` messages — so the inspector (and any curious toy)
// consumes runtime truth as ordinary published intent, through its own
// accept-set, like everything else.
//
// Honesty notes:
//   - BusFact deliveries themselves are not re-observed (the one filter that
//     keeps the mirror from reflecting itself).
//   - The bridge relays; it never interprets. `authored_role` is copied from
//     the envelope's stamped fact and is EMPTY for personal speech — filling
//     it from current role membership would be inventing provenance (MSG-07).
//   - Shared between the shell and the test suite so the witnesses watch the
//     real bridge, not a copy.

#include "vocabulary.hpp"

#include <zen/switchboard.hpp>

#include <cstdint>
#include <memory>

namespace workshop {

inline loom::ObserverId install_bus_fact_bridge(loom::Switchboard& bus) {
    auto counter = std::make_shared<std::int64_t>(0);
    return bus.add_observer([&bus, counter](const loom::BusEvent& e) {
        if (e.schema_name == BusFact::zen_name) {
            return; // the mirror does not reflect itself
        }
        BusFact fact;
        fact.seq = ++*counter;
        switch (e.kind) {
        case loom::EventKind::Delivered: fact.kind = "Delivered"; break;
        case loom::EventKind::Refused: fact.kind = "Refused"; break;
        case loom::EventKind::Died: fact.kind = "Died"; break;
        case loom::EventKind::Revived: fact.kind = "Revived"; break;
        }
        if (e.kind == loom::EventKind::Refused) {
            fact.reason = loom::name_of(e.refusal.reason);
            fact.detail = e.refusal.message();
        }
        fact.schema = e.schema_name;
        fact.schema_version = static_cast<std::int64_t>(e.schema_version);
        fact.sender = static_cast<std::int64_t>(e.sender.value);
        fact.target = static_cast<std::int64_t>(e.target.value);
        fact.authored_role = e.authored_role;
        bus.publish(loom::Message(loom::to_value(fact)));
    });
}

} // namespace workshop

#endif // WORKSHOP_BRIDGE_HPP
