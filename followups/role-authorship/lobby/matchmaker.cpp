// The matchmaker — ONE source, and the pull workaround is GONE.
//
// The marathon built this service twice (push: natural and forgeable; pull:
// attestable and priced — one Loom-wide answer budget, strict players stranded
// across honest replacement, observers uncoverable). This replay builds it
// once, the natural way, and authors the statement that matters AS THE OFFICE:
//
//     mail.as_role("lobby.matchmaker").send(player, MatchCreated{...});
//
// Loom verifies the office at that moment; the player verifies the delivery
// fact at its end. No SeekMatch, no deferred answer, no registry — grep this
// file for `defer` and find nothing, which is the measurement.
//
// The same weave can still chat: `personal` pushes the SAME shape through the
// ordinary send, which is exactly the statement a strict player must NOT act
// on — and now can tell apart.

#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <string>

#ifndef MATCHMAKER_LABEL
#define MATCHMAKER_LABEL "v1"
#endif

namespace {

using namespace loom;

struct MatchmakerState {
    std::int64_t matches = 0;
    ZEN_SHAPE(MatchmakerState, 1, ZEN_FIELD(matches));
};

class Matchmaker
    : public WeaveBase<Matchmaker, MatchmakerState,
                       Accept<lobby::MakeMatch, loom::Activated>,
                       Emit<lobby::MatchCreated>> {
public:
    void on(const lobby::MakeMatch& cmd, Mail& mail) {
        ++state_.matches;
        const WeaveId player{static_cast<std::uint64_t>(cmd.player)};
        const lobby::MatchCreated match{std::string("srv-") + MATCHMAKER_LABEL,
                                        state_.matches};
        if (cmd.personal) {
            // The same holder, the same shape, the personal capacity. Chatter.
            (void)mail.send(player, match);
            return;
        }
        // The office speaking, deliberately, for this one statement. If this
        // weave does not hold the office, Loom refuses and NOTHING goes out —
        // never a silent downgrade to the personal send above.
        (void)mail.as_role("lobby.matchmaker").send(player, match);
    }

    /// A successor's first breath, when it is admitted over an incumbent.
    void on(const loom::Activated&, Mail&) {}
};

} // namespace

ZEN_EXPORT_WEAVE(Matchmaker)
