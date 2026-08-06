// The signal box: the interlocking for the flat junction.
//
// This is the one piece of the railway that is loaded from a shared library,
// and that is not an arbitrary choice about which file to put in a .so. An
// interlocking is separately-built, separately-certified equipment that gets
// plugged into a plant it did not build and does not own. It knows the track
// layout and it knows one rule; it does not know how many trains there are,
// what the timetable is, or who is asking.
//
// It is also the one thing that can be *taken out*, and the railway has to
// behave sanely when it is. That is what makes load/unload mean something here
// rather than being a mechanism on display.
//
// What it may do is decided entirely by the host at load: this weave is given
// permission to answer with a verdict, to claim its own status as the
// interlocking office, and to observe occupancy. It cannot send anything else
// anywhere, and it does not ask to.

#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>

#include <string>

namespace {

using namespace signalbox;

class SignalBox : public loom::WeaveBase<SignalBox, BoxState,
                                         loom::Accept<PutInService, RouteRequest, RouteRelease>,
                                         loom::Emit<RouteVerdict>,
                                         loom::Claims<BoxStatus>> {
public:
    // Freshly loaded equipment has claimed nothing, so nobody can see it is
    // there. This is the moment it says so.
    void on(const PutInService&, loom::Mail& mail) { publish_status(mail); }

    void on(const RouteRequest& req, loom::Mail& mail) {
        const std::string reason = why_not(req.route, mail);
        if (!reason.empty()) {
            ++state_.refused;
            mail.answer(RouteVerdict{req.route, req.train, false, reason});
            publish_status(mail);
            return;
        }
        lock(req.route, true);
        ++state_.granted;
        mail.answer(RouteVerdict{req.route, req.train, true, ""});
        publish_status(mail);
    }

    void on(const RouteRelease& rel, loom::Mail& mail) {
        lock(rel.route, false);
        publish_status(mail);
    }

private:
    // The whole safety rule, in one place, stated as a refusal reason rather
    // than a bool — because a signaller standing at a red signal needs to know
    // WHICH thing is in the way, and "false" sends them to look at everything.
    std::string why_not(const std::string& route, loom::Mail& mail) {
        const std::vector<std::string> sections = route_sections(route);
        if (sections.empty()) {
            return "no such route: " + route;
        }
        if (locked(route)) {
            return "route " + route + " is already set";
        }

        // Conflict is not a hard-coded pair of route names: it is the fact that
        // two routes want the same piece of railway. Ask the layout.
        for (const std::string& other : all_routes()) {
            if (other == route || !locked(other)) {
                continue;
            }
            for (const std::string& mine : sections) {
                for (const std::string& theirs : route_sections(other)) {
                    if (mine == theirs) {
                        return "conflicts with route " + other + " over " + mine;
                    }
                }
            }
        }

        for (const std::string& section : sections) {
            const loom::SenseReading reading =
                mail.latest_from_office<Occupancy>(section_office(section));

            // ABSENCE OF EVIDENCE IS NOT EVIDENCE OF CLEARANCE. A track circuit
            // that has never claimed, or whose office is unheld, is a track
            // circuit this box cannot see — and an interlocking that treats
            // "I don't know" as "clear" is the failure mode the whole trade
            // exists to prevent. Loom hands us the distinction for free:
            // NoClaim and NotAuthorized are different answers, and neither of
            // them is `false`.
            if (!reading) {
                return "no occupancy evidence for " + section + " (" +
                       loom::name_of(reading.refusal) + ")";
            }

            const Occupancy occ = loom::from_value<Occupancy>(*reading.value);
            if (occ.occupied) {
                return section + " occupied by " + occ.train;
            }
        }
        return "";
    }

    bool locked(const std::string& route) const {
        if (route == kRouteUp) {
            return state_.up_locked;
        }
        if (route == kRouteDown) {
            return state_.down_locked;
        }
        return false;
    }

    void lock(const std::string& route, bool value) {
        if (route == kRouteUp) {
            state_.up_locked = value;
        } else if (route == kRouteDown) {
            state_.down_locked = value;
        }
    }

    // Claimed AS THE OFFICE, not personally. A personal claim would live under
    // a different key and would be unreachable through
    // latest_from_office("interlocking") — which is the whole point: what the
    // signaller wants to read is "what does the box say", not "what does weave
    // 7 say".
    void publish_status(loom::Mail& mail) {
        mail.as_role(kOfficeInterlocking)
            .claim(BoxStatus{true, state_.up_locked, state_.down_locked});
    }
};

} // namespace

ZEN_EXPORT_WEAVE(SignalBox)
