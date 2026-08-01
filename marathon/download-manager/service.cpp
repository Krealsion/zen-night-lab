// The download service — one weave, one role, many operations in flight.
//
// ONE SOURCE, TWO SERVICES, and the difference between them is the experiment:
//
//     download-service        (default) spends its one authenticated answer at
//                             once, on `DownloadAccepted`. Everything after that
//                             — progress and the terminal message — is an
//                             ordinary directed message with no attestation.
//
//     download-service-holds  (DOWNLOAD_HOLD_THE_ANSWER) DEFERS the answer and
//                             holds it for the whole operation, so the one
//                             authenticated thing the client hears is the
//                             OUTCOME. The acknowledgment becomes ordinary.
//
// Both are honest readings of "the service authentically acknowledges
// responsibility". Neither can be both, because Loom grants exactly one
// authenticated answer per request — which is the kitchen's finding, restated
// by a second package that reached it from a different direction.
//
// WHAT THE SECOND BUILD COSTS, and why it is worth building to find out:
// `Switchboard::kMaxDeferredAnswers` is 64 and it is **one Loom's**, not one
// weave's. A service that holds an answer right for the duration of a long
// operation is therefore spending a globally shared substrate resource for as
// long as the operation runs — and when it runs out, it is not only this
// service that cannot defer. The suite measures that rather than describing it.
//
// THE CONTINUITY CONTRACT. A transfer that has not reached a terminal message
// when this service is replaced is FAILED by the successor, naming the bytes
// discarded. The bytes themselves do not cross and are not pretended to: a
// half-downloaded file IS its bytes, and a letter that described progress
// without carrying them would make the successor claim work it does not hold.
// What crosses is the OBLIGATION — who is owed a terminal message, about what,
// and how far it had got.

#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using namespace marathon::downloads;
namespace timer = zengine::timer;

#if defined(DOWNLOAD_HOLD_THE_ANSWER)
constexpr bool kHoldTheAnswer = true;
constexpr const char* kStyle = "holds-the-answer";
#else
constexpr bool kHoldTheAnswer = false;
constexpr const char* kStyle = "acknowledges-at-once";
#endif

#if defined(DOWNLOAD_SERVICE_LABEL)
constexpr const char* kLabel = DOWNLOAD_SERVICE_LABEL;
#else
constexpr const char* kLabel = "v1";
#endif

/// One transfer in flight. The bytes are not here — `bytes_done` is a count, and
/// a real service would hold a buffer or a file handle beside it. That absence
/// is the point of the continuity contract: this struct is describable, and the
/// thing it describes is not.
struct Transfer {
    std::string ticket;
    std::string client; ///< canonical decimal of the client's WeaveId
    std::int64_t correlation = 0;
    std::string source;
    std::int64_t bytes_done = 0;
    std::int64_t total_bytes = 0;
    std::int64_t breaks_at = 0;
    ZEN_SHAPE(Transfer, 1, ZEN_FIELD(ticket), ZEN_FIELD(client), ZEN_FIELD(correlation),
              ZEN_FIELD(source), ZEN_FIELD(bytes_done), ZEN_FIELD(total_bytes),
              ZEN_FIELD(breaks_at));
};

struct ServiceState {
    std::vector<Transfer> transfers;
    /// Debts taken over from a predecessor during preparation, held until this
    /// incarnation is activated and can actually speak to the clients they name.
    /// In the state and not beside it, because a candidate is asked before it is
    /// alive and must not lose what it agreed to between the two moments.
    std::vector<Obligation> inherited;
    std::int64_t accepted = 0;
    std::int64_t refused = 0;
    std::int64_t completed = 0;
    std::int64_t failed = 0;
    std::int64_t cancelled = 0;
    std::int64_t discharged = 0;   ///< inherited debts reported to their clients
    std::int64_t unanswerable = 0; ///< answers this service tried to send and could not
    std::int64_t no_answer_right = 0; ///< accepts refused because no right could be taken
    ZEN_EXPOSE();
    ZEN_SHAPE(ServiceState, 1, ZEN_FIELD(transfers), ZEN_FIELD(inherited), ZEN_FIELD(accepted),
              ZEN_FIELD(refused), ZEN_FIELD(completed), ZEN_FIELD(failed), ZEN_FIELD(cancelled),
              ZEN_FIELD(discharged), ZEN_FIELD(unanswerable), ZEN_FIELD(no_answer_right));
};

