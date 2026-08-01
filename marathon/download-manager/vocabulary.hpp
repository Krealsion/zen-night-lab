#ifndef MARATHON_DOWNLOADS_VOCABULARY_HPP
#define MARATHON_DOWNLOADS_VOCABULARY_HPP

// A download manager — the whole contract in one file.
//
// THE ARCHITECTURAL QUESTION. The kitchen found that Loom authenticates the
// ACKNOWLEDGMENT of a job and never its FULFILMENT, because an answer right
// belongs to the life that earned it and one request grants exactly one answer.
// The kitchen's jobs were short and its progress was private. This one's are
// neither:
//
//     Is the original answer capability the right thing to hold for the entire
//     operation?
//
// The question is not rhetorical and this package answers it by MEASUREMENT.
// One source builds two services that differ in nothing but that decision:
//
//     download-service        answers `Accepted` at once, then speaks in
//                             ordinary directed messages for the rest of the
//                             operation.
//     download-service-holds  DEFERS the answer and holds it until the terminal
//                             message, so the one authenticated thing the client
//                             hears is the OUTCOME rather than the promise.
//
// Both are honest. The suite runs both and reports what each costs.
//
// WHAT MAKES THIS DIFFERENT FROM THE KITCHEN, and it is deliberate rather than
// decorative:
//
//   * PROGRESS IS PUBLIC. A station's `passes_left` was nobody's business; a
//     download's bytes-so-far is the whole reason a client is watching. So
//     progress is a channel that is neither an answer nor an outcome, and this
//     package has to invent it.
//   * PROGRESS IS THE BYTES. The kitchen could carry its work across a
//     replacement because its progress was three integers that described work
//     rather than being it. A half-downloaded file IS its bytes; describing them
//     in a letter would be copying them, and claiming progress without them
//     would be a lie. That forces a different, harsher continuity contract.
//   * MANY OPERATIONS AT ONCE, from many clients, all advancing on one beat.
//
// THE CONTINUITY CONTRACT, CHOSEN AND NOT DEFAULTED TO:
//
//     A download that has not reached a terminal message when the service is
//     replaced is FAILED, explicitly, by the successor, naming how many bytes
//     were discarded.
//
// It would have been easy to write "the successor continues the download" and
// nothing would have caught the lie — the successor would have re-fetched from
// zero while reporting inherited progress. So what crosses a replacement here is
// NOT the work. It is the OBLIGATION: who is owed a terminal message, about
// what, and how far it had got when it stopped.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace marathon::downloads {

// ---- the address that outlives its holder -----------------------------------

/// The one service role. Clients address it by role and never by id, so a client
/// keeps its reach across the service being replaced.
inline constexpr const char* kServiceRole = "download.service";

// ---- what a client says -----------------------------------------------------

/// Fetch this. `ticket` is the CLIENT's own name for the operation, scoped to
/// the client, so two clients naming a download "1" never collide.
///
/// The service answers this exactly once. WHICH answer, and WHEN, is the
/// experiment — see the two builds above.
struct StartDownload {
    std::string ticket;
    std::string source;      ///< a name in the service's catalogue
    std::string destination; ///< where the client wants it; the service only echoes it
    ZEN_SHAPE(StartDownload, 1, ZEN_FIELD(ticket), ZEN_FIELD(source),
              ZEN_FIELD(destination));
};

/// Stop it. CANCELLATION EXISTS BECAUSE THE MODEL DEMANDED IT, not because
/// long-running work sounds like it should have it — see REPORT.md. The short
/// version: without cancellation, a client that has given up is still owed a
/// terminal message, and the only way for it to stop being owed one is for the
/// service to invent a deadline it has no basis for. The client withdrawing is
/// the honest alternative, and it is a message the client sends rather than a
/// state the service guesses.
struct CancelDownload {
    std::string ticket;
    ZEN_SHAPE(CancelDownload, 1, ZEN_FIELD(ticket));
};

// ---- what the service says --------------------------------------------------

/// "I have taken responsibility for this, and here is what I know about it."
/// `total_bytes` is the service's own knowledge, available at accept time
/// because the catalogue is local — a real fetcher would answer with what its
/// HEAD said, or with 0 for an unknown length.
struct DownloadAccepted {
    std::string ticket;
    std::string source;
    std::int64_t total_bytes = 0;
    ZEN_SHAPE(DownloadAccepted, 1, ZEN_FIELD(ticket), ZEN_FIELD(source),
              ZEN_FIELD(total_bytes));
};

