#ifndef MARATHON_IMPORT_HARNESS_HPP
#define MARATHON_IMPORT_HARNESS_HPP

// The import pipeline's host. Fourth project, fourth time writing the same six
// fixture pieces (operator, coordinator-with-a-raw-handle-pointer, rogue,
// beat-budget observer, delivery-order tap, trace observer). Recorded in
// FRICTION.md as F12 rather than extracted: a repeating TEST FIXTURE is weaker
// evidence than a repeating application, and the shared-helper law is about
// domain vocabulary.
//
// The one substitution is the Timer's CLOCK (`zengine-timer-virtual.so`).
//
// WHAT IS NEW HERE: a rogue that forges a CHOICE. Three projects have had to
// write "there is no way to check the sender"; this one can, and the rogue
// exists to prove the wall is real rather than assumed.

#include "requester.hpp"
#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include <zen/host/prepared_replacement.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace marathon::import_testing {

namespace imp = marathon::importer;
namespace timer = zengine::timer;

#ifndef MARATHON_IMPORT_RUNTIME_DIR
#error "MARATHON_IMPORT_RUNTIME_DIR must be defined by the build"
#endif

inline std::string weave_path(const std::string& stem) {
    return std::string(MARATHON_IMPORT_RUNTIME_DIR) + "/" + stem + ".so";
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

// ---- the curator: coordinator of an importer replacement -------------------

struct CuratorDesk {
    loom::PreparedReplacement* upgrade = nullptr;
    std::vector<loom::TxnResult> offers;
    std::vector<imp::PendingImport> described;
    std::int64_t next_menu = 1; ///< where the incumbent's menu numbering got to
    bool described_arrived = false;
    std::vector<std::string> notes;
    std::string catalogue_answer = "house";
    std::int64_t offered_without_handle = 0;
    std::int64_t unattested = 0;
};

struct CuratorState {
    std::int64_t descriptions = 0;
    std::int64_t ready = 0;
    std::int64_t not_ready = 0;
    std::int64_t consulted = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(CuratorState, 1, ZEN_FIELD(descriptions), ZEN_FIELD(ready), ZEN_FIELD(not_ready),
              ZEN_FIELD(consulted));
};

class Curator : public loom::WeaveBase<Curator, CuratorState,
                                       loom::Accept<imp::ConversationsDescribed,
                                                    imp::ImporterReady, imp::ImporterNotReady,
                                                    imp::AskCatalogueName>,
                                       loom::Emit<imp::DescribeConversations,
                                                  imp::PrepareImporter, imp::CatalogueNameIs>> {
public:
    explicit Curator(CuratorDesk& desk) : desk_(&desk) {}

    void on(const imp::ConversationsDescribed& d, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ++desk_->unattested;
            return;
        }
        ++state_.descriptions;
        desk_->described = d.open;
        desk_->next_menu = d.next_menu;
        desk_->described_arrived = true;
        desk_->notes.push_back("the importer is mid-conversation with " +
                               std::to_string(d.open.size()) + " requester(s)");
    }

    void on(const imp::ImporterReady& r, loom::Mail&) {
        ++state_.ready;
        desk_->notes.push_back("candidate says READY (adopted " + std::to_string(r.adopted) + ")");
        offer(loom::PreparationAnswer::Ready);
    }

    void on(const imp::ImporterNotReady& r, loom::Mail&) {
        ++state_.not_ready;
        desk_->notes.push_back("candidate says NOT READY: " + r.reason);
        offer(loom::PreparationAnswer::Refused);
    }

    void on(const imp::AskCatalogueName&, loom::Mail& mail) {
        ++state_.consulted;
        (void)mail.answer(imp::CatalogueNameIs{desk_->catalogue_answer});
    }

private:
    void offer(loom::PreparationAnswer answer) {
        if (desk_->upgrade == nullptr) {
            ++desk_->offered_without_handle;
            return;
        }
        desk_->offers.push_back(desk_->upgrade->offer_current_answer(answer));
    }
    CuratorDesk* desk_;
};

// ---- a rogue: the whole threat model, as one ordinary weave ----------------

struct RogueState {
    std::int64_t sent = 0;
    ZEN_SHAPE(RogueState, 1, ZEN_FIELD(sent));
};

