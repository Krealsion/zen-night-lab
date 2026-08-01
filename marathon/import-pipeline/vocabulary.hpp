#ifndef MARATHON_IMPORT_VOCABULARY_HPP
#define MARATHON_IMPORT_VOCABULARY_HPP

// A media import pipeline — the whole contract in one file.
//
// THE ARCHITECTURAL QUESTION. The Timer package authored a conversation shape
// and the kitchen used it unchanged:
//
//     request  ->  available choices  ->  selected choice  ->  resolved choice
//                                                          ->  receipt
//
// with two rules attached: **refusal is an outcome and never a menu choice**,
// and **an unknown spelling is refused rather than guessed at**. Two sightings
// is a candidate; a third from a materially different domain is evidence. This
// package is that third, and it is built to find out whether the shape survives
// a domain where the menu is *derived from the input* rather than declared by
// the requester.
//
// WHAT IS DIFFERENT HERE, and it is what makes the sighting independent:
//
//   * THE REQUESTER CANNOT KNOW THE MENU IN ADVANCE. A diner knows the words
//     "grill" and "any_station" before ordering. Somebody importing a file does
//     not know what is in it; the OPTIONS ARE DISCOVERED BY THE SERVICE and the
//     conversation cannot be completed in one exchange even in principle.
//   * A MENU HAS AN IDENTITY. `ImportOptions::menu` is minted by the importer,
//     and a choice naming a menu that is no longer open is refused. One field
//     answers three hostile cases — the stale choice, the duplicate choice, and
//     the choice that arrives after a replacement.
//   * THE COUNTERPARTY IS A SPECIFIC WEAVE, not a role. That single fact makes
//     the second half of the standing consumer obligation PERFORMABLE here:
//     the importer offered a menu to a particular requester, and a choice
//     arrives with a bus-stamped sender it can compare. Three projects have now
//     failed to check that; this one can, and the difference is worth naming.
//
// ---- WHAT SURVIVES A REPLACEMENT, AND THE THIRD DISTINCT ANSWER -------------
//
// The kitchen carried its WORK across (progress was three integers). The
// download manager could carry nothing and carried the OBLIGATION TO FAIL. This
// package can do neither and does a third thing:
//
//     A MENU BELONGS TO THE LIFE THAT OFFERED IT.
//
// The successor re-derives the options from the file — they are a function of
// the input, so it can — mints a NEW menu identity, and RE-OFFERS. The
// conversation is neither continued nor ended: it is REOPENED. A choice naming
// the dead menu is refused with the current menu named in the refusal, so a
// requester that raced the replacement is never left guessing.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace marathon::importer {

/// The one service role.
inline constexpr const char* kImporterRole = "import.pipeline";

// ---- what a requester says --------------------------------------------------

/// "Import this." `ticket` is the REQUESTER's own name for the operation.
///
/// The importer answers this exactly once, and the answer is THE MENU — not a
/// receipt, and not an acknowledgement. There is nothing to acknowledge yet: the
/// service has not agreed to import anything, because it does not yet know what
/// the requester wants it to be.
struct ImportAsset {
    std::string ticket;
    std::string file;
    ZEN_SHAPE(ImportAsset, 1, ZEN_FIELD(ticket), ZEN_FIELD(file));
};

/// "That one." `menu` is the identity of the menu being answered — not a secret,
/// and not authority: it is how the importer knows WHICH offer this replies to.
struct ChooseOption {
    std::string ticket;
    std::string menu;
    std::string choice;
    ZEN_SHAPE(ChooseOption, 1, ZEN_FIELD(ticket), ZEN_FIELD(menu), ZEN_FIELD(choice));
};

/// "Never mind." A requester may leave the conversation; the importer then owes
/// it nothing more.
struct AbandonImport {
    std::string ticket;
    ZEN_SHAPE(AbandonImport, 1, ZEN_FIELD(ticket));
};

// ---- what the importer says -------------------------------------------------

/// One interpretation the file admits. `label` is what a requester names in a
/// choice; the rest is what the requester needs in order to choose.
struct Interpretation {
    std::string label;
    std::string codec;
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::int64_t bytes = 0;
    ZEN_SHAPE(Interpretation, 1, ZEN_FIELD(label), ZEN_FIELD(codec), ZEN_FIELD(width),
              ZEN_FIELD(height), ZEN_FIELD(bytes));
};

