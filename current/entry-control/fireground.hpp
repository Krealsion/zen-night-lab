// entry-control — the vocabulary of a fireground breathing-apparatus board.
//
// Everything in this file is something a person at the incident can say, hear,
// or read off a board. The one thing that decides whether the procedure worked
// is NOT in it:
//
//     how fast a wearer actually breathes
//         lives in that wearer's own .so, and is readable only afterwards,
//         out of their own account
//
// The board cannot inspect it and never could. An entry control officer plans
// on a NOMINAL rate and the person inside spends their OWN, and the whole
// apparatus of tallies, pressure checks and turn-around pressures exists
// because of the gap between those two numbers. That is not a simplification of
// entry control; it is what entry control IS.
//
// SCALE. Two entry control points, three crews of two, one emergency crew held
// by BA main control between them. At a larger incident there would be an
// emergency crew at each point and a great deal more paperwork. This is a small
// working fire.

#ifndef ENTRY_CONTROL_FIREGROUND_HPP
#define ENTRY_CONTROL_FIREGROUND_HPP

#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace fireground {

// ---------------------------------------------------------------------------
// The arithmetic on the board.
//
// It is the same arithmetic at both entry control points and for everybody, so
// it lives here rather than inside anybody. A wearer never performs it: a
// wearer looks at a gauge.
// ---------------------------------------------------------------------------

/// The pressure at which the low-pressure warning whistle actuates. Below this
/// you are on your reserve and you should already be out.
inline constexpr std::int64_t kSafetyMargin = 55;

/// What the board plans on, in bar per minute. A number printed on the board,
/// not measured from anybody. Every wearer's real rate differs from it, and
/// that difference is what the pressure checks are for.
inline constexpr std::int64_t kNominalRate = 8;

/// How long it takes to get from the working position back out to the entry
/// control point. Known from the briefing; the same for both ways in here.
inline constexpr std::int64_t kTripOut = 5;

/// Usable air: everything above the whistle.
inline std::int64_t usable(std::int64_t gauge) { return gauge - kSafetyMargin; }

/// THE TURN-AROUND PRESSURE — half your usable air, plus the margin. Reach this
/// and you come out, whatever you are in the middle of.
inline std::int64_t turn_around_bar(std::int64_t entry_gauge) {
    return kSafetyMargin + usable(entry_gauge) / 2;
}

/// THE TIME DUE OUT — the minute at which, at the nominal rate, the whistle
/// would actuate. It is the latest you may still be in the building, and it is
/// the number the board is judged on afterwards.
inline std::int64_t due_out_minute(std::int64_t entry_minute, std::int64_t entry_gauge) {
    return entry_minute + usable(entry_gauge) / kNominalRate;
}

/// When a wearer last seen at `bar` at minute `at`, having used `used` bar over
/// `elapsed` minutes, will reach `target`. The board's projection — the whole
/// reason a pressure check is worth asking for. Returns a minute, or a very
/// large number when nothing has been measured yet.
inline std::int64_t projected_minute_at(std::int64_t bar, std::int64_t at, std::int64_t used,
                                        std::int64_t elapsed, std::int64_t target) {
    const std::int64_t rate = (elapsed > 0 && used > 0) ? (used / elapsed) : kNominalRate;
    const std::int64_t r = rate > 0 ? rate : 1;
    if (bar <= target) {
        return at;
    }
    return at + (bar - target) / r;
}

/// The tally number that means "not a tally anybody holds".
inline constexpr const char* kNoTally = "";

// ---------------------------------------------------------------------------
// What the incident says to its own people.
// ---------------------------------------------------------------------------

/// "Report to entry control at the front with your set on." The incident tells
/// a wearer where their entry control point is; from then on that is the only
/// person they talk to.
struct ReportTo {
    std::string point{};  ///< the entry control point's office
    std::string crew{};
    ZEN_SHAPE(ReportTo, 1, ZEN_FIELD(point), ZEN_FIELD(crew));
};

