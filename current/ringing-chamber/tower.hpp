// The tower's vocabulary.
//
// A ringing chamber contains six people, six ropes, and no shared picture of
// what is being rung. That absence is the whole application, so the vocabulary
// is arranged to make it structural rather than a promise:
//
//   NOBODY RINGS THE ROW.
//
//   A ringer knows one thing -- its own line, learned before the touch -- and
//   decides, each row, which place it is striking in. A row is not sent
//   anywhere and is not computed anywhere. It is the order in which six bells
//   happened to strike, and it exists only in the ears of whoever was
//   listening.
//
// So the shapes below carry a BLOW (`Struck`), never a row; a CALL, never a
// composition; and a LINE for one place bell, never a method. The only place a
// whole row is ever assembled is in a listener, from what it heard.
//
// This header is the vocabulary and nothing else. The place notation that turns
// a method into lines lives in `method.hpp`, which ONLY the method libraries
// include -- so the host physically cannot work out a row, rather than merely
// being trusted not to.

#ifndef RINGING_CHAMBER_TOWER_HPP
#define RINGING_CHAMBER_TOWER_HPP

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace tower {

// ---------------------------------------------------------------------------
// The offices. A rope is a job, not a person: `bell.3` is whoever is on the
// third tonight, and next week it is somebody else.
// ---------------------------------------------------------------------------

inline constexpr const char* kOfficeConductor = "conductor";
inline constexpr const char* kOfficePricker = "pricker";
inline constexpr const char* kOfficeMethod = "method";

inline std::string rope(std::int64_t bell) { return "bell." + std::to_string(bell); }

// The four things a conductor says out loud. They are strings because they are
// words shouted across a room, and because a ringer who does not know one of
// them should ignore it rather than fail to compile.
inline constexpr const char* kBob = "Bob";
inline constexpr const char* kSingle = "Single";
inline constexpr const char* kThatsAll = "That's all";
inline constexpr const char* kStand = "Stand";

// ---------------------------------------------------------------------------
// What is so.
//
// A bell is either up or it is down, and that is a standing fact about the
// bell rather than something that happens -- so it is a claim and not a
// message. You cannot ring changes on a bell that is down, and from where the
// conductor is standing an unclaimed rope and an empty one look the same until
// somebody speaks.
// ---------------------------------------------------------------------------

struct BellUp {
    std::int64_t bell = 0;
    bool up = false;
    ZEN_SHAPE(BellUp, 1, ZEN_FIELD(bell), ZEN_FIELD(up));
};

// ---------------------------------------------------------------------------
// Learning the method.
//
// You learn a method before you ring it, out of a book, and then you ring it
// from memory. The method is asked exactly one kind of question and it answers
// for ONE PLACE BELL AT A TIME -- it is never asked, and can never be asked,
// what the row is.
// ---------------------------------------------------------------------------

struct LearnTheMethod {
    std::int64_t bells_in_tower = 0; ///< how many ropes there are; ask about each
    ZEN_SHAPE(LearnTheMethod, 1, ZEN_FIELD(bells_in_tower));
};

struct WhatIsMyLine {
    std::int64_t place_bell = 0;
    ZEN_SHAPE(WhatIsMyLine, 1, ZEN_FIELD(place_bell));
};

/// One place bell's path through one lead, and what each call does to it at the
/// lead end. `covering` is the honest answer for a bell the method does not
/// use -- the tenor behind Doubles -- and it is not a refusal: it is a job.
struct YourLine {
    std::string method;
    std::int64_t bells = 0;       ///< how many bells the method changes
    std::int64_t lead = 0;        ///< changes in a lead
    std::int64_t place_bell = 0;  ///< which place bell this line is for
    bool covering = false;        ///< no line: ring last, every row
    std::vector<std::int64_t> path; ///< positions for rows 1 .. lead-1
    std::int64_t plain_end = 0;   ///< position at the lead-end row, plain
    std::int64_t bob_end = 0;     ///< ... at a bob
    std::int64_t single_end = 0;  ///< ... at a single
    ZEN_SHAPE(YourLine, 1, ZEN_FIELD(method), ZEN_FIELD(bells), ZEN_FIELD(lead),
              ZEN_FIELD(place_bell), ZEN_FIELD(covering), ZEN_FIELD(path),
              ZEN_FIELD(plain_end), ZEN_FIELD(bob_end), ZEN_FIELD(single_end));
};

// ---------------------------------------------------------------------------
// The ringing.
// ---------------------------------------------------------------------------

struct RingUp {
    std::int64_t bell = 0;
    ZEN_SHAPE(RingUp, 1, ZEN_FIELD(bell));
};

struct RingDown {
    std::int64_t bell = 0;
    ZEN_SHAPE(RingDown, 1, ZEN_FIELD(bell));
};

/// One blow. The host says when the band pulls; each ringer decides where.
struct Pull {
    std::int64_t row = 0;
    ZEN_SHAPE(Pull, 1, ZEN_FIELD(row));
};

/// A BELL SOUNDING. Published, because that is what a bell is: it is heard by
/// everybody in the chamber and it is addressed to nobody. Authored as the
/// rope's office, because a bell's voice is its own and a hand slapped against
/// the wall is not the third.
struct Struck {
    std::int64_t bell = 0;
    std::int64_t place = 0;
    std::int64_t row = 0;
    ZEN_SHAPE(Struck, 1, ZEN_FIELD(bell), ZEN_FIELD(place), ZEN_FIELD(row));
};

/// A word from the conductor. Published for the same reason a bell is, and
/// authored as the office for a stronger one: a voice from the stairs saying
/// "Bob" is not a call, and the band must be able to tell without looking up.
struct Call {
    std::string what;
    std::int64_t row = 0; ///< the row it was called before; diagnostics only
    ZEN_SHAPE(Call, 1, ZEN_FIELD(what), ZEN_FIELD(row));
};

// ---------------------------------------------------------------------------
// Setting up. The composition is the conductor's business; the paper is the
// pricker's; neither is ever shown to the other.
// ---------------------------------------------------------------------------

struct TheComposition {
    std::string name;
    std::int64_t band = 0;              ///< how many are ringing
    std::vector<std::string> calls;     ///< one per lead: "" plain, "Bob", "Single"
    ZEN_SHAPE(TheComposition, 1, ZEN_FIELD(name), ZEN_FIELD(band), ZEN_FIELD(calls));
};

struct LookRound {
    std::int64_t at = 0;
    ZEN_SHAPE(LookRound, 1, ZEN_FIELD(at));
};

struct TakeUpYourPen {
    std::int64_t bells = 0;
    std::string what;
    ZEN_SHAPE(TakeUpYourPen, 1, ZEN_FIELD(bells), ZEN_FIELD(what));
};

// ---------------------------------------------------------------------------
// Declared state.
// ---------------------------------------------------------------------------

struct RingerState {
    std::int64_t bell = 0;
    std::int64_t place_bell = 0;
    std::int64_t row_in_lead = 1;
    std::int64_t blows = 0;
    std::int64_t refused = 0;      ///< blows not struck: bell down, or no line
    std::int64_t unreachable = 0;  ///< "I cannot get there from here"
    std::int64_t not_the_conductor = 0;
    std::int64_t not_from_the_method = 0;
    bool up = false;
    bool covering = false;
    bool learned = false;
    ZEN_SHAPE(RingerState, 1, ZEN_FIELD(bell), ZEN_FIELD(place_bell), ZEN_FIELD(row_in_lead),
              ZEN_FIELD(blows), ZEN_FIELD(refused), ZEN_FIELD(unreachable),
              ZEN_FIELD(not_the_conductor), ZEN_FIELD(not_from_the_method), ZEN_FIELD(up),
              ZEN_FIELD(covering), ZEN_FIELD(learned));
};

struct ConductorState {
    std::int64_t rows_heard = 0;
    std::int64_t calls_made = 0;
    std::int64_t strikes_ignored = 0;
    bool going = false;
    bool came_round = false;
    bool stood = false;
    ZEN_SHAPE(ConductorState, 1, ZEN_FIELD(rows_heard), ZEN_FIELD(calls_made),
              ZEN_FIELD(strikes_ignored), ZEN_FIELD(going), ZEN_FIELD(came_round),
              ZEN_FIELD(stood));
};

struct PrickerState {
    std::int64_t rows = 0;
    std::int64_t strikes = 0;
    std::int64_t clashes = 0;
    std::int64_t short_rows = 0;
    std::int64_t ignored = 0;   ///< strikes that were not a bell
    std::int64_t repeats = 0;
    std::int64_t covered = 0;   ///< rows whose last place was the heaviest bell
    ZEN_SHAPE(PrickerState, 1, ZEN_FIELD(rows), ZEN_FIELD(strikes), ZEN_FIELD(clashes),
              ZEN_FIELD(short_rows), ZEN_FIELD(ignored), ZEN_FIELD(repeats),
              ZEN_FIELD(covered));
};

struct MethodState {
    std::string name;
    std::int64_t bells = 0;
    std::int64_t lead = 0;
    std::int64_t lines_given = 0;
    ZEN_SHAPE(MethodState, 1, ZEN_FIELD(name), ZEN_FIELD(bells), ZEN_FIELD(lead),
              ZEN_FIELD(lines_given));
};

} // namespace tower

#endif // RINGING_CHAMBER_TOWER_HPP
