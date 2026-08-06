// The records committee's vocabulary.
//
// A county rarities committee assesses claims of birds that should not have
// been here. Five assessors read the same submission and reach their own
// verdicts; the secretary applies the committee's published rules to whatever
// votes came back; the county recorder keeps the list.
//
// Three kinds of fact, and the domain keeps them apart because it has to:
//
//   THE SUBMISSION    what the observer says they saw          circulated
//   THE VOTE          what one assessor makes of it            an ANSWER, per ballot
//   THE LIST          what the county has ever accepted        DURABLE, decades old
//
// There is not one Sense here. A committee has nothing to say in the present
// tense: it has things it decided, on dates, by tallies, and a list that
// outlives every member who ever sat on it. What is *so* about a county's
// avifauna is not a latest observation — it is an accumulated judgement, and
// the only honest place for it is a file.
//
// The votes travel as answers, and that is not decoration. A vote is only a
// vote if it answers a ballot the secretary issued: anything else arriving in
// the same shape is a voice in the corridor. Loom decides that question for
// this application (`answers_ask`), and the ballot number the secretary minted
// says which seat and which record and which round the vote belongs to.

#ifndef RECORDS_COMMITTEE_HPP
#define RECORDS_COMMITTEE_HPP

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace committee {

// ---------------------------------------------------------------------------
// The committee.
// ---------------------------------------------------------------------------

inline constexpr const char* kCounty = "Marchfield";

// The offices. A SEAT, not a person: a seat is what the secretary circulates
// to, and it survives the member who currently occupies it. Five of them,
// because the rules below need an odd number and a quorum that a single
// vacancy cannot break.
inline constexpr int kSeats = 5;

inline std::string seat_office(int n) { return "seat." + std::to_string(n); }

inline constexpr const char* kOfficeSecretary = "secretary";
inline constexpr const char* kOfficeArchive = "archive";

// The published rules, in the committee's own words:
//
//   Round one    unanimous accept          -> ACCEPTED
//                unanimous reject          -> NOT ACCEPTED
//                anything else             -> recirculate, with the comments
//   Round two    four or more accepts      -> ACCEPTED
//                otherwise                 -> NOT ACCEPTED
//   Quorum       four votes. Below it the record is HELD OVER, undecided,
//                to the next sitting — never decided by whoever happened to
//                be at home.
//
// "NOT ACCEPTED" is not "the observer is lying". It is "not proven", which is
// the only verdict a committee is ever entitled to.
inline constexpr std::int64_t kQuorum = 4;
inline constexpr std::int64_t kAcceptsInRoundTwo = 4;

inline constexpr const char* kAccepted = "ACCEPTED";
inline constexpr const char* kNotAccepted = "NOT ACCEPTED";
inline constexpr const char* kHeldOver = "HELD OVER";

// ---------------------------------------------------------------------------
// The submission. This is the record form, and it is the whole of what an
// assessor gets: five people read exactly these fields and disagree.
// ---------------------------------------------------------------------------

struct Submission {
    std::string record_id;   // "1979-017"
    std::string species;     // "Little Bunting"
    std::string observer;    // "R. Pyke"
    std::string month;       // "Nov"
    bool photograph = false;
    bool sound = false;
    bool in_season = false;  // is the date plausible for this species here
    std::int64_t observers = 1;
    // What this bird can be confused with, and which of those the written
    // description actually eliminates. The gap between the two lists is where
    // most of the disagreement in this application lives, and that is true of
    // the real thing as well.
    std::vector<std::string> confusion;
    std::vector<std::string> ruled_out;
    // Submitted after the closing date for the sitting. Not a judgement about
    // the record — it simply cannot be circulated in time, so it waits.
    bool after_closing_date = false;
    ZEN_SHAPE(Submission, 1, ZEN_FIELD(record_id), ZEN_FIELD(species), ZEN_FIELD(observer),
              ZEN_FIELD(month), ZEN_FIELD(photograph), ZEN_FIELD(sound), ZEN_FIELD(in_season),
              ZEN_FIELD(observers), ZEN_FIELD(confusion), ZEN_FIELD(ruled_out),
              ZEN_FIELD(after_closing_date));
};

inline bool has_media(const Submission& s) { return s.photograph || s.sound; }

inline bool eliminated(const Submission& s, const std::string& species) {
    for (const std::string& r : s.ruled_out) {
        if (r == species) {
            return true;
        }
    }
    return false;
}

// Every confusion species the description explicitly eliminates. An empty
// confusion list makes this vacuously true, which is correct: a bird with
// nothing to confuse it with has nothing left to eliminate.
inline bool ruled_out_all(const Submission& s) {
    for (const std::string& c : s.confusion) {
        if (!eliminated(s, c)) {
            return false;
        }
    }
    return true;
}

inline bool ruled_out_any(const Submission& s) {
    for (const std::string& c : s.confusion) {
        if (eliminated(s, c)) {
            return true;
        }
    }
    return false;
}

// The first confusion species the description leaves standing, or empty.
// Assessors name it in their comments, and one assessor changes its mind when
// somebody else names one — which is the entire reason a second round exists.
inline std::string first_unaddressed(const Submission& s) {
    for (const std::string& c : s.confusion) {
        if (!eliminated(s, c)) {
            return c;
        }
    }
    return "";
}

// ---------------------------------------------------------------------------
// The circulation.
// ---------------------------------------------------------------------------

// One ballot: this record, to one seat, for this round. In round two it also
// carries what the dissenters said — which is the point of recirculating at
// all, and not a courtesy.
struct Circulate {
    Submission record;
    std::int64_t round = 1;
    std::vector<std::string> dissent;
    ZEN_SHAPE(Circulate, 1, ZEN_FIELD(record), ZEN_FIELD(round), ZEN_FIELD(dissent));
};

// The vote. It carries no seat name, because it does not need one: it travels
// as the answer to a ballot, and the ballot number is the secretary's own.
// A seat name in the payload would be a claim the payload cannot support.
struct Vote {
    bool accept = false;
    std::string comment;
    ZEN_SHAPE(Vote, 1, ZEN_FIELD(accept), ZEN_FIELD(comment));
};

// ---------------------------------------------------------------------------
// The recorder. Only the archive touches the durable list, and only one
// assessor ever asks it anything — which is why only that assessor is granted
// the reach to.
// ---------------------------------------------------------------------------

struct IsItOnTheList {
    std::string species;
    ZEN_SHAPE(IsItOnTheList, 1, ZEN_FIELD(species));
};

struct OnTheList {
    std::string species;
    bool listed = false;
    std::string first_record; // the record that established it, empty if unlisted
    ZEN_SHAPE(OnTheList, 1, ZEN_FIELD(species), ZEN_FIELD(listed), ZEN_FIELD(first_record));
};

// The minute. Spoken AS THE SECRETARY OFFICE, because the county list is not
// something anybody who can spell this shape may add to.
struct RecordDetermination {
    std::string record_id;
    std::string species;
    std::string observer;
    std::string decision; // ACCEPTED / NOT ACCEPTED / HELD OVER
    std::int64_t round = 0;
    std::int64_t accepts = 0;
    std::int64_t rejects = 0;
    std::string note; // why it was held over, when it was
    Submission record; // carried so a held-over record can be circulated next year
    ZEN_SHAPE(RecordDetermination, 1, ZEN_FIELD(record_id), ZEN_FIELD(species),
              ZEN_FIELD(observer), ZEN_FIELD(decision), ZEN_FIELD(round), ZEN_FIELD(accepts),
              ZEN_FIELD(rejects), ZEN_FIELD(note), ZEN_FIELD(record));
};

// What the recorder can say that nobody else can. "First county record" is not
// a property of the bird or of the submission — it is a property of the file,
// and it is the sentence this whole application exists to be able to write
// truthfully.
struct Minuted {
    std::string record_id;
    bool first_for_county = false;
    std::string resubmission_of;  // the earlier record by the same observer, or empty
    std::string previous_decision;
    ZEN_SHAPE(Minuted, 1, ZEN_FIELD(record_id), ZEN_FIELD(first_for_county),
              ZEN_FIELD(resubmission_of), ZEN_FIELD(previous_decision));
};

// ---------------------------------------------------------------------------
// The meeting. The host owns the calendar and the agenda; the secretary owns
// the circulation.
// ---------------------------------------------------------------------------

struct PutOnTheAgenda {
    Submission record;
    ZEN_SHAPE(PutOnTheAgenda, 1, ZEN_FIELD(record));
};

struct SendOutTheCirculation {
    std::int64_t round = 1;
    ZEN_SHAPE(SendOutTheCirculation, 1, ZEN_FIELD(round));
};

// THE CLOSING DATE. A circulation ends because the calendar says so, not
// because everybody replied — the secretary cannot be told that a ballot to an
// empty seat went nowhere, and a committee that waited for a member who has
// resigned would never meet again. Quorum is what makes that survivable.
struct CloseTheCirculation {
    std::int64_t round = 1;
    ZEN_SHAPE(CloseTheCirculation, 1, ZEN_FIELD(round));
};

struct Adjourn {
    std::int64_t year = 0;
    ZEN_SHAPE(Adjourn, 1, ZEN_FIELD(year));
};

// ---------------------------------------------------------------------------
// Declared state.
// ---------------------------------------------------------------------------

struct AssessorState {
    std::int64_t ballots = 0;
    std::int64_t accepts = 0;
    std::int64_t rejects = 0;
    ZEN_SHAPE(AssessorState, 1, ZEN_FIELD(ballots), ZEN_FIELD(accepts), ZEN_FIELD(rejects));
};

struct SecretaryState {
    std::int64_t on_the_agenda = 0;
    std::int64_t ballots_issued = 0;
    std::int64_t votes_counted = 0;
    std::int64_t unsolicited = 0;   // arrived in the shape of a vote, answering nothing
    std::int64_t late = 0;          // answered a ballot whose round had already closed
    std::int64_t determined = 0;
    std::int64_t held_over = 0;
    ZEN_SHAPE(SecretaryState, 1, ZEN_FIELD(on_the_agenda), ZEN_FIELD(ballots_issued),
              ZEN_FIELD(votes_counted), ZEN_FIELD(unsolicited), ZEN_FIELD(late),
              ZEN_FIELD(determined), ZEN_FIELD(held_over));
};

struct ArchiveState {
    std::int64_t species_on_the_list = 0;
    std::int64_t determinations = 0;
    std::int64_t minuted_this_sitting = 0;
    std::int64_t unauthored = 0;    // a determination nobody was entitled to make
    std::int64_t consulted = 0;     // an assessor asking what the county has had before
    ZEN_SHAPE(ArchiveState, 1, ZEN_FIELD(species_on_the_list), ZEN_FIELD(determinations),
              ZEN_FIELD(minuted_this_sitting), ZEN_FIELD(unauthored), ZEN_FIELD(consulted));
};

} // namespace committee

#endif // RECORDS_COMMITTEE_HPP
