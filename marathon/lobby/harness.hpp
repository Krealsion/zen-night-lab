#ifndef MARATHON_LOBBY_HARNESS_HPP
#define MARATHON_LOBBY_HARNESS_HPP

// The lobby's host. Fifth project, fifth time writing the same fixture pieces
// (operator, coordinator-with-a-raw-handle-pointer, rogue, beat-budget observer,
// trace observer). Recorded in FRICTION.md as F12, not extracted.
//
// The one substitution is the Timer's CLOCK — and this project barely uses it,
// because a lobby is event-driven: nothing here waits for time to pass.
//
// FOUR PLAYERS, TWO POLICIES. Two players act on any `MatchCreated`; two act
// only on one Loom vouched for. Running both against both matchmaker builds is
// what fills in the truth table this project exists to produce.

#include "player.hpp"
#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include <zen/host/prepared_replacement.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace marathon::lobby_testing {

namespace lob = marathon::lobby;
namespace timer = zengine::timer;

#ifndef MARATHON_LOBBY_RUNTIME_DIR
#error "MARATHON_LOBBY_RUNTIME_DIR must be defined by the build"
#endif

inline std::string weave_path(const std::string& stem) {
    return std::string(MARATHON_LOBBY_RUNTIME_DIR) + "/" + stem + ".so";
}

// ---- the operator ----------------------------------------------------------

struct OperatorLog {
    std::map<std::uint64_t, std::string> pending;
    std::vector<std::string> answers;
    std::uint64_t next_correlation = 1;
};

