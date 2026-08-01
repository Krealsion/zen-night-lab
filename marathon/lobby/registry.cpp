// The lobby registry — the authority on WHO IS HERE, and nothing else.
//
// It is the second weave that legitimately speaks about lobby state, and having
// two is what makes this project's question real rather than hypothetical: a
// player hears about the lobby from the registry and about matches from the
// matchmaker, and the two facts have very different consequences if believed
// wrongly.
//
// It publishes `LobbyChanged` — an ordinary publication, with no attestation at
// all — and it is a CONSUMER of `MatchCreated`, which is where it has exactly
// the same problem the players have: a forged match empties its lobby.

#include "vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace marathon::lobby;

struct Member {
    std::string name;
    std::string weave; ///< canonical decimal of the player's WeaveId
    bool ready = false;
    ZEN_SHAPE(Member, 1, ZEN_FIELD(name), ZEN_FIELD(weave), ZEN_FIELD(ready));
};

struct RegistryState {
    std::vector<Member> members;
    std::int64_t joined = 0;
    std::int64_t left = 0;
    std::int64_t matched_away = 0; ///< removed because a MatchCreated named them
    ZEN_EXPOSE();
    ZEN_SHAPE(RegistryState, 1, ZEN_FIELD(members), ZEN_FIELD(joined), ZEN_FIELD(left),
              ZEN_FIELD(matched_away));
};

class Registry
    : public loom::WeaveBase<Registry, RegistryState,
                             loom::Accept<JoinLobby, LeaveLobby, SetReady, LobbyStatus,
                                          MatchStarted, loom::Activated>,
                             loom::Emit<LobbyJoined, JoinRefused, LobbyChanged, loom::Result,
                                        loom::Ack, loom::Refused>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) { (void)activation_.accept(mail, a); }

    void on(const JoinLobby& j, loom::Mail& mail) {
        const std::string who = std::to_string(mail.sender().value);
        if (state_.members.size() >= kMaxPlayers) {
            (void)mail.answer(JoinRefused{"the lobby is full (" + std::to_string(kMaxPlayers) +
                                          " players)"});
            return;
        }
        if (j.player.empty()) {
            (void)mail.answer(JoinRefused{"a player needs a name"});
            return;
        }
        for (const Member& m : state_.members) {
            if (m.name == j.player) {
                (void)mail.answer(
                    JoinRefused{"somebody is already in this lobby as '" + j.player + "'"});
                return;
            }
            if (m.weave == who) {
                (void)mail.answer(JoinRefused{"you are already in this lobby as '" + m.name +
                                              "'"});
                return;
            }
        }
        state_.members.push_back(Member{j.player, who, false});
        ++state_.joined;
        (void)mail.answer(
            LobbyJoined{j.player, static_cast<std::int64_t>(state_.members.size())});
        announce(mail);
    }

    void on(const LeaveLobby&, loom::Mail& mail) {
        const std::string who = std::to_string(mail.sender().value);
        for (std::size_t i = 0; i < state_.members.size(); ++i) {
            if (state_.members[i].weave != who) {
                continue;
            }
            state_.members.erase(state_.members.begin() + static_cast<std::ptrdiff_t>(i));
            ++state_.left;
            (void)mail.answer(loom::Ack{});
            announce(mail);
            return;
        }
        (void)mail.answer(loom::Refused{"you are not in this lobby"});
    }

    void on(const SetReady& r, loom::Mail& mail) {
        const std::string who = std::to_string(mail.sender().value);
        for (Member& m : state_.members) {
            if (m.weave != who) {
                continue;
            }
            m.ready = r.ready;
            (void)mail.answer(loom::Ack{});
            announce(mail);
            return;
        }
        (void)mail.answer(loom::Refused{"you are not in this lobby"});
    }

    /// THE REGISTRY HAS THE PLAYERS' PROBLEM, AND WORSE.
    ///
    /// When a match starts the matched players are no longer in the lobby, so
    /// this weave acts on it. But it is not a PARTY to the match -- it asked
    /// nobody anything -- so what reaches it is a PUBLICATION, and a publication
    /// can never be attested to anybody in any style. The pull matchmaker's whole
    /// mechanism protects the players and does exactly nothing for this weave.
    ///
    /// It does the one thing available: it only removes members it actually has.
    /// That bounds the damage without touching the cause.
    void on(const MatchStarted& m, loom::Mail&) {
        for (const std::string& name : m.players) {
            for (std::size_t i = 0; i < state_.members.size(); ++i) {
                if (state_.members[i].name == name) {
                    state_.members.erase(state_.members.begin() +
                                         static_cast<std::ptrdiff_t>(i));
                    ++state_.matched_away;
                    break;
                }
            }
        }
    }

    void on(const LobbyStatus&, loom::Mail& mail) {
        std::string here;
        std::int64_t ready = 0;
        for (const Member& m : state_.members) {
            here += (here.empty() ? "" : ",") + m.name + (m.ready ? "*" : "");
            ready += m.ready ? 1 : 0;
        }
        (void)mail.answer(loom::Result{
            "lobby: joined=" + std::to_string(state_.joined) + " left=" +
            std::to_string(state_.left) + " matched_away=" +
            std::to_string(state_.matched_away) + " here=" + std::to_string(state_.members.size()) +
            " ready=" + std::to_string(ready) + " [" + (here.empty() ? "(empty)" : here) + "]"});
    }

private:
    /// AN ORDINARY PUBLICATION, with no attestation. Everybody who cares about
    /// the lobby learns about it this way, including the matchmaker.
    void announce(loom::Mail& mail) {
        LobbyChanged change;
        for (const Member& m : state_.members) {
            change.members.push_back(m.name);
            if (m.ready) {
                change.ready.push_back(m.name);
                change.ready_weaves.push_back(m.weave);
            }
        }
        mail.publish(change);
    }

    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Registry)
