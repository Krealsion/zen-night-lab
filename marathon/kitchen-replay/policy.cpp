// The kitchen's routing brain — REPLACEABLE DOMAIN POLICY, and nothing else.
//
// One source, two libraries:
//
//     kitchen-policy-house   honours the diner. A preference is a wish the house
//                            grants when it can.
//     kitchen-policy-rush    honours the KITCHEN. A preference is advice; the
//                            dish goes to the specialist — unless the diner made
//                            the preference REQUIRED, which binds every policy.
//
// Swapping one for the other changes where an identical order goes, and changes
// the reason the diner is told, with no line of the expediter, the stations, the
// vocabulary or the host altered.
//
// IT IS PURE, AND THE PURITY IS STRUCTURAL. The query carries the roster; the
// answer carries the decision. This weave holds no roster, starts no timers,
// keeps no tickets, and has no way to learn who is open — because "who is open"
// is liveness bookkeeping and belongs to the party that watches promises.
//
// REPLAY NOTE — THE ONE LINE THAT CHANGED IN THIS FILE. Night One could not
// write `mail.answer(...)` here: this weave is dynamically loaded, and the
// immediate authenticated answer did not cross the `.so` seam. It did not fail,
// it did NOTHING, silently, and the whole kitchen hung on it. The file carried a
// `answer_across_the_seam()` workaround at every call site with the finding
// written above it. ABI v4 grew the `answer` door and the workaround is deleted.
// `repro_answer_seam.cpp` measures that rather than trusting this comment.

#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace marathon::kitchen;

#if defined(KITCHEN_POLICY_RUSH)
constexpr const char* kPolicyName = "rush";
constexpr bool kPrefersSpecialist = true;
#else
constexpr const char* kPolicyName = "house";
constexpr bool kPrefersSpecialist = false;
#endif

/// One honest counter per outcome. Exposed: a routing brain holds no secrets,
/// and being able to poke "how many did you refuse?" is the cheapest diagnostic
/// there is.
struct PolicyState {
    std::int64_t queries = 0;
    std::int64_t preferred = 0;
    std::int64_t fallbacks = 0;
    std::int64_t refusals = 0;
    std::int64_t unanswerable = 0; ///< asks that carried no answer authority at all
    ZEN_EXPOSE();
    ZEN_SHAPE(PolicyState, 1, ZEN_FIELD(queries), ZEN_FIELD(preferred), ZEN_FIELD(fallbacks),
              ZEN_FIELD(refusals), ZEN_FIELD(unanswerable));
};

/// Does this station's comma-joined menu contain `dish`? Exact element match —
/// "fries" must not match "curly fries" by accident.
bool menu_has(const std::string& menu, const std::string& dish) {
    std::size_t at = 0;
    while (at <= menu.size()) {
        const std::size_t comma = menu.find(',', at);
        const std::size_t end = comma == std::string::npos ? menu.size() : comma;
        if (menu.compare(at, end - at, dish) == 0) {
            return true;
        }
        if (comma == std::string::npos) {
            return false;
        }
        at = comma + 1;
    }
    return false;
}

std::size_t menu_size(const std::string& menu) {
    if (menu.empty()) {
        return 0;
    }
    std::size_t n = 1;
    for (const char c : menu) {
        if (c == ',') {
            ++n;
        }
    }
    return n;
}