struct OperatorState {
    std::int64_t answers = 0;
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

class Operator : public loom::WeaveBase<Operator, OperatorState,
                                        loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                        loom::Emit<loom::LoadWeave>> {
public:
    explicit Operator(OperatorLog& log) : log_(&log) {}
    void on(const loom::Result& r, loom::Mail& mail) { record(mail, "-> " + r.value); }
    void on(const loom::Ack&, loom::Mail& mail) { record(mail, "-> done"); }
    void on(const loom::Refused& r, loom::Mail& mail) { record(mail, "-> refused: " + r.reason); }

private:
    void record(loom::Mail& mail, const std::string& outcome) {
        ++state_.answers;
        const auto it = log_->pending.find(mail.correlation());
        if (it == log_->pending.end()) {
            log_->answers.push_back("(unsolicited) " + outcome);
            return;
        }
        log_->answers.push_back(it->second + " " + outcome);
        log_->pending.erase(it);
    }
    OperatorLog* log_;
};

// ---- the warden: coordinator of a matchmaker replacement -------------------

struct WardenDesk {
    loom::PreparedReplacement* upgrade = nullptr;
    std::vector<loom::TxnResult> offers;
    std::vector<lob::WaitingPlayer> waiting;
    std::int64_t held_answer_rights = 0;
    bool described_arrived = false;
    std::vector<std::string> notes;
    std::int64_t match_size = static_cast<std::int64_t>(lob::kMatchSize);
    std::int64_t offered_without_handle = 0;
};

struct WardenState {
    std::int64_t descriptions = 0;
    std::int64_t ready = 0;
    std::int64_t not_ready = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(WardenState, 1, ZEN_FIELD(descriptions), ZEN_FIELD(ready), ZEN_FIELD(not_ready));
};

class Warden : public loom::WeaveBase<Warden, WardenState,
                                      loom::Accept<lob::WaitingDescribed, lob::MatchmakerReady,
                                                   lob::MatchmakerNotReady>,
                                      loom::Emit<lob::DescribeWaiting, lob::PrepareMatchmaker>> {
public:
    explicit Warden(WardenDesk& desk) : desk_(&desk) {}

    void on(const lob::WaitingDescribed& d, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        ++state_.descriptions;
        desk_->waiting = d.waiting;
        desk_->held_answer_rights = d.held_answer_rights;
        desk_->described_arrived = true;
        desk_->notes.push_back("the office owes " + std::to_string(d.waiting.size()) +
                               " player(s), " + std::to_string(d.held_answer_rights) +
                               " of them an ATTESTED answer no successor can give");
    }

    void on(const lob::MatchmakerReady& r, loom::Mail&) {
        ++state_.ready;
        desk_->notes.push_back("candidate says READY (inherited " +
                               std::to_string(r.inherited) + ")");
        offer(loom::PreparationAnswer::Ready);
    }

    void on(const lob::MatchmakerNotReady& r, loom::Mail&) {
        ++state_.not_ready;
        desk_->notes.push_back("candidate says NOT READY: " + r.reason);
        offer(loom::PreparationAnswer::Refused);
    }

private:
    void offer(loom::PreparationAnswer answer) {
        if (desk_->upgrade == nullptr) {
            ++desk_->offered_without_handle;
            return;
        }
        desk_->offers.push_back(desk_->upgrade->offer_current_answer(answer));
    }
    WardenDesk* desk_;
};

// ---- a rogue ---------------------------------------------------------------

struct RogueState {
    std::int64_t sent = 0;
    ZEN_SHAPE(RogueState, 1, ZEN_FIELD(sent));
};

/// "Tell those players they are in a match." An UNRELATED WEAVE holding an
/// ordinary grant for an ordinary shape. It knows the player names and addresses
/// because `LobbyChanged` is a publication — none of this is a secret.
struct ForgeMatch {
    std::string match;
    std::vector<std::string> players;
    std::vector<std::string> weaves;
    ZEN_SHAPE(ForgeMatch, 1, ZEN_FIELD(match), ZEN_FIELD(players), ZEN_FIELD(weaves));
};

/// "Announce to the world that a match started." Aimed at OBSERVERS rather than
/// parties -- which is the attack neither matchmaker style can do anything about.
struct ForgeMatchStarted {
    std::string match;
    std::vector<std::string> players;
    ZEN_SHAPE(ForgeMatchStarted, 1, ZEN_FIELD(match), ZEN_FIELD(players));
};

class Rogue : public loom::WeaveBase<Rogue, RogueState,
                                     loom::Accept<ForgeMatch, ForgeMatchStarted>,
                                     loom::Emit<lob::MatchCreated, lob::MatchStarted>> {
public:
    void on(const ForgeMatchStarted& f, loom::Mail& mail) {
        ++state_.sent;
        mail.publish(lob::MatchStarted{f.match, f.players});
    }

    void on(const ForgeMatch& f, loom::Mail& mail) {
        ++state_.sent;
        for (const std::string& w : f.weaves) {
            std::uint64_t id = 0;
            for (const char ch : w) {
                id = id * 10 + static_cast<std::uint64_t>(ch - '0');
            }
            mail.send(loom::WeaveId{id},
                      lob::MatchCreated{f.match, f.players, "server-" + f.match});
        }
    }
};

// ---- the inspector ---------------------------------------------------------

struct InspectorState {
    std::int64_t results = 0;
    ZEN_SHAPE(InspectorState, 1, ZEN_FIELD(results));
};

struct AskLobby {
    ZEN_SHAPE(AskLobby, 1);
};
struct AskOffice {
    ZEN_SHAPE(AskOffice, 1);
};
/// Tell the office to speak in its PERSONAL capacity. Routed through the console
/// weave rather than sent as the office itself, because a `send_as(office, ...)`
/// is authorized against the OFFICE's grant and `SpeakPersonally` is not
/// something the office emits — F8, learned twice, avoided here on purpose.
struct PokePersonal {
    std::string match;
    std::vector<std::string> players;
    std::vector<std::string> weaves;
    ZEN_SHAPE(PokePersonal, 1, ZEN_FIELD(match), ZEN_FIELD(players), ZEN_FIELD(weaves));
};

class Inspector
    : public loom::WeaveBase<Inspector, InspectorState,
                             loom::Accept<AskLobby, AskOffice, PokePersonal, loom::Result>,
                             loom::Emit<lob::LobbyStatus, lob::MatchmakerStatus,
                                        lob::SpeakPersonally>> {
public:
    explicit Inspector(std::vector<std::string>& seen) : seen_(&seen) {}
    void on(const AskLobby&, loom::Mail& mail) {
        mail.send_to_role(lob::kRegistryRole, lob::LobbyStatus{});
    }
    void on(const AskOffice&, loom::Mail& mail) {
        mail.send_to_role(lob::kMatchmakerRole, lob::MatchmakerStatus{});
    }
    void on(const PokePersonal& p, loom::Mail& mail) {
        mail.send_to_role(lob::kMatchmakerRole,
                          lob::SpeakPersonally{p.match, p.players, p.weaves});
    }
    void on(const loom::Result& r, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        ++state_.results;
        seen_->push_back(r.value);
    }

private:
    std::vector<std::string>* seen_;
};

// ---- the fixture -----------------------------------------------------------

/// Four players: two LAX (act on anything), two STRICT (act only on an attested
/// statement). The indices are stable and the tests name them by constant.
enum PlayerSlot : std::size_t { kLax1 = 0, kLax2 = 1, kStrict1 = 2, kStrict2 = 3 };

class Lobby {
public:
    Lobby() {
        control_ = loom::mount_control(kernel_, bus_);
        manager_ = loom::mount_manager(control_, bus_);

        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager_);
        op_ = loom::mount_granted<Operator>(bus_, std::move(reach), oplog_);

