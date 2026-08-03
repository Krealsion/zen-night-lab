// workshop-inspector — the machine has no secrets.
//
// An ORDINARY loadable weave. It watches nothing directly: it accepts the
// `BusFact` publications the S-3 bridge relays, keeps honest tallies and a
// ring of recent facts, paints a live line of intent (slot "inspector"), and
// answers `QueryEvents` asks. Loaded late it knows less, replaced it forgets —
// both honest, both the score-weave stance.
//
// Truth discipline:
//   - facts it stores/returns: FACT (runtime events, relayed by the bridge)
//   - tallies: DERIVED (computed here from those facts)
//   - it never invents: `authored_role` stays exactly as stamped (empty =
//     personal speech, whatever offices the sender may hold), and a refusal
//     it reports is one that actually happened on the bus.

#include "explain.hpp"
#include "vocabulary.hpp"

#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <string>

namespace workshop {

namespace surface = zengine::surface;

inline constexpr std::int64_t kRefusalKeep = 16;
inline constexpr std::int64_t kRecentKeep = 32;
inline constexpr std::int64_t kPaintEvery = 50;

struct InspectorState {
    std::int64_t delivered = 0;
    std::int64_t refused = 0;
    std::vector<BusFact> recent_refusals;
    std::vector<BusFact> recent;
    std::vector<BusFact> doors;
    ZEN_EXPOSE();
    ZEN_SHAPE(InspectorState, 2, ZEN_FIELD(delivered), ZEN_FIELD(refused),
              ZEN_FIELD(recent_refusals), ZEN_FIELD(recent), ZEN_FIELD(doors));
};

class Inspector : public loom::WeaveBase<Inspector, InspectorState,
                                         loom::Accept<BusFact, QueryEvents>,
                                         loom::Emit<surface::SurfaceText, EventsReport>> {
public:
    void on(const BusFact& fact, loom::Mail& mail) {
        keep(state_.recent, fact, kRecentKeep);
        if (fact.kind == "Delivered" &&
            (fact.schema == "zen.LoadWeave" || fact.schema == "zen.ReloadWeave" ||
             fact.schema == "zen.SwapWeave" || fact.schema == "zen.ListLoaded")) {
            keep(state_.doors, fact, kRefusalKeep);
        }
        if (fact.kind == "Refused") {
            ++state_.refused;
            keep(state_.recent_refusals, fact, kRefusalKeep);
            paint(mail, fact);
        } else {
            if (fact.kind == "Delivered") {
                ++state_.delivered;
            }
            if (state_.delivered % kPaintEvery == 0) {
                paint(mail, nullptr_fact());
            }
        }
    }

    void on(const QueryEvents&, loom::Mail& mail) {
        mail.answer(EventsReport{state_.delivered, state_.refused, state_.recent_refusals,
                                 state_.recent, state_.doors});
    }

private:
    static const BusFact& nullptr_fact() {
        static const BusFact none{};
        return none;
    }

    static void keep(std::vector<BusFact>& ring, const BusFact& fact, std::int64_t cap) {
        ring.push_back(fact);
        if (static_cast<std::int64_t>(ring.size()) > cap) {
            ring.erase(ring.begin());
        }
    }

    void paint(loom::Mail& mail, const BusFact& refusal) {
        std::string line = "seen " + std::to_string(state_.delivered) + " | refused " +
                           std::to_string(state_.refused);
        if (!refusal.reason.empty()) {
            line += " | last: " + refusal.reason + " " + refusal.schema + " -> weave " +
                    std::to_string(refusal.target) + " (" + explain_refusal(refusal.reason) + ")";
        }
        mail.publish(surface::SurfaceText{"inspector", line});
    }
};

} // namespace workshop

ZEN_EXPORT_WEAVE(workshop::Inspector)