class KitchenPolicy : public loom::WeaveBase<KitchenPolicy, PolicyState, loom::Accept<RouteQuery>,
                                             loom::Emit<RouteChoice>> {
public:
    void on(const RouteQuery& q, loom::Mail& mail) {
        ++state_.queries;

        // 1. The wire contract first, and it is REFUSED rather than guessed at.
        //    An unknown fallback word is the diner asking for something this
        //    kitchen has never defined; picking the nearest one on their behalf
        //    would be the kitchen deciding something the diner did not say.
        if (q.fallback != kFallbackAnyStation && q.fallback != kFallbackNone) {
            refuse(mail, q, "fallback '" + q.fallback +
                                "' is not a word this kitchen knows (expected '" +
                                std::string(kFallbackAnyStation) + "' or an empty string meaning "
                                                                   "the preference is required)");
            return;
        }
        // The roster arrives as two parallel lists. A mismatch is a malformed
        // question, not a routing problem — say so rather than index past an end.
        if (q.open_stations.size() != q.open_menus.size()) {
            refuse(mail, q, "the roster in this query is malformed (" +
                                std::to_string(q.open_stations.size()) + " stations but " +
                                std::to_string(q.open_menus.size()) + " menus)");
            return;
        }
        if (q.open_stations.empty()) {
            refuse(mail, q, "the kitchen is closed: no station has announced itself");
            return;
        }

        const bool required = q.fallback == kFallbackNone;
        const bool wants_a_station = !q.prefer.empty() && q.prefer != kPreferAny;

        // 2. Can the preferred station take it?
        const std::size_t preferred_at = find_station(q, q.prefer);
        const bool preferred_can =
            wants_a_station && preferred_at < q.open_stations.size() &&
            menu_has(q.open_menus[preferred_at], q.dish);

        // 3. Who else could? (The specialist first, so `rush` has something to
        //    prefer and `house` has a deterministic fallback.)
        const std::size_t other_at = find_specialist(q, /*excluding=*/q.prefer);

        // 4. A REQUIRED preference binds every policy, including this one. An
        //    empty fallback is the diner saying "this station or nothing", and a
        //    policy that overrode it would be turning a refusal the diner asked
        //    for into a dish they did not.
        if (required) {
            if (!wants_a_station) {
                refuse(mail, q,
                       "no preferred station was named and no fallback is acceptable, so there is "
                       "nothing this kitchen is permitted to choose");
                return;
            }
            if (!preferred_can) {
                refuse(mail, q, "station '" + q.prefer + "' " + why_not(q, preferred_at) +
                                    ", and no fallback is acceptable");
                return;
            }
            choose(mail, q, q.prefer, kRoutedPreferred,
                   "station '" + q.prefer + "' was required and can cook '" + q.dish + "'");
            return;
        }

        // 5. The fallback IS acceptable, so this is where the two policies part.
        if (kPrefersSpecialist && other_at < q.open_stations.size() &&
            (!preferred_can || menu_size(q.open_menus[other_at]) <
                                   menu_size(q.open_menus[preferred_at]))) {
            const std::string& s = q.open_stations[other_at];
            choose(mail, q, s, kRoutedFallback,
                   "the rush kitchen sends '" + q.dish + "' to the specialist '" + s + "'" +
                       (wants_a_station ? " rather than the preferred '" + q.prefer + "'" : ""));
            return;
        }
        if (preferred_can) {
            choose(mail, q, q.prefer, kRoutedPreferred,
                   "station '" + q.prefer + "' was preferred and can cook '" + q.dish + "'");
            return;
        }
        if (other_at < q.open_stations.size()) {
            const std::string& s = q.open_stations[other_at];
            choose(mail, q, s, kRoutedFallback,
                   (wants_a_station ? "station '" + q.prefer + "' " + why_not(q, preferred_at) +
                                          ", so the fallback took it to '" + s + "'"
                                    : "no station was preferred, so the fallback took it to '" + s +
                                          "'"));
            return;
        }
        refuse(mail, q, "no open station cooks '" + q.dish + "'");
    }

private:
    static std::size_t find_station(const RouteQuery& q, const std::string& name) {
        for (std::size_t i = 0; i < q.open_stations.size(); ++i) {
            if (q.open_stations[i] == name) {
                return i;
            }
        }
        return q.open_stations.size();
    }

    /// The open station with the SHORTEST menu that can cook the dish — the
    /// specialist. Ties break on announcement order, so the answer is a function
    /// of the query and nothing else.
    static std::size_t find_specialist(const RouteQuery& q, const std::string& excluding) {
        std::size_t best = q.open_stations.size();
        for (std::size_t i = 0; i < q.open_stations.size(); ++i) {
            if (q.open_stations[i] == excluding || !menu_has(q.open_menus[i], q.dish)) {
                continue;
            }
            if (best == q.open_stations.size() ||
                menu_size(q.open_menus[i]) < menu_size(q.open_menus[best])) {
                best = i;
            }
        }
        return best;
    }

    /// The difference between "that station is not here" and "that station is
    /// here and does not cook this" matters to a diner, so it is never collapsed.
    static std::string why_not(const RouteQuery& q, std::size_t at) {
        return at < q.open_stations.size() ? "does not cook '" + q.dish + "'" : "is not open";
    }

    void choose(loom::Mail& mail, const RouteQuery& q, const std::string& station,
                const char* resolved, std::string reason) {
        if (std::string(resolved) == kRoutedPreferred) {
            ++state_.preferred;
        } else {
            ++state_.fallbacks;
        }
        say(mail, RouteChoice{q.order_id, station, resolved,
                              "[" + std::string(kPolicyName) + "] " + std::move(reason)});
    }

    void refuse(loom::Mail& mail, const RouteQuery& q, std::string reason) {
        ++state_.refusals;
        say(mail, RouteChoice{q.order_id, "", kRoutedRefused,
                              "[" + std::string(kPolicyName) + "] " + std::move(reason)});
    }

    /// THE ONE ANSWER, and the ticket is LOOKED AT.
    ///
    /// Night One's lesson was not "answer is broken" — it was that the documented
    /// call shape `mail.answer(msg);` discards the only signal there is. An
    /// invalid ticket here is a real condition (a root send has nobody to answer)
    /// and it is counted rather than dropped, so a future silence is visible in
    /// this weave's own state instead of being invisible to both parties.
    void say(loom::Mail& mail, const RouteChoice& choice) {
        if (!mail.answer(choice).valid()) {
            ++state_.unanswerable;
        }
    }
};

} // namespace

ZEN_EXPORT_WEAVE(KitchenPolicy)
