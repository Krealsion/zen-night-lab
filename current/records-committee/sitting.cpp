// One sitting of the Marchfield Records Committee.
//
// The host is the meeting: it owns the calendar, the agenda, and the minutes.
// Everything that judges anything is a weave.
//
// Run it:
//   ./sitting --archive <path> --year 1979 --found  aldis.so brant.so corve.so denny.so elsdon.so
//   ./sitting --archive <path> --year 1980          aldis.so brant.so corve.so denny.so elsdon.so
//
// The first form founds a county list. The second requires one to already
// exist, and REFUSES TO SIT if it does not — which is the whole point of this
// application. A committee that cannot read its own list cannot tell a second
// county record from a first, and a sitting that quietly starts from nothing
// will announce a first county record for a bird the county has had for years,
// print a completely happy report, and be wrong for as long as anybody keeps
// the minutes.
//
// Exit codes:  0  the sitting happened and every check held
//              1  a check failed
//              2  the committee could not sit (and wrote nothing)

#include "committee.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace committee;

namespace {

// ===========================================================================
// The local office binder
// ===========================================================================

// `loom::mount<T>()` and `loom::mount_granted<T>()` take no role, and
// `register_weave(weave, grant, role)` is the raw door, so it does not do the
// `zen_set_self` wiring the mount helpers do. A native weave that holds an
// office therefore needs this, and forgetting the one line is silent.
template <class W, class... Args>
W* mount_office(loom::Switchboard& bus, loom::Grant grant, const std::string& office,
                Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    const loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant), office);
    raw->zen_set_self(id);
    return raw;
}

// ===========================================================================
// The recorder
// ===========================================================================

struct Determination {
    std::string record_id;
    std::string species;
    std::string observer;
    std::string decision;
    std::int64_t round = 0;
    std::int64_t accepts = 0;
    std::int64_t rejects = 0;
    std::string note;
};

enum class Opened {
    Read,          // an existing list, and it is ours
    Founded,       // there was no list and we were asked to start one
    NoArchive,     // there is no list and nobody said to start one
    WrongCounty,   // this is somebody else's list
    Malformed,     // the file is not whole
    AlreadyFounded // asked to start a list where one already exists
};

const char* why(Opened o) {
    switch (o) {
    case Opened::Read:
        return "read";
    case Opened::Founded:
        return "founded";
    case Opened::NoArchive:
        return "there is no county list at that path, and this sitting was not asked to found one";
    case Opened::WrongCounty:
        return "that list belongs to another county";
    case Opened::Malformed:
        return "that list is not whole -- it ends in the middle of itself";
    case Opened::AlreadyFounded:
        return "there is already a county list at that path";
    }
    return "?";
}

// Trailing empty fields are kept. A determination with no note is a real
// determination with an empty last field, and a splitter that quietly drops it
// makes a perfectly good line look like a short one.
std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (const char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

std::string join(const std::vector<std::string>& v, char sep) {
    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i != 0) {
            out += sep;
        }
        out += v[i];
    }
    return out;
}

