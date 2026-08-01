// THE RE-TEST of Night One's sharpest core finding.
//
// Night One found this, with a reproducer of exactly this shape:
//
//     `loom::Mail::answer()` — Loom's authenticated one-shot answer — does not
//     cross the `.so` seam, and fails SILENTLY when a dynamically loaded weave
//     calls it. Nothing queued, no refusal event, and the only signal was a
//     Ticket the documented call shape (`mail.answer(msg);`) does not look at.
//
// ABI v4 grew an `answer` door. This file does not take that on trust; it
// MEASURES it, and it measures three separate things, because "the door exists"
// and "the door is honest" are different claims:
//
//     answer-direct     mail.answer(...)                  must answer
//     answer-deferred   defer_answer() + spend            must answer
//     answer-twice      mail.answer(...) twice            the SECOND must come
//                                                         back invalid
//
// The third probe is the one that matters most. Night One's real complaint was
// never "answer does nothing" — it was that a weave could not TELL. A door that
// works but reports success for an answer that was refused would be the same
// failure wearing better clothes. Loom grants one authenticated answer per
// request; the second call must therefore fail, and it must fail VISIBLY across
// the seam or the finding is only half closed.
//
// HOW THE WEAVE'S OWN BELIEF GETS OUT: it sends the host an ordinary `Report`.
// Reading it out of the weave's serialized state would work too and would need a
// hand-built schema; a message is what a weave has, and using it keeps this file
// to one idea.
//
// Exit 0 means: the seam is closed and the report across it is honest. Exit
// non-zero means something regressed, and the printed lines say which of three.
//
// Run:  ./repro-answer-seam

#include <zen/weave/shape.hpp>

#include <cstdint>

namespace repro {

struct Question {
    std::int64_t n = 0;
    ZEN_SHAPE(Question, 1, ZEN_FIELD(n));
};

struct Answer {
    std::int64_t n = 0;
    ZEN_SHAPE(Answer, 1, ZEN_FIELD(n));
};

/// What the answering weave BELIEVES happened — the only view from the far side
/// of the seam, and the whole point of the exercise.
struct Report {
    std::int64_t said_ok = 0;
    std::int64_t said_refused = 0;
    ZEN_SHAPE(Report, 1, ZEN_FIELD(said_ok), ZEN_FIELD(said_refused));
};

} // namespace repro

#if defined(REPRO_WEAVE)

// ---- the library half -------------------------------------------------------

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

