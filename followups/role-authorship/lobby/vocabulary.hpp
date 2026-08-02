#ifndef FOLLOWUP_LOBBY_VOCABULARY_HPP
#define FOLLOWUP_LOBBY_VOCABULARY_HPP

// The lobby, replayed against a Loom that can answer the question the marathon
// could only ask:
//
//     Is THIS MatchCreated the matchmaker's office speaking, or somebody —
//     possibly the very same weave — merely talking?
//
// The marathon's answer was a pair of workarounds, both priced. This replay's
// answer is one delivery fact: `mail.authored_from_role("lobby.matchmaker")`.
// The push stays a push. The strict player stays strict. Nothing is inverted,
// deferred, or registered.

#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace lobby {

/// The statement that matters: a player that trusts one LEAVES for `server`.
struct MatchCreated {
    std::string server;
    std::int64_t match_id = 0;
    ZEN_SHAPE(MatchCreated, 1, ZEN_FIELD(server), ZEN_FIELD(match_id));
};

/// Drive the matchmaker: push a match at `player`. `personal` asks the SAME
/// holder to say the SAME shape in its personal capacity — the marathon's
/// chatter case, now distinguishable.
struct MakeMatch {
    std::int64_t player = 0;
    bool personal = false;
    ZEN_SHAPE(MakeMatch, 1, ZEN_FIELD(player), ZEN_FIELD(personal));
};

} // namespace lobby

#endif // FOLLOWUP_LOBBY_VOCABULARY_HPP