// The county recorder. THE ONLY PARTICIPANT THAT TOUCHES THE FILE.
//
// It is a weave and not a corner of the host because "may this be written
// down" is a question about authority, and the answer is a Loom answer: the
// recorder writes what the SECRETARY OFFICE says was determined, and nothing
// else, however well-formed.
class Archive : public loom::WeaveBase<Archive, ArchiveState,
                                       loom::Accept<IsItOnTheList, RecordDetermination>,
                                       loom::Emit<OnTheList, Minuted>> {
public:
    Archive(std::string path, std::string county, std::int64_t year)
        : path_(std::move(path)), county_(std::move(county)), year_(year) {}

    // -- the durable half ----------------------------------------------------

    Opened open(bool found) {
        std::ifstream in(path_);
        if (!in) {
            if (!found) {
                return Opened::NoArchive;
            }
            edition_ = 1;
            return Opened::Founded;
        }
        if (found) {
            return Opened::AlreadyFounded;
        }

        std::string line;
        std::int64_t body = 0;
        bool ended = false;
        std::int64_t declared = -1;
        std::string file_county;

        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const std::size_t sp = line.find(' ');
            const std::string key = line.substr(0, sp);
            std::string rest = (sp == std::string::npos) ? "" : line.substr(sp);
            while (!rest.empty() && rest.front() == ' ') {
                rest.erase(rest.begin());
            }

            if (key == "county") {
                file_county = rest;
            } else if (key == "edition") {
                edition_ = std::stoll(rest);
            } else if (key == "sittings") {
                for (const std::string& y : split(rest, ',')) {
                    if (!y.empty()) {
                        sittings_.push_back(std::stoll(y));
                    }
                }
            } else if (key == "det") {
                const std::vector<std::string> f = split(rest, '|');
                if (f.size() < 8) {
                    return Opened::Malformed;
                }
                determinations_.push_back(Determination{f[0], f[1], f[2], f[3], std::stoll(f[4]),
                                                        std::stoll(f[5]), std::stoll(f[6]), f[7]});
                ++body;
            } else if (key == "list") {
                const std::vector<std::string> f = split(rest, '|');
                if (f.size() < 2) {
                    return Opened::Malformed;
                }
                list_[f[0]] = f[1];
                ++body;
            } else if (key == "held") {
                const std::vector<std::string> f = split(rest, '|');
                if (f.size() < 11) {
                    return Opened::Malformed;
                }
                Submission s;
                s.record_id = f[0];
                s.species = f[1];
                s.observer = f[2];
                s.month = f[3];
                s.photograph = (f[4] == "1");
                s.sound = (f[5] == "1");
                s.in_season = (f[6] == "1");
                s.observers = std::stoll(f[7]);
                s.confusion = f[8].empty() ? std::vector<std::string>{} : split(f[8], ',');
                s.ruled_out = f[9].empty() ? std::vector<std::string>{} : split(f[9], ',');
                // Deliberately NOT carried across: a record is late only for
                // the sitting it missed. Next year it is simply a record.
                s.after_closing_date = false;
                held_.push_back(s);
                ++body;
            } else if (key == "end") {
                declared = std::stoll(rest);
                ended = true;
            }
        }

        // THE WHOLE-FILE CHECK, and it is the application's own promise rather
        // than a general one: a list that does not say how long it is, or that
        // disagrees with itself about it, is a list somebody stopped writing
        // half way through. There is no honest way to sit on half a list.
        if (!ended || declared != body) {
            return Opened::Malformed;
        }
        if (file_county != county_) {
            return Opened::WrongCounty;
        }

        state_.species_on_the_list = static_cast<std::int64_t>(list_.size());
        state_.determinations = static_cast<std::int64_t>(determinations_.size());
        return Opened::Read;
    }

    bool write_out() {
        std::ofstream out(path_);
        if (!out) {
            return false;
        }
        sittings_.push_back(year_);

        std::vector<std::string> years;
        for (std::int64_t y : sittings_) {
            years.push_back(std::to_string(y));
        }

        out << "# the " << county_ << " records committee -- the county list and the minutes\n";
        out << "county   " << county_ << "\n";
        out << "edition  " << edition_ << "\n";
        out << "sittings " << join(years, ',') << "\n";

        std::int64_t body = 0;
        for (const Determination& d : determinations_) {
            out << "det   " << d.record_id << "|" << d.species << "|" << d.observer << "|"
                << d.decision << "|" << d.round << "|" << d.accepts << "|" << d.rejects << "|"
                << d.note << "\n";
            ++body;
        }
        for (const auto& [species, record] : list_) {
            out << "list  " << species << "|" << record << "\n";
            ++body;
        }
        for (const Submission& s : held_) {
            out << "held  " << s.record_id << "|" << s.species << "|" << s.observer << "|"
                << s.month << "|" << (s.photograph ? 1 : 0) << "|" << (s.sound ? 1 : 0) << "|"
                << (s.in_season ? 1 : 0) << "|" << s.observers << "|" << join(s.confusion, ',')
                << "|" << join(s.ruled_out, ',') << "|held over\n";
            ++body;
        }
        out << "end   " << body << "\n";
        return static_cast<bool>(out);
    }

    // -- the live half -------------------------------------------------------

    void on(const IsItOnTheList& q, loom::Mail& mail) {
        ++state_.consulted;
        const auto it = list_.find(q.species);
        mail.answer(it == list_.end() ? OnTheList{q.species, false, ""}
                                      : OnTheList{q.species, true, it->second});
    }

    void on(const RecordDetermination& d, loom::Mail& mail) {
        // THE COUNTY LIST IS NOT OPEN TO ANYBODY WHO CAN SPELL THIS SHAPE.
        // The recorder minutes what the secretary determined, and the fact
        // that it was the secretary speaking is Loom's to attest, not the
        // payload's to claim.
        if (!mail.authored_from_role(kOfficeSecretary)) {
            ++state_.unauthored;
            return;
        }

        // "First county record" is a property of the FILE, and this is the
        // only place in the application that can answer it.
        const bool listed = list_.find(d.species) != list_.end();
        const bool first = (d.decision == kAccepted) && !listed;

        std::string resubmission_of;
        std::string previous_decision;
        for (auto it = determinations_.rbegin(); it != determinations_.rend(); ++it) {
            if (it->species == d.species && it->observer == d.observer &&
                it->record_id != d.record_id && it->decision != kHeldOver) {
                resubmission_of = it->record_id;
                previous_decision = it->decision;
                break;
            }
        }

        determinations_.push_back(Determination{d.record_id, d.species, d.observer, d.decision,
                                                d.round, d.accepts, d.rejects, d.note});
        if (first) {
            list_[d.species] = d.record_id;
        }

        // A record that was held over waits in the file with its submission
        // intact, because next year's committee has to be able to circulate
        // it and nobody else is keeping it.
        held_.erase(std::remove_if(held_.begin(), held_.end(),
                                   [&](const Submission& s) { return s.record_id == d.record_id; }),
                    held_.end());
        if (d.decision == kHeldOver) {
            held_.push_back(d.record);
        }

        state_.species_on_the_list = static_cast<std::int64_t>(list_.size());
        state_.determinations = static_cast<std::int64_t>(determinations_.size());
        ++state_.minuted_this_sitting;
        mail.answer(Minuted{d.record_id, first, resubmission_of, previous_decision});
    }

    // -- what the meeting may read ------------------------------------------

    const std::map<std::string, std::string>& list() const { return list_; }
    const std::vector<Determination>& determinations() const { return determinations_; }
    const std::vector<Submission>& held_over() const { return held_; }
    std::int64_t unauthored() const { return state_.unauthored; }
    std::int64_t consulted() const { return state_.consulted; }
    std::int64_t minuted() const { return state_.minuted_this_sitting; }