namespace {

struct AnswererState {
    std::int64_t asked = 0;
    std::int64_t answered_ok = 0;
    std::int64_t answered_refused = 0;
    ZEN_SHAPE(AnswererState, 1, ZEN_FIELD(asked), ZEN_FIELD(answered_ok),
              ZEN_FIELD(answered_refused));
};

class Answerer : public loom::WeaveBase<Answerer, AnswererState, loom::Accept<repro::Question>,
                                        loom::Emit<repro::Answer, repro::Report>> {
public:
    void on(const repro::Question& q, loom::Mail& mail) {
        ++state_.asked;
#if defined(REPRO_DIRECT) || defined(REPRO_TWICE)
        // The documented shape, straight out of zen/weave/weave.hpp.
        tally(mail.answer(repro::Answer{q.n * 2}));
#else
        // Night One's workaround: take the answer right away, spend it at once.
        loom::DeferredAnswer right = mail.defer_answer();
        tally(right.valid() ? loom::answer_deferred(right, mail, repro::Answer{q.n * 2})
                            : loom::Ticket{});
#endif
#if defined(REPRO_TWICE)
        // The second answer to the same request. Loom grants ONE, so this must
        // come back invalid — and it must do so across the seam, where Night One
        // found silence.
        tally(mail.answer(repro::Answer{q.n * 3}));
#endif
        mail.send(mail.sender(), repro::Report{state_.answered_ok, state_.answered_refused});
    }

private:
    void tally(loom::Ticket t) {
        if (t.valid()) {
            ++state_.answered_ok;
        } else {
            ++state_.answered_refused;
        }
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Answerer)

#else

// ---- the host half ----------------------------------------------------------

#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <unistd.h>

#include <cstdio>
#include <string>

namespace {

struct Probe {
    bool loaded = false;
    int attested = 0;    ///< attested answers the asker actually heard
    int unattested = 0;  ///< Answer deliveries that carried no attestation
    int on_the_wire = 0; ///< Answer deliveries the BUS saw
    int refusals = 0;    ///< refusals anywhere in the run
    std::int64_t said_ok = -1;
    std::int64_t said_refused = -1;
};

struct AskerState {
    std::int64_t heard = 0;
    ZEN_SHAPE(AskerState, 1, ZEN_FIELD(heard));
};

/// An ordinary asker. It checks Loom's attestation, exactly as every honest
/// consumer in this lab does, and it collects the far side's own account.
class Asker : public loom::WeaveBase<Asker, AskerState,
                                     loom::Accept<repro::Answer, repro::Report>,
                                     loom::Emit<repro::Question>> {
public:
    explicit Asker(Probe& out) : out_(&out) {}

    void on(const repro::Answer&, loom::Mail& mail) {
        ++state_.heard;
        if (mail.answers_ask()) {
            ++out_->attested;
        } else {
            ++out_->unattested;
        }
    }

    void on(const repro::Report& r, loom::Mail&) {
        out_->said_ok = r.said_ok;
        out_->said_refused = r.said_refused;
    }

private:
    Probe* out_;
};

std::string exe_dir() {
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return ".";
    }
    const std::string path(buf, static_cast<std::size_t>(n));
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

/// Load `stem`, ask it one question, and report what came back — plus what the
/// BUS saw, which is the only place a silent failure is visible at all.
Probe probe(const char* stem) {
    loom::Switchboard bus;
    loom::Kernel kernel(bus);

    Probe out;
    bus.add_observer([&out](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Refused) {
            ++out.refusals;
        } else if (ev.kind == loom::EventKind::Delivered &&
                   ev.schema_name == repro::Answer::zen_name) {
            ++out.on_the_wire;
        }
    });

    const loom::WeaveId asker = loom::mount<Asker>(bus, out);
    const loom::LoadResult loaded = kernel.load(stem, exe_dir() + "/" + stem + ".so");
    if (!loaded.ok) {
        std::printf("  %-22s COULD NOT LOAD: %s\n", stem, loaded.error.c_str());
        return out;
    }
    out.loaded = true;
    bus.send_as(asker, loaded.id,
                loom::Message(loom::to_value(repro::Question{21}), asker, asker, 7));
    bus.pump();

    std::printf("  %-22s attested: %d | unattested: %d | on the wire: %d | the weave was told "
                "ok=%lld refused=%lld | refusals: %d\n",
                stem, out.attested, out.unattested, out.on_the_wire,
                static_cast<long long>(out.said_ok), static_cast<long long>(out.said_refused),
                out.refusals);
    return out;
}

} // namespace

int main() {
    std::printf("re-test: Night One found Mail::answer() native-only and SILENT across the .so "
                "seam.\n         ABI v4 grew the door. Measuring, not assuming.\n\n");

    const Probe direct = probe("repro-answer-direct");
    const Probe deferred = probe("repro-answer-deferred");
    const Probe twice = probe("repro-answer-twice");

    // 1. The door exists and carries an authenticated answer.
    const bool direct_ok = direct.loaded && direct.attested == 1 && direct.unattested == 0 &&
                           direct.on_the_wire == 1 && direct.said_ok == 1 &&
                           direct.said_refused == 0;
    // 2. Night One's workaround still works — deleting it was a choice, not a
    //    forced migration, and that distinction is worth keeping true.
    const bool deferred_ok = deferred.loaded && deferred.attested == 1 &&
                             deferred.unattested == 0 && deferred.on_the_wire == 1 &&
                             deferred.said_ok == 1 && deferred.said_refused == 0;
    // 3. THE HONESTY OF THE DOOR. One answer per request: the second call must
    //    fail, the weave must be TOLD it failed, and no second Answer may reach
    //    the asker by any door.
    const bool twice_ok = twice.loaded && twice.attested == 1 && twice.unattested == 0 &&
                          twice.on_the_wire == 1 && twice.said_ok == 1 &&
                          twice.said_refused == 1;

    std::printf("\n  1. mail.answer() crosses the seam                      -> %s\n",
                direct_ok ? "YES  (Night One's finding A is CLOSED)" : "NO   (REGRESSED)");
    std::printf("  2. defer_answer()+spend still crosses it              -> %s\n",
                deferred_ok ? "YES" : "NO   (REGRESSED)");
    std::printf("  3. a SECOND answer is refused, and the weave is told  -> %s\n",
                twice_ok ? "YES  (the silence is gone, not merely the gap)"
                         : "NO   (the door works but cannot report)");

    if (direct_ok && deferred_ok && twice_ok) {
        std::printf("\n  => the seam is closed and the report across it is honest.\n");
        return 0;
    }
    std::printf("\n  => SOMETHING REGRESSED. See the three lines above.\n");
    return 1;
}

#endif
