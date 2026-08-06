// shutter-line — the Admiralty telegraph from Whitehall to Portsmouth.
//
// Six hilltops between two offices. A message is coded into shutter numbers at
// one end, crawls down the line one hill a minute, and is written out of the
// book at the other. NOBODY ON A HILL KNOWS WHAT THEY ARE SAYING: a station's
// whole job is to show, faithfully, what it just saw, and its whole failure is
// to show something else.
//
// Two things can therefore go wrong that both end with a perfectly good order
// arriving at Portsmouth:
//
//   A SHUTTER IS MISREAD      one number changes; the line's answer is the
//                             end-to-end repeat, which catches it
//   THE BOOKS DISAGREE        no number changes at all; the repeat agrees
//                             perfectly, and the words are different
//
// The tower owns the clock, the weather and the traffic, and it cannot decode
// anything: the code -> word tables live only in the two codebook libraries.

#include "line.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// The log. Everything the day says goes through here so the minute is always
// on the left.
// ---------------------------------------------------------------------------

std::int64_t g_minute = 0;

void say(const std::string& who, const std::string& what) {
    char stamp[16];
    std::snprintf(stamp, sizeof stamp, "  %4lld  ", static_cast<long long>(g_minute));
    std::cout << stamp;
    std::string w = who;
    w.resize(9, ' ');
    std::cout << w << what << "\n";
}

void note(const std::string& what) { std::cout << "  --    " << what << "\n"; }

// ---------------------------------------------------------------------------
// Binding a native weave to an office.
//
// `loom::mount<T>()` and `loom::mount_granted<T>()` do not take a role, and
// Switchboard::register_weave is the only binder — but it is the raw door, so
// it does not do the zen_set_self() wiring the mount helpers do. Everything in
// this application is a job rather than a person, so everything here holds an
// office and everything here needs this. (Written from scratch, like the four
// experiments before it; the duplication is the finding, so it stays.)
// ---------------------------------------------------------------------------

template <class W, class... Args>
loom::WeaveId mount_office(loom::Switchboard& bus, loom::Grant grant, const std::string& office,
                           Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant), office);
    raw->zen_set_self(id);
    return id;
}

/// Read a participant's own account of itself back through the ordinary gate.
/// The application cannot hold a typed pointer into a loaded library, and it
/// should not hold one into a native weave either if it wants the answer to be
/// the weave's and not its own bookkeeping.
template <class T>
T account_of(const loom::Switchboard& bus, loom::WeaveId id, const char* who) {
    const std::string bytes = bus.snapshot_bytes(id);
    loom::Unverified unverified = loom::parse(bytes);
    loom::Admission admitted = loom::admit(unverified, loom::schema_of<T>());
    if (!admitted) {
        std::cout << "  !!    " << who << "'s account did not pass the gate: "
                  << admitted.first_error().message() << "\n";
        return T{};
    }
    return loom::from_value<T>(admitted.value());
}

// ---------------------------------------------------------------------------
// A hilltop station.
//
// It holds one frame at a time, because a frame IS one setting of six shutters
// and a hill cannot show two. On the minute it shows what is in its hand to
// the next hill along; when it sees a frame on a neighbour it takes it in.
//
// It decides three things, and they are the only three decisions it makes:
// whether what it saw was really its neighbour's frame, whether it could see
// at all, and what the shutters said.
// ---------------------------------------------------------------------------

class Station : public loom::WeaveBase<Station, line::StationState,
                                       loom::Accept<line::Frame, line::Minute, line::Weather>,
                                       loom::Emit<line::Frame>, loom::Claims<line::Visibility>> {
public:
    Station(std::string role, std::string display, std::string above, std::string above_name,
            std::string below, std::string below_name)
        : role_(std::move(role)), above_(std::move(above)), above_name_(std::move(above_name)),
          below_(std::move(below)), below_name_(std::move(below_name)) {
        state_.name = std::move(display);
    }

    void on(const line::Weather& w, loom::Mail& mail) {
        up_clear_ = w.up_clear;
        down_clear_ = w.down_clear;
        hazy_ = w.hazy;
        // WHETHER I CAN SEE IS TRUE IN THE PRESENT TENSE, so it is a claim and
        // not a message. Anybody who wants to know whether the line is working
        // reads the hills; nobody has to be told, and nothing has to be sent.
        mail.as_role(role_).claim(line::Visibility{up_clear_, down_clear_});
    }

    void on(const line::Frame& f, loom::Mail& mail) {
        (void)mail;
        const std::string& expected = f.down ? above_ : below_;
        const std::string& expected_name = f.down ? above_name_ : below_name_;
        if (!mail.authored_from_role(expected)) {
            ++state_.not_my_neighbour;
            say(state_.name, "that frame is not " + expected_name + "'s -- nothing repeated");
            return;
        }
        const bool can_see = f.down ? up_clear_ : down_clear_;
        if (!can_see) {
            ++state_.could_not_read;
            say(state_.name, "cannot see " + expected_name + " -- nothing repeated");
            return;
        }
        std::int64_t read = f.code;
        if (hazy_) {
            read ^= line::kFragileShutter;
        }
        if (held_) {
            ++state_.hands_clashed;
        }
        held_ = true;
        hand_ = line::Frame{read, f.message, f.position, f.down};
        state_.journal.push_back(
            line::JournalLine{g_minute, f.message, f.position, read, f.down});
        if (read != f.code) {
            say(state_.name, "read " + line::shutters(read) + " (" + std::to_string(read) +
                                 ") through the squall; it was " + line::shutters(f.code) + " (" +
                                 std::to_string(f.code) + ")");
        }
    }

    void on(const line::Minute&, loom::Mail& mail) {
        if (!held_) {
            return;
        }
        held_ = false;
        const std::string& to = hand_.down ? below_ : above_;
        mail.as_role(role_).send_to_role(to, hand_);
    }

private:
    std::string role_;
    std::string above_;
    std::string above_name_;
    std::string below_;
    std::string below_name_;
    bool up_clear_ = true;
    bool down_clear_ = true;
    bool hazy_ = false;
    bool held_ = false;
    line::Frame hand_{};
};