/// One unspent answer right, held for the whole operation. Only the
/// holds-the-answer build ever has any.
///
/// PER-INCARNATION AND NEVER STATE, for the reason the kitchen found and this
/// package re-confirms: a `DeferredAnswer` is a CAPABILITY bound to this weave's
/// life and incarnation. There is no representation of one that a successor
/// could be handed — which is precisely why the obligations, and not the rights,
/// are what cross a replacement.
struct HeldAnswer {
    std::string ticket;
    std::string client;
    loom::DeferredAnswer right;
};

std::uint64_t parse_u64(const std::string& text) {
    std::uint64_t v = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const std::from_chars_result r = std::from_chars(first, last, v);
    if (r.ec != std::errc{} || r.ptr != last) {
        return 0;
    }
    return v;
}

class Service
    : public loom::WeaveBase<
          Service, ServiceState,
          loom::Accept<StartDownload, CancelDownload, ServiceStatus, DescribeObligations,
                       PrepareService, CatalogueSize, loom::Activated, timer::TimerReady,
                       timer::TimerFired>,
          loom::Emit<DownloadAccepted, DownloadRefused, DownloadProgress, DownloadCompleted,
                     DownloadFailed, ObligationsDescribed, ServiceReady, ServiceNotReady,
                     AskCatalogueSize, timer::StartRoleTimer, loom::Result, loom::Ack,
                     loom::Refused>> {
public:
    // ---- arrival ------------------------------------------------------------

    /// This incarnation is live. Two things happen here and nowhere else: the
    /// pulse is asked for, and every debt taken over during preparation is
    /// DISCHARGED.
    ///
    /// Discharging at activation and not at preparation is not an implementation
    /// convenience — a sealed candidate cannot speak to anybody but its
    /// coordinator, so this is the first moment at which the clients owed a
    /// terminal message are reachable at all. It is also the right moment: until
    /// admission commits, this weave owes nothing to anyone, and a candidate that
    /// told clients their downloads had failed and then was never admitted would
    /// have lied on behalf of a service that is still running perfectly.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return; // unattested, duplicate or replayed: nothing happens twice
        }
        discharge_inherited(mail);
        ask_for_the_pulse(mail);
    }

    void on(const timer::TimerReady&, loom::Mail& mail) { ask_for_the_pulse(mail); }

    // ---- the front desk -----------------------------------------------------

    /// Take responsibility, or say why not.
    ///
    /// A REFUSAL IS ALWAYS THE IMMEDIATE AUTHENTICATED ANSWER, in both builds.
    /// There is no reason to defer a refusal: nothing has to survive a
    /// replacement for it to arrive, and a client that asked for a source this
    /// service has never heard of deserves to be told inside one delivery.
    void on(const StartDownload& s, loom::Mail& mail) {
        const std::string client = std::to_string(mail.sender().value);
        if (state_.transfers.size() >= kMaxOpenTransfers) {
            refuse(mail, s.ticket,
                   "this service is holding its maximum of " +
                       std::to_string(kMaxOpenTransfers) + " transfers; nothing was started");
            return;
        }
        for (const Transfer& t : state_.transfers) {
            if (t.ticket == s.ticket && t.client == client) {
                refuse(mail, s.ticket,
                       "you already have a transfer named '" + s.ticket + "' with this service");
                return;
            }
        }
        const Source* src = find_source(s.source);
        if (src == nullptr) {
            refuse(mail, s.ticket,
                   "there is no source named '" + s.source + "' in this service's catalogue");
            return;
        }
        if (s.destination.empty()) {
            refuse(mail, s.ticket, "a transfer needs a destination to be delivered to");
            return;
        }

        Transfer t;
        t.ticket = s.ticket;
        t.client = client;
        t.correlation = static_cast<std::int64_t>(mail.correlation());
        t.source = s.source;
        t.bytes_done = 0;
        t.total_bytes = src->bytes;
        t.breaks_at = src->breaks_at;

        if (kHoldTheAnswer) {
            // THE OTHER READING. The answer right is taken away and kept for the
            // whole operation, so the terminal message can be the authenticated
            // one. If the right cannot be taken there is no honest way to finish
            // the conversation later, so the transfer is not accepted — taking a
            // job while knowing its ending can never be attested is the silent
            // failure both this package and the kitchen exist to avoid.
            loom::DeferredAnswer right = mail.defer_answer();
            if (!right.valid()) {
                ++state_.no_answer_right;
                // THE MEASUREMENT. `kMaxDeferredAnswers` is one LOOM's capacity,
                // not one weave's, so this refusal can be caused by traffic that
                // has nothing to do with downloads. The reason says so.
                refuse_ordinary(mail, s.ticket,
                                "this build holds an answer right for the whole transfer and "
                                "this Loom has no unfinished-conversation slots left; the "
                                "limit reached was the substrate's, not this service's");
                return;
            }
            held_.push_back(HeldAnswer{s.ticket, client, std::move(right)});
            // The acknowledgment is now an ORDINARY message. The client can match
            // its correlation and nothing more.
            mail.send(mail.sender(), DownloadAccepted{s.ticket, s.source, t.total_bytes},
                      mail.correlation());
        } else {
            say(mail, DownloadAccepted{s.ticket, s.source, t.total_bytes});
        }
        state_.transfers.push_back(std::move(t));
        ++state_.accepted;
    }

    /// The client withdraws. See vocabulary.hpp for why this exists at all.
    ///
    /// The withdrawal is answered with an ordinary `zen.Ack` or `zen.Refused` —
    /// the legibility shapes, used exactly as they were meant to be — and the
    /// TRANSFER's own terminal message still goes out, because the client is owed
    /// one and "you asked me to stop" is a perfectly good ending.
    void on(const CancelDownload& c, loom::Mail& mail) {
        const std::string client = std::to_string(mail.sender().value);
        for (std::size_t i = 0; i < state_.transfers.size(); ++i) {
            Transfer& t = state_.transfers[i];
            if (t.ticket != c.ticket || t.client != client) {
                continue;
            }
            ++state_.cancelled;
            const std::int64_t done = t.bytes_done;
            finish_failed(mail, i,
                          "cancelled by the client after " + std::to_string(done) + " of " +
                              std::to_string(t.total_bytes) + " bytes");
            say(mail, loom::Ack{});
            return;
        }
        say(mail, loom::Refused{"no transfer named '" + c.ticket + "' is open for you here"});
    }

    /// The one diagnostic, answered authentically so an asker knows it reached
    /// the service and not a lookalike.
    void on(const ServiceStatus&, loom::Mail& mail) {
        std::int64_t bytes = 0;
        for (const Transfer& t : state_.transfers) {
            bytes += t.bytes_done;
        }
        say(mail, loom::Result{
                      "downloads[" + std::string(kLabel) + "/" + std::string(kStyle) +
                      "]: accepted=" + std::to_string(state_.accepted) + " refused=" +
                      std::to_string(state_.refused) + " completed=" +
                      std::to_string(state_.completed) + " failed=" +
                      std::to_string(state_.failed) + " cancelled=" +
                      std::to_string(state_.cancelled) + " discharged=" +
                      std::to_string(state_.discharged) + " open=" +
                      std::to_string(state_.transfers.size()) + " bytes_in_flight=" +
                      std::to_string(bytes)});
    }

    // ---- the work -----------------------------------------------------------

    /// One beat moves every transfer forward by one chunk.
    ///
    /// The consumer obligation on a role beat: an id this service never asked for
    /// is DATA, not a drive. A role can be aimed at by anyone.
    void on(const timer::TimerFired& f, loom::Mail& mail) {
        if (f.id != kPumpTimerId) {
            return;
        }
        std::size_t at = 0;
        while (at < state_.transfers.size()) {
            Transfer& t = state_.transfers[at];
            const std::int64_t was = t.bytes_done;
            t.bytes_done = was + kChunkBytes;
            if (t.bytes_done > t.total_bytes) {
                t.bytes_done = t.total_bytes;
            }
            if (t.breaks_at > 0 && t.bytes_done >= t.breaks_at) {
                const std::int64_t discarded = t.breaks_at;
                finish_failed(mail, at,
                              "the source went bad at byte " + std::to_string(discarded) +
                                  "; the transfer cannot continue");
                continue; // finish_* erased at `at`, so the cursor stays put
            }
            // PROGRESS IS A CHANNEL OF ITS OWN. Not an answer (there is one of
            // those and it is spent elsewhere), not a terminal (nothing is over).
            // It carries no attestation and the client knows it.
            mail.send(client_of(t), DownloadProgress{t.ticket, t.bytes_done, t.total_bytes},
                      static_cast<std::uint64_t>(t.correlation));
            if (t.bytes_done >= t.total_bytes) {
                finish_completed(mail, at);
                continue;
            }
            ++at;
        }
    }

    // ---- the replacement conversation ---------------------------------------

    /// "What do you still owe anybody?" Asked of the LIVE incumbent. It changes
    /// NOTHING: no transfer stops, no client is told anything, and an operator
    /// that asks and walks away has done nothing at all.
    void on(const DescribeObligations&, loom::Mail& mail) {
        ObligationsDescribed described;
        for (const Transfer& t : state_.transfers) {
            if (described.open.size() >= kMaxInheritedObligations) {
                break; // the published bound, honoured from the writing side
            }
            described.open.push_back(Obligation{t.ticket, t.client, t.correlation, t.source,
                                                t.bytes_done, t.total_bytes});
        }
        say(mail, described);
    }

    /// THE PREPARATION ASK, heard from inside the seal.
    ///
    /// This weave is outside the world when this arrives: it holds no role, its
    /// beat does not reach it, and the only party it may speak to is the
    /// coordinator that sealed it. It says no to two things, and both are
    /// promises it would otherwise have to break:
    ///
    ///   * more debt than any honest predecessor could have owed;
    ///   * debt naming a source it cannot serve, which would leave a client owed
    ///     a terminal message this service has no way to produce.
    void on(const PrepareService& p, loom::Mail& mail) {
        if (p.inherit.size() > kMaxInheritedObligations) {
            refuse_preparation(mail, "a preparation may hand over at most " +
                                         std::to_string(kMaxInheritedObligations) +
                                         " obligations, and this one carries " +
                                         std::to_string(p.inherit.size()));
            return;
        }
        for (const Obligation& o : p.inherit) {
            if (o.client.empty() || parse_u64(o.client) == 0) {
                refuse_preparation(mail, "obligation '" + o.ticket +
                                             "' names no client this service could ever answer");
                return;
            }
            if (p.verify_sources && find_source(o.source) == nullptr) {
                refuse_preparation(mail, "obligation '" + o.ticket + "' names source '" +
                                             o.source + "', which is not in this service's "
                                                        "catalogue");
                return;
            }
        }
        if (p.verify_sources) {
            // THE CANDIDATE ASKS BACK, using the one door a sealed weave has. The
            // readiness answer is taken away and held until the reply lands.
            pending_ = mail.defer_answer();
            if (!pending_.valid()) {
                refuse_preparation(mail, "this preparation carried no answer authority to hold");
                return;
            }
            staged_ = p.inherit;
            mail.send(mail.sender(), AskCatalogueSize{});
            return;
        }
        state_.inherited = p.inherit;
        say(mail, ServiceReady{static_cast<std::int64_t>(state_.inherited.size())});
    }

    /// The coordinator's answer to the candidate's own question.
    void on(const CatalogueSize& c, loom::Mail& mail) {
        if (!mail.answers_ask() || !pending_.valid()) {
            return; // not an answer to our ask, or we are not holding a preparation
        }
        std::vector<Obligation> staged;
        staged.swap(staged_);
        if (c.sources != static_cast<std::int64_t>(kCatalogueSize)) {
            // The house and this artifact disagree about what is servable. Saying
            // yes here would be agreeing to obligations neither party has checked.
            (void)loom::answer_deferred(
                pending_, mail,
                ServiceNotReady{"the operator expects a catalogue of " +
                                std::to_string(c.sources) + " sources and this artifact ships " +
                                std::to_string(kCatalogueSize)});
            pending_ = loom::DeferredAnswer{};
            return;
        }
        state_.inherited = std::move(staged);
        (void)loom::answer_deferred(
            pending_, mail, ServiceReady{static_cast<std::int64_t>(state_.inherited.size())});
        pending_ = loom::DeferredAnswer{};
    }