/// THE MENU. Sent as the authenticated answer to `ImportAsset` the first time,
/// and as an ORDINARY directed message when a successor re-offers after a
/// replacement — the answer right died with the incarnation that earned it.
/// A requester can tell the two apart with `answers_ask()`, and this package
/// measures that it does.
///
/// REFUSAL IS NOT IN HERE. A menu is the set of things that CAN be done; "no"
/// is what happens when none of them can, and it arrives as `ImportRefused`.
/// Putting refusal in the menu would make declining look like a choice the
/// service was offering, which is exactly backwards.
struct ImportOptions {
    std::string ticket;
    std::string menu; ///< this offer's identity, minted by the importer
    std::vector<Interpretation> options;
    ZEN_SHAPE(ImportOptions, 1, ZEN_FIELD(ticket), ZEN_FIELD(menu), ZEN_FIELD(options));
};

/// "There is nothing I can offer, and here is why." An OUTCOME.
struct ImportRefused {
    std::string ticket;
    std::string reason;
    ZEN_SHAPE(ImportRefused, 1, ZEN_FIELD(ticket), ZEN_FIELD(reason));
};

/// THE RESOLVED CHOICE, and it is a distinct step rather than a formality.
///
/// A requester may name something UNDERSPECIFIED — "h264" when the file admits
/// two h264 interpretations. The importer resolves it to exactly one and says
/// which, and why. That is the difference between a service that guesses and a
/// service that decides in the open: the requester finds out what it is going to
/// get BEFORE the work starts, and can abandon if it disagrees.
struct ChoiceResolved {
    std::string ticket;
    std::string menu;
    std::string chose;       ///< what the requester said
    std::string resolved_to; ///< the exact interpretation label
    std::string why;
    ZEN_SHAPE(ChoiceResolved, 1, ZEN_FIELD(ticket), ZEN_FIELD(menu), ZEN_FIELD(chose),
              ZEN_FIELD(resolved_to), ZEN_FIELD(why));
};

/// "That choice is not one I can act on." The authenticated answer to a
/// `ChooseOption` the importer will not take.
struct ChoiceRefused {
    std::string ticket;
    std::string menu;
    std::string reason;
    ZEN_SHAPE(ChoiceRefused, 1, ZEN_FIELD(ticket), ZEN_FIELD(menu), ZEN_FIELD(reason));
};

/// The work is done. An ORDINARY directed message: the two answer rights this
/// conversation earned were spent on the menu and on the resolution, and there
/// is not a third.
struct ImportReceipt {
    std::string ticket;
    std::string asset;
    std::string interpretation;
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::int64_t bytes = 0;
    ZEN_SHAPE(ImportReceipt, 1, ZEN_FIELD(ticket), ZEN_FIELD(asset),
              ZEN_FIELD(interpretation), ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(bytes));
};

/// The work could not be finished after all.
struct ImportFailed {
    std::string ticket;
    std::string reason;
    ZEN_SHAPE(ImportFailed, 1, ZEN_FIELD(ticket), ZEN_FIELD(reason));
};

// ---- diagnostics ------------------------------------------------------------

struct ImporterStatus {
    ZEN_SHAPE(ImporterStatus, 1);
};

// ---- the replacement conversation -------------------------------------------

/// One conversation in flight, described in words a successor can act on.
///
/// NOTE WHAT IS ABSENT: the menu identity. It belonged to the life that offered
/// it. A successor handed a menu id would be claiming to have made an offer it
/// never made, and a requester's stale choice would then be silently accepted
/// against a different set of options.
struct PendingImport {
    std::string ticket;
    std::string requester;        ///< canonical decimal of the requester's WeaveId
    std::int64_t correlation = 0; ///< the requester's own number
    std::string file;
    std::string resolved_to;      ///< non-empty once the choice was resolved
    ZEN_SHAPE(PendingImport, 1, ZEN_FIELD(ticket), ZEN_FIELD(requester), ZEN_FIELD(correlation),
              ZEN_FIELD(file), ZEN_FIELD(resolved_to));
};

struct DescribeConversations {
    ZEN_SHAPE(DescribeConversations, 1);
};

