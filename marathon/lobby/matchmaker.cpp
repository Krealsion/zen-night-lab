// The matchmaker — the office whose statements matter, built TWICE from one
// source so the two honest readings can be measured against each other.
//
//     (default)            PUSH. Watches `LobbyChanged`, and when enough players
//                          are ready it ANNOUNCES a match to each of them as an
//                          ordinary directed message.
//
//     MATCHMAKER_PULL      PULL. Players ASK (`SeekMatch`); the matchmaker takes
//                          each request's answer right away and spends it, later,
//                          on the `MatchCreated` that names that player.
//
// ---- THE THING THE PAIR EXISTS TO SHOW -------------------------------------
//
// A PUSH IS TRIGGERED BY A FACT ABOUT THE WORLD. "Enough people are ready" is
// not an answer to anybody's question, and Loom attests ANSWERS. So there is
// nothing to attach an attestation to, and a `MatchCreated` announced this way is
// indistinguishable from one any weave with an ordinary grant can send.
//
// A PULL IS TRIGGERED BY A QUESTION, which is exactly why it can be attested —
// and re-founding the service on each player's own question is not a refactor,
// it is a different service. What it costs is measured in the suite:
//
//   * the answer capacity is ONE LOOM's (64), not one weave's;
//   * an answer right belongs to the life that earned it, so a replaced
//     matchmaker strands every waiting player — and the successor, doing the only
//     thing it can, sends an UNATTESTED `MatchCreated` that a strict player is
//     obliged to reject.
//
// The last one is the sharpest: the pull style's wall keeps out the forger AND
// the honest successor, because from the receiver's side they look identical.

#include "vocabulary.hpp"

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

using namespace marathon::lobby;

#if defined(MATCHMAKER_PULL)
constexpr bool kPull = true;
constexpr const char* kStyle = "pull";
#else
constexpr bool kPull = false;
constexpr const char* kStyle = "push";
#endif

#if defined(MATCHMAKER_LABEL)
constexpr const char* kLabel = MATCHMAKER_LABEL;
#else
constexpr const char* kLabel = "v1";
#endif

/// A player who has asked to be matched (pull) — the words half.
struct Seeker {
    std::string player;
    std::string weave;
    std::int64_t correlation = 0;
    ZEN_SHAPE(Seeker, 1, ZEN_FIELD(player), ZEN_FIELD(weave), ZEN_FIELD(correlation));
};

struct MatchmakerState {
    std::vector<Seeker> seekers;         ///< pull only
    std::vector<WaitingPlayer> inherited; ///< taken over during a preparation
    std::vector<std::string> last_ready;
    std::vector<std::string> last_ready_weaves;
    std::int64_t next_match = 1;
    std::int64_t matches = 0;
    std::int64_t seek_refused = 0;
    std::int64_t stranded = 0;   ///< inherited seekers this incarnation cannot attest to
    std::int64_t personal = 0;   ///< statements this weave made in its PERSONAL capacity
    ZEN_EXPOSE();
    ZEN_SHAPE(MatchmakerState, 1, ZEN_FIELD(seekers), ZEN_FIELD(inherited),
              ZEN_FIELD(last_ready), ZEN_FIELD(last_ready_weaves), ZEN_FIELD(next_match),
              ZEN_FIELD(matches), ZEN_FIELD(seek_refused), ZEN_FIELD(stranded),
              ZEN_FIELD(personal));
};