        warden_ = loom::mount<Warden>(bus_, desk_);

        static const char* kNames[4] = {"alice", "bob", "carol", "dave"};
        for (std::size_t i = 0; i < 4; ++i) {
            logs_[i].name = kNames[i];
            players_[i] = loom::mount<lob::Player>(bus_, logs_[i], /*strict=*/i >= 2);
        }

        loom::Grant rogue_grant;
        rogue_grant.allow_to_any(ForgeMatch::zen_name, ForgeMatch::zen_version);
        rogue_grant.allow_to_any(ForgeMatchStarted::zen_name, ForgeMatchStarted::zen_version);
        rogue_grant.allow_to_any(lob::MatchCreated::zen_name, lob::MatchCreated::zen_version);
        rogue_grant.allow_to_any(lob::MatchStarted::zen_name, lob::MatchStarted::zen_version);
        rogue_ = loom::mount_granted<Rogue>(bus_, std::move(rogue_grant));

        loom::Grant inspector_grant;
        inspector_grant.allow_to_any(AskLobby::zen_name, AskLobby::zen_version);
        inspector_grant.allow_to_any(AskOffice::zen_name, AskOffice::zen_version);
        inspector_grant.allow_to_role(lob::LobbyStatus::zen_name, lob::LobbyStatus::zen_version,
                                      lob::kRegistryRole);
        inspector_grant.allow_to_role(lob::MatchmakerStatus::zen_name,
                                      lob::MatchmakerStatus::zen_version, lob::kMatchmakerRole);
        inspector_grant.allow_to_any(PokePersonal::zen_name, PokePersonal::zen_version);
        inspector_grant.allow_to_role(lob::SpeakPersonally::zen_name,
                                      lob::SpeakPersonally::zen_version, lob::kMatchmakerRole);
        inspector_ = loom::mount_granted<Inspector>(bus_, std::move(inspector_grant), status_);

        if (const char* on = std::getenv("MARATHON_TRACE"); on != nullptr && *on == '1') {
            bus_.add_observer([](const loom::BusEvent& ev) {
                const char* kind = ev.kind == loom::EventKind::Delivered  ? "deliver"
                                   : ev.kind == loom::EventKind::Refused  ? "REFUSED"
                                                                          : "life";
                std::fprintf(stderr, "[trace] %-7s %s v%u  %llu -> %llu  %s\n", kind,
                             ev.schema_name.c_str(), ev.schema_version,
                             static_cast<unsigned long long>(ev.sender.value),
                             static_cast<unsigned long long>(ev.target.value),
                             ev.kind == loom::EventKind::Refused ? ev.refusal.message().c_str()
                                                                 : "");
            });
        }