/// "I will not take responsibility for this, and here is why." A REFUSAL IS AN
/// OUTCOME AND NEVER A SILENT DROP: a client that asked is always told
/// something, and the shape it is told is the shape it was expecting.
struct DownloadRefused {
    std::string ticket;
    std::string reason;
    ZEN_SHAPE(DownloadRefused, 1, ZEN_FIELD(ticket), ZEN_FIELD(reason));
};

/// How far along. ORDINARY AND DIRECTED, never an answer: Loom grants one
/// authenticated answer per request, and an operation that reports progress
/// wants many messages. So progress is the channel that has no attestation at
/// all, and the client's only wall is the correlation it chose.
struct DownloadProgress {
    std::string ticket;
    std::int64_t bytes_done = 0;
    std::int64_t total_bytes = 0;
    ZEN_SHAPE(DownloadProgress, 1, ZEN_FIELD(ticket), ZEN_FIELD(bytes_done),
              ZEN_FIELD(total_bytes));
};

/// It is done. `digest` is a cheap deterministic fold over the bytes actually
/// transferred — present so that "the file arrived" is checkable rather than
/// asserted, which matters the moment a replacement claims inherited progress.
struct DownloadCompleted {
    std::string ticket;
    std::int64_t bytes = 0;
    std::int64_t digest = 0;
    ZEN_SHAPE(DownloadCompleted, 1, ZEN_FIELD(ticket), ZEN_FIELD(bytes), ZEN_FIELD(digest));
};

/// It will not be done, and here is the honest reason, including how much was
/// thrown away. `bytes_discarded` is not decoration: it is the difference
/// between "nothing happened" and "you nearly had it", and a client deciding
/// whether to retry wants to know which.
struct DownloadFailed {
    std::string ticket;
    std::int64_t bytes_discarded = 0;
    std::string reason;
    ZEN_SHAPE(DownloadFailed, 1, ZEN_FIELD(ticket), ZEN_FIELD(bytes_discarded),
              ZEN_FIELD(reason));
};

// ---- diagnostics ------------------------------------------------------------

/// Ask the service how it is doing. Answered — authenticated — with a
/// `zen.Result` whose text is a one-line, stranger-readable tally.
struct ServiceStatus {
    ZEN_SHAPE(ServiceStatus, 1);
};

// ---- the replacement conversation -------------------------------------------
//
// THE OBLIGATION CROSSES; THE WORK DOES NOT. See the header comment.

/// One operation in flight, described in the service's own words.
///
/// `client` is canonical decimal Text and not an Int, for the house reason: a
/// WeaveId is unsigned 64-bit and the wire's Int is signed, so an Int field
/// would silently narrow the top half of the range. This is the SECOND package
/// to need that workaround.
struct Obligation {
    std::string ticket;
    std::string client;              ///< canonical decimal of the client's WeaveId
    std::int64_t correlation = 0;    ///< the client's own number for this operation
    std::string source;
    std::int64_t bytes_done = 0;
    std::int64_t total_bytes = 0;
    ZEN_SHAPE(Obligation, 1, ZEN_FIELD(ticket), ZEN_FIELD(client), ZEN_FIELD(correlation),
              ZEN_FIELD(source), ZEN_FIELD(bytes_done), ZEN_FIELD(total_bytes));
};

/// "What do you still owe anybody?" An ORDINARY ask to the LIVE incumbent, from
/// the operator, during the one interval in which the incumbent is alive and the
/// successor is reachable. It changes nothing: no transfer is stopped, no client
/// is told anything, and an operator that asks and then walks away has done
/// exactly nothing.
struct DescribeObligations {
    ZEN_SHAPE(DescribeObligations, 1);
};

/// The incumbent's authenticated answer.
struct ObligationsDescribed {
    std::vector<Obligation> open;
    ZEN_SHAPE(ObligationsDescribed, 1, ZEN_FIELD(open));
};

/// THE PREPARATION ASK: "take over the service, and take over these debts."
///
/// It carries no transaction id — the bus proves which conversation an answer
/// belongs to. `verify_sources` makes the candidate check that it can actually
/// serve every source named in the inherited obligations before it agrees, which
/// is the difference between a successor that is ready and one that merely
/// exists.
struct PrepareService {
    std::vector<Obligation> inherit;
    bool verify_sources = false;
    ZEN_SHAPE(PrepareService, 1, ZEN_FIELD(inherit), ZEN_FIELD(verify_sources));
};