/// One unspent answer right per waiting player. Pull only. Per-incarnation and
/// never state, for the reason four projects have now recorded: a
/// `DeferredAnswer` is a capability bound to this weave's life.
struct HeldSeek {
    std::string weave;
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

class Matchmaker
    : public loom::WeaveBase<
          Matchmaker, MatchmakerState,
          loom::Accept<LobbyChanged, SeekMatch, MatchmakerStatus, SpeakPersonally,
                       DescribeWaiting, PrepareMatchmaker, loom::Activated>,
          loom::Emit<MatchCreated, MatchStarted, MatchCancelled, LobbyChatter, WaitingDescribed,
                     MatchmakerReady, MatchmakerNotReady, loom::Result, loom::Ack,
                     loom::Refused>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        strand_or_serve(mail);
    }

    // ---- the push trigger ---------------------------------------------------

    /// A fact about the world arrived. In the push build this is the whole
    /// mechanism; in the pull build it is only bookkeeping, because a fact is not
    /// a question and cannot be answered.
    void on(const LobbyChanged& c, loom::Mail& mail) {
        state_.last_ready = c.ready;
        state_.last_ready_weaves = c.ready_weaves;
        if (kPull) {
            try_pull_match(mail);
            return;
        }
        if (c.ready.size() < kMatchSize || c.ready.size() != c.ready_weaves.size()) {
            return;
        }
        std::vector<std::string> names;
        std::vector<std::string> weaves;
        for (std::size_t i = 0; i < kMatchSize; ++i) {
            names.push_back(c.ready[i]);
            weaves.push_back(c.ready_weaves[i]);
        }
        const std::string match = mint_match();
        ++state_.matches;
        // ANNOUNCED. There is no ask to answer, so this is an ordinary directed
        // message and it carries nothing a receiver can check.
        for (const std::string& w : weaves) {
            const loom::WeaveId who{parse_u64(w)};
            if (who.valid()) {
                mail.send(who, MatchCreated{match, names, "server-" + match});
            }
        }
        // Observers get the weaker fact. See MatchStarted.
        mail.publish(MatchStarted{match, names});
    }

    // ---- the pull trigger ---------------------------------------------------

    /// A player asked. THE ANSWER RIGHT IS TAKEN AWAY and held until a match
    /// exists — which is the only construction in Zen today that lets an office's
    /// statement be attested.
    void on(const SeekMatch&, loom::Mail& mail) {
        if (!kPull) {
            (void)mail.answer(loom::Refused{
                "this matchmaker announces matches rather than answering requests for them"});
            return;
        }
        const std::string who = std::to_string(mail.sender().value);
        for (const Seeker& s : state_.seekers) {
            if (s.weave == who) {
                (void)mail.answer(loom::Refused{"you are already waiting for a match"});
                return;
            }
        }
        loom::DeferredAnswer right = mail.defer_answer();
        if (!right.valid()) {
            // THE COST, VISIBLE AT THE CALL SITE THAT PAYS IT. The capacity that
            // ran out is one LOOM's, not this weave's.
            ++state_.seek_refused;
            (void)mail.answer(loom::Refused{
                "this matchmaker holds an answer right for every waiting player and this Loom "
                "has no unfinished-conversation slots left"});
            return;
        }
        state_.seekers.push_back(Seeker{name_of(who), who,
                                        static_cast<std::int64_t>(mail.correlation())});
        held_.push_back(HeldSeek{who, std::move(right)});
        try_pull_match(mail);
    }

    // ---- the same weave, speaking personally --------------------------------

    /// Not an attack and not a lie: this weave really is saying this. It is just
    /// not saying it AS THE MATCHMAKER — and nothing on the wire records the
    /// difference.
    void on(const SpeakPersonally& p, loom::Mail& mail) {
        ++state_.personal;
        for (const std::string& w : p.weaves) {
            const loom::WeaveId who{parse_u64(w)};
            if (who.valid()) {
                mail.send(who, MatchCreated{p.match, p.players, "server-" + p.match});
            }
        }
    }

    void on(const MatchmakerStatus&, loom::Mail& mail) {
        (void)mail.answer(loom::Result{
            "matchmaker[" + std::string(kLabel) + "/" + std::string(kStyle) +
            "]: matches=" + std::to_string(state_.matches) + " waiting=" +
            std::to_string(state_.seekers.size()) + " seek_refused=" +
            std::to_string(state_.seek_refused) + " stranded=" +
            std::to_string(state_.stranded) + " personal=" + std::to_string(state_.personal)});
    }

    // ---- the replacement conversation ---------------------------------------

    /// What this office still owes. The `held_answer_rights` field is the honest
    /// part: it says how many of those waiting players were promised an ATTESTED
    /// answer that no successor can give them.
    void on(const DescribeWaiting&, loom::Mail& mail) {
        WaitingDescribed described;
        for (const Seeker& s : state_.seekers) {
            described.waiting.push_back(WaitingPlayer{s.player, s.weave, s.correlation});
        }
        described.held_answer_rights = static_cast<std::int64_t>(held_.size());
        (void)mail.answer(described);
    }

    void on(const PrepareMatchmaker& p, loom::Mail& mail) {
        if (p.match_size != static_cast<std::int64_t>(kMatchSize)) {
            (void)mail.answer(MatchmakerNotReady{
                "the house matches " + std::to_string(p.match_size) +
                " players and this artifact matches " + std::to_string(kMatchSize)});
            return;
        }
        if (p.inherit.size() > kMaxPlayers) {
            (void)mail.answer(MatchmakerNotReady{
                "a preparation may hand over at most " + std::to_string(kMaxPlayers) +
                " waiting players, and this one carries " + std::to_string(p.inherit.size())});
            return;
        }
        state_.inherited = p.inherit;
        (void)mail.answer(MatchmakerReady{static_cast<std::int64_t>(state_.inherited.size())});
    }