// ---------------------------------------------------------------------------
// The Admiralty signal office.
//
// It codes a message out of the book on its desk, hoists it one setting a
// minute, and then waits to be told what Portsmouth heard. It compares the
// repeat against its own file COPY OF THE NUMBERS, never against the words —
// which is exactly why the repeat cannot catch two offices holding different
// books.
// ---------------------------------------------------------------------------

constexpr std::uint64_t kBookCorrelation = 900000;
constexpr std::uint64_t kCodingCorrelation = 100000;
constexpr std::int64_t kPatience = 20;

class Admiralty
    : public loom::WeaveBase<Admiralty, line::AdmiraltyState,
                             loom::Accept<line::OpenTheBook, line::ThisBook, line::SendThis,
                                          line::Coded, line::Frame, line::Minute>,
                             loom::Emit<line::WhichBook, line::Coding, line::Frame>> {
public:
    Admiralty(std::vector<std::string> hills, std::vector<std::string> hill_names)
        : hills_(std::move(hills)), hill_names_(std::move(hill_names)) {}

    void on(const line::OpenTheBook&, loom::Mail& mail) {
        mail.send_to_role("book.admiralty", line::WhichBook{}, kBookCorrelation);
    }

    void on(const line::ThisBook& b, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != kBookCorrelation) {
            return;
        }
        state_.edition = b.edition;
        say("ADM", "the book on this desk is " + b.title);
    }

    void on(const line::SendThis& s, loom::Mail& mail) {
        say("ADM", "to " + s.addressee + ": \"" + joined(s.words) + "\"" +
                       (s.repeat_required ? "" : "   [NO REPEAT REQUIRED -- urgent]"));
        report_the_line(mail);

        pending_ = s;
        codes_.assign(s.words.size(), 0);
        known_.assign(s.words.size(), false);
        got_ = 0;
        for (std::size_t i = 0; i < s.words.size(); ++i) {
            mail.send_to_role("book.admiralty", line::Coding{s.words[i]},
                              kCodingCorrelation + i);
        }
    }

    void on(const line::Coded& c, loom::Mail& mail) {
        (void)mail;
        if (!mail.answers_ask() || mail.correlation() < kCodingCorrelation) {
            return;
        }
        const std::size_t i = static_cast<std::size_t>(mail.correlation() - kCodingCorrelation);
        if (i >= codes_.size()) {
            return;
        }
        codes_[i] = c.code;
        known_[i] = c.known;
        if (++got_ < codes_.size()) {
            return;
        }

        line::Filed filed{};
        filed.message = pending_.message;
        filed.words = pending_.words;
        for (std::size_t k = 0; k < codes_.size(); ++k) {
            if (!known_[k]) {
                filed.refused = pending_.words[k] + " is not in this vocabulary";
                say("ADM", "\"" + pending_.words[k] +
                               "\" is not in the book -- the message does not go");
                state_.file.push_back(filed);
                return;
            }
        }

        std::vector<std::int64_t> hoists;
        hoists.push_back(line::kAttention);
        if (!pending_.repeat_required) {
            hoists.push_back(line::kNoRepeat);
        }
        if (pending_.announce_vocabulary) {
            hoists.push_back(line::kVocabularyBase + state_.edition);
        } else {
            say("ADM", "sent in the old form -- no vocabulary signal");
        }
        hoists.insert(hoists.end(), codes_.begin(), codes_.end());
        hoists.push_back(line::kEnds);

        filed.hoists = hoists;
        filed.sent = true;
        state_.file.push_back(filed);
        outbox_.assign(hoists.begin(), hoists.end());
        position_ = 0;
        awaiting_ = pending_.repeat_required;
        waited_ = 0;
        say("ADM", "coded in " + std::to_string(hoists.size()) + " hoists; up she goes");
    }

    void on(const line::Minute&, loom::Mail& mail) {
        if (!outbox_.empty()) {
            const std::int64_t code = outbox_.front();
            outbox_.erase(outbox_.begin());
            ++position_;
            mail.as_role("admiralty")
                .send_to_role("station.1",
                              line::Frame{code, pending_.message, position_, true});
            return;
        }
        if (!awaiting_) {
            return;
        }
        if (++waited_ <= kPatience) {
            return;
        }
        awaiting_ = false;
        say("ADM", "no repeat from Portsmouth -- the line is interrupted");
        report_the_line(mail);
    }

    /// The repeat, coming back up the line one hoist a minute.
    void on(const line::Frame& f, loom::Mail& mail) {
        if (!mail.authored_from_role("station.1")) {
            say("ADM", "a frame that is not Chelsea's -- ignored");
            return;
        }
        waited_ = 0;
        if (f.code == line::kAttention) {
            repeat_.clear();
        }
        repeat_.push_back(f.code);
        if (f.code != line::kEnds) {
            return;
        }
        awaiting_ = false;

        line::Filed* filed = current_file();
        if (filed == nullptr) {
            return;
        }
        filed->repeated = repeat_;
        if (repeat_ == filed->hoists) {
            filed->repeat_agreed = true;
            say("ADM", "the repeat agrees -- CORRECT");
            outbox_.push_back(line::kCorrect);
            position_ = 0;
            return;
        }

        ++state_.repeats_called_for;
        say("ADM", "the repeat DOES NOT agree" + difference(filed->hoists, repeat_) +
                       " -- call for a repeat");
        outbox_.push_back(line::kRepeat);
        outbox_.insert(outbox_.end(), filed->hoists.begin(), filed->hoists.end());
        position_ = 0;
        awaiting_ = true;
        waited_ = 0;
    }