private:
    std::string path_;
    std::string county_;
    std::int64_t year_ = 0;
    std::int64_t edition_ = 1;
    std::vector<std::int64_t> sittings_;
    std::map<std::string, std::string> list_;
    std::vector<Determination> determinations_;
    std::vector<Submission> held_;
};

// ===========================================================================
// The secretary
// ===========================================================================

// Issues the ballots, keeps the ballot book, applies the published rules to
// whatever came back, and minutes the result as the secretary office.
//
// It never decides a record itself and it has no opinion about birds.
class Secretary : public loom::WeaveBase<
                      Secretary, SecretaryState,
                      loom::Accept<PutOnTheAgenda, SendOutTheCirculation, CloseTheCirculation,
                                   Adjourn, Vote, Minuted>,
                      loom::Emit<Circulate, RecordDetermination>> {
public:
    struct Item {
        Submission record;
        std::int64_t round = 1;
        std::vector<std::string> dissent;
        bool decided = false;
        bool parked = false; // held over; will not be circulated again this sitting
        std::string decision;
        std::string note;
        std::int64_t decided_round = 0;
        std::int64_t accepts = 0;
        std::int64_t rejects = 0;
        std::int64_t returned_r1 = 0;
        std::int64_t accepts_r1 = 0;
        // filled in by the recorder's answer
        bool first_for_county = false;
        std::string resubmission_of;
        std::string previous_decision;
    };

    void on(const PutOnTheAgenda& p, loom::Mail&) {
        Item item;
        item.record = p.record;
        if (p.record.after_closing_date) {
            item.parked = true;
            item.note = "received after the closing date";
            ++state_.held_over;
        }
        agenda_.push_back(std::move(item));
        ++state_.on_the_agenda;
    }

    void on(const SendOutTheCirculation& c, loom::Mail& mail) {
        for (std::size_t i = 0; i < agenda_.size(); ++i) {
            Item& item = agenda_[i];
            if (item.decided || item.parked || item.round != c.round) {
                continue;
            }
            for (int seat = 1; seat <= kSeats; ++seat) {
                const std::uint64_t ballot = next_ballot_++;
                book_[ballot] = Ballot{i, seat, c.round};
                ++state_.ballots_issued;
                // Sent to the SEAT, not to a member: the secretary circulates
                // to whoever holds seat three, and an empty seat refuses at
                // delivery without the secretary being told. That is why the
                // circulation ends on a date and not on a headcount.
                mail.as_role(kOfficeSecretary)
                    .send_to_role(seat_office(seat), Circulate{item.record, c.round, item.dissent},
                                  ballot);
            }
        }
    }

    void on(const Vote& v, loom::Mail& mail) {
        // A VOTE IS ONLY A VOTE IF IT ANSWERS A BALLOT. Loom decides the first
        // half (this delivery is the authorized answer to something this weave
        // asked); the ballot book decides the second (which record, which seat,
        // which round) — and the ballot number is the secretary's own, so no
        // payload has to be believed about any of it.
        if (!mail.answers_ask()) {
            ++state_.unsolicited;
            return;
        }
        const auto it = book_.find(mail.correlation());
        if (it == book_.end()) {
            ++state_.unsolicited;
            return;
        }
        if (it->second.closed) {
            ++state_.late;
            return;
        }
        it->second.answered = true;
        it->second.accept = v.accept;
        it->second.comment = v.comment;
        ++state_.votes_counted;
    }

    void on(const CloseTheCirculation& c, loom::Mail&) {
        for (std::size_t i = 0; i < agenda_.size(); ++i) {
            Item& item = agenda_[i];
            if (item.decided || item.parked || item.round != c.round) {
                continue;
            }

            std::int64_t returned = 0;
            std::int64_t accepts = 0;
            std::vector<std::string> dissent;
            for (auto& [ballot, b] : book_) {
                if (b.item != i || b.round != c.round || !b.answered) {
                    continue;
                }
                ++returned;
                if (b.accept) {
                    ++accepts;
                } else {
                    dissent.push_back("seat " + std::to_string(b.seat) + ": " + b.comment);
                }
            }
            const std::int64_t rejects = returned - accepts;

            if (c.round == 1) {
                item.returned_r1 = returned;
                item.accepts_r1 = accepts;
            }

            // QUORUM. Below it there is no decision to be had — not a refusal,
            // not a rejection, and above all not a decision by whoever was at
            // home. The record waits for a fuller committee.
            if (returned < kQuorum) {
                item.parked = true;
                item.note = "only " + std::to_string(returned) + " of " +
                            std::to_string(kSeats) + " seats voted; below the quorum of " +
                            std::to_string(kQuorum);
                ++state_.held_over;
                continue;
            }

            if (c.round == 1) {
                if (accepts == returned) {
                    decide(item, kAccepted, 1, accepts, rejects);
                } else if (accepts == 0) {
                    decide(item, kNotAccepted, 1, accepts, rejects);
                } else {
                    item.round = 2;
                    item.dissent = dissent;
                }
            } else {
                decide(item, accepts >= kAcceptsInRoundTwo ? kAccepted : kNotAccepted, c.round,
                       accepts, rejects);
            }
        }

        for (auto& [ballot, b] : book_) {
            if (b.round == c.round) {
                b.closed = true;
            }
        }
    }

    void on(const Adjourn&, loom::Mail& mail) {
        for (std::size_t i = 0; i < agenda_.size(); ++i) {
            Item& item = agenda_[i];
            if (!item.decided) {
                item.decision = kHeldOver;
                if (item.note.empty()) {
                    item.note = "not reached";
                    ++state_.held_over;
                }
            }
            const std::uint64_t minute = next_minute_++;
            minutes_[minute] = i;
            // AS THE SECRETARY. The recorder will not write down a
            // determination that was not spoken by the office entitled to make
            // one, and holding the office is not the same as speaking as it.
            mail.as_role(kOfficeSecretary)
                .send_to_role(kOfficeArchive,
                              RecordDetermination{item.record.record_id, item.record.species,
                                                  item.record.observer, item.decision,
                                                  item.decided_round, item.accepts, item.rejects,
                                                  item.note, item.record},
                              minute);
        }
    }

    void on(const Minuted& m, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        const auto it = minutes_.find(mail.correlation());
        if (it == minutes_.end()) {
            return;
        }
        Item& item = agenda_[it->second];
        item.first_for_county = m.first_for_county;
        item.resubmission_of = m.resubmission_of;
        item.previous_decision = m.previous_decision;
    }

    const std::vector<Item>& agenda() const { return agenda_; }
    std::int64_t ballots_issued() const { return state_.ballots_issued; }
    std::int64_t votes_counted() const { return state_.votes_counted; }
    std::int64_t unsolicited() const { return state_.unsolicited; }
    std::int64_t late() const { return state_.late; }
    std::int64_t determined() const { return state_.determined; }
    std::int64_t held_over() const { return state_.held_over; }

private:
    struct Ballot {
        std::size_t item = 0;
        int seat = 0;
        std::int64_t round = 0;
        bool answered = false;
        bool accept = false;
        bool closed = false;
        std::string comment;
    };

    void decide(Item& item, const char* decision, std::int64_t round, std::int64_t accepts,
                std::int64_t rejects) {
        item.decided = true;
        item.decision = decision;
        item.decided_round = round;
        item.accepts = accepts;
        item.rejects = rejects;
        ++state_.determined;
    }

    std::vector<Item> agenda_;
    std::map<std::uint64_t, Ballot> book_;
    std::map<std::uint64_t, std::size_t> minutes_;
    std::uint64_t next_ballot_ = 1;
    std::uint64_t next_minute_ = 1000; // a minute is not a ballot; keep them visibly apart
};

