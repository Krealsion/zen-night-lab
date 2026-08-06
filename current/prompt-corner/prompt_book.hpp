// The prompt book.
//
// This is a theatre, and the thing being modelled is the act of CALLING a show:
// a Deputy Stage Manager sits at the prompt corner with the book open, gives a
// standby to a department, waits to hear that they are standing by, and then
// says GO. Nothing on this stage moves because a clock said so. Everything moves
// because somebody with the book said the word, and somebody who had been warned
// heard it.
//
// So the two things that matter here are not "what is currently so" but
//
//     WHO SAID IT      a GO from anybody but the person calling the show is not
//                      a GO at all -- it is a voice in the dark
//     WERE THEY WARNED an operator does not take a cue they were not stood by
//                      for. On the fly floor that rule is why nobody dies.
//
// Both are causal, both are conversations, and neither is a latest-claim. There
// is not one Sense in this application, and that is not restraint -- a running
// show has nothing it wants to say in the present tense.
//
// The book below is the score. It is compiled into the caller, because a prompt
// book is not configuration: it IS the equipment, marked up in rehearsal, and
// two DSMs calling from two different editions of it is precisely the accident
// this application refuses to have.

#ifndef PROMPT_CORNER_PROMPT_BOOK_HPP
#define PROMPT_CORNER_PROMPT_BOOK_HPP

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace promptcorner {

// ---------------------------------------------------------------------------
// The offices. Everything in this theatre is addressed by the job, never by the
// person -- which is the whole reason the person can be changed mid-show.
// ---------------------------------------------------------------------------

inline constexpr const char* kOfficeCaller = "caller"; // the DSM at the prompt corner
inline constexpr const char* kOfficeCsm = "csm";       // the company stage manager
inline constexpr const char* kOfficeDeck = "deck";     // the stage crew
inline constexpr const char* kOfficeLx = "lx";         // the lighting board
inline constexpr const char* kOfficeSound = "sound";   // the sound desk
inline constexpr const char* kOfficeFlys = "flys";     // the fly floor

/// The edition of the prompt book this production is running from. A relief who
/// arrives holding a different edition is holding a different show.
inline constexpr const char* kEdition = "The Salt Harvest / opening night";

// ---------------------------------------------------------------------------
// The show, as it happens.
// ---------------------------------------------------------------------------

/// The performance moving forward one beat. The house owns this; nothing in the
/// theatre invents a clock, and no cue is attached to it -- the caller decides
/// what this beat means by looking at the book.
struct Moment {
    std::int64_t at = 0;
    ZEN_SHAPE(Moment, 1, ZEN_FIELD(at));
};

/// "Standby LX 5." An ask, because the caller genuinely needs the answer before
/// it may say the next word, and because a send whose fate the sender cannot see
/// is exactly what a standby must not be.
struct Standby {
    std::string cue;
    std::string effect;
    ZEN_SHAPE(Standby, 1, ZEN_FIELD(cue), ZEN_FIELD(effect));
};

/// "LX 5 standing by." The answer, and the caller checks that Loom says it is
/// the answer to its own ask rather than trusting the words.
struct StandingBy {
    std::string cue;
    ZEN_SHAPE(StandingBy, 1, ZEN_FIELD(cue));
};

/// "LX 5 ... GO." Authored as the office, always, because what makes this a cue
/// is that the person calling the show said it.
struct Go {
    std::string cue;
    std::string effect;
    std::int64_t at = 0;
    ZEN_SHAPE(Go, 1, ZEN_FIELD(cue), ZEN_FIELD(effect), ZEN_FIELD(at));
};

/// The fly floor cannot answer a standby out of its own head: a bar does not
/// move until somebody has looked at the deck under it. This is why fly standbys
/// go early.
struct CheckTheRail {
    std::string cue;
    ZEN_SHAPE(CheckTheRail, 1, ZEN_FIELD(cue));
};

struct RailClear {
    std::string cue;
    ZEN_SHAPE(RailClear, 1, ZEN_FIELD(cue));
};

// ---------------------------------------------------------------------------
// The handover. Somebody has to take the book, live, without the show stopping.
// ---------------------------------------------------------------------------

/// House -> company stage manager: relieve the caller now.
///
/// `edition` is what the CSM will write on the brief (empty means "whatever the
/// outgoing DSM says it is" -- the honest case). `carry_the_book` false is the
/// deliberately broken handover: a relief briefed with nothing.
struct BeginHandover {
    std::int64_t at = 0;
    std::string edition;
    bool carry_the_book = true;
    ZEN_SHAPE(BeginHandover, 1, ZEN_FIELD(at), ZEN_FIELD(edition), ZEN_FIELD(carry_the_book));
};

/// CSM -> the caller: the boundary. After this the outgoing DSM calls nothing.
/// Loom gives this message no standing whatsoever; what makes it a boundary is
/// that the caller's own handler stops calling in it and authors its final book
/// position there, where nothing further can change it.
struct StandDown {
    std::int64_t at = 0;
    ZEN_SHAPE(StandDown, 1, ZEN_FIELD(at));
};

/// The outgoing DSM's answer to StandDown: where we are, and what is in the air.
/// `exact` is true because it was authored after the caller stopped calling --
/// a book position read off a DSM who is still working is a guess.
struct TheBook {
    std::string edition;
    std::int64_t next_index = 0;
    std::vector<std::string> given; // standbys given whose GO has not been called
    bool exact = false;
    ZEN_SHAPE(TheBook, 1, ZEN_FIELD(edition), ZEN_FIELD(next_index), ZEN_FIELD(given),
              ZEN_FIELD(exact));
};

/// The brief. The one thing said to the relief before they take the book.
struct BriefTheRelief {
    std::string edition;
    std::int64_t next_index = 0;
    std::vector<std::string> given;
    ZEN_SHAPE(BriefTheRelief, 1, ZEN_FIELD(edition), ZEN_FIELD(next_index), ZEN_FIELD(given));
};

struct HaveTheBook {
    std::int64_t from_index = 0;
    ZEN_SHAPE(HaveTheBook, 1, ZEN_FIELD(from_index));
};

struct CannotTakeTheBook {
    std::string why;
    ZEN_SHAPE(CannotTakeTheBook, 1, ZEN_FIELD(why));
};

/// CSM -> the caller, when the relief could not take it: as you were.
struct CarryOn {
    std::int64_t at = 0;
    ZEN_SHAPE(CarryOn, 1, ZEN_FIELD(at));
};

// ---------------------------------------------------------------------------
// Declared state.
// ---------------------------------------------------------------------------

struct CallerState {
    std::string edition;
    std::int64_t index = 0;              // the next cue in the book that has not gone
    std::vector<std::string> given;      // standbys given, GO not yet called
    std::vector<std::string> confirmed;  // of those, the ones standing by
    bool calling = false;
    std::int64_t called = 0;
    std::int64_t standbys = 0;
    std::int64_t re_given = 0;    // standbys re-given on taking the book
    std::int64_t held = 0;        // beats where a cue was due and nobody was standing by
    std::int64_t after_boundary = 0; // beats refused because this DSM had stood down
    ZEN_SHAPE(CallerState, 1, ZEN_FIELD(edition), ZEN_FIELD(index), ZEN_FIELD(given),
              ZEN_FIELD(confirmed), ZEN_FIELD(calling), ZEN_FIELD(called), ZEN_FIELD(standbys),
              ZEN_FIELD(re_given), ZEN_FIELD(held), ZEN_FIELD(after_boundary));
};

struct DeptState {
    std::string dept;
    std::string live;                     // what this department currently has on stage
    std::vector<std::string> took;        // cues taken, in the order they were taken
    std::vector<std::string> standing;    // cues currently standing by
    std::int64_t warned = 0;              // standbys accepted, counting repeats
    std::int64_t queried = 0;             // "we've had that cue" -- refused as already taken
    std::int64_t no_standby = 0;          // GOs refused because nobody had been stood by
    std::int64_t unauthored = 0;          // GOs ignored: not from whoever is calling the show
    ZEN_SHAPE(DeptState, 1, ZEN_FIELD(dept), ZEN_FIELD(live), ZEN_FIELD(took),
              ZEN_FIELD(standing), ZEN_FIELD(warned), ZEN_FIELD(queried), ZEN_FIELD(no_standby),
              ZEN_FIELD(unauthored));
};

struct DeckState {
    std::vector<std::string> waiting;
    std::int64_t cleared = 0;
    ZEN_SHAPE(DeckState, 1, ZEN_FIELD(waiting), ZEN_FIELD(cleared));
};

struct CsmState {
    std::int64_t handovers = 0;
    std::int64_t briefed = 0;
    std::int64_t ready = 0;
    std::int64_t refused = 0;
    std::string last;
    ZEN_SHAPE(CsmState, 1, ZEN_FIELD(handovers), ZEN_FIELD(briefed), ZEN_FIELD(ready),
              ZEN_FIELD(refused), ZEN_FIELD(last));
};

// ---------------------------------------------------------------------------
// The book itself: one act of The Salt Harvest, marked up.
//
// `standby` and `go` are positions in the text, not times. The book is in GO
// order, which is what makes "the next cue" a single number the relief can be
// told -- and what makes calling them out of order a thing this application can
// detect rather than a thing it hopes does not happen.
// ---------------------------------------------------------------------------

struct Cue {
    const char* id;
    const char* dept;
    std::int64_t standby;
    std::int64_t go;
    const char* effect;
};

inline const std::vector<Cue>& the_book() {
    static const std::vector<Cue> book = {
        {"LX 1", kOfficeLx, 1, 2, "house to half"},
        {"LX 2", kOfficeLx, 2, 3, "houselights out"},
        {"SQ 1", kOfficeSound, 2, 4, "the sea, distant"},
        {"LX 3", kOfficeLx, 4, 6, "first light"},
        {"FLY 1", kOfficeFlys, 5, 8, "the gauze out"},
        {"LX 4", kOfficeLx, 8, 9, "full sun"},
        {"SQ 2", kOfficeSound, 9, 11, "gulls"},
        {"LX 5", kOfficeLx, 12, 16, "the salt house"},
        {"FLY 2", kOfficeFlys, 15, 18, "the wall in"},
        {"SQ 3", kOfficeSound, 18, 19, "the knock"},
        {"LX 6", kOfficeLx, 19, 20, "blackout"},
        {"LX 7", kOfficeLx, 21, 22, "curtain call"},
        {"SQ 4", kOfficeSound, 22, 23, "playout"},
        {"LX 8", kOfficeLx, 23, 24, "houselights up"},
    };
    return book;
}

inline constexpr std::int64_t kCurtainDown = 26; // beats in the act

} // namespace promptcorner

#endif // PROMPT_CORNER_PROMPT_BOOK_HPP