/// The clock on the fireground. Delivered to everybody who has a reason to
/// count minutes, which on a fireground is everybody.
struct Minute {
    std::int64_t at = 0;
    ZEN_SHAPE(Minute, 1, ZEN_FIELD(at));
};

// ---------------------------------------------------------------------------
// The tally.
//
// A physical brass-and-plastic tag with a wearer's name on it. You cannot enter
// without handing it to the entry control officer, and you are not out until it
// is back in your hand. IT IS THE ONLY THING ON THE FIREGROUND THAT IS A
// PERSON. Paperwork can be wrong; a tally is somebody.
// ---------------------------------------------------------------------------

/// "Aish, Red crew, three hundred bar." Handing the tally over.
struct ReportForEntry {
    std::string tally{};
    std::string crew{};
    std::int64_t gauge_bar = 0;
    ZEN_SHAPE(ReportForEntry, 1, ZEN_FIELD(tally), ZEN_FIELD(crew), ZEN_FIELD(gauge_bar));
};

/// "I have your tally. Turn round at a hundred and seventy-seven; you are due
/// out at minute thirty-four." Written on the board and read back to the wearer,
/// which is also their permission to go in.
struct TallyTaken {
    std::string tally{};
    std::int64_t wear = 0;
    std::int64_t entry_minute = 0;
    std::int64_t turn_around_bar = 0;
    std::int64_t due_out_minute = 0;
    ZEN_SHAPE(TallyTaken, 1, ZEN_FIELD(tally), ZEN_FIELD(wear), ZEN_FIELD(entry_minute),
              ZEN_FIELD(turn_around_bar), ZEN_FIELD(due_out_minute));
};

/// The tally, back in the wearer's own hand. THE ONLY THING THAT MEANS OUT.
struct TallyReturned {
    std::string tally{};
    std::int64_t wear = 0;
    ZEN_SHAPE(TallyReturned, 1, ZEN_FIELD(tally), ZEN_FIELD(wear));
};

// ---------------------------------------------------------------------------
// The radio traffic between an entry control point and the crews on its
// channel. Each point has its own channel, which is why a wearer may address
// exactly one of them.
// ---------------------------------------------------------------------------

/// "Entry control to Red crew, radio check, pass your pressures." An ask.
struct PressureCheck {
    std::string tally{};
    std::int64_t wear = 0;
    ZEN_SHAPE(PressureCheck, 1, ZEN_FIELD(tally), ZEN_FIELD(wear));
};

/// The answer, off the gauge on the wearer's own chest.
///
/// IT CARRIES ITS OWN WEAR NUMBER. A firefighter who has been out, rested and
/// gone back in with a fresh cylinder is on their second wear, and a reading
/// that arrived late from the first one is not a reading about now. The board
/// asks "what is this about?" of every answer it gets, and the domain supplies
/// the answer for free.
struct Gauge {
    std::string tally{};
    std::int64_t wear = 0;
    std::int64_t bar = 0;
    ZEN_SHAPE(Gauge, 1, ZEN_FIELD(tally), ZEN_FIELD(wear), ZEN_FIELD(bar));
};

/// "Turn round and come out." Sent by the entry control officer when the board
/// says so — reached turn-around pressure, or the projection says you will
/// before you can get out.
struct Withdraw {
    std::string tally{};
    std::int64_t wear = 0;
    std::string why{};
    ZEN_SHAPE(Withdraw, 1, ZEN_FIELD(tally), ZEN_FIELD(wear), ZEN_FIELD(why));
};

/// "Entry control, Red crew, out." The radio report. IT IS NOT THE TALLY, and
/// this application exists partly to say so.
struct OutOfBuilding {
    std::string tally{};
    std::int64_t wear = 0;
    std::int64_t bar = 0;
    ZEN_SHAPE(OutOfBuilding, 1, ZEN_FIELD(tally), ZEN_FIELD(wear), ZEN_FIELD(bar));
};

