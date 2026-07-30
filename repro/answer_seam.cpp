// THE REPRODUCER for the one core seam this experiment found.
//
//     `loom::Mail::answer()` — Loom's authenticated one-shot answer — does not
//     cross the `.so` seam, and fails SILENTLY when a dynamically loaded weave
//     calls it.
//
// This file is the whole thing: one weave library built TWICE from one source,
// differing only in which door it answers through, and a host that watches the
// bus and reports what actually came back.
//
//     repro-answer-direct    calls mail.answer(...)          -> nothing happens
//     repro-answer-deferred  calls defer_answer() + spend    -> the answer arrives
//
// WHY IT IS SILENT, exactly:
//   * `loom::Bus::answer()` has a base implementation that returns an invalid
//     Ticket and does nothing. Its comment explains why, and the reason is good:
//     "a Bus that is not a live delivery (a library-side shim, a future mailbox)
//     truthfully answers nothing rather than pretending."
//   * `loom::detail::HostApiBus` (zen/kernel/export.hpp) is the Bus a loaded
//     weave is handed. It overrides send, publish, send_to_role,
//     make_deferred_answer, spend_deferred, release_deferred — and NOT `answer`.
//   * `ZenHostApi` (zen/kernel/abi.h, v3) has `defer_answer` and
//     `answer_deferred`, and no `answer`.
//   * So a loaded weave inherits the base: nothing is queued, no refusal event
//     is raised, and the only signal is a Ticket the documented call shape does
//     not look at (`zen/weave/weave.hpp`'s own example is `mail.answer(msg);`).
//
// The asker cannot tell. The answerer cannot tell. The HOST can, and only from a
// bus tap — which is how this was found: a routing weave answered every query it
// received and its caller waited forever, with no refusal anywhere in the trace.
//
// Build:  it is part of the lab's ordinary build.
// Run:    ./repro-answer-seam

#if defined(REPRO_WEAVE)

// ---- the library half -------------------------------------------------------

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>

namespace {

struct Question {
    std::int64_t n = 0;
    ZEN_SHAPE(Question, 1, ZEN_FIELD(n));
};

struct Answer {
    std::int64_t n = 0;
    ZEN_SHAPE(Answer, 1, ZEN_FIELD(n));
};

struct AnswererState {
    std::int64_t asked = 0;
    std::int64_t answered_ok = 0; ///< how many answers this weave BELIEVES it sent
    ZEN_SHAPE(AnswererState, 1, ZEN_FIELD(asked), ZEN_FIELD(answered_ok));
};

class Answerer : public loom::WeaveBase<Answerer, AnswererState, loom::Accept<Question>,
                                        loom::Emit<Answer>> {
public:
    void on(const Question& q, loom::Mail& mail) {
        ++state_.asked;
#if defined(REPRO_DIRECT)
        // The documented shape, straight out of zen/weave/weave.hpp.
        const loom::Ticket t = mail.answer(Answer{q.n * 2});
        if (t.valid()) {
            ++state_.answered_ok;
        }
#else
        // The workaround: take the answer right away and spend it immediately.
        loom::DeferredAnswer right = mail.defer_answer();
        if (right.valid() && loom::answer_deferred(right, mail, Answer{q.n * 2}).valid()) {
            ++state_.answered_ok;
        }
#endif
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Answerer)

#else

// ---- the host half ----------------------------------------------------------

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <string>

namespace {

struct Question {
    std::int64_t n = 0;
    ZEN_SHAPE(Question, 1, ZEN_FIELD(n));
};

struct Answer {
    std::int64_t n = 0;
    ZEN_SHAPE(Answer, 1, ZEN_FIELD(n));
};

struct AskerState {
    std::int64_t heard = 0;
    ZEN_SHAPE(AskerState, 1, ZEN_FIELD(heard));
};

/// An ordinary asker. It checks Loom's attestation, exactly as every honest
/// consumer in this lab does.
class Asker : public loom::WeaveBase<Asker, AskerState, loom::Accept<Answer>,
                                     loom::Emit<Question>> {
public:
    explicit Asker(int& heard) : heard_(&heard) {}
    void on(const Answer&, loom::Mail& mail) {
        ++state_.heard;
        if (mail.answers_ask()) {
            ++*heard_;
        }
    }

private:
    int* heard_;
};

std::string exe_dir() {
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return ".";
    }
    buf[n] = '\0';
    const std::string path(buf, static_cast<std::size_t>(n));
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

/// Load `stem`, ask it one question, and report what came back — plus what the
/// BUS saw, which is the only place the difference is visible.
int probe(const char* stem) {
    loom::Switchboard bus;
    loom::Kernel kernel(bus);

    int refusals = 0;
    int answers_on_the_wire = 0;
    bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Refused) {
            ++refusals;
        } else if (ev.kind == loom::EventKind::Delivered &&
                   ev.schema_name == Answer::zen_name) {
            ++answers_on_the_wire;
        }
    });

    int heard = 0;
    const loom::WeaveId asker = loom::mount<Asker>(bus, heard);
    const loom::LoadResult loaded = kernel.load(stem, exe_dir() + "/" + stem + ".so");
    if (!loaded.ok) {
        std::printf("  %-24s COULD NOT LOAD: %s\n", stem, loaded.error.c_str());
        return 1;
    }
    bus.send_as(asker, loaded.id,
                loom::Message(loom::to_value(Question{21}), asker, asker, 7));
    bus.pump();

    std::printf("  %-24s answers delivered on the bus: %d | asker heard an attested answer: %s"
                " | refusals seen anywhere: %d\n",
                stem, answers_on_the_wire, heard == 1 ? "yes" : "NO ", refusals);
    return heard == 1 ? 0 : 1;
}

} // namespace

int main() {
    std::printf("repro: does Mail::answer() cross the .so seam?\n");
    const int direct = probe("repro-answer-direct");
    const int deferred = probe("repro-answer-deferred");

    std::printf("\n  direct   (mail.answer)                 -> %s\n",
                direct == 0 ? "answered" : "SILENTLY DID NOTHING");
    std::printf("  deferred (defer_answer + spend)        -> %s\n",
                deferred == 0 ? "answered" : "SILENTLY DID NOTHING");
    // The finding is that these two DIFFER. Exit non-zero if they ever stop
    // differing, so this file turns into a regression check the day the ABI
    // grows an `answer` door — at which point it should be deleted.
    if (direct != 0 && deferred == 0) {
        std::printf("\n  => reproduced: the immediate authenticated answer is native-only.\n");
        return 0;
    }
    std::printf("\n  => NOT reproduced. Either the seam was closed (good: delete this file and\n"
                "     kitchen/answering.hpp) or the deferred path broke too (bad).\n");
    return 1;
}

#endif
