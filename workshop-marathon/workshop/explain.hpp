#ifndef WORKSHOP_EXPLAIN_HPP
#define WORKSHOP_EXPLAIN_HPP

// Refusal reasons rendered as directions — the diagnostics guide's table,
// mechanized. This is teaching material derived from real system vocabulary
// (the RefusalReason enum), not a hard-coded tutorial: every sentence explains
// an event that actually happened. (Gate 9 grows from here.)

#include <string_view>

namespace workshop {

inline const char* explain_refusal(std::string_view reason) {
    if (reason == "NotAccepted") {
        return "the target never declared that shape - its Accept<...> has no such door";
    }
    if (reason == "CapabilityDenied") {
        return "the sender's grant does not permit that shape to that target - reach is the "
               "host's to give, so ask for it rather than grab it";
    }
    if (reason == "GateRefused") {
        return "the payload did not conform to the shape it claimed - the field path and "
               "expected/actual are in the refusal's own words";
    }
    if (reason == "NoSuchTarget") {
        return "unknown id or unheld role - or a sealed candidate you cannot know exists";
    }
    if (reason == "TargetUnavailable") {
        return "the target is dead, awaiting revival";
    }
    if (reason == "SenderLifeEnded") {
        return "the author's life ended before delivery - expected after a kill or revival";
    }
    if (reason == "AnswerTargetChanged") {
        return "the asker was replaced; the answer refused rather than misdelivering to a "
               "different life at the same address";
    }
    if (reason == "SealedSpeech") {
        return "a prepared candidate reached for the world - only its coordinator may hear it";
    }
    if (reason == "ForeignAuthority") {
        return "an authority another Loom issued - the grant may be fine; the authority "
               "domain is wrong";
    }
    if (reason == "Exhausted") {
        return "a published bound was reached - release or redesign, not retry";
    }
    if (reason == "AdmissionRevoked") {
        return "a scheduled admission met a drifted world - NOTHING changed; read the "
               "transaction outcome";
    }
    if (reason == "RoleAuthorshipDenied") {
        return "the sender asked to speak AS an office it does not hold - refused loudly, "
               "never downgraded to personal speech";
    }
    return "an unrecognized refusal reason (newer substrate than this Workshop?)";
}

} // namespace workshop

#endif // WORKSHOP_EXPLAIN_HPP