// ===========================================================================
// The agenda. The host owns the submissions; it does not judge them.
// ===========================================================================

Submission sub(std::string id, std::string species, std::string observer, std::string month,
               bool photograph, bool sound, bool in_season, std::int64_t observers,
               std::vector<std::string> confusion, std::vector<std::string> ruled_out,
               bool late = false) {
    Submission s;
    s.record_id = std::move(id);
    s.species = std::move(species);
    s.observer = std::move(observer);
    s.month = std::move(month);
    s.photograph = photograph;
    s.sound = sound;
    s.in_season = in_season;
    s.observers = observers;
    s.confusion = std::move(confusion);
    s.ruled_out = std::move(ruled_out);
    s.after_closing_date = late;
    return s;
}

std::vector<Submission> submissions_for(std::int64_t year) {
    if (year == 1979) {
        return {
            sub("1979-004", "Rustic Bunting", "E. Wray", "Oct", true, false, true, 3,
                {"Little Bunting"}, {"Little Bunting"}),
            sub("1979-011", "Pallid Harrier", "T. Bewick", "May", false, false, true, 1,
                {"Montagu's Harrier", "Hen Harrier"}, {"Montagu's Harrier"}),
            sub("1979-017", "Little Bunting", "R. Pyke", "Nov", false, false, true, 2,
                {"Reed Bunting", "Rustic Bunting"}, {"Reed Bunting", "Rustic Bunting"}),
            sub("1979-023", "Booted Warbler", "M. Kerr", "Sep", false, true, true, 1,
                {"Sykes's Warbler"}, {}),
            sub("1979-044", "Greenish Warbler", "A. Quill", "Sep", false, false, true, 2,
                {"Arctic Warbler", "Chiffchaff"}, {"Chiffchaff"}, /*late=*/true),
        };
    }
    if (year == 1980) {
        return {
            // A second Little Bunting, by a different observer. Whether this is
            // a first county record is not a question about this bird.
            sub("1980-006", "Little Bunting", "J. Stannard", "Oct", true, false, true, 2,
                {"Reed Bunting", "Rustic Bunting"}, {"Reed Bunting", "Rustic Bunting"}),
            // The 1979 Pallid Harrier again, resubmitted with the photograph
            // the observer found on a contact sheet over the winter.
            sub("1980-012", "Pallid Harrier", "T. Bewick", "May", true, false, true, 1,
                {"Montagu's Harrier", "Hen Harrier"}, {"Montagu's Harrier", "Hen Harrier"}),
            // A description-only Rustic Bunting. The county has had one, which
            // changes what one member of this committee requires of it.
            sub("1980-022", "Rustic Bunting", "H. Ives", "Nov", false, false, true, 1,
                {"Little Bunting", "Reed Bunting"}, {"Reed Bunting"}),
        };
    }
    return {};
}

