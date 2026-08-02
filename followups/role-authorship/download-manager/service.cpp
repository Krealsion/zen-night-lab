// The download service — both halves attested, neither by a parked capability.
//
//   on StartDownload:   mail.answer(DownloadAccepted{...})   — spent NOW.
//   on FinishDownload:  mail.as_role("download.service").send(client,
//                       DownloadDone{...})                   — authored NOW.
//
// Grep this file for `defer`: nothing. The marathon's version had to hold a
// DeferredAnswer for the lifetime of the transfer to make the terminal truth
// provable — one Loom-wide slot per active download, and a replaced service
// stranded every waiting client. Here the answer right is spent at the moment
// it exists, and the office fact is minted at the moment the terminal truth is
// spoken, by whoever legitimately holds the office THEN.

#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace {

using namespace loom;

struct ServiceState {
    std::int64_t client = 0; ///< who asked (the stamped sender of the ask)
    std::string url;
    ZEN_SHAPE(ServiceState, 1, ZEN_FIELD(client), ZEN_FIELD(url));
};

class Service : public WeaveBase<Service, ServiceState,
                                 Accept<dl::StartDownload, dl::FinishDownload>,
                                 Emit<dl::DownloadAccepted, dl::DownloadDone>> {
public:
    void on(const dl::StartDownload& ask, Mail& mail) {
        state_.client = static_cast<std::int64_t>(mail.sender().value);
        state_.url = ask.url;
        // The authenticated half of the operation, spent immediately: THIS
        // exact request was accepted by the respondent it actually reached.
        (void)mail.answer(dl::DownloadAccepted{ask.url});
    }

    void on(const dl::FinishDownload&, Mail& mail) {
        // The terminal half, minutes later in real life: the OFFICE speaks,
        // deliberately, once. No capability survived the gap — none needed to.
        (void)mail.as_role("download.service")
            .send(WeaveId{static_cast<std::uint64_t>(state_.client)},
                  dl::DownloadDone{state_.url, true});
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Service)
