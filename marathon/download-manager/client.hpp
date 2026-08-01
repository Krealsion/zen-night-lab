#ifndef MARATHON_DOWNLOADS_CLIENT_HPP
#define MARATHON_DOWNLOADS_CLIENT_HPP

// The example consumer — a client, as a host-native weave.
//
// It is the other end of every promise the service makes, and it is written to
// be pedantic about the consumer obligation rather than convenient, because what
// this experiment is actually measuring is WHICH PARTS OF A LONG OPERATION CAN
// BE ATTESTED AT ALL.
//
// So the client does not merely accept messages. For every one it records
// whether Loom vouched for it, and the suite reads those counters back. That is
// the whole apparatus: the same client talks to both builds of the service, and
// the difference between the two shows up as a difference in which counters
// moved.
//
//   * `answers_ask()` is available on exactly ONE message per operation, because
//     Loom grants exactly one authenticated answer per request. Which one that is
//     is the service's choice and the client cannot influence it.
//   * Everything else — progress, and whichever of the acknowledgment or the
//     ending did not get the answer — arrives with no attestation whatsoever.
//     The client's only wall is the correlation IT chose, which is a real wall
//     (nothing outside the conversation was told it) and a thin one (it is on the
//     bus for anyone watching).
//
// It is host-native on purpose. A native weave is never `zen.Activated`, so this
// weave has no activation ceremony at all — which keeps the example about
// downloading things.

#include "vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace marathon::downloads {

/// What the client is waiting on and what it heard. Owned by the HOST, so a test
/// or a demo can start downloads as this weave and read back exactly what it
/// experienced.
struct ClientLedger {
    std::map<std::uint64_t, std::string> outstanding; ///< correlation -> ticket
    std::vector<std::string> heard;                   ///< the transcript, in arrival order
    std::uint64_t next_correlation = 1;

    // ---- the measurement ----------------------------------------------------
    std::int64_t accepts_attested = 0;
    std::int64_t accepts_unattested = 0;
    std::int64_t terminals_attested = 0;
    std::int64_t terminals_unattested = 0;
    std::int64_t progress_attested = 0;   ///< expected to stay ZERO, and it is asserted
    std::int64_t progress_unattested = 0;
    std::int64_t ignored = 0;             ///< arrivals that failed the consumer obligation

    std::uint64_t open(const std::string& ticket) {
        const std::uint64_t c = next_correlation++;
        outstanding[c] = ticket;
        return c;
    }

    std::vector<std::string> matching(const std::string& needle) const {
        std::vector<std::string> out;
        for (const std::string& line : heard) {
            if (line.find(needle) != std::string::npos) {
                out.push_back(line);
            }
        }
        return out;
    }

    /// The last byte count this client was told about `ticket`, or -1.
    std::int64_t last_progress_of(const std::string& ticket) const {
        std::int64_t last = -1;
        for (const auto& kv : progress) {
            if (kv.first == ticket) {
                last = kv.second;
            }
        }
        return last;
    }

    /// Every progress report, in arrival order, as (ticket, bytes_done). Kept
    /// separately from the transcript because "did progress go backwards?" is a
    /// question about numbers and not about words.
    std::vector<std::pair<std::string, std::int64_t>> progress;

    /// What this client asked for, per ticket — filled in by the host when it
    /// starts a transfer as this weave, so the client can check a completion's
    /// digest against the source it actually requested. It lives here rather than
    /// inside the weave because `loom::mount` returns an id and not a pointer,
    /// and the host is the party that knows what it asked for.
    std::map<std::string, std::string> sources;
};

struct ClientState {
    std::int64_t accepted = 0;
    std::int64_t refused = 0;
    std::int64_t progress = 0;
    std::int64_t completed = 0;
    std::int64_t failed = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ClientState, 1, ZEN_FIELD(accepted), ZEN_FIELD(refused), ZEN_FIELD(progress),
              ZEN_FIELD(completed), ZEN_FIELD(failed));
};