private:
    static std::string joined(const std::vector<std::string>& words) {
        std::string out;
        for (const std::string& w : words) {
            if (!out.empty()) {
                out += ' ';
            }
            out += w;
        }
        return out;
    }

    static std::string difference(const std::vector<std::int64_t>& sent,
                                  const std::vector<std::int64_t>& back) {
        if (sent.size() != back.size()) {
            return " (it came back " + std::to_string(back.size()) + " hoists, not " +
                   std::to_string(sent.size()) + ")";
        }
        for (std::size_t i = 0; i < sent.size(); ++i) {
            if (sent[i] != back[i]) {
                return " at hoist " + std::to_string(i + 1) + ": sent " +
                       line::shutters(sent[i]) + ", back " + line::shutters(back[i]);
            }
        }
        return "";
    }

    line::Filed* current_file() {
        for (auto it = state_.file.rbegin(); it != state_.file.rend(); ++it) {
            if (it->sent) {
                return &*it;
            }
        }
        return nullptr;
    }

    void report_the_line(loom::Mail& mail) {
        std::string broken;
        std::string unproved;
        for (std::size_t i = 0; i < hills_.size(); ++i) {
            const std::string& name = hill_names_[i];
            loom::SenseReading r = mail.latest_from_office<line::Visibility>(hills_[i]);
            if (!r) {
                unproved += (unproved.empty() ? "" : ", ") + name + " (" +
                            loom::name_of(r.refusal) + ")";
                continue;
            }
            const line::Visibility v = loom::from_value<line::Visibility>(*r.value);
            if (!v.up_clear || !v.down_clear) {
                broken += (broken.empty() ? "" : ", ") + name;
            }
        }
        if (!unproved.empty()) {
            say("ADM", "the line is not proved at " + unproved);
        }
        if (!broken.empty()) {
            say("ADM", "the line is interrupted at " + broken);
        }
        if (unproved.empty() && broken.empty()) {
            say("ADM", "the line is working through to Portsmouth");
        }
    }

    std::vector<std::string> hills_;
    std::vector<std::string> hill_names_;
    line::SendThis pending_{};
    std::vector<std::int64_t> codes_{};
    std::vector<bool> known_{};
    std::size_t got_ = 0;
    std::vector<std::int64_t> outbox_{};
    std::vector<std::int64_t> repeat_{};
    std::int64_t position_ = 0;
    bool awaiting_ = false;
    std::int64_t waited_ = 0;
};

// ---------------------------------------------------------------------------
// The Portsmouth signal office.
// ---------------------------------------------------------------------------

constexpr std::uint64_t kDecodeCorrelation = 200000;

class Portsmouth
    : public loom::WeaveBase<Portsmouth, line::PortsmouthState,
                             loom::Accept<line::OpenTheBook, line::ThisBook, line::Frame,
                                          line::Decoded, line::Minute>,
                             loom::Emit<line::WhichBook, line::Decoding, line::Frame>> {
public:
    void on(const line::OpenTheBook&, loom::Mail& mail) {
        mail.send_to_role("book.portsmouth", line::WhichBook{}, kBookCorrelation);
    }

    void on(const line::ThisBook& b, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != kBookCorrelation) {
            return;
        }
        state_.edition = b.edition;
        say("PMH", "the book on this desk is " + b.title);
    }

    void on(const line::Frame& f, loom::Mail& mail) {
        if (!mail.authored_from_role("station.6")) {
            ++state_.not_my_neighbour;
            say("PMH", "a frame that is not Blackdown's -- nothing taken down");
            return;
        }
        if (f.code == line::kCorrect) {
            say("PMH", "London says CORRECT");
            write_it_out(mail);
            return;
        }
        if (f.code == line::kRepeat) {
            say("PMH", "London calls for a repeat -- standing by");
            held_.clear();
            return;
        }
        if (f.code == line::kAttention) {
            inbound_.clear();
            message_ = f.message;
        }
        inbound_.push_back(f.code);
        if (f.code != line::kEnds) {
            return;
        }
        held_ = inbound_;
        const bool urgent = std::find(held_.begin(), held_.end(), line::kNoRepeat) != held_.end();
        if (urgent) {
            say("PMH", "no repeat called for -- writing it out at once");
            write_it_out(mail);
            return;
        }
        say("PMH", "taken down in " + std::to_string(held_.size()) + " hoists -- repeating back");
        outbox_ = held_;
        position_ = 0;
    }

    void on(const line::Decoded& d, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() < kDecodeCorrelation) {
            return;
        }
        const std::size_t i = static_cast<std::size_t>(mail.correlation() - kDecodeCorrelation);
        if (i >= words_.size()) {
            return;
        }
        words_[i] = d.known ? d.word : ("<" + std::to_string(d.code) + " not in this book>");
        if (++decoded_ < words_.size()) {
            return;
        }
        line::BookEntry& entry = state_.book.back();
        entry.words = words_;
        entry.delivered = true;
        say("PMH", "delivered to the Port Admiral: \"" + joined(words_) + "\"");
    }

    void on(const line::Minute&, loom::Mail& mail) {
        if (outbox_.empty()) {
            return;
        }
        const std::int64_t code = outbox_.front();
        outbox_.erase(outbox_.begin());
        ++position_;
        mail.as_role("portsmouth")
            .send_to_role("station.6", line::Frame{code, message_, position_, false});
    }

