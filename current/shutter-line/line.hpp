// shutter-line — the vocabulary of the Admiralty's shutter telegraph.
//
// Everything that travels between London and Portsmouth is a SHUTTER CODE: a
// number between 0 and 63, because the frame on each hilltop carries six
// shutters and each one is either open or shut. Nobody on a hill knows what a
// code means. That is not a simplification — it is the point of the machine.
//
// This header is shared by the host and by the codebook artifacts, and by
// nothing else. The code -> word tables live ONLY inside the codebook
// libraries (see codebook.hpp), so there is no table anywhere in the tower's
// own translation unit that could turn a 23 into SAIL.

#ifndef SHUTTER_LINE_LINE_HPP
#define SHUTTER_LINE_LINE_HPP

#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace line {

// ---------------------------------------------------------------------------
// The codes.
//
// Six shutters give sixty-four settings. Eight of them are the line's own
// signals and are the same in every edition of the vocabulary, because a line
// that could not say "repeat" until both ends agreed which book they held
// would be no use at all. Eight more announce which vocabulary is in force.
// The rest are the vocabulary, and they are the part that changes.
// ---------------------------------------------------------------------------

inline constexpr std::int64_t kAtRest = 0;      ///< all six shutters shut
inline constexpr std::int64_t kAttention = 1;   ///< a message begins
inline constexpr std::int64_t kRepeat = 2;      ///< I could not agree it; send it again
inline constexpr std::int64_t kCorrect = 3;     ///< the repeat agreed; act on it
inline constexpr std::int64_t kEnds = 4;        ///< the message is complete
inline constexpr std::int64_t kCannotRead = 5;  ///< the frame above me is not legible
inline constexpr std::int64_t kCloseDown = 6;   ///< the line closes for the night
inline constexpr std::int64_t kNoRepeat = 7;    ///< urgent: do not wait for the check

/// Code 8+n says "vocabulary n is in force". A message that carries one cannot
/// be decoded by an office holding a different book; a message that carries
/// none will be decoded by whatever book is on the desk.
inline constexpr std::int64_t kVocabularyBase = 8;
inline constexpr std::int64_t kFirstWord = 16;

inline bool is_line_signal(std::int64_t code) { return code >= kAttention && code < kVocabularyBase; }
inline bool is_vocabulary_signal(std::int64_t code) {
    return code >= kVocabularyBase && code < kFirstWord;
}
inline bool is_word(std::int64_t code) { return code >= kFirstWord && code < 64; }

/// How a code is written in a journal. Six shutters, open (#) or shut (.),
/// most significant first — which is how a man at a telescope would read it.
inline std::string shutters(std::int64_t code) {
    std::string s(6, '.');
    for (int bit = 0; bit < 6; ++bit) {
        if ((code >> (5 - bit)) & 1) {
            s[static_cast<std::size_t>(bit)] = '#';
        }
    }
    return s;
}

/// THE SHUTTER THAT GOES WRONG IN BAD LIGHT. A frame read through drizzle at
/// four miles loses one shutter before it loses the rest, and on this line it
/// is always the same one — the second from the bottom, which sits behind the
/// ridge. Flipping it is what a misread IS here: not noise, one shutter.
inline constexpr std::int64_t kFragileShutter = 2;

// ---------------------------------------------------------------------------
// What travels.
// ---------------------------------------------------------------------------

/// One setting of the frame, shown to a neighbouring hill. A station may show
/// its frame to the two stations it can see and to nobody else; that is a
/// grant, not a convention (see the grants in telegraph.cpp).
struct Frame {
    std::int64_t code = 0;
    std::int64_t message = 0;  ///< which message this hoist belongs to
    std::int64_t position = 0; ///< its place in that message, 1-based
    bool down = true;          ///< true: London to Portsmouth
    ZEN_SHAPE(Frame, 1, ZEN_FIELD(code), ZEN_FIELD(message), ZEN_FIELD(position),
              ZEN_FIELD(down));
};

/// The clock on the wall. Every station shows what is in its hand on the
/// minute; the whole line is a wave that moves one hill per minute.
struct Minute {
    std::int64_t at = 0;
    ZEN_SHAPE(Minute, 1, ZEN_FIELD(at));
};

/// The weather on one hill, as the day gives it. Only the host says this; a
/// station cannot choose whether it can see.
struct Weather {
    bool up_clear = true;   ///< I can see the station towards London
    bool down_clear = true; ///< I can see the station towards Portsmouth
    bool hazy = false;      ///< I can see, but not well enough to be sure
    ZEN_SHAPE(Weather, 1, ZEN_FIELD(up_clear), ZEN_FIELD(down_clear), ZEN_FIELD(hazy));
};

/// What a station claims about what it can see. This is the one thing on this
/// line that is true in the present tense rather than being something that
/// happened, so it is a claim and not a message: an office that wants to know
/// whether the line is working reads the hills, and reads them without asking
/// anybody anything.
struct Visibility {
    bool up_clear = true;
    bool down_clear = true;
    ZEN_SHAPE(Visibility, 1, ZEN_FIELD(up_clear), ZEN_FIELD(down_clear));
};

/// The clerk coming in and looking at the title page of the book on the desk.
/// An office cannot tell which edition it holds by holding it, which is the
/// entire trouble; it has to look, and it has to look again after the book is
/// changed.
struct OpenTheBook {
    ZEN_SHAPE(OpenTheBook, 1);
};