// ===========================================================================
// The tap: an independent witness
// ===========================================================================

// The assessors are shared libraries. The meeting CANNOT hold a typed pointer
// into one and read its tallies the way it can with the recorder and the
// secretary — so the honest independent witness for what the members did is
// not their state at all, it is what crossed the bus.
struct Tap {
    std::int64_t votes_delivered = 0;
    std::int64_t circulations_refused = 0;
    std::int64_t answers_refused = 0;
    std::vector<std::string> refusals;
};

// ===========================================================================
// The meeting
// ===========================================================================

struct Options {
    std::string archive;
    std::int64_t year = 0;
    bool found = false;
    int seats = kSeats;
    bool controls = false;
    std::vector<std::string> assessors;
};

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cout << "  CHECK FAILED   " << what << "\n";
        ++failures;
    }
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--archive" && i + 1 < argc) {
            opt.archive = argv[++i];
        } else if (a == "--year" && i + 1 < argc) {
            opt.year = std::stoll(argv[++i]);
        } else if (a == "--found") {
            opt.found = true;
        } else if (a == "--controls") {
            opt.controls = true;
        } else if (a == "--seats" && i + 1 < argc) {
            opt.seats = std::stoi(argv[++i]);
        } else {
            opt.assessors.push_back(a);
        }
    }
    if (opt.archive.empty() || opt.year == 0 || opt.assessors.size() < 1) {
        std::cerr << "usage: sitting --archive <path> --year <n> [--found] [--seats n] "
                     "[--controls] <assessor .so>...\n";
        return 2;
    }

    std::cout << "\n  THE " << kCounty << " RECORDS COMMITTEE\n";
    std::cout << "  the " << opt.year << " sitting\n\n";

    // ---- the list, before anything else ------------------------------------
    //
    // A committee that cannot read its own list does not sit. It does not sit
    // on an empty one, it does not sit on somebody else's, and it does not sit
    // on half of one.
    loom::Switchboard bus;
    loom::Kernel kernel(bus);

    auto recorder = std::make_unique<Archive>(opt.archive, kCounty, opt.year);
    const Opened opened = recorder->open(opt.found);
    if (opened != Opened::Read && opened != Opened::Founded) {
        std::cout << "  THE COMMITTEE CANNOT SIT: " << why(opened) << "\n";
        std::cout << "  nothing was written.\n\n";
        return 2;
    }
    std::cout << "  the county list  " << (opened == Opened::Founded ? "founded (this county has "
                                                                      "kept no records before)"
                                                                    : "read")
              << "\n";
    std::cout << "  species on it    " << recorder->list().size() << "\n";
    std::cout << "  held over to us  " << recorder->held_over().size() << "\n\n";

    // ---- the room ----------------------------------------------------------

    Archive* archive = recorder.get();
    // What this sitting inherited, captured before it adds anything of its own.
    // Asking the recorder afterwards would only tell us what we just told it.
    const std::size_t inherited_determinations = recorder->determinations().size();
    const loom::WeaveId archive_id =
        bus.register_weave(std::move(recorder),
                           loom::Grant{}.allow_to_any("OnTheList", 1).allow_to_any("Minuted", 1),
                           kOfficeArchive);
    archive->zen_set_self(archive_id);

    loom::Grant sec;
    for (int seat = 1; seat <= kSeats; ++seat) {
        sec.allow_to_role("Circulate", 1, seat_office(seat));
    }
    sec.allow_to_role("RecordDetermination", 1, kOfficeArchive);
    Secretary* secretary = mount_office<Secretary>(bus, sec, kOfficeSecretary);

    // Each member is a separately built artifact, because a committee gets a
    // member's verdict and never their reasoning. Only the member who consults
    // the recorder is granted the reach to.
    const char* names[kSeats] = {"aldis", "brant", "corve", "denny", "elsdon"};
    const int seated = std::min<int>(opt.seats, static_cast<int>(opt.assessors.size()));
    for (int seat = 1; seat <= seated; ++seat) {
        loom::Grant g = loom::Grant{}.allow_to_any("Vote", 1);
        if (seat == 3) {
            g.allow_to_role("IsItOnTheList", 1, kOfficeArchive);
        }
        const loom::LoadResult lr = kernel.load(names[seat - 1], opt.assessors[seat - 1],
                                                seat_office(seat), g);
        if (!lr.ok) {
            std::cerr << "  could not seat " << names[seat - 1] << ": " << lr.error << "\n";
            return 2;
        }
    }
    std::cout << "  seats filled     " << seated << " of " << kSeats << "\n";
    if (seated < kSeats) {
        for (int seat = seated + 1; seat <= kSeats; ++seat) {
            std::cout << "  " << seat_office(seat) << " is vacant\n";
        }
    }
    std::cout << "\n";

    Tap tap;
    bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered && e.schema_name == "Vote") {
            ++tap.votes_delivered;
        }
        if (e.kind == loom::EventKind::Refused) {
            std::string line = std::string(loom::name_of(e.refusal.reason)) + " on " +
                               e.schema_name + "  (from weave " +
                               std::to_string(e.sender.value) + ")";
            const std::string detail = e.refusal.message();
            if (!detail.empty()) {
                line += "  " + detail;
            }
            tap.refusals.push_back(line);
            if (e.schema_name == "Circulate") {
                ++tap.circulations_refused;
            }
            if (e.schema_name == "Vote") {
                ++tap.answers_refused;
            }
        }
    });

    // ---- the agenda --------------------------------------------------------

    std::vector<Submission> agenda = archive->held_over();
    for (const Submission& s : submissions_for(opt.year)) {
        agenda.push_back(s);
    }
    for (const Submission& s : agenda) {
        bus.send(bus.role_holder(kOfficeSecretary), loom::Message(loom::to_value(PutOnTheAgenda{s})));
    }
    bus.pump();
    std::cout << "  on the agenda    " << agenda.size();
    if (!archive->held_over().empty()) {
        std::cout << " (" << archive->held_over().size() << " carried over from an earlier sitting)";
    }
    std::cout << "\n\n";

    const loom::WeaveId sec_id = bus.role_holder(kOfficeSecretary);

    // ---- the first circulation ---------------------------------------------

    bus.send(sec_id, loom::Message(loom::to_value(SendOutTheCirculation{1})));
    bus.pump();
    bus.send(sec_id, loom::Message(loom::to_value(CloseTheCirculation{1})));
    bus.pump();

    // ---- two labelled controls, and one deliberate forgery -----------------

    std::int64_t counted_before_forged_ballot = 0;
    std::int64_t counted_after_forged_ballot = 0;
    if (opt.controls) {
        std::cout << "  --  CONTROL: the house casts a vote of its own\n";
        bus.send(sec_id, loom::Message(loom::to_value(Vote{true, "the chair is minded to accept"})));
        bus.pump();

        // A ballot that did not come from the secretary is self-defeating: the
        // member reads it and votes, and the vote answers whoever asked — which
        // was the house, not the secretary. The tally cannot be reached from
        // outside it, and it takes no rule in this application to say so.
        std::cout << "  --  CONTROL: the house circulates a ballot to seat one\n";
        counted_before_forged_ballot = secretary->votes_counted();
        bus.send(bus.role_holder(seat_office(1)),
                 loom::Message(loom::to_value(Circulate{
                     sub("XXXX-999", "Marmora's Warbler", "nobody", "Jun", true, true, true, 9, {},
                         {}),
                     1, {}})));
        bus.pump();
        counted_after_forged_ballot = secretary->votes_counted();

        std::cout << "  --  CONTROL: the house minutes a determination of its own\n";
        bus.send(archive_id,
                 loom::Message(loom::to_value(RecordDetermination{
                     "XXXX-999", "Marmora's Warbler", "nobody", kAccepted, 1, 5, 0, "", Submission{}})));
        bus.pump();
        std::cout << "\n";
    }

    // ---- the recirculation -------------------------------------------------

    bus.send(sec_id, loom::Message(loom::to_value(SendOutTheCirculation{2})));
    bus.pump();
    bus.send(sec_id, loom::Message(loom::to_value(CloseTheCirculation{2})));
    bus.pump();

    // ---- rise --------------------------------------------------------------

    bus.send(sec_id, loom::Message(loom::to_value(Adjourn{opt.year})));
    bus.pump();

    // ===========================================================================
    // The minutes
    // ===========================================================================

    std::cout << "  MINUTES OF THE " << opt.year << " SITTING\n\n";
    for (const Secretary::Item& item : secretary->agenda()) {
        std::cout << "  " << item.record.record_id << "  " << item.record.species << "\n";
        std::cout << "        " << item.record.observer << ", " << item.record.month;
        if (item.record.photograph) {
            std::cout << ", photograph";
        }
        if (item.record.sound) {
            std::cout << ", recording";
        }
        if (item.record.observers > 1) {
            std::cout << ", " << item.record.observers << " observers";
        }
        std::cout << "\n";
        if (item.decision == kHeldOver) {
            std::cout << "        HELD OVER -- " << item.note << "\n";
        } else {
            std::cout << "        " << item.decision << " in round " << item.decided_round << " ("
                      << item.accepts << "-" << item.rejects << ")";
            if (item.decided_round > 1) {
                std::cout << "; round one was " << item.accepts_r1 << "-"
                          << (item.returned_r1 - item.accepts_r1);
            }
            std::cout << "\n";
            if (item.first_for_county) {
                std::cout << "        *** FIRST COUNTY RECORD ***\n";
            } else if (item.decision == kAccepted) {
                std::cout << "        (not a first; the county list already had it)\n";
            }
            if (!item.resubmission_of.empty()) {
                std::cout << "        a resubmission of " << item.resubmission_of << ", which this "
                          << "committee recorded " << item.previous_decision << "\n";
            }
        }
        for (const std::string& d : item.dissent) {
            std::cout << "          " << d << "\n";
        }
        std::cout << "\n";
    }

    // ---- the members' own account of themselves ----------------------------
    //
    // The meeting cannot hold a typed pointer into a shared library the way it
    // holds one into the recorder and the secretary. What it CAN do is ask the
    // bus for each seat's declared state as native bytes and put them through
    // the ordinary gate — parse to an Unverified, which by construction is not
    // yet a value and can only tell you what it CLAIMS to be, then admit it
    // against the state schema this side compiled.
    //
    // Which makes it a genuine second witness rather than a nicer way of asking
    // the same source twice: the tap counted what crossed the bus, and this
    // counts what five separately-built artifacts each believe they did.
    std::int64_t members_ballots = 0;
    std::int64_t members_votes = 0;
    int seats_read = 0;
    for (int seat = 1; seat <= seated; ++seat) {
        const std::string bytes = bus.snapshot_bytes(bus.role_holder(seat_office(seat)));
        const loom::Unverified claim = loom::parse(bytes);
        const loom::Admission admitted = loom::admit(claim, loom::schema_of<AssessorState>());
        if (!admitted) {
            continue;
        }
        const AssessorState s = loom::from_value<AssessorState>(admitted.value());
        members_ballots += s.ballots;
        members_votes += s.accepts + s.rejects;
        ++seats_read;
    }

    const bool written = archive->write_out();

    std::cout << "  the county list now\n";
    for (const auto& [species, record] : archive->list()) {
        std::cout << "        " << species << "  (" << record << ")\n";
    }
    std::cout << "\n";
    std::cout << "  ballots issued   " << secretary->ballots_issued() << "\n";
    std::cout << "  votes counted    " << secretary->votes_counted() << "\n";
    std::cout << "  votes on the tap " << tap.votes_delivered << "\n";
    std::cout << "  the members say  " << members_ballots << " ballots read, " << members_votes
              << " votes cast (" << seats_read << " seats' own snapshots)\n";
    std::cout << "  determined       " << secretary->determined() << "\n";
    std::cout << "  held over        " << secretary->held_over() << "\n";
    std::cout << "  recorder asked   " << archive->consulted() << " times by a member\n";
    std::cout << "  unsolicited      " << secretary->unsolicited() << "\n";
    std::cout << "  unauthored       " << archive->unauthored() << "\n";
    if (!tap.refusals.empty()) {
        std::cout << "  bus refusals     " << tap.refusals.size() << "\n";
        for (const std::string& r : tap.refusals) {
            std::cout << "        " << r << "\n";
        }
    }
    std::cout << "\n";

    // ===========================================================================
    // What this sitting is allowed to claim
    // ===========================================================================

    check(written, "the recorder wrote the list out");
    check(secretary->votes_counted() == tap.votes_delivered - secretary->unsolicited(),
          "every vote the secretary counted is a vote the tap saw");

    // Three accounts of the same afternoon, from three places: the secretary's
    // ballot book, the tap, and the five members' own declared state. If the
    // secretary were counting something that did not happen, only one of the
    // three would say so.
    const std::int64_t solicited_by_the_house = opt.controls ? 1 : 0;
    check(seats_read == seated,
          "every seated member's own state came back through the gate as its declared shape");
    check(members_ballots == members_votes, "no member read a ballot it did not vote on");
    check(members_votes == secretary->votes_counted() + solicited_by_the_house,
          "the members' own count of the votes they cast agrees with the secretary's, and the "
          "only difference is the one the house solicited for itself");
    check(secretary->determined() + secretary->held_over() ==
              static_cast<std::int64_t>(agenda.size()),
          "every record on the agenda was either determined or held over");
    check(archive->minuted() == static_cast<std::int64_t>(agenda.size()),
          "the recorder minuted every record on the agenda");

    const auto find = [&](const std::string& id) -> const Secretary::Item* {
        for (const Secretary::Item& item : secretary->agenda()) {
            if (item.record.record_id == id) {
                return &item;
            }
        }
        return nullptr;
    };
    const auto decided = [&](const std::string& id, const char* decision, std::int64_t round,
                             std::int64_t accepts, std::int64_t rejects) {
        const Secretary::Item* item = find(id);
        check(item != nullptr, id + " is on the agenda");
        if (item == nullptr) {
            return;
        }
        check(item->decision == decision, id + " was " + decision);
        check(item->decided_round == round, id + " was decided in round " + std::to_string(round));
        check(item->accepts == accepts && item->rejects == rejects,
              id + " was " + std::to_string(accepts) + "-" + std::to_string(rejects));
    };

    if (opt.controls) {
        check(secretary->unsolicited() == 1,
              "the house's own vote answered no ballot and was not counted");
        check(archive->unauthored() == 1,
              "the house's own determination was not spoken as the secretary and was not written");
        check(archive->list().find("Marmora's Warbler") == archive->list().end(),
              "Marmora's Warbler did not reach the county list");
        check(counted_after_forged_ballot == counted_before_forged_ballot,
              "the house's forged ballot produced no vote the secretary could count");
        // The same weave, in this same run, sent this same shape as a vote six
        // times under this same grant, and every one of them was delivered. So
        // the refusal the tap reports for the forged ballot's answer cannot be
        // about the grant, whatever it says.
        check(tap.answers_refused == 1,
              "seat one's answer to the house was refused, and it was the only refusal");
    }

    if (opt.seats < kSeats) {
        // Below quorum nothing may be decided, however clear the record looks.
        check(secretary->determined() == 0, "an inquorate committee determined nothing");
        check(secretary->held_over() == static_cast<std::int64_t>(agenda.size()),
              "an inquorate committee held everything over");
        check(archive->list().empty(), "an inquorate committee added nothing to the county list");
        // THE SEAM, IN THIS DOMAIN. Every ballot that produced no vote was
        // refused on the bus and the secretary was told about none of them. It
        // learns of the vacancies only by counting what came back, which is why
        // the circulation closes on a date.
        check(tap.circulations_refused == secretary->ballots_issued() - secretary->votes_counted(),
              "every ballot to an empty seat was refused on the bus, and the secretary was told "
              "of none of them");
    } else if (opt.year == 1979) {
        decided("1979-004", kAccepted, 1, 5, 0);
        decided("1979-011", kNotAccepted, 1, 0, 5);
        decided("1979-017", kAccepted, 2, 4, 1);
        decided("1979-023", kNotAccepted, 2, 3, 2);

        const Secretary::Item* late = find("1979-044");
        check(late != nullptr && late->decision == kHeldOver,
              "1979-044 arrived after the closing date and was held over");

        check(find("1979-004")->first_for_county, "1979-004 is a first county record");
        check(find("1979-017")->first_for_county, "1979-017 is a first county record");
        check(archive->list().size() == 2, "the county list gained exactly two species");
        check(archive->held_over().size() == 1, "one record waits for the next sitting");
    } else if (opt.year == 1980) {
        const bool from_1979 = inherited_determinations > 0;
        decided("1980-006", kAccepted, 1, 5, 0);
        decided("1980-012", kAccepted, 1, 5, 0);

        if (from_1979) {
            // The whole point. Everything below is a reading of a file written
            // by a process that has already exited.
            check(find("1979-044") != nullptr,
                  "the record held over in 1979 came back onto the agenda by itself");
            decided("1979-044", kNotAccepted, 2, 1, 4);
            decided("1980-022", kNotAccepted, 2, 0, 5);

            check(!find("1980-006")->first_for_county,
                  "the second Little Bunting is NOT a first county record");
            check(find("1980-012")->first_for_county,
                  "the Pallid Harrier IS a first county record -- the 1979 one was not accepted");
            check(find("1980-012")->resubmission_of == "1979-011",
                  "the Pallid Harrier was recognised as a resubmission of 1979-011");
            check(find("1980-012")->previous_decision == kNotAccepted,
                  "and the earlier determination was NOT ACCEPTED");
            check(find("1980-022")->accepts_r1 == 1,
                  "seat three accepted 1980-022 in round one, because the county has had one");
            check(find("1980-022")->accepts == 0,
                  "and changed its vote in round two, on a colleague's comment");
        } else {
            // The list was lost and re-founded. This is the false green.
            check(find("1980-006")->first_for_county,
                  "WITHOUT the 1979 file this sitting announces a FALSE first county record");
            check(find("1980-022")->accepts_r1 == 0,
                  "and seat three votes differently in round one, having no list to consult");
        }
    }

    if (failures == 0) {
        std::cout << "  THE SITTING ROSE AT " << opt.year << ". Minutes agreed.\n\n";
        return 0;
    }
    std::cout << "  " << failures << " CHECK(S) FAILED\n\n";
    return 1;
}