private:
    std::string mint_match() { return "match-" + std::to_string(state_.next_match++); }

    std::string name_of(const std::string& weave) const {
        for (std::size_t i = 0; i < state_.last_ready_weaves.size(); ++i) {
            if (state_.last_ready_weaves[i] == weave && i < state_.last_ready.size()) {
                return state_.last_ready[i];
            }
        }
        return "player-" + weave;
    }

    /// Enough seekers exist: spend their rights.
    void try_pull_match(loom::Mail& mail) {
        if (!kPull || state_.seekers.size() < kMatchSize) {
            return;
        }
        std::vector<std::string> names;
        std::vector<std::string> weaves;
        for (std::size_t i = 0; i < kMatchSize; ++i) {
            names.push_back(state_.seekers[i].player);
            weaves.push_back(state_.seekers[i].weave);
        }
        const std::string match = mint_match();
        ++state_.matches;
        for (const std::string& w : weaves) {
            spend(mail, w, MatchCreated{match, names, "server-" + match});
        }
        // Observers get the weaker fact in BOTH styles. See MatchStarted.
        mail.publish(MatchStarted{match, names});
        state_.seekers.erase(state_.seekers.begin(),
                             state_.seekers.begin() + static_cast<std::ptrdiff_t>(kMatchSize));
    }

    void spend(loom::Mail& mail, const std::string& weave, const MatchCreated& m) {
        for (std::size_t i = 0; i < held_.size(); ++i) {
            if (held_[i].weave != weave) {
                continue;
            }
            (void)loom::answer_deferred(held_[i].right, mail, m);
            held_.erase(held_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
        // No right to spend: an INHERITED seeker. See strand_or_serve.
        const loom::WeaveId who{parse_u64(weave)};
        if (who.valid()) {
            mail.send(who, m);
        }
    }

    /// THE PULL STYLE'S REPLACEMENT PROBLEM, made visible rather than hidden.
    ///
    /// A successor inherits the FACT that players are waiting — words, so they
    /// cross — but not the answer rights, which belonged to a life that has
    /// ended. So every inherited player is now owed a statement this incarnation
    /// can only make as an ordinary message: the attestation they were promised
    /// is gone, and a strict player will refuse the honest successor for exactly
    /// the same reason it refuses a forger.
    ///
    /// The count is kept because a service that quietly downgraded its own
    /// guarantee would be doing the thing this whole marathon is against.
    void strand_or_serve(loom::Mail& mail) {
        std::vector<WaitingPlayer> inherited;
        inherited.swap(state_.inherited);
        for (const WaitingPlayer& w : inherited) {
            state_.seekers.push_back(Seeker{w.player, w.weave, w.correlation});
            ++state_.stranded;
        }
        try_pull_match(mail);
    }

    zengine::ActivationCursor activation_;
    std::vector<HeldSeek> held_;
};

} // namespace

static_assert(kLabel[0] != '\0', "a matchmaker generation needs a label");

ZEN_EXPORT_WEAVE(Matchmaker)