private:
    static std::string joined(const std::vector<std::string>& words) {
        std::string out;
        for (const std::string& w : words) {
            if (!out.empty()) {
                out += ' ';
            }
            out += w;
        }
        return out;
    }

    void write_it_out(loom::Mail& mail) {
        if (held_.empty()) {
            return;
        }
        line::BookEntry entry{};
        entry.message = message_;
        entry.hoists = held_;
        entry.decoded_with = state_.edition;

        // THE VOCABULARY SIGNAL. If the message says which book it was written
        // with and this desk holds another, the numbers are meaningless here —
        // and they are meaningless in a way that would read as perfectly good
        // English if this office went ahead anyway.
        std::int64_t announced = 0;
        for (std::int64_t code : held_) {
            if (line::is_vocabulary_signal(code)) {
                announced = code - line::kVocabularyBase;
            }
        }
        if (announced != 0 && announced != state_.edition) {
            entry.refused = "written with vocabulary " + std::to_string(announced) +
                            "; this office holds vocabulary " + std::to_string(state_.edition);
            state_.book.push_back(entry);
            say("PMH", "CANNOT WRITE IT OUT -- " + entry.refused);
            held_.clear();
            return;
        }
        if (announced == 0) {
            say("PMH", "no vocabulary signal -- writing it out of the book on the desk");
        }

        std::vector<std::int64_t> word_codes;
        for (std::int64_t code : held_) {
            if (line::is_word(code)) {
                word_codes.push_back(code);
            }
        }
        state_.book.push_back(entry);
        words_.assign(word_codes.size(), std::string{});
        decoded_ = 0;
        held_.clear();
        for (std::size_t i = 0; i < word_codes.size(); ++i) {
            mail.send_to_role("book.portsmouth", line::Decoding{word_codes[i]},
                              kDecodeCorrelation + i);
        }
    }

    std::vector<std::int64_t> inbound_{};
    std::vector<std::int64_t> held_{};
    std::vector<std::int64_t> outbox_{};
    std::vector<std::string> words_{};
    std::size_t decoded_ = 0;
    std::int64_t message_ = 0;
    std::int64_t position_ = 0;
};

// ---------------------------------------------------------------------------
// The day.
// ---------------------------------------------------------------------------

struct Hill {
    const char* role;
    const char* display;
};

const std::array<Hill, 6> kHills{{
    {"station.1", "CHELSEA"},
    {"station.2", "PUTNEY"},
    {"station.3", "COOMBE"},
    {"station.4", "NETLEY"},
    {"station.5", "HASCOMBE"},
    {"station.6", "BLACKDOWN"},
}};

enum class Scenario { OpenLine, InAHurry, HalfTheLine, Fog };

struct Tap {
    // Every Frame delivery the host's own observer saw, and every refusal.
    std::map<std::int64_t, std::array<std::int64_t, 8>> line_state;
    std::int64_t frames_delivered = 0;
    std::vector<std::string> refusals;
};

