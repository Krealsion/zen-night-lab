#ifndef MARATHON_FARM_REQUESTER_HPP
#define MARATHON_FARM_REQUESTER_HPP

// The example consumer — somebody who wants a build, as a host-native weave.
//
// It is written to be pedantic about two things the download manager's client
// was not asked to care about, because this domain has them and that one did
// not:
//
//   * PROGRESS CAN GO BACKWARDS, and that is a CONTRACT rather than a bug. A
//     build resumed on a replaced worker starts again at `fetch`. The requester
//     therefore tracks progress PER ATTEMPT, and records separately whether the
//     stage sequence ever went backwards WITHIN an attempt — which would be a
//     real defect — from whether the attempt number went up, which is the farm
//     keeping its promise.
//   * A BUILD MAY BE PROMISED FROM BEHIND A QUEUE. `BuildAccepted::queued_behind`
//     is recorded, because "accepted" and "started" are different facts and a
//     consumer that conflated them would misread every busy farm.

#include "vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace marathon::farm {

struct RequesterBook {
    std::map<std::uint64_t, std::string> outstanding; ///< correlation -> build id
    std::vector<std::string> heard;
    std::uint64_t next_correlation = 1;

    // ---- the measurement ----------------------------------------------------
    std::int64_t accepts_attested = 0;
    std::int64_t accepts_unattested = 0;
    std::int64_t terminals_attested = 0;
    std::int64_t terminals_unattested = 0;
    std::int64_t progress_attested = 0;
    std::int64_t progress_unattested = 0;
    std::int64_t ignored = 0;

    /// Every progress report, in arrival order: (id, attempt, stage index).
    std::vector<std::tuple<std::string, std::int64_t, std::int64_t>> progress;

    /// Did the stage index ever go DOWN inside one attempt? That would be a real
    /// defect, as opposed to a restart, which is the contract.
    bool backwards_within_an_attempt = false;

    /// The highest attempt number this requester was ever shown, per build.
    std::map<std::string, std::int64_t> highest_attempt;

    std::uint64_t open(const std::string& id) {
        const std::uint64_t c = next_correlation++;
        outstanding[c] = id;
        return c;
    }
};

struct RequesterState {
    std::int64_t accepted = 0;
    std::int64_t rejected = 0;
    std::int64_t progress = 0;
    std::int64_t succeeded = 0;
    std::int64_t failed = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(RequesterState, 1, ZEN_FIELD(accepted), ZEN_FIELD(rejected), ZEN_FIELD(progress),
              ZEN_FIELD(succeeded), ZEN_FIELD(failed));
};

class Requester : public loom::WeaveBase<Requester, RequesterState,
                                         loom::Accept<BuildAccepted, BuildRejected, BuildProgress,
                                                      BuildSucceeded, BuildFailed, loom::Ack,
                                                      loom::Refused>,
                                         loom::Emit<SubmitBuild, WithdrawBuild>> {
public:
    explicit Requester(RequesterBook& book) : book_(&book) {}

    void on(const BuildAccepted& a, loom::Mail& mail) {
        if (!mine(mail, a.id, "accepted")) {
            return;
        }
        (mail.answers_ask() ? book_->accepts_attested : book_->accepts_unattested) += 1;
        ++state_.accepted;
        book_->heard.push_back("accepted " + a.id + " (queued behind " +
                               std::to_string(a.queued_behind) + ")" + attested(mail));
    }

    void on(const BuildRejected& r, loom::Mail& mail) {
        if (!mine(mail, r.id, "rejected")) {
            return;
        }
        (mail.answers_ask() ? book_->terminals_attested : book_->terminals_unattested) += 1;
        ++state_.rejected;
        book_->heard.push_back("rejected " + r.id + ": " + r.reason + attested(mail));
        book_->outstanding.erase(mail.correlation());
    }

    /// Progress, per attempt. See the header for why the two "went backwards"
    /// questions are different questions.
    void on(const BuildProgress& p, loom::Mail& mail) {
        const std::string* id = lookup(mail.correlation());
        if (id == nullptr || *id != p.id) {
            ignore("progress for '" + p.id + "' matches no build I am waiting on");
            return;
        }
        (mail.answers_ask() ? book_->progress_attested : book_->progress_unattested) += 1;
        ++state_.progress;

        const auto seen = last_of(p.id, p.attempt);
        if (seen >= 0 && p.step <= seen) {
            book_->backwards_within_an_attempt = true;
        }
        std::int64_t& high = book_->highest_attempt[p.id];
        high = p.attempt > high ? p.attempt : high;
        book_->progress.emplace_back(p.id, p.attempt, p.step);
        book_->heard.push_back("progress " + p.id + " attempt " + std::to_string(p.attempt) +
                               ": " + p.stage + " (" + std::to_string(p.step) + "/" +
                               std::to_string(p.of_steps) + ") on " + p.worker);
    }

    void on(const BuildSucceeded& s, loom::Mail& mail) {
        if (!mine(mail, s.id, "succeeded")) {
            return;
        }
        (mail.answers_ask() ? book_->terminals_attested : book_->terminals_unattested) += 1;
        ++state_.succeeded;
        book_->heard.push_back("succeeded " + s.id + ": " + s.artifact + " on " + s.worker +
                               " after " + std::to_string(s.attempts) + " attempt(s)" +
                               attested(mail));
        book_->outstanding.erase(mail.correlation());
    }

    void on(const BuildFailed& f, loom::Mail& mail) {
        if (!mine(mail, f.id, "failed")) {
            return;
        }
        (mail.answers_ask() ? book_->terminals_attested : book_->terminals_unattested) += 1;
        ++state_.failed;
        book_->heard.push_back("failed " + f.id +
                               (f.stage.empty() ? "" : " in " + f.stage) +
                               (f.worker.empty() ? "" : " on " + f.worker) + ": " + f.reason +
                               attested(mail));
        book_->outstanding.erase(mail.correlation());
    }

    void on(const loom::Ack&, loom::Mail&) { book_->heard.push_back("withdrawal acknowledged"); }
    void on(const loom::Refused& r, loom::Mail&) {
        book_->heard.push_back("withdrawal refused: " + r.reason);
    }

private:
    const std::string* lookup(std::uint64_t correlation) const {
        const auto it = book_->outstanding.find(correlation);
        return it == book_->outstanding.end() ? nullptr : &it->second;
    }

    bool mine(const loom::Mail& mail, const std::string& id, const char* kind) {
        const std::string* open = lookup(mail.correlation());
        if (open == nullptr || *open != id) {
            ignore(std::string(kind) + " '" + id + "' matches no build I am waiting on");
            return false;
        }
        return true;
    }

    /// The last stage index seen for (id, attempt), or -1.
    std::int64_t last_of(const std::string& id, std::int64_t attempt) const {
        std::int64_t last = -1;
        for (const auto& p : book_->progress) {
            if (std::get<0>(p) == id && std::get<1>(p) == attempt) {
                last = std::get<2>(p);
            }
        }
        return last;
    }

    static const char* attested(const loom::Mail& mail) {
        return mail.answers_ask() ? "  [attested]" : "  [unattested]";
    }

    void ignore(std::string why) {
        ++book_->ignored;
        book_->heard.push_back("IGNORED: " + std::move(why));
    }

    RequesterBook* book_;
};

} // namespace marathon::farm

#endif // MARATHON_FARM_REQUESTER_HPP