// ---------------------------------------------------------------------------
// THE SENTENCE NOBODY BUT THE INCIDENT COMMANDER MAY SAY.
//
// The tactical withdrawal is the one order that abandons the task. A "get out"
// from an unidentified voice on the fireground is not an evacuation — it is a
// voice on the fireground — and a wearer who acted on every one of them could
// be walked out of a building by anybody who could work a radio.
//
// Every wearer here enforces that against everybody, including against the
// entry control officer who is looking after them and who would have every
// reason to want it.
// ---------------------------------------------------------------------------

struct Evacuate {
    std::string why{};
    ZEN_SHAPE(Evacuate, 1, ZEN_FIELD(why));
};

// ---------------------------------------------------------------------------
// What passes between the entry control points, BA main control and command.
// ---------------------------------------------------------------------------

/// "Commit Red crew to search the first floor." Command to an entry control
/// point. `emergency` is the committal of the emergency crew, which goes at
/// once and is booked in as it passes the board — which is exactly why it is
/// the dangerous moment.
struct Commit {
    std::string crew{};
    std::string point{};
    std::string task{};
    bool emergency = false;
    ZEN_SHAPE(Commit, 1, ZEN_FIELD(crew), ZEN_FIELD(point), ZEN_FIELD(task),
              ZEN_FIELD(emergency));
};

/// "Standing in front of you, with my hand out." A wearer presents themselves
/// at the entry control point to get their tally back.
///
/// THIS IS NOT THE SAME EVENT AS THE RADIO REPORT and the difference is the
/// whole of this application's primary false green. A radio report is a voice
/// saying it is out. This is a person, at the board, in front of the officer.
struct AtTheBoard {
    std::string tally{};
    std::int64_t wear = 0;
    ZEN_SHAPE(AtTheBoard, 1, ZEN_FIELD(tally), ZEN_FIELD(wear));
};

/// "Red crew, in you go." Said out loud, to the fireground, by the incident
/// commander and by nobody else — which is why every wearer checks who said it
/// before they move.
///
/// An EMERGENCY committal is different in one way that matters: it goes at
/// once, and the tallies are taken as the crew passes the board rather than
/// before. That is real, and it is exactly why an emergency committal is the
/// moment somebody gets into a building without being written down.
struct GoIn {
    std::string crew{};
    std::string task{};
    bool emergency = false;
    ZEN_SHAPE(GoIn, 1, ZEN_FIELD(crew), ZEN_FIELD(task), ZEN_FIELD(emergency));
};

/// The entry control point's answer to command. A refusal names its reason, and
/// there is exactly one reason in this application: there is no emergency crew.
struct Committal {
    std::string crew{};
    std::string point{};
    bool committed = false;
    std::string why{};
    ZEN_SHAPE(Committal, 1, ZEN_FIELD(crew), ZEN_FIELD(point), ZEN_FIELD(committed),
              ZEN_FIELD(why));
};

/// "Bravo to command: Blue crew have missed two checks." The entry control
/// officer cannot say what has gone wrong, only that nobody answered — which is
/// the honest report and the whole reason the procedure exists.
struct Overdue {
    std::string crew{};
    std::string point{};
    std::string tally{};
    std::int64_t checks_missed = 0;
    ZEN_SHAPE(Overdue, 1, ZEN_FIELD(crew), ZEN_FIELD(point), ZEN_FIELD(tally),
              ZEN_FIELD(checks_missed));
};

/// An entry control point telling BA main control what it has done. BA main
/// holds the incident's overall account and is the only participant that may
/// read a board.
struct PointReport {
    std::string point{};
    std::string crew{};
    std::string what{};
    ZEN_SHAPE(PointReport, 1, ZEN_FIELD(point), ZEN_FIELD(crew), ZEN_FIELD(what));
};

