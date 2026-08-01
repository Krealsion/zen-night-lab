#ifndef MARATHON_LOBBY_VOCABULARY_HPP
#define MARATHON_LOBBY_VOCABULARY_HPP

// A multiplayer lobby — the whole contract in one file.
//
// THE ARCHITECTURAL QUESTION, and it is the only one this project exists for:
//
//     Are SPEAKER IDENTITY and OFFICE AUTHORSHIP observably different security
//     facts in a real application?
//
// Four projects have now written some version of "there is no way to ask Loom
// whether the sender of this message holds the role it claims", and every one of
// them treated it as a bookkeeping annoyance. This one makes it a security
// question with teeth, because in a lobby the statement that matters is:
//
//     MatchCreated { match, players, server }
//
// A player that receives one LEAVES THE LOBBY AND CONNECTS SOMEWHERE. So a weave
// that can forge it can pull players out of a real lobby into a fake match, and
// a weave that can forge `MatchCancelled` can tear a real match down. Neither is
// a misreported counter.
//
// ---- THE CASE THAT MAKES THE DISTINCTION UNAVOIDABLE ------------------------
//
// The matchmaker is an ordinary weave. It has an OFFICE (it holds
// `lobby.matchmaker`) and it also has a PERSONAL capacity: it can chat in the
// lobby like anybody else. Those are the same `WeaveId`.
//
// So "which exact weave sent this?" cannot separate:
//
//     the matchmaker, speaking as the matchmaker      -> act on it
//     the matchmaker, speaking personally             -> it is chatter
//
// and role-addressing cannot help either, because a role is a property of the
// RECEIVER of a message, never of its sender. `send_to_role` says where a
// message went; nothing on the wire says what capacity it came from.
//
// ---- TWO MATCHMAKERS, ONE SOURCE, AND THE PRICE OF THE WORKAROUND ----------
//
// This package builds the service twice, and the pair is the finding:
//
//     lobby-matchmaker-push   ANNOUNCES a match, as an ordinary directed
//                             message. Natural, cheap, replaceable — and
//                             FORGEABLE, which the suite measures.
//
//     lobby-matchmaker-pull   the player ASKS (`SeekMatch`) and the matchmaker
//                             DEFERS the answer until a match forms, so the
//                             `MatchCreated` a player acts on is Loom's
//                             authenticated answer to that player's own request.
//                             UNFORGEABLE, which the suite also measures.
//
// The pull style is a genuine answer to the security question. It is also the
// exact thing the download manager measured the cost of:
//
//   * `Switchboard::kMaxDeferredAnswers` is 64 and belongs to ONE LOOM, so a
//     lobby of sixty-five waiting players exhausts the whole process's capacity
//     for unfinished conversations;
//   * an answer right belongs to the life that earned it, so REPLACING THE
//     MATCHMAKER silently strands every waiting player.
//
// So the honest result is not "impossible". It is: **the only way to attest an
// office's statement today is to turn a push into a pull, and that trade is
// exactly the one a lobby cannot make.**

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace marathon::lobby {

/// The office. Players address it by role and it can be replaced underneath.
inline constexpr const char* kMatchmakerRole = "lobby.matchmaker";

/// The second weave that legitimately speaks about lobby state: it owns
/// membership and readiness, and it is NOT the matchmaker. Two speakers about
/// one subject is what makes "who said this, in what capacity" a real question
/// rather than a hypothetical.
inline constexpr const char* kRegistryRole = "lobby.registry";

// ---- what a player says -----------------------------------------------------

struct JoinLobby {
    std::string player;
    ZEN_SHAPE(JoinLobby, 1, ZEN_FIELD(player));
};

struct LeaveLobby {
    ZEN_SHAPE(LeaveLobby, 1);
};

struct SetReady {
    bool ready = false;
    ZEN_SHAPE(SetReady, 1, ZEN_FIELD(ready));
};

/// "Put me in a match when one exists." THE PULL STYLE'S WHOLE MECHANISM: the
/// matchmaker takes this request's answer right away and spends it, later, on
/// the `MatchCreated` that names this player. That is what makes the statement
/// attestable — and what makes it expensive.
struct SeekMatch {
    ZEN_SHAPE(SeekMatch, 1);
};

// ---- what the registry says -------------------------------------------------

struct LobbyJoined {
    std::string player;
    std::int64_t seat = 0;
    ZEN_SHAPE(LobbyJoined, 1, ZEN_FIELD(player), ZEN_FIELD(seat));
};

struct JoinRefused {
    std::string reason;
    ZEN_SHAPE(JoinRefused, 1, ZEN_FIELD(reason));
};

/// Published by the registry whenever membership or readiness changes. The
/// MATCHMAKER is a consumer of this: it is not the authority on who is here.
///
/// THIS IS THE FACT THE PUSH STYLE IS TRIGGERED BY, and noticing that is half
/// the finding. A push-style matchmaker acts on a fact ABOUT THE WORLD ("enough
/// people are ready"), and there is no such thing as an authenticated statement
/// about the world — Loom attests ANSWERS. To get an attestation you must
/// re-found the whole service on each player's own question, which is what the
/// pull build does, and what it costs is the rest of the finding.
struct LobbyChanged {
    std::vector<std::string> members;
    std::vector<std::string> ready;
    /// Parallel to `ready`: canonical decimal WeaveIds, because the matchmaker
    /// has to be able to SPEAK to the people it matches and a name is not an
    /// address. (Fourth project to stringify a `WeaveId` onto the wire.)
    std::vector<std::string> ready_weaves;
    ZEN_SHAPE(LobbyChanged, 1, ZEN_FIELD(members), ZEN_FIELD(ready),
              ZEN_FIELD(ready_weaves));
};

// ---- what the matchmaker says -----------------------------------------------

/// THE STATEMENT WHOSE AUTHORSHIP MATTERS. A player that believes one of these
/// stops waiting and goes somewhere.
struct MatchCreated {
    std::string match;
    std::vector<std::string> players;
    std::string server;
    ZEN_SHAPE(MatchCreated, 1, ZEN_FIELD(match), ZEN_FIELD(players), ZEN_FIELD(server));
};

/// THE SAME FACT, PUBLISHED FOR OBSERVERS -- and it is a strictly weaker thing.
///
/// `MatchCreated` is addressed to the players, and in the pull style it is the
/// authenticated answer to each player's own request. Everybody ELSE who needs to
/// know a match happened -- the registry, most obviously, since those players are
/// no longer in the lobby -- is not a party to any of those conversations and
/// gets this instead.
///
/// A PUBLICATION CAN NEVER BE ATTESTED TO ANYBODY, in either style. So the pull
/// workaround protects the PARTIES and does nothing at all for an OBSERVER, and
/// this shape is where that shows.
struct MatchStarted {
    std::string match;
    std::vector<std::string> players;
    ZEN_SHAPE(MatchStarted, 1, ZEN_FIELD(match), ZEN_FIELD(players));
};

/// The other one. Believing a forged cancellation costs a real match.
struct MatchCancelled {
    std::string match;
    std::string reason;
    ZEN_SHAPE(MatchCancelled, 1, ZEN_FIELD(match), ZEN_FIELD(reason));
};

/// THE SAME WEAVE, SPEAKING PERSONALLY. It is not a lie, it is not an attack,
/// and it carries exactly as much attestation as a `MatchCreated` does in the
/// push style — which is none. Its existence is what turns "check the sender"
/// into a wall made of nothing: the sender IS the matchmaker.
struct LobbyChatter {
    std::string from;
    std::string text;
    ZEN_SHAPE(LobbyChatter, 1, ZEN_FIELD(from), ZEN_FIELD(text));
};

/// A TEST-ONLY DOOR, and labelled as one. It makes the matchmaker weave say a
/// `MatchCreated` **in its own personal capacity** — not as the office, not in
/// response to anything.
///
/// It lives in the shared vocabulary rather than in the harness because both
/// halves need it: the host sends it and the weave receives it. That is the
/// honest cost of expressing "the same weave, a different capacity" at all —
/// there is no way to say it except by giving the weave a second door.
struct SpeakPersonally {
    std::string match;
    std::vector<std::string> players;
    std::vector<std::string> weaves;
    ZEN_SHAPE(SpeakPersonally, 1, ZEN_FIELD(match), ZEN_FIELD(players), ZEN_FIELD(weaves));
};

// ---- diagnostics ------------------------------------------------------------

struct LobbyStatus {
    ZEN_SHAPE(LobbyStatus, 1);
};

struct MatchmakerStatus {
    ZEN_SHAPE(MatchmakerStatus, 1);
};

// ---- the replacement conversation -------------------------------------------

/// One waiting player, described in words.
///
/// NOTE WHAT CANNOT BE HERE, and it is the pull style's whole problem: the
/// ANSWER RIGHT. A successor inherits the fact that a player is waiting and can
/// speak to them — but only as an ordinary message, which is precisely the
/// attestation the pull style existed to provide.
struct WaitingPlayer {
    std::string player;
    std::string weave;            ///< canonical decimal of the player's WeaveId
    std::int64_t correlation = 0;
    ZEN_SHAPE(WaitingPlayer, 1, ZEN_FIELD(player), ZEN_FIELD(weave), ZEN_FIELD(correlation));
};

struct DescribeWaiting {
    ZEN_SHAPE(DescribeWaiting, 1);
};

struct WaitingDescribed {
    std::vector<WaitingPlayer> waiting;
    std::int64_t held_answer_rights = 0; ///< how many of those are unattestable after this
    ZEN_SHAPE(WaitingDescribed, 1, ZEN_FIELD(waiting), ZEN_FIELD(held_answer_rights));
};

struct PrepareMatchmaker {
    std::vector<WaitingPlayer> inherit;
    std::int64_t match_size = 0; ///< the house rule the candidate must agree to
    ZEN_SHAPE(PrepareMatchmaker, 1, ZEN_FIELD(inherit), ZEN_FIELD(match_size));
};

struct MatchmakerReady {
    std::int64_t inherited = 0;
    ZEN_SHAPE(MatchmakerReady, 1, ZEN_FIELD(inherited));
};

struct MatchmakerNotReady {
    std::string reason;
    ZEN_SHAPE(MatchmakerNotReady, 1, ZEN_FIELD(reason));
};

// ---- the published bounds ---------------------------------------------------

inline constexpr std::size_t kMaxPlayers = 16;

/// How many ready players make a match. Small, so a suite can fill one.
inline constexpr std::size_t kMatchSize = 2;

inline constexpr const char* kTickTimerId = "lobby.tick";
inline constexpr std::int64_t kTickMs = 20;

} // namespace marathon::lobby

#endif // MARATHON_LOBBY_VOCABULARY_HPP