/// "Choose for somebody else." The ticket and the menu identity are BOTH public
/// — they are on the wire — so this forgery has everything a listener could have
/// collected. Everything except being the requester.
struct ForgeChoice {
    std::string ticket;
    std::string menu;
    std::string choice;
    std::int64_t correlation = 0;
    ZEN_SHAPE(ForgeChoice, 1, ZEN_FIELD(ticket), ZEN_FIELD(menu), ZEN_FIELD(choice),
              ZEN_FIELD(correlation));
};

struct ForgeImporterReady {
    ZEN_SHAPE(ForgeImporterReady, 1);
};

class Rogue : public loom::WeaveBase<Rogue, RogueState,
                                     loom::Accept<ForgeChoice, ForgeImporterReady>,
                                     loom::Emit<imp::ChooseOption, imp::ImporterReady>> {
public:
    explicit Rogue(loom::WeaveId curator) : curator_(curator) {}

    void on(const ForgeChoice& f, loom::Mail& mail) {
        ++state_.sent;
        mail.send_to_role(imp::kImporterRole,
                          imp::ChooseOption{f.ticket, f.menu, f.choice},
                          static_cast<std::uint64_t>(f.correlation));
    }

    void on(const ForgeImporterReady&, loom::Mail& mail) {
        ++state_.sent;
        mail.send(curator_, imp::ImporterReady{0});
    }

private:
    loom::WeaveId curator_;
};

// ---- the inspector ---------------------------------------------------------

struct InspectorState {
    std::int64_t results = 0;
    std::int64_t ignored = 0;
    ZEN_SHAPE(InspectorState, 1, ZEN_FIELD(results), ZEN_FIELD(ignored));
};

struct AskStatus {
    ZEN_SHAPE(AskStatus, 1);
};

class Inspector : public loom::WeaveBase<Inspector, InspectorState,
                                         loom::Accept<AskStatus, loom::Result>,
                                         loom::Emit<imp::ImporterStatus>> {
public:
    explicit Inspector(std::vector<std::string>& seen) : seen_(&seen) {}
    void on(const AskStatus&, loom::Mail& mail) {
        mail.send_to_role(imp::kImporterRole, imp::ImporterStatus{});
    }
    void on(const loom::Result& r, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            ++state_.ignored;
            return;
        }
        ++state_.results;
        seen_->push_back(r.value);
    }

private:
    std::vector<std::string>* seen_;
};

// ---- the fixture -----------------------------------------------------------

class Pipeline {
public:
    Pipeline() {
        control_ = loom::mount_control(kernel_, bus_);
        manager_ = loom::mount_manager(control_, bus_);

        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager_);
        op_ = loom::mount_granted<Operator>(bus_, std::move(reach), oplog_);

        curator_ = loom::mount<Curator>(bus_, desk_);
        requester_ = loom::mount<imp::Requester>(bus_, book_);
        second_ = loom::mount<imp::Requester>(bus_, second_book_);

        loom::Grant rogue_grant;
        rogue_grant.allow_to_any(ForgeChoice::zen_name, ForgeChoice::zen_version);
        rogue_grant.allow_to_any(ForgeImporterReady::zen_name, ForgeImporterReady::zen_version);
        rogue_grant.allow_to_any(imp::ChooseOption::zen_name, imp::ChooseOption::zen_version);
        rogue_grant.allow_to_any(imp::ImporterReady::zen_name, imp::ImporterReady::zen_version);
        rogue_ = loom::mount_granted<Rogue>(bus_, std::move(rogue_grant), curator_);

        loom::Grant inspector_grant;
        inspector_grant.allow_to_any(AskStatus::zen_name, AskStatus::zen_version);
        inspector_grant.allow_to_role(imp::ImporterStatus::zen_name,
                                      imp::ImporterStatus::zen_version, imp::kImporterRole);
        inspector_ = loom::mount_granted<Inspector>(bus_, std::move(inspector_grant), status_);

        if (const char* on = std::getenv("MARATHON_TRACE"); on != nullptr && *on == '1') {
            bus_.add_observer([](const loom::BusEvent& ev) {
                const char* kind = ev.kind == loom::EventKind::Delivered  ? "deliver"
                                   : ev.kind == loom::EventKind::Refused  ? "REFUSED"
                                   : ev.kind == loom::EventKind::Died     ? "died"
                                                                          : "revived";
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
            ++beats_;
            if (budget_ > 0 && --budget_ == 0) {
                bus_.stop();
            }
        });
    }