        bus_.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered ||
                ev.schema_name != timer::Drive::zen_name) {
                return;
            }
            if (budget_ > 0 && --budget_ == 0) {
                bus_.stop();
            }
        });
    }

    loom::Switchboard& bus() { return bus_; }
    loom::Kernel& kernel() { return kernel_; }
    WardenDesk& desk() { return desk_; }
    OperatorLog& oplog() { return oplog_; }
    loom::WeaveId warden() const { return warden_; }
    lob::PlayerLog& log(std::size_t slot) { return logs_[slot]; }
    loom::WeaveId player(std::size_t slot) const { return players_[slot]; }
    std::string player_weave(std::size_t slot) const {
        return std::to_string(players_[slot].value);
    }
    const std::vector<std::string>& status() const { return status_; }

    std::vector<std::string>* watch(loom::WeaveId who) {
        watched_.push_back(std::vector<std::string>{});
        std::vector<std::string>* sink = &watched_.back();
        bus_.add_observer([sink, who](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Delivered && ev.target == who) {
                sink->push_back(ev.schema_name);
            }
        });
        return sink;
    }

    template <class Cmd>
    std::uint64_t command(std::string label, const Cmd& cmd) {
        const std::uint64_t corr = oplog_.next_correlation++;
        oplog_.pending[corr] = std::move(label);
        bus_.send_as(op_, manager_, loom::Message(loom::to_value(cmd), op_, op_, corr));
        return corr;
    }

    std::uint64_t load(const std::string& stem, const std::string& role = "") {
        return command("load " + stem, loom::LoadWeave{stem, weave_path(stem), role});
    }

    template <class T>
    void as_player(std::size_t slot, const std::string& role, const T& msg,
                   std::uint64_t corr = 0) {
        bus_.send_as_to_role(players_[slot], role,
                             loom::Message(loom::to_value(msg), players_[slot], players_[slot],
                                           corr));
    }

    void join(std::size_t slot) {
        as_player(slot, lob::kRegistryRole, lob::JoinLobby{logs_[slot].name});
    }
    void set_ready(std::size_t slot, bool ready) {
        as_player(slot, lob::kRegistryRole, lob::SetReady{ready});
    }
    /// The pull style's request. The correlation is the player's own; the match
    /// it is eventually told about answers exactly this.
    void seek(std::size_t slot) {
        as_player(slot, lob::kMatchmakerRole, lob::SeekMatch{}, ++seek_corr_);
    }

    template <class T>
    void rogue_does(const T& order_) {
        bus_.send_as(rogue_, rogue_, loom::Message(loom::to_value(order_), rogue_, rogue_, 0));
    }

    /// Make the matchmaker weave speak in its PERSONAL capacity — same WeaveId,
    /// same shape, no office behind it.
    void office_speaks_personally(const std::string& match,
                                  const std::vector<std::size_t>& slots) {
        PokePersonal p;
        p.match = match;
        for (const std::size_t s : slots) {
            p.players.push_back(logs_[s].name);
            p.weaves.push_back(player_weave(s));
        }
        bus_.send_as(inspector_, inspector_,
                     loom::Message(loom::to_value(p), inspector_, inspector_, 0));
    }

    void describe_waiting() {
        desk_.described_arrived = false;
        bus_.send_as_to_role(warden_, lob::kMatchmakerRole,
                             loom::Message(loom::to_value(lob::DescribeWaiting{}), warden_,
                                           warden_, 0));
    }

    void ask_lobby() {
        bus_.send_as(inspector_, inspector_,
                     loom::Message(loom::to_value(AskLobby{}), inspector_, inspector_, 0));
    }
    void ask_office() {
        bus_.send_as(inspector_, inspector_,
                     loom::Message(loom::to_value(AskOffice{}), inspector_, inspector_, 0));
    }

    loom::PreparedReplacement& new_upgrade() {
        upgrade_ = loom::PreparedReplacement(bus_, kernel_);
        desk_.upgrade = &upgrade_;
        return upgrade_;
    }
    loom::PreparedReplacement& upgrade() { return upgrade_; }
    void forget_upgrade() { desk_.upgrade = nullptr; }

    void boot(const std::string& matchmaker = "lobby-matchmaker-push") {
        load("zengine-timer-virtual", timer::kTimerRole);
        load("lobby-registry", lob::kRegistryRole);
        load(matchmaker, lob::kMatchmakerRole);
        pump(20);
    }

    /// A lobby is event-driven: `pump` here mostly means "drain the queue". The
    /// beat budget is still the stop lever, because the Timer's chain never
    /// quiesces once it is running.
    void pump(std::int64_t beats) {
        budget_ = beats;
        bus_.pump();
        budget_ = 0;
    }

private:
    loom::Switchboard bus_;
    loom::Kernel kernel_{bus_};
    loom::WeaveId control_{};
    loom::WeaveId manager_{};
    loom::WeaveId op_{};
    loom::WeaveId warden_{};
    loom::WeaveId rogue_{};
    loom::WeaveId inspector_{};
    std::array<loom::WeaveId, 4> players_{};
    std::array<lob::PlayerLog, 4> logs_{};
    OperatorLog oplog_;
    WardenDesk desk_;
    std::vector<std::string> status_;
    loom::PreparedReplacement upgrade_{bus_, kernel_};
    std::deque<std::vector<std::string>> watched_;
    std::uint64_t seek_corr_ = 100;
    std::int64_t budget_ = 0;
};

} // namespace marathon::lobby_testing

#endif // MARATHON_LOBBY_HARNESS_HPP