/// The host handing a signal officer a message to send.
struct SendThis {
    std::int64_t message = 0;
    std::vector<std::string> words{};
    std::string addressee{};
    bool repeat_required = true;    ///< false is the admiral in a hurry
    bool announce_vocabulary = true; ///< false is the old form, before the signal existed
    ZEN_SHAPE(SendThis, 1, ZEN_FIELD(message), ZEN_FIELD(words), ZEN_FIELD(addressee),
              ZEN_FIELD(repeat_required), ZEN_FIELD(announce_vocabulary));
};

// ---------------------------------------------------------------------------
// The codebook's three questions. A book answers; it never speaks first.
// ---------------------------------------------------------------------------

struct WhichBook {
    ZEN_SHAPE(WhichBook, 1);
};
struct ThisBook {
    std::int64_t edition = 0;
    std::string title{};
    ZEN_SHAPE(ThisBook, 1, ZEN_FIELD(edition), ZEN_FIELD(title));
};

struct Coding {
    std::string word{};
    ZEN_SHAPE(Coding, 1, ZEN_FIELD(word));
};
struct Coded {
    std::string word{};
    std::int64_t code = 0;
    std::int64_t edition = 0;
    bool known = false;
    ZEN_SHAPE(Coded, 1, ZEN_FIELD(word), ZEN_FIELD(code), ZEN_FIELD(edition), ZEN_FIELD(known));
};

struct Decoding {
    std::int64_t code = 0;
    ZEN_SHAPE(Decoding, 1, ZEN_FIELD(code));
};
struct Decoded {
    std::int64_t code = 0;
    std::string word{};
    std::int64_t edition = 0;
    bool known = false;
    ZEN_SHAPE(Decoded, 1, ZEN_FIELD(code), ZEN_FIELD(word), ZEN_FIELD(edition), ZEN_FIELD(known));
};

// ---------------------------------------------------------------------------
// What each participant can be asked to show of itself afterwards. These are
// the states the host reads back through the ordinary gate at the end of the
// day; they are the only account of the day that is not the host's own.
// ---------------------------------------------------------------------------

/// One line of a station's journal: what it read, and when.
struct JournalLine {
    std::int64_t minute = 0;
    std::int64_t message = 0;
    std::int64_t position = 0;
    std::int64_t code = 0;
    bool down = true;
    ZEN_SHAPE(JournalLine, 1, ZEN_FIELD(minute), ZEN_FIELD(message), ZEN_FIELD(position),
              ZEN_FIELD(code), ZEN_FIELD(down));
};

struct StationState {
    std::string name{};
    std::vector<JournalLine> journal{};
    std::int64_t could_not_read = 0;
    std::int64_t not_my_neighbour = 0;
    std::int64_t hands_clashed = 0;
    ZEN_SHAPE(StationState, 1, ZEN_FIELD(name), ZEN_FIELD(journal), ZEN_FIELD(could_not_read),
              ZEN_FIELD(not_my_neighbour), ZEN_FIELD(hands_clashed));
};

/// The Admiralty's file copy of one message.
struct Filed {
    std::int64_t message = 0;
    std::vector<std::string> words{};
    std::vector<std::int64_t> hoists{};   ///< what went on the line
    std::vector<std::int64_t> repeated{}; ///< what came back
    bool repeat_agreed = false;
    bool sent = false;
    std::string refused{};
    ZEN_SHAPE(Filed, 1, ZEN_FIELD(message), ZEN_FIELD(words), ZEN_FIELD(hoists),
              ZEN_FIELD(repeated), ZEN_FIELD(repeat_agreed), ZEN_FIELD(sent),
              ZEN_FIELD(refused));
};

struct AdmiraltyState {
    std::int64_t edition = 0;
    std::vector<Filed> file{};
    std::int64_t repeats_called_for = 0;
    ZEN_SHAPE(AdmiraltyState, 1, ZEN_FIELD(edition), ZEN_FIELD(file),
              ZEN_FIELD(repeats_called_for));
};

/// One entry in the receiving office's message book.
struct BookEntry {
    std::int64_t message = 0;
    std::vector<std::int64_t> hoists{};
    std::vector<std::string> words{};
    std::int64_t decoded_with = 0; ///< the edition of the book that was open
    bool delivered = false;
    std::string refused{};
    ZEN_SHAPE(BookEntry, 1, ZEN_FIELD(message), ZEN_FIELD(hoists), ZEN_FIELD(words),
              ZEN_FIELD(decoded_with), ZEN_FIELD(delivered), ZEN_FIELD(refused));
};

struct PortsmouthState {
    std::int64_t edition = 0;
    std::vector<BookEntry> book{};
    std::int64_t not_my_neighbour = 0;
    ZEN_SHAPE(PortsmouthState, 1, ZEN_FIELD(edition), ZEN_FIELD(book),
              ZEN_FIELD(not_my_neighbour));
};

/// A codebook's own account of itself.
struct BookState {
    std::int64_t edition = 0;
    std::string title{};
    std::int64_t questions_answered = 0;
    ZEN_SHAPE(BookState, 1, ZEN_FIELD(edition), ZEN_FIELD(title),
              ZEN_FIELD(questions_answered));
};

} // namespace line

#endif // SHUTTER_LINE_LINE_HPP