/// BA main control to command: the state of the emergency crew, said out loud
/// because command must know before it orders anything.
struct EmergencyCrewGone {
    std::string crew{};
    std::string point{};
    ZEN_SHAPE(EmergencyCrewGone, 1, ZEN_FIELD(crew), ZEN_FIELD(point));
};

// ---------------------------------------------------------------------------
// THE BOARDS. Two of them, and they are the reason this application has
// anything to say.
//
// A board is true in the present tense — it is what is committed RIGHT NOW, not
// something that happened — so it is a claim and not a message, and the entry
// control officer claims it as the office. Each carries its own point name,
// because a reading that is perfectly valid and about the other end of the
// building is a strictly harder thing to notice than no reading at all.
//
// THE TWO BOARDS ARE INDEPENDENT ACCOUNTS. Neither entry control officer can
// see the other's — they are at opposite ends of a burning building — and only
// BA main control reads both. If either could read the other, the incident
// would stop having two accounts and start having one.
// ---------------------------------------------------------------------------

/// One line on the board. Everything the entry control officer wrote down when
/// the tally was taken, plus the last thing the wearer said.
struct OnTheBoard {
    std::string tally{};
    std::string crew{};
    std::int64_t wear = 0;
    std::int64_t entry_bar = 0;
    std::int64_t entry_minute = 0;
    std::int64_t turn_around_bar = 0;
    std::int64_t due_out_minute = 0;
    std::int64_t last_gauge_bar = 0;
    std::int64_t last_gauge_minute = -1;
    ZEN_SHAPE(OnTheBoard, 1, ZEN_FIELD(tally), ZEN_FIELD(crew), ZEN_FIELD(wear),
              ZEN_FIELD(entry_bar), ZEN_FIELD(entry_minute), ZEN_FIELD(turn_around_bar),
              ZEN_FIELD(due_out_minute), ZEN_FIELD(last_gauge_bar),
              ZEN_FIELD(last_gauge_minute));
};

struct Board {
    std::string point{};       ///< WHICH BOARD THIS IS. Never inferred from the key.
    std::int64_t at_minute = 0;
    std::vector<OnTheBoard> committed{}; ///< entries still open
    std::int64_t tallies_held = 0;       ///< tallies physically in the officer's hand
    std::int64_t entries_opened = 0;
    std::int64_t entries_closed = 0;
    ZEN_SHAPE(Board, 1, ZEN_FIELD(point), ZEN_FIELD(at_minute), ZEN_FIELD(committed),
              ZEN_FIELD(tallies_held), ZEN_FIELD(entries_opened), ZEN_FIELD(entries_closed));
};

/// Is there an emergency crew, and where. Nobody may be committed without one,
/// so both entry control points read this before they commit anything — and
/// they may read this and nothing else.
struct EmergencyCrew {
    bool available = false;
    std::string crew{};
    std::string point{};
    ZEN_SHAPE(EmergencyCrew, 1, ZEN_FIELD(available), ZEN_FIELD(crew), ZEN_FIELD(point));
};

// ---------------------------------------------------------------------------
// What each participant can be asked to show of itself afterwards. Read back
// through the ordinary gate at the debrief, which is when a fire service finds
// out what actually happened.
// ---------------------------------------------------------------------------

/// One wear: one cylinder, one trip in, one trip out.
struct WearRecord {
    std::int64_t wear = 0;
    std::string point{};
    std::int64_t gauge_at_entry = 0;
    std::int64_t gauge_at_exit = 0;
    std::int64_t entered_minute = -1;
    std::int64_t out_minute = -1;
    bool booked_in = false;      ///< somebody took my tally and wrote me down
    bool tally_back = false;     ///< and gave it back
    bool went_in = false;        ///< I actually entered the building
    bool on_the_whistle = false; ///< the low-pressure warning actuated while I was in
    ZEN_SHAPE(WearRecord, 1, ZEN_FIELD(wear), ZEN_FIELD(point), ZEN_FIELD(gauge_at_entry),
              ZEN_FIELD(gauge_at_exit), ZEN_FIELD(entered_minute), ZEN_FIELD(out_minute),
              ZEN_FIELD(booked_in), ZEN_FIELD(tally_back), ZEN_FIELD(went_in),
              ZEN_FIELD(on_the_whistle));
};

