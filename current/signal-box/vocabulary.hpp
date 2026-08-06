// The signal box's vocabulary.
//
// Two kinds of fact, kept apart on purpose, because the railway keeps them
// apart:
//
//   MESSAGES   a train moved / set me a route / release it     causal, queued
//   SENSES     this section is occupied / the box is open      latest claim
//
// A track circuit does not narrate. It sits there being one bit, and anyone who
// wants to know pulls it. An interlocking that had to subscribe to a stream of
// occupancy events — and reason about which ones it had missed — would be a
// worse interlocking than one that looks at the panel. That is the entire
// reason this application has Senses in it, and the reason it has messages too:
// "a train entered section C" is a thing that happened, and "section C is
// occupied" is a thing that is so, and they are not the same sentence.

#ifndef SIGNAL_BOX_VOCABULARY_HPP
#define SIGNAL_BOX_VOCABULARY_HPP

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace signalbox {

// ---------------------------------------------------------------------------
// The layout. A diamond crossing — two lines that cross at grade, which is the
// smallest piece of railway that needs an interlocking at all:
//
//                       up-departure
//                            │
//     down-approach ──── junction ──── down-departure
//                            │
//                       up-approach
//
// A train on route UP runs up-approach → junction → up-departure.
// A train on route DOWN runs down-approach → junction → down-departure.
// Both need the junction, and they must never have it at the same time.
//
// The approach section is the BERTH: it is where a train stands waiting, so a
// route never locks it. A route locks only what the train is about to move
// into. Getting this wrong is the difference between an interlocking and a
// thing that refuses to let a waiting train depart because it can see the
// waiting train.
// ---------------------------------------------------------------------------

inline constexpr const char* kRouteUp = "UP";
inline constexpr const char* kRouteDown = "DOWN";

// The offices. A reader addresses "whoever is currently the junction track
// circuit", never one exact weave — which is what makes the plant and the box
// independently replaceable.
inline constexpr const char* kOfficeInterlocking = "interlocking";
inline constexpr const char* kOfficeMonitor = "monitor";
inline constexpr const char* kOfficeSignaller = "signaller";

inline std::string section_office(const std::string& section) {
    return "section." + section;
}

// ---------------------------------------------------------------------------
// What a track circuit currently claims is so.
// ---------------------------------------------------------------------------

struct Occupancy {
    std::string section;
    bool occupied = false;
    std::string train; // empty when clear
    ZEN_SHAPE(Occupancy, 1, ZEN_FIELD(section), ZEN_FIELD(occupied), ZEN_FIELD(train));
};

// What the box currently claims about itself. Its absence is the interesting
// case: an office claim is erased when the office becomes unheld, so "no claim
// under the interlocking office" is exactly "there is no signal box", and a
// signaller can read that without asking anyone anything.
struct BoxStatus {
    bool open = false;
    bool up_locked = false;
    bool down_locked = false;
    ZEN_SHAPE(BoxStatus, 1, ZEN_FIELD(open), ZEN_FIELD(up_locked), ZEN_FIELD(down_locked));
};

// ---------------------------------------------------------------------------
// Things that happen.
// ---------------------------------------------------------------------------

struct TrainEnters {
    std::string train;
    ZEN_SHAPE(TrainEnters, 1, ZEN_FIELD(train));
};

struct TrainLeaves {
    std::string train;
    ZEN_SHAPE(TrainLeaves, 1, ZEN_FIELD(train));
};

// Commissioning. A box that has just been installed has not said anything yet,
// so the panel is dark even though the equipment is there — and "dark" is
// exactly how "there is no box" looks too. Ringing the box is how a signaller
// tells those apart: if anybody is home, the panel lights.
struct PutInService {
    std::int64_t at = 0;
    ZEN_SHAPE(PutInService, 1, ZEN_FIELD(at));
};

struct RouteRequest {
    std::string route;
    std::string train;
    ZEN_SHAPE(RouteRequest, 1, ZEN_FIELD(route), ZEN_FIELD(train));
};

struct RouteRelease {
    std::string route;
    std::string train;
    ZEN_SHAPE(RouteRelease, 1, ZEN_FIELD(route), ZEN_FIELD(train));
};

// The box's answer to a request. It travels as an *answer* rather than an
// ordinary send, so the signaller can ask Loom whether this is the reply to its
// own request rather than trusting a correlation field.
struct RouteVerdict {
    std::string route;
    std::string train;
    bool granted = false;
    std::string reason; // empty when granted
    ZEN_SHAPE(RouteVerdict, 1, ZEN_FIELD(route), ZEN_FIELD(train), ZEN_FIELD(granted),
              ZEN_FIELD(reason));
};

// The monitor's word. Authored as the monitor office so the signaller can act on
// it without trusting whoever happens to be able to send it this shape.
struct SafetyFault {
    std::string what;
    std::int64_t at = 0;
    ZEN_SHAPE(SafetyFault, 1, ZEN_FIELD(what), ZEN_FIELD(at));
};

// The clock. The host owns time; nothing here invents one.
struct Tick {
    std::int64_t at = 0;
    ZEN_SHAPE(Tick, 1, ZEN_FIELD(at));
};

struct Sweep {
    std::int64_t at = 0;
    ZEN_SHAPE(Sweep, 1, ZEN_FIELD(at));
};

// ---------------------------------------------------------------------------
// Declared state.
// ---------------------------------------------------------------------------

struct SectionState {
    std::string name;
    bool occupied = false;
    std::int64_t entries = 0;
    ZEN_SHAPE(SectionState, 1, ZEN_FIELD(name), ZEN_FIELD(occupied), ZEN_FIELD(entries));
};

struct BoxState {
    std::int64_t granted = 0;
    std::int64_t refused = 0;
    bool up_locked = false;
    bool down_locked = false;
    ZEN_SHAPE(BoxState, 1, ZEN_FIELD(granted), ZEN_FIELD(refused), ZEN_FIELD(up_locked),
              ZEN_FIELD(down_locked));
};

struct MonitorState {
    std::int64_t sweeps = 0;
    std::int64_t faults = 0;
    std::int64_t unsupervised = 0;
    ZEN_SHAPE(MonitorState, 1, ZEN_FIELD(sweeps), ZEN_FIELD(faults), ZEN_FIELD(unsupervised));
};

struct SignallerState {
    std::int64_t step = 0;
    std::int64_t granted = 0;
    std::int64_t refused = 0;
    std::int64_t held = 0;      // movements not attempted because the box was shut
    std::int64_t lost = 0;      // requests that never came back
    std::int64_t re_set = 0;    // routes re-set after the box changed underneath us
    bool at_danger = false;
    ZEN_SHAPE(SignallerState, 1, ZEN_FIELD(step), ZEN_FIELD(granted), ZEN_FIELD(refused),
              ZEN_FIELD(held), ZEN_FIELD(lost), ZEN_FIELD(re_set), ZEN_FIELD(at_danger));
};

// The sections a route locks, in the order the train will occupy them — not
// including the berth it is standing on. The interlocking for a junction is a
// specific piece of equipment built for that junction; this table is the
// equipment, not configuration.
inline std::vector<std::string> route_sections(const std::string& route) {
    if (route == kRouteUp) {
        return {"junction", "up-departure"};
    }
    if (route == kRouteDown) {
        return {"junction", "down-departure"};
    }
    return {};
}

inline const char* route_berth(const std::string& route) {
    if (route == kRouteUp) {
        return "up-approach";
    }
    if (route == kRouteDown) {
        return "down-approach";
    }
    return "";
}

inline std::vector<std::string> all_routes() { return {kRouteUp, kRouteDown}; }

inline std::vector<std::string> all_sections() {
    return {"up-approach", "down-approach", "junction", "up-departure", "down-departure"};
}

} // namespace signalbox

#endif // SIGNAL_BOX_VOCABULARY_HPP