class Client : public loom::WeaveBase<Client, ClientState,
                                      loom::Accept<DownloadAccepted, DownloadRefused,
                                                   DownloadProgress, DownloadCompleted,
                                                   DownloadFailed, loom::Ack, loom::Refused>,
                                      loom::Emit<StartDownload, CancelDownload>> {
public:
    explicit Client(ClientLedger& ledger) : ledger_(&ledger) {}

    void on(const DownloadAccepted& a, loom::Mail& mail) {
        if (!mine(mail, a.ticket, "accepted")) {
            return;
        }
        (mail.answers_ask() ? ledger_->accepts_attested : ledger_->accepts_unattested) += 1;
        ++state_.accepted;
        ledger_->heard.push_back("accepted " + a.ticket + ": " + a.source + " (" +
                                 std::to_string(a.total_bytes) + " bytes)" + attested(mail));
    }

    /// A refusal ends the conversation: nothing was started, so nothing more is
    /// owed and the correlation is closed here.
    void on(const DownloadRefused& r, loom::Mail& mail) {
        if (!mine(mail, r.ticket, "refused")) {
            return;
        }
        (mail.answers_ask() ? ledger_->terminals_attested : ledger_->terminals_unattested) += 1;
        ++state_.refused;
        ledger_->heard.push_back("refused " + r.ticket + ": " + r.reason + attested(mail));
        ledger_->outstanding.erase(mail.correlation());
    }

    /// Progress does not close anything and cannot be attested. Both facts are
    /// recorded rather than assumed.
    void on(const DownloadProgress& p, loom::Mail& mail) {
        const std::string* ticket = lookup(mail.correlation());
        if (ticket == nullptr || *ticket != p.ticket) {
            ignore("progress for '" + p.ticket + "' matches no transfer I am waiting on");
            return;
        }
        (mail.answers_ask() ? ledger_->progress_attested : ledger_->progress_unattested) += 1;
        ++state_.progress;
        ledger_->progress.emplace_back(p.ticket, p.bytes_done);
    }

    void on(const DownloadCompleted& c, loom::Mail& mail) {
        if (!mine(mail, c.ticket, "completed")) {
            return;
        }
        (mail.answers_ask() ? ledger_->terminals_attested : ledger_->terminals_unattested) += 1;
        ++state_.completed;
        // THE BYTES ARE CHECKED, not taken on trust. A successor that claimed
        // inherited progress it did not hold would produce a digest for a byte
        // count it never transferred, and this is where that would show.
        const bool honest = c.digest == digest_of(source_of(c.ticket), c.bytes);
        ledger_->heard.push_back("completed " + c.ticket + ": " + std::to_string(c.bytes) +
                                 " bytes, digest " + (honest ? "checks out" : "IS WRONG") +
                                 attested(mail));
        ledger_->outstanding.erase(mail.correlation());
    }

    void on(const DownloadFailed& f, loom::Mail& mail) {
        if (!mine(mail, f.ticket, "failed")) {
            return;
        }
        (mail.answers_ask() ? ledger_->terminals_attested : ledger_->terminals_unattested) += 1;
        ++state_.failed;
        ledger_->heard.push_back("failed " + f.ticket + " after " +
                                 std::to_string(f.bytes_discarded) + " bytes: " + f.reason +
                                 attested(mail));
        ledger_->outstanding.erase(mail.correlation());
    }

    /// The answers to a cancellation, which is a different conversation from the
    /// transfer it names.
    void on(const loom::Ack&, loom::Mail&) { ledger_->heard.push_back("cancel acknowledged"); }
    void on(const loom::Refused& r, loom::Mail&) {
        ledger_->heard.push_back("cancel refused: " + r.reason);
    }

private:
    const std::string* lookup(std::uint64_t correlation) const {
        const auto it = ledger_->outstanding.find(correlation);
        return it == ledger_->outstanding.end() ? nullptr : &it->second;
    }

    /// The consumer obligation, in the only half that is performable here.
    bool mine(const loom::Mail& mail, const std::string& ticket, const char* kind) {
        const std::string* open = lookup(mail.correlation());
        if (open == nullptr || *open != ticket) {
            ignore(std::string(kind) + " '" + ticket + "' matches no transfer I am waiting on");
            return false;
        }
        return true;
    }

    static const char* attested(const loom::Mail& mail) {
        return mail.answers_ask() ? "  [attested]" : "  [unattested]";
    }

    std::string source_of(const std::string& ticket) const {
        const auto it = ledger_->sources.find(ticket);
        return it == ledger_->sources.end() ? std::string{} : it->second;
    }

    void ignore(std::string why) {
        ++ledger_->ignored;
        ledger_->heard.push_back("IGNORED: " + std::move(why));
    }

    ClientLedger* ledger_;
};

} // namespace marathon::downloads

#endif // MARATHON_DOWNLOADS_CLIENT_HPP