struct WearerState {
    std::string name{};
    std::string tally{};
    std::string crew{};
    /// MY OWN CONSUMPTION, in bar per minute. The board never sees this and
    /// never could; it is asked for at the debrief and nowhere else.
    std::int64_t rate = 0;
    std::int64_t gauge = 0; ///< where my needle is now
    std::vector<WearRecord> wears{};
    std::int64_t checks_answered = 0;
    std::int64_t checks_unanswered = 0;
    /// "Get out" from somebody who was not the incident commander.
    std::int64_t not_from_command = 0;
    ZEN_SHAPE(WearerState, 1, ZEN_FIELD(name), ZEN_FIELD(tally), ZEN_FIELD(crew),
              ZEN_FIELD(rate), ZEN_FIELD(gauge), ZEN_FIELD(wears), ZEN_FIELD(checks_answered),
              ZEN_FIELD(checks_unanswered), ZEN_FIELD(not_from_command));
};

struct PointState {
    std::string point{};
    std::int64_t tallies_taken = 0;
    std::int64_t tallies_returned = 0;
    std::int64_t entries_opened = 0;
    std::int64_t entries_closed = 0;
    std::int64_t checks_asked = 0;
    std::int64_t checks_answered = 0;
    std::int64_t committals_refused = 0;
    /// A pressure reading from a crew that is not on my board. Somebody on
    /// another point's channel reaching me. THIS COUNTER STAYS AT ZERO, and the
    /// reason it does is a grant rather than anything written here.
    std::int64_t not_on_my_board = 0;
    /// Times I tried to read the other point's board and was told I may not,
    /// and what I was told. The reason matters more than the count: "nobody has
    /// claimed anything" and "you may not read this" send an officer to
    /// opposite ends of the fireground.
    std::int64_t board_reads_refused = 0;
    std::string board_read_refusal{};
    std::vector<std::string> tallies_in_hand{};
    std::vector<OnTheBoard> still_committed{};
    ZEN_SHAPE(PointState, 1, ZEN_FIELD(point), ZEN_FIELD(tallies_taken),
              ZEN_FIELD(tallies_returned), ZEN_FIELD(entries_opened), ZEN_FIELD(entries_closed),
              ZEN_FIELD(checks_asked), ZEN_FIELD(checks_answered), ZEN_FIELD(committals_refused),
              ZEN_FIELD(not_on_my_board), ZEN_FIELD(board_reads_refused),
              ZEN_FIELD(board_read_refusal), ZEN_FIELD(tallies_in_hand),
              ZEN_FIELD(still_committed));
};

struct MainState {
    std::int64_t board_reads = 0;
    std::int64_t crews_committed = 0;
    std::int64_t emergency_crews_provided = 0;
    std::vector<std::string> log{};
    ZEN_SHAPE(MainState, 1, ZEN_FIELD(board_reads), ZEN_FIELD(crews_committed),
              ZEN_FIELD(emergency_crews_provided), ZEN_FIELD(log));
};

struct CommandState {
    std::int64_t committals_ordered = 0;
    std::int64_t committals_refused = 0;
    std::int64_t evacuations_ordered = 0;
    std::int64_t crews_overdue = 0;
    ZEN_SHAPE(CommandState, 1, ZEN_FIELD(committals_ordered), ZEN_FIELD(committals_refused),
              ZEN_FIELD(evacuations_ordered), ZEN_FIELD(crews_overdue));
};

} // namespace fireground

#endif // ENTRY_CONTROL_FIREGROUND_HPP