struct ConversationsDescribed {
    std::vector<PendingImport> open;
    /// WHERE THE MENU NUMBERING GOT TO, and a test found out why this is here.
    ///
    /// A menu identity is minted from a per-incarnation counter. A successor
    /// starts that counter at 1 — so its FIRST menu is called `m1`, which is
    /// exactly the name a requester may still be holding from the predecessor.
    /// The stale-choice check then passes on a NAME COLLISION and a choice is
    /// acted on against a different set of options.
    ///
    /// The identity is per-incarnation; the NAMESPACE must not be. So the number
    /// crosses — it is a word, and words cross.
    std::int64_t next_menu = 1;
    ZEN_SHAPE(ConversationsDescribed, 1, ZEN_FIELD(open), ZEN_FIELD(next_menu));
};

/// THE PREPARATION ASK.
struct PrepareImporter {
    std::vector<PendingImport> adopt;
    bool verify_files = false; ///< make the candidate check it can read every file first
    std::int64_t next_menu = 1; ///< see ConversationsDescribed::next_menu
    ZEN_SHAPE(PrepareImporter, 1, ZEN_FIELD(adopt), ZEN_FIELD(verify_files),
              ZEN_FIELD(next_menu));
};

/// The candidate's own question, from inside the seal.
struct AskCatalogueName {
    ZEN_SHAPE(AskCatalogueName, 1);
};

struct CatalogueNameIs {
    std::string name;
    ZEN_SHAPE(CatalogueNameIs, 1, ZEN_FIELD(name));
};

struct ImporterReady {
    std::int64_t adopted = 0;
    ZEN_SHAPE(ImporterReady, 1, ZEN_FIELD(adopted));
};

struct ImporterNotReady {
    std::string reason;
    ZEN_SHAPE(ImporterNotReady, 1, ZEN_FIELD(reason));
};

// ---- the published bounds ---------------------------------------------------

inline constexpr std::size_t kMaxOpenConversations = 12;
inline constexpr std::size_t kMaxAdoptedConversations = kMaxOpenConversations;

/// How many beats the work takes once a choice is resolved. Comfortably longer
/// than a round trip, so that "resolved but not finished" is a state a test can
/// actually stand in — the first draft used three and every case that wanted to
/// observe that window kept racing past it.
inline constexpr std::int64_t kWorkBeats = 8;

inline constexpr const char* kTickTimerId = "import.tick";
inline constexpr std::int64_t kTickMs = 20;

// ---- the catalogue ----------------------------------------------------------
//
// Deterministic. The options a file admits are a FUNCTION OF THE FILE, which is
// exactly why a successor can re-derive them — and exactly why the menu's
// IDENTITY still cannot cross, since an identity is a promise about a
// conversation and not a fact about a file.

/// The catalogue's own name, so two importer artifacts can honestly disagree
/// about what they know how to read.
#ifndef MARATHON_IMPORT_CATALOGUE_NAME
#define MARATHON_IMPORT_CATALOGUE_NAME "house"
#endif

struct FileKind {
    const char* file;
    /// Interpretations, in the order the importer offers them. The FIRST is the
    /// one an underspecified choice resolves to — "best available", stated on
    /// the wire rather than assumed.
    const Interpretation* options;
    std::size_t count;
};

inline const Interpretation kMovieOptions[] = {
    {"h264-1080", "h264", 1920, 1080, 41000},
    {"h264-720", "h264", 1280, 720, 18000},
    {"prores-1080", "prores", 1920, 1080, 260000},
};
inline const Interpretation kScanOptions[] = {
    {"rgb-600", "tiff", 5100, 6600, 96000},
    {"gray-600", "tiff", 5100, 6600, 32000},
};
inline const Interpretation kNoteOptions[] = {
    {"utf8", "text", 0, 0, 900},
};

inline const FileKind kCatalogue[] = {
    {"holiday.mov", kMovieOptions, 3},
    {"scan.tif", kScanOptions, 2},
    {"note.txt", kNoteOptions, 1},
    {"corrupt.bin", nullptr, 0}, ///< readable, and admits nothing
};

inline constexpr std::size_t kCatalogueCount = sizeof(kCatalogue) / sizeof(kCatalogue[0]);

inline const FileKind* find_file(const std::string& name) {
    for (const FileKind& k : kCatalogue) {
        if (name == k.file) {
            return &k;
        }
    }
    return nullptr;
}

} // namespace marathon::importer

#endif // MARATHON_IMPORT_VOCABULARY_HPP
