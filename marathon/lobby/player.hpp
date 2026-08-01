#ifndef MARATHON_LOBBY_PLAYER_HPP
#define MARATHON_LOBBY_PLAYER_HPP

// A player, as a host-native weave — and the whole measuring instrument of this
// project.
//
// It has ONE POLICY KNOB, `require_attestation`, and every result in the report
// is a cell in the table it produces:
//
//     require_attestation = false   act on any MatchCreated whose correlation
//                                   I recognise, or that names me. This is what
//                                   a push-style lobby forces, because nothing
//                                   is ever attested.
//
//     require_attestation = true    act only on a MatchCreated Loom vouched for
//                                   as the answer to MY OWN request. Available
//                                   only in the pull style.
//
// The point of the knob is that it makes "can the receiver tell?" a measurement
// rather than an argument. A strict player in a push lobby joins nothing —
// including real matches. A lax player in either lobby joins forgeries. A strict
// player in a pull lobby joins exactly the real ones — **until the matchmaker is
// replaced**, at which point it also refuses the honest successor, because from
// here the successor and the forger look identical.

#include "vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace marathon::lobby {

struct PlayerLog {
    std::string name;
    std::vector<std::string> heard;

    /// Matches this player ACTED ON (left the lobby for), split by whether Loom
    /// vouched for the statement.
    std::int64_t joined_attested = 0;
    std::int64_t joined_unattested = 0;
    /// Matches refused because the statement carried no attestation. In a push
    /// lobby this counts REAL matches too, which is the finding.
    std::int64_t refused_unattested = 0;

    std::int64_t chatter = 0;
    std::int64_t lobby_updates = 0;
    bool in_lobby = false;
    std::string current_match;

    bool saw(const std::string& needle) const {
        for (const std::string& l : heard) {
            if (l.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

struct PlayerState {
    std::int64_t joined = 0;
    std::int64_t matched = 0;
    std::int64_t refused = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(PlayerState, 1, ZEN_FIELD(joined), ZEN_FIELD(matched), ZEN_FIELD(refused));
};

class Player : public loom::WeaveBase<Player, PlayerState,
                                      loom::Accept<LobbyJoined, JoinRefused, LobbyChanged,
                                                   MatchCreated, MatchCancelled, LobbyChatter,
                                                   loom::Ack, loom::Refused>,
                                      loom::Emit<JoinLobby, LeaveLobby, SetReady, SeekMatch>> {
public:
    Player(PlayerLog& log, bool require_attestation)
        : log_(&log), strict_(require_attestation) {}

    void on(const LobbyJoined& j, loom::Mail& mail) {
        log_->in_lobby = true;
        ++state_.joined;
        log_->heard.push_back("joined as " + j.player + " in seat " + std::to_string(j.seat) +
                              attested(mail));
    }

    void on(const JoinRefused& r, loom::Mail& mail) {
        log_->heard.push_back("join refused: " + r.reason + attested(mail));
    }

    void on(const LobbyChanged& c, loom::Mail&) {
        ++log_->lobby_updates;
        std::string who;
        for (const std::string& m : c.members) {
            who += (who.empty() ? "" : ",") + m;
        }
        log_->heard.push_back("lobby now: [" + who + "] ready=" +
                              std::to_string(c.ready.size()));
    }

    /// THE DECISION THIS WHOLE PROJECT IS ABOUT.
    void on(const MatchCreated& m, loom::Mail& mail) {
        const bool attested_now = mail.answers_ask();
        if (strict_ && !attested_now) {
            ++state_.refused;
            ++log_->refused_unattested;
            log_->heard.push_back("REFUSED match " + m.match +
                                  ": nothing vouches that the matchmaking service said this");
            return;
        }
        ++state_.matched;
        (attested_now ? log_->joined_attested : log_->joined_unattested) += 1;
        log_->in_lobby = false;
        log_->current_match = m.match;
        std::string with;
        for (const std::string& p : m.players) {
            with += (with.empty() ? "" : ",") + p;
        }
        log_->heard.push_back("ACTED ON match " + m.match + " on " + m.server + " with [" + with +
                              "]" + attested(mail));
    }

    void on(const MatchCancelled& c, loom::Mail& mail) {
        log_->heard.push_back("match " + c.match + " cancelled: " + c.reason + attested(mail));
    }

    void on(const LobbyChatter& c, loom::Mail&) {
        ++log_->chatter;
        log_->heard.push_back("<" + c.from + "> " + c.text);
    }

    void on(const loom::Ack&, loom::Mail&) { log_->heard.push_back("ok"); }
    void on(const loom::Refused& r, loom::Mail&) {
        log_->heard.push_back("refused: " + r.reason);
    }

private:
    static const char* attested(const loom::Mail& mail) {
        return mail.answers_ask() ? "  [attested]" : "  [unattested]";
    }

    PlayerLog* log_;
    bool strict_;
};

} // namespace marathon::lobby

#endif // MARATHON_LOBBY_PLAYER_HPP