/// "How big is the catalogue you expect me to serve?" The candidate's own
/// question, asked FROM INSIDE THE SEAL to the one party it may speak to. It is
/// what makes preparation a conversation rather than a form, and it is the arm
/// of the ceremony in which the candidate DEFERS its readiness answer.
struct AskCatalogueSize {
    ZEN_SHAPE(AskCatalogueSize, 1);
};

struct CatalogueSize {
    std::int64_t sources = 0;
    ZEN_SHAPE(CatalogueSize, 1, ZEN_FIELD(sources));
};

/// "I am ready to be the service." The candidate's authenticated answer.
struct ServiceReady {
    std::int64_t obligations_taken = 0;
    ZEN_SHAPE(ServiceReady, 1, ZEN_FIELD(obligations_taken));
};

/// "I will not be the service, and here is why." An AUTHENTIC refusal: the
/// candidate spends the one answer authority the ask earned it to say no, so the
/// transaction ends with the successor's own verdict and the incumbent simply
/// continues serving.
struct ServiceNotReady {
    std::string reason;
    ZEN_SHAPE(ServiceNotReady, 1, ZEN_FIELD(reason));
};

// ---- the published bounds ---------------------------------------------------

/// How many operations the service will hold at once. A full book REFUSES
/// visibly rather than queueing into something nobody bounded.
///
/// CHOSEN LARGER THAN `Switchboard::kMaxDeferredAnswers` (64) ON PURPOSE. The
/// two builds of this service differ in whether they hold an answer right for
/// the whole operation, and Loom's deferred-answer capacity is **one Loom's**,
/// not one weave's. If the application's own bound were the smaller of the two
/// it would mask the substrate's, and the experiment's central measurement —
/// what holding a capability for a long operation actually costs — would be
/// invisible. So this number is deliberately the one that does NOT bind first.
inline constexpr std::size_t kMaxOpenTransfers = 80;

/// How many obligations a preparation ask may hand over. Same number as the
/// book, so a successor can never be handed more debt than an honest predecessor
/// could have owed.
inline constexpr std::size_t kMaxInheritedObligations = kMaxOpenTransfers;

/// Bytes moved per transfer per beat. Small and exact: every deadline in this
/// package is an integer nobody had to wait for.
inline constexpr std::int64_t kChunkBytes = 64;

/// The service's beat. ROLE-addressed, so the successor of a replaced service
/// inherits the pulse instead of waiting for its own first ask to land.
inline constexpr const char* kPumpTimerId = "download.pump";
inline constexpr std::int64_t kPumpMs = 20;

/// The correlation the service puts on its one letter-claim per activation.
/// Published on purpose: a correlation is a conversation label, never a secret.
inline constexpr std::uint64_t kClaimCorrelation = 0xD09D10AD;

// ---- the catalogue ----------------------------------------------------------
//
// Deterministic in-memory sources. No network, no filesystem, no clock — the
// point of the experiment is the conversation, and a real socket would only add
// ways for it to be flaky.

struct Source {
    const char* name;
    std::int64_t bytes;
    /// At which byte this source fails, or 0 for one that never does. A source
    /// that breaks partway is what makes "how much was discarded" a real number.
    std::int64_t breaks_at;
};

inline constexpr Source kCatalogue[] = {
    {"manifest.json", 192, 0},
    {"index.db", 640, 0},
    {"kernel.img", 1536, 0},
    {"truncated.iso", 896, 448}, ///< breaks exactly halfway
};

inline constexpr std::size_t kCatalogueSize = sizeof(kCatalogue) / sizeof(kCatalogue[0]);

inline const Source* find_source(const std::string& name) {
    for (const Source& s : kCatalogue) {
        if (name == s.name) {
            return &s;
        }
    }
    return nullptr;
}

/// A cheap deterministic fold over the first `bytes` bytes of `name`'s content.
/// Both the service and the client can compute it, which is what makes
/// "the bytes really arrived" checkable instead of asserted.
inline std::int64_t digest_of(const std::string& name, std::int64_t bytes) {
    std::int64_t h = 1469598103934665603LL;
    for (const char c : name) {
        h = (h ^ static_cast<std::int64_t>(static_cast<unsigned char>(c))) * 1099511628211LL;
    }
    for (std::int64_t i = 0; i < bytes; ++i) {
        h = (h ^ (i & 0xFF)) * 1099511628211LL;
    }
    return h & 0x7FFFFFFFFFFFFFFFLL;
}

} // namespace marathon::downloads

#endif // MARATHON_DOWNLOADS_VOCABULARY_HPP