    loom::Switchboard& bus() { return bus_; }
    loom::Kernel& kernel() { return kernel_; }
    imp::RequesterBook& book() { return book_; }
    imp::RequesterBook& second_book() { return second_book_; }
    OperatorLog& oplog() { return oplog_; }
    CuratorDesk& desk() { return desk_; }
    loom::WeaveId curator() const { return curator_; }
    loom::WeaveId requester() const { return requester_; }
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

    /// Ask for an import AS the requester. The correlation returned is the one
    /// every later message about this conversation echoes — including the choice,
    /// so that the resolution answers the right ask.
    std::uint64_t ask(const std::string& ticket, const std::string& file) {
        const std::uint64_t corr = book_.open(ticket);
        bus_.send_as_to_role(requester_, imp::kImporterRole,
                             loom::Message(loom::to_value(imp::ImportAsset{ticket, file}),
                                           requester_, requester_, corr));
        return corr;
    }

    std::uint64_t ask_second(const std::string& ticket, const std::string& file) {
        const std::uint64_t corr = second_book_.open(ticket);
        bus_.send_as_to_role(second_, imp::kImporterRole,
                             loom::Message(loom::to_value(imp::ImportAsset{ticket, file}),
                                           second_, second_, corr));
        return corr;
    }

    /// Choose, AS the requester, naming the menu the requester currently
    /// believes is open. Passing a menu explicitly is how the stale-choice case
    /// is expressed without the harness cheating.
    void choose(const std::string& ticket, const std::string& choice, std::uint64_t corr,
                const std::string& menu = std::string{}) {
        const std::string m = menu.empty() ? book_.menu_of[ticket] : menu;
        bus_.send_as_to_role(requester_, imp::kImporterRole,
                             loom::Message(loom::to_value(imp::ChooseOption{ticket, m, choice}),
                                           requester_, requester_, corr));
    }

    void abandon(const std::string& ticket, std::uint64_t corr) {
        bus_.send_as_to_role(requester_, imp::kImporterRole,
                             loom::Message(loom::to_value(imp::AbandonImport{ticket}), requester_,
                                           requester_, corr));
    }

    void describe_conversations() {
        desk_.described_arrived = false;
        bus_.send_as_to_role(curator_, imp::kImporterRole,
                             loom::Message(loom::to_value(imp::DescribeConversations{}), curator_,
                                           curator_, 0));
    }

    template <class T>
    void rogue_does(const T& order_) {
        bus_.send_as(rogue_, rogue_, loom::Message(loom::to_value(order_), rogue_, rogue_, 0));
    }

    void ask_status() {
        bus_.send_as(inspector_, inspector_,
                     loom::Message(loom::to_value(AskStatus{}), inspector_, inspector_, 0));
    }

    loom::PreparedReplacement& new_upgrade() {
        upgrade_ = loom::PreparedReplacement(bus_, kernel_);
        desk_.upgrade = &upgrade_;
        return upgrade_;
    }
    loom::PreparedReplacement& upgrade() { return upgrade_; }
    void forget_upgrade() { desk_.upgrade = nullptr; }

    void boot(const std::string& importer = "import-pipeline") {
        load("zengine-timer-virtual", timer::kTimerRole);
        load(importer, imp::kImporterRole);
        pump(20);
    }

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
    loom::WeaveId curator_{};
    loom::WeaveId requester_{};
    loom::WeaveId second_{};
    loom::WeaveId rogue_{};
    loom::WeaveId inspector_{};
    OperatorLog oplog_;
    CuratorDesk desk_;
    imp::RequesterBook book_;
    imp::RequesterBook second_book_;
    std::vector<std::string> status_;
    loom::PreparedReplacement upgrade_{bus_, kernel_};
    std::deque<std::vector<std::string>> watched_;
    std::int64_t budget_ = 0;
    std::int64_t beats_ = 0;
};

} // namespace marathon::import_testing

#endif // MARATHON_IMPORT_HARNESS_HPP
