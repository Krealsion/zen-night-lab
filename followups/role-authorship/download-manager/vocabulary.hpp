#ifndef FOLLOWUP_DOWNLOAD_VOCABULARY_HPP
#define FOLLOWUP_DOWNLOAD_VOCABULARY_HPP

// The download manager, replayed for the marathon's sharpest architectural
// finding: YOU GET ONE ATTESTATION PER OPERATION, so a long-lived operation
// had to choose which half to defend — the acceptance (an answer, provable,
// but then the terminal truth is ordinary) or the terminal truth (hold the
// deferred answer for the whole download, parking a slot of the Loom-wide 64
// and stranding the client across an honest replacement).
//
// The replay asks whether role authorship dissolved the choice: the acceptance
// stays an authenticated ANSWER (this exact request was accepted by its
// respondent), and the terminal truth becomes OFFICE-AUTHORED ORDINARY SPEECH
// (the download-service office deliberately said it finished). Two different
// proofs, both verifiable, no capability held for the duration.

#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace dl {

struct StartDownload {
    std::string url;
    ZEN_SHAPE(StartDownload, 1, ZEN_FIELD(url));
};

/// The acceptance — spent immediately as the authenticated answer.
struct DownloadAccepted {
    std::string url;
    ZEN_SHAPE(DownloadAccepted, 1, ZEN_FIELD(url));
};

/// The terminal truth — office-authored ordinary speech, minutes later.
struct DownloadDone {
    std::string url;
    bool ok = true;
    ZEN_SHAPE(DownloadDone, 1, ZEN_FIELD(url), ZEN_FIELD(ok));
};

/// Drive the service: the download "finished" (the passage of time, compressed).
struct FinishDownload {
    ZEN_SHAPE(FinishDownload, 1);
};

} // namespace dl

#endif // FOLLOWUP_DOWNLOAD_VOCABULARY_HPP