int run(Scenario scenario, const std::string& book_1805, const std::string& book_1806) {
    loom::Switchboard bus;
    loom::Kernel kernel(bus);

    std::cout << "\n  shutter-line -- Whitehall to Portsmouth, six hills\n";
    std::cout << "  containment: " << loom::Kernel::containment_note() << "\n\n";

    // ---- the hills --------------------------------------------------------
    std::vector<loom::WeaveId> hill_ids;
    std::vector<std::string> hill_roles;
    std::vector<std::string> hill_names;
    for (std::size_t i = 0; i < kHills.size(); ++i) {
        const std::string above = (i == 0) ? "admiralty" : kHills[i - 1].role;
        const std::string above_name = (i == 0) ? "the Admiralty" : kHills[i - 1].display;
        const std::string below = (i + 1 == kHills.size()) ? "portsmouth" : kHills[i + 1].role;
        const std::string below_name =
            (i + 1 == kHills.size()) ? "Portsmouth" : kHills[i + 1].display;
        // A STATION MAY SHOW ITS FRAME TO THE TWO HILLS IT CAN SEE AND TO
        // NOBODY ELSE. This is the sentence the whole application rests on: if
        // a station could reach past its neighbours, the journals would prove
        // nothing about where a message had been.
        loom::Grant grant;
        grant.allow_to_role("Frame", 1, above).allow_to_role("Frame", 1, below);
        hill_ids.push_back(mount_office<Station>(
            bus, std::move(grant), kHills[i].role, std::string(kHills[i].role),
            std::string(kHills[i].display), above, above_name, below, below_name));
        hill_roles.push_back(kHills[i].role);
        hill_names.push_back(kHills[i].display);
    }

    // ---- the two offices --------------------------------------------------
    loom::Grant adm_grant;
    adm_grant.allow_to_role("Frame", 1, "station.1")
        .allow_to_role("Coding", 1, "book.admiralty")
        .allow_to_role("WhichBook", 1, "book.admiralty")
        .allow_observe("Visibility", 1);
    const loom::WeaveId adm =
        mount_office<Admiralty>(bus, std::move(adm_grant), "admiralty", hill_roles, hill_names);

    loom::Grant pmh_grant;
    pmh_grant.allow_to_role("Frame", 1, "station.6")
        .allow_to_role("Decoding", 1, "book.portsmouth")
        .allow_to_role("WhichBook", 1, "book.portsmouth");
    const loom::WeaveId pmh = mount_office<Portsmouth>(bus, std::move(pmh_grant), "portsmouth");

    // ---- the books --------------------------------------------------------
    // A book answers the desk it sits on and nothing else. Both offices open
    // the same edition this morning; whether they still hold the same one
    // later is the day's other question.
    auto book_grant = [](loom::WeaveId desk) {
        loom::Grant g;
        g.allow("ThisBook", 1, desk).allow("Coded", 1, desk).allow("Decoded", 1, desk);
        return g;
    };
    loom::LoadResult a_book =
        kernel.load("book-adm", book_1805, "book.admiralty", book_grant(adm));
    loom::LoadResult p_book =
        kernel.load("book-pmh", book_1805, "book.portsmouth", book_grant(pmh));
    if (!a_book.ok || !p_book.ok) {
        std::cout << "  !!    no vocabulary: " << a_book.error << " / " << p_book.error << "\n";
        return 2;
    }

    // ---- the tap ----------------------------------------------------------
    Tap tap;
    std::map<std::uint64_t, int> column;
    column[adm.value] = 0;
    for (std::size_t i = 0; i < hill_ids.size(); ++i) {
        column[hill_ids[i].value] = static_cast<int>(i) + 1;
    }
    column[pmh.value] = 7;
    bus.add_observer([&tap, &column](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Refused) {
            tap.refusals.push_back(std::string(loom::name_of(e.refusal.reason)) + " on " +
                                   e.schema_name);
            return;
        }
        if (e.kind != loom::EventKind::Delivered || e.schema_name != "Frame" ||
            e.payload == nullptr) {
            return;
        }
        ++tap.frames_delivered;
        auto it = column.find(e.target.value);
        if (it == column.end()) {
            return;
        }
        const line::Frame f = loom::from_value<line::Frame>(*e.payload);
        // A row of zeroes is "nothing changed on this hill this minute"; the
        // code is stored one higher so a genuine code 0 is not silence.
        auto& row = tap.line_state[g_minute];
        row[static_cast<std::size_t>(it->second)] = f.code + 1;
    });

    // ---- the day's programme ---------------------------------------------
    const bool fog = (scenario == Scenario::Fog);
    const std::int64_t squall_at = (scenario == Scenario::InAHurry) ? 14 : 13;
    const bool squall = (scenario == Scenario::OpenLine || scenario == Scenario::InAHurry);

    auto host_send = [&bus](const std::string& role, auto&& payload) {
        bus.send_to_role(role, loom::Message(loom::to_value(payload)));
    };

    const std::vector<std::string> message_one{"FRENCH", "FLEET", "AT", "SEA", "SAIL"};
    const std::vector<std::string> message_two{"ADMIRAL", "IS", "OFF", "USHANT"};

    const std::int64_t day = 130;
    for (g_minute = 1; g_minute <= day; ++g_minute) {
        // --- what the day does at this minute ---
        if (g_minute == 1) {
            note("the line opens; every hill reports what it can see");
            for (std::size_t i = 0; i < kHills.size(); ++i) {
                const bool blind = fog && i == 4; // Hascombe, towards London
                if (blind) {
                    note("fog on Hascombe -- it cannot see Netley Heath");
                }
                host_send(kHills[i].role, line::Weather{!blind, true, false});
            }
            host_send("admiralty", line::OpenTheBook{});
            host_send("portsmouth", line::OpenTheBook{});
        }
        if (g_minute == 2 && scenario == Scenario::HalfTheLine) {
            note("PORTSMOUTH ALONE TAKES THE NEW BOOK; Whitehall has not had its copy yet");
            kernel.unload("book-pmh");
            loom::LoadResult r =
                kernel.load("book-pmh-2", book_1806, "book.portsmouth", book_grant(pmh));
            if (!r.ok) {
                std::cout << "  !!    " << r.error << "\n";
                return 2;
            }
            host_send("portsmouth", line::OpenTheBook{});
        }
        if (g_minute == 3) {
            line::SendThis s{};
            s.message = 1;
            s.words = message_one;
            s.addressee = "the Port Admiral, Portsmouth";
            s.repeat_required = (scenario != Scenario::InAHurry);
            s.announce_vocabulary = true;
            host_send("admiralty", s);
        }
        if (squall && g_minute == squall_at) {
            note("a squall crosses Netley Heath");
            host_send("station.4", line::Weather{true, true, true});
        }
        if (squall && g_minute == squall_at + 1) {
            note("the squall passes");
            host_send("station.4", line::Weather{true, true, false});
        }
        if (scenario == Scenario::HalfTheLine && g_minute == 60) {
            note("the same message again, in the old form -- no vocabulary signal");
            line::SendThis s{};
            s.message = 2;
            s.words = message_one;
            s.addressee = "the Port Admiral, Portsmouth";
            s.repeat_required = true;
            s.announce_vocabulary = false;
            host_send("admiralty", s);
        }
        if (scenario == Scenario::OpenLine && g_minute == 71) {
            note("CONTROL: somebody on Putney Heath sets up a frame of their own");
            bus.send_to_role("station.3",
                             loom::Message(loom::to_value(line::Frame{line::kAttention, 99, 1,
                                                                      true})));
        }
        if (scenario == Scenario::OpenLine && g_minute == 72) {
            // THE ONE FRAME THE HONEST API CANNOT PUT ON THE WIRE. Coombe
            // Warren's own code only ever addresses its two neighbours, so the
            // day forges the frame on its behalf with the verified host door:
            // the sender is stamped as Coombe Warren, the office it asks to
            // speak as is one it genuinely holds, so AUTHORSHIP SUCCEEDS and
            // the only thing left between this frame and Portsmouth is the
            // grant.
            note("CONTROL: Coombe Warren signals Portsmouth over the heads of the other four");
            bus.office_send_to_role_as(
                hill_ids[2], "station.3", "portsmouth",
                loom::Message(loom::to_value(line::Frame{line::kAttention, 99, 1, true})));
        }
        if (scenario == Scenario::OpenLine && g_minute == 74) {
            // The other half of the same boundary: a hill cannot claim to be a
            // different hill. This one IS addressed to a neighbour the grant
            // permits, so the grant is not what stops it.
            note("CONTROL: Coombe Warren shows Netley Heath a frame, claiming to be Hascombe");
            bus.office_send_to_role_as(
                hill_ids[2], "station.5", "station.4",
                loom::Message(loom::to_value(line::Frame{line::kAttention, 97, 1, true})));
        }
        if (scenario == Scenario::OpenLine && g_minute == 73) {
            note("CONTROL: a word that is not in the 1805 book");
            line::SendThis s{};
            s.message = 98;
            s.words = {"CONVOY", "IS", "AT", "SEA"};
            s.addressee = "the Port Admiral, Portsmouth";
            host_send("admiralty", s);
        }
        if (scenario == Scenario::OpenLine && g_minute == 76) {
            note("MIDDAY -- the 1806 vocabulary comes into force; both offices change books");
            kernel.unload("book-adm");
            kernel.unload("book-pmh");
            loom::LoadResult a2 =
                kernel.load("book-adm-2", book_1806, "book.admiralty", book_grant(adm));
            loom::LoadResult p2 =
                kernel.load("book-pmh-2", book_1806, "book.portsmouth", book_grant(pmh));
            if (!a2.ok || !p2.ok) {
                std::cout << "  !!    " << a2.error << " / " << p2.error << "\n";
                return 2;
            }
            host_send("admiralty", line::OpenTheBook{});
            host_send("portsmouth", line::OpenTheBook{});
        }
        if (scenario == Scenario::OpenLine && g_minute == 79) {
            line::SendThis s{};
            s.message = 2;
            s.words = message_two;
            s.addressee = "the Port Admiral, Portsmouth";
            host_send("admiralty", s);
        }

        bus.publish(loom::Message(loom::to_value(line::Minute{g_minute})));
        bus.pump();
    }

    // ---- the line, minute by minute, as the tap saw it --------------------
    std::cout << "\n  what each station took in, minute by minute (the host's own tap)\n\n";
    std::cout << "   min   ADM  CHEL PUTN COOM NETL HASC BLAC  PMH\n";
    for (const auto& [minute, row] : tap.line_state) {
        bool any = false;
        for (std::int64_t v : row) {
            any = any || v != 0;
        }
        if (!any) {
            continue;
        }
        char stamp[16];
        std::snprintf(stamp, sizeof stamp, "  %4lld  ", static_cast<long long>(minute));
        std::cout << stamp;
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (row[i] == 0) {
                std::cout << "  .  ";
            } else {
                char cell[8];
                std::snprintf(cell, sizeof cell, " %3lld ", static_cast<long long>(row[i] - 1));
                std::cout << cell;
            }
        }
        std::cout << "\n";
    }

    // ---- the accounts -----------------------------------------------------
    //
    // Every hill's journal is that hill's own account of what it saw, read back
    // through the ordinary gate rather than out of a pointer the tower kept.
    // Laid side by side they say where a number changed hands, which is the one
    // question the two offices cannot answer between them.
    std::vector<line::StationState> journals;
    for (std::size_t i = 0; i < hill_ids.size(); ++i) {
        journals.push_back(account_of<line::StationState>(bus, hill_ids[i], kHills[i].display));
    }

    const line::AdmiraltyState whitehall = account_of<line::AdmiraltyState>(bus, adm, "ADM");
    const line::PortsmouthState portsmouth = account_of<line::PortsmouthState>(bus, pmh, "PMH");
    const line::BookState adm_book =
        account_of<line::BookState>(bus, bus.role_holder("book.admiralty"), "the London book");
    const line::BookState pmh_book =
        account_of<line::BookState>(bus, bus.role_holder("book.portsmouth"), "the Portsmouth book");

    std::cout << "\n  the journals on the hills, read back through the ordinary gate\n";
    for (const line::Filed& f : whitehall.file) {
        if (!f.sent) {
            continue;
        }
        std::cout << "\n    message " << f.message << ", down the line\n";
        for (const line::StationState& j : journals) {
            std::string codes;
            for (const line::JournalLine& l : j.journal) {
                if (l.message == f.message && l.down) {
                    codes += (codes.empty() ? "" : " ") + std::to_string(l.code);
                }
            }
            std::string name = j.name;
            name.resize(12, ' ');
            std::cout << "      " << name << codes << "\n";
        }
    }

    std::cout << "\n";
    for (const line::BookEntry& e : portsmouth.book) {
        std::string codes;
        for (std::int64_t c : e.hoists) {
            codes += (codes.empty() ? "" : " ") + std::to_string(c);
        }
        std::cout << "    message " << e.message << "  taken down: " << codes << "\n";
        if (!e.refused.empty()) {
            std::cout << "               NOT WRITTEN OUT -- " << e.refused << "\n";
        } else {
            std::string words;
            for (const std::string& w : e.words) {
                words += (words.empty() ? "" : " ") + w;
            }
            std::cout << "               written out of vocabulary " << e.decoded_with << ": \""
                      << words << "\"\n";
        }
    }

    // ---- what the day is allowed to claim ---------------------------------
    int bad = 0;
    auto check = [&bad](bool ok, const std::string& what) {
        std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
        if (!ok) {
            ++bad;
        }
    };

    std::cout << "\n";
    std::int64_t journal_lines = 0;
    std::int64_t could_not_read = 0;
    std::int64_t not_neighbour = 0;
    std::int64_t clashes = 0;
    for (const line::StationState& j : journals) {
        journal_lines += static_cast<std::int64_t>(j.journal.size());
        could_not_read += j.could_not_read;
        not_neighbour += j.not_my_neighbour;
        clashes += j.hands_clashed;
    }
    std::cout << "    frames on the tap        " << tap.frames_delivered << "\n";
    std::cout << "    lines in the journals    " << journal_lines << "\n";
    std::cout << "    could not read           " << could_not_read << "\n";
    std::cout << "    not my neighbour         " << not_neighbour << " (hills) + "
              << portsmouth.not_my_neighbour << " (Portsmouth)\n";
    std::cout << "    repeats called for       " << whitehall.repeats_called_for << "\n";
    std::cout << "    the book now on the desk " << adm_book.questions_answered << " (London) + "
              << pmh_book.questions_answered << " (Portsmouth) question(s)\n";
    std::cout << "    bus refusals seen        " << tap.refusals.size();
    if (!tap.refusals.empty()) {
        std::cout << "  [";
        for (std::size_t i = 0; i < tap.refusals.size(); ++i) {
            std::cout << (i ? ", " : "") << tap.refusals[i];
        }
        std::cout << "]";
    }
    std::cout << "\n\n";

    check(clashes == 0, "no hill was ever asked to show two frames at once");
    check(tap.frames_delivered > 0 && journal_lines > 0,
          "the tap and the journals both saw traffic (an audit of nothing is not an audit)");

    // Find, from the journals alone, where the numbers stopped agreeing.
    auto message_codes = [](const line::StationState& j, std::int64_t message, bool down) {
        std::vector<std::int64_t> out;
        for (const line::JournalLine& l : j.journal) {
            if (l.message == message && l.down == down) {
                out.push_back(l.code);
            }
        }
        return out;
    };

    switch (scenario) {
        case Scenario::OpenLine: {
            check(portsmouth.book.size() == 2, "two messages were written out");
            const bool delivered_right =
                portsmouth.book.size() == 2 && portsmouth.book[0].delivered &&
                portsmouth.book[0].words == message_one && portsmouth.book[1].delivered &&
                portsmouth.book[1].words == message_two;
            check(delivered_right, "both messages reached the Port Admiral exactly as sent");
            check(whitehall.repeats_called_for == 1,
                  "the repeat caught the squall exactly once (it is a measurement, not a zero)");
            // The first attempt is in the journals; the misread is at Netley Heath.
            const std::vector<std::int64_t> coombe = message_codes(journals[2], 1, true);
            const std::vector<std::int64_t> netley = message_codes(journals[3], 1, true);
            bool located = coombe.size() > 6 && netley.size() > 6 && coombe[6] == 23 &&
                           netley[6] == 21;
            check(located,
                  "the journals put the change between Coombe Warren and Netley Heath: "
                  "23 went in and 21 came out");
            check(whitehall.file.size() == 3 && !whitehall.file[1].refused.empty(),
                  "a word that is not in the book stops the message at the desk");
            check(not_neighbour == 1,
                  "the frame set up by a stranger on Putney Heath was not repeated");
            check(tap.refusals.size() == 2 && tap.refusals[0] == "CapabilityDenied on Frame",
                  "Coombe Warren cannot signal past its neighbours, speaking as itself: "
                  "CapabilityDenied on Frame");
            check(tap.refusals.size() == 2 && tap.refusals[1] == "RoleAuthorshipDenied on Frame",
                  "and it cannot claim to be Hascombe: RoleAuthorshipDenied on Frame");
            check(portsmouth.not_my_neighbour == 0,
                  "Portsmouth never saw the short-circuit at all -- the grant refused it before "
                  "the domain rule was reached");
            check(whitehall.edition == 2 && portsmouth.edition == 2,
                  "both offices are on the 1806 vocabulary after midday");
            break;
        }
        case Scenario::InAHurry: {
            // EVERY MECHANICAL MEASURE SUCCEEDS HERE. The message is complete,
            // it went through every hand, nothing was refused, and the wrong
            // order is on the Port Admiral's desk.
            const bool complete = portsmouth.book.size() == 1 && portsmouth.book[0].delivered;
            check(complete, "the message was taken down complete and written out");
            check(whitehall.repeats_called_for == 0, "no repeat was called for -- none was asked");
            check(could_not_read == 0 && clashes == 0, "the line itself behaved perfectly");
            const bool wrong = complete && portsmouth.book[0].words != message_one;
            check(wrong, "AND THE ORDER IS WRONG: the squall changed one shutter");
            if (complete) {
                std::string got;
                for (const std::string& w : portsmouth.book[0].words) {
                    got += (got.empty() ? "" : " ") + w;
                }
                std::cout << "          sent      FRENCH FLEET AT SEA SAIL\n";
                std::cout << "          delivered " << got << "\n";
                check(portsmouth.book[0].words.size() == message_one.size() &&
                          portsmouth.book[0].words[4] == "ANCHOR",
                      "one word differs, and it is the opposite order");
            }
            const std::vector<std::int64_t> coombe = message_codes(journals[2], 1, true);
            const std::vector<std::int64_t> netley = message_codes(journals[3], 1, true);
            check(coombe.size() > 7 && netley.size() > 7 && coombe[7] == 23 && netley[7] == 21,
                  "the journals name the hill it happened on");
            break;
        }
        case Scenario::HalfTheLine: {
            check(portsmouth.book.size() == 2, "both messages were taken down");
            const bool refused = portsmouth.book.size() == 2 && !portsmouth.book[0].refused.empty();
            check(refused, "the vocabulary signal stopped the first one being written out");
            const bool second_wrong = portsmouth.book.size() == 2 &&
                                      portsmouth.book[1].delivered &&
                                      portsmouth.book[1].words != message_one;
            check(second_wrong,
                  "AND THE OLD-FORM MESSAGE WAS WRITTEN OUT, wrongly, out of the wrong book");
            if (second_wrong) {
                std::string got;
                for (const std::string& w : portsmouth.book[1].words) {
                    got += (got.empty() ? "" : " ") + w;
                }
                std::cout << "          sent      FRENCH FLEET AT SEA SAIL\n";
                std::cout << "          delivered " << got << "\n";
            }
            // The numbers were perfect. This is the half that matters: the
            // check that catches a misread cannot see this at all.
            const bool numbers_agreed =
                whitehall.file.size() == 2 && whitehall.file[1].repeat_agreed;
            check(numbers_agreed,
                  "and the repeat AGREED -- every number was right from end to end");
            bool journals_agree = true;
            const std::vector<std::int64_t> first = message_codes(journals[0], 2, true);
            for (const line::StationState& j : journals) {
                journals_agree = journals_agree && message_codes(j, 2, true) == first;
            }
            check(journals_agree && !first.empty(),
                  "every journal on the line holds the same numbers -- there is nothing to find");
            break;
        }
        case Scenario::Fog: {
            check(portsmouth.book.empty(), "nothing reached Portsmouth");
            check(!message_codes(journals[3], 1, true).empty(),
                  "the message got as far as Netley Heath");
            check(message_codes(journals[5], 1, true).empty(), "and no further than Hascombe");
            check(journals[4].could_not_read > 0,
                  "Hascombe could not read the frame above it, and said so");
            check(whitehall.file.size() == 1 && !whitehall.file[0].repeat_agreed,
                  "the Admiralty never got its repeat");
            break;
        }
    }

    std::cout << "\n";
    if (bad != 0) {
        std::cout << "  " << bad << " THING(S) THIS DAY CANNOT CLAIM\n\n";
        return 1;
    }
    switch (scenario) {
        case Scenario::OpenLine:
            std::cout << "  A GOOD DAY ON THE LINE\n\n";
            break;
        case Scenario::InAHurry:
            std::cout << "  THE FLEET WAS TOLD TO ANCHOR, AND NOTHING ON THE LINE COMPLAINED\n\n";
            break;
        case Scenario::HalfTheLine:
            std::cout << "  THE SIGNAL STOPPED ONE OF THE TWO; THE OTHER WENT OUT WRONG\n\n";
            break;
        case Scenario::Fog:
            std::cout << "  THE LINE WAS INTERRUPTED AT HASCOMBE AND NOTHING WAS DELIVERED\n\n";
            break;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: telegraph <vocabulary-1805.so> <vocabulary-1806.so> [--in-a-hurry |"
                     " --half-the-line | --fog]\n";
        return 2;
    }
    Scenario scenario = Scenario::OpenLine;
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--in-a-hurry") == 0) {
            scenario = Scenario::InAHurry;
        } else if (std::strcmp(argv[i], "--half-the-line") == 0) {
            scenario = Scenario::HalfTheLine;
        } else if (std::strcmp(argv[i], "--fog") == 0) {
            scenario = Scenario::Fog;
        }
    }
    return run(scenario, argv[1], argv[2]);
}