private:
    // ---- the three endings, one implementation each -------------------------

    void finish_completed(loom::Mail& mail, std::size_t at) {
        const Transfer t = state_.transfers[at];
        ++state_.completed;
        terminal(mail, t,
                 DownloadCompleted{t.ticket, t.bytes_done, digest_of(t.source, t.bytes_done)});
        erase(at, t.ticket, t.client);
    }

    void finish_failed(loom::Mail& mail, std::size_t at, std::string reason) {
        const Transfer t = state_.transfers[at];
        ++state_.failed;
        terminal(mail, t, DownloadFailed{t.ticket, t.bytes_done, std::move(reason)});
        erase(at, t.ticket, t.client);
    }

    /// The terminal message, sent whichever way this build believes in.
    ///
    /// This is the ONE place the two builds visibly differ at the end of an
    /// operation, and the asymmetry is the finding: in the holds-the-answer build
    /// the ending is attested and the acknowledgment was not; in the other, the
    /// acknowledgment was attested and the ending is not. Nothing available makes
    /// both true.
    template <class T>
    void terminal(loom::Mail& mail, const Transfer& t, const T& msg) {
        if (kHoldTheAnswer) {
            for (std::size_t i = 0; i < held_.size(); ++i) {
                if (held_[i].ticket != t.ticket || held_[i].client != t.client) {
                    continue;
                }
                if (!loom::answer_deferred(held_[i].right, mail, msg).valid()) {
                    ++state_.unanswerable;
                }
                held_.erase(held_.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
            // An inherited transfer has no right to spend — its predecessor's
            // rights died with the predecessor. That is a fact about succession,
            // not an error, and the message still goes out as an ordinary one.
        }
        mail.send(client_of(t), msg, static_cast<std::uint64_t>(t.correlation));
    }

    void erase(std::size_t at, const std::string& ticket, const std::string& client) {
        state_.transfers.erase(state_.transfers.begin() + static_cast<std::ptrdiff_t>(at));
        for (std::size_t i = 0; i < held_.size(); ++i) {
            if (held_[i].ticket == ticket && held_[i].client == client) {
                held_.erase(held_.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

    /// Every inherited debt, reported to the client that is owed it, naming the
    /// bytes that were thrown away.
    ///
    /// It is an ORDINARY message and could not be anything else: the right that
    /// would have made it an answer belonged to a life that has ended, and no
    /// part of it is representable in a letter. So a client that was promised an
    /// attested ending gets an unattested one, and the only reason it is believed
    /// at all is the correlation it chose itself.
    void discharge_inherited(loom::Mail& mail) {
        std::vector<Obligation> debts;
        debts.swap(state_.inherited);
        for (const Obligation& o : debts) {
            const loom::WeaveId client{parse_u64(o.client)};
            if (!client.valid()) {
                continue;
            }
            ++state_.discharged;
            mail.send(client,
                      DownloadFailed{o.ticket, o.bytes_done,
                                     "the download service was replaced while this transfer was "
                                     "in progress; " +
                                         std::to_string(o.bytes_done) + " of " +
                                         std::to_string(o.total_bytes) +
                                         " bytes were discarded because a partial transfer is "
                                         "its bytes, which cannot be handed over"},
                      static_cast<std::uint64_t>(o.correlation));
        }
    }

    static loom::WeaveId client_of(const Transfer& t) {
        return loom::WeaveId{parse_u64(t.client)};
    }

    void refuse(loom::Mail& mail, const std::string& ticket, std::string reason) {
        ++state_.refused;
        say(mail, DownloadRefused{ticket, std::move(reason)});
    }

    /// The one refusal that cannot be an answer: it happens exactly when there
    /// were no answer slots left, which is also why `mail.answer` would work
    /// here (the immediate opportunity survives a refused deferral) — it is sent
    /// as an answer, and only the REASON differs.
    void refuse_ordinary(loom::Mail& mail, const std::string& ticket, std::string reason) {
        ++state_.refused;
        say(mail, DownloadRefused{ticket, std::move(reason)});
    }

    void refuse_preparation(loom::Mail& mail, std::string reason) {
        say(mail, ServiceNotReady{std::move(reason)});
    }

    /// THE IMMEDIATE ANSWER, with its ticket looked at. Night One's real lesson
    /// was not "answer is broken" but that the documented call shape discards the
    /// only signal there is.
    template <class T>
    void say(loom::Mail& mail, const T& msg) {
        if (!mail.answer(msg).valid()) {
            ++state_.unanswerable;
        }
    }

    void ask_for_the_pulse(loom::Mail& mail) {
        mail.send_to_role(timer::kTimerRole,
                          timer::StartRoleTimer{kPumpTimerId, kPumpMs, /*repeat=*/true,
                                                kServiceRole});
    }

    zengine::ActivationCursor activation_;

    /// Answer rights held for the duration of an operation. Only the
    /// holds-the-answer build ever fills this, and it is per-incarnation for the
    /// same reason the kitchen's promises were: there is no representation of a
    /// capability that a successor could be handed.
    std::vector<HeldAnswer> held_;

    /// The readiness answer, taken away while the candidate asks its own
    /// question, and the obligations staged behind it.
    loom::DeferredAnswer pending_;
    std::vector<Obligation> staged_;
};

} // namespace

// The label is compiled in so a tap can tell two generations apart. Reference it
// once so -Werror keeps it honest rather than letting it rot.
static_assert(kLabel[0] != '\0', "a service generation needs a label");

ZEN_EXPORT_WEAVE(Service)
