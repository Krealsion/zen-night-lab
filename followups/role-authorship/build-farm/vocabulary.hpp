#ifndef FOLLOWUP_FARM_VOCABULARY_HPP
#define FOLLOWUP_FARM_VOCABULARY_HPP

// The build farm, replayed for its two sharpest forgeries:
//
//     JobDone     directed/role-addressed office truth — a fabricated success
//                 put a build that never ran into the books;
//     WorkerOpen  PUBLISHED office truth — in the farm an announcement is
//                 evidence, and a forged one destroyed healthy work.
//
// The marathon proved a publication unattestable by construction (the pull
// workaround has no answer for observers). This replay covers BOTH shapes with
// one fact: the office authored it, or it did not.

#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace farm {

/// Directed office truth, addressed to the dispatcher OFFICE (two roles, two
/// facts: authored as worker.a, delivered to whoever holds farm.dispatcher).
struct JobDone {
    std::int64_t job = 0;
    std::string by;
    ZEN_SHAPE(JobDone, 1, ZEN_FIELD(job), ZEN_FIELD(by));
};

/// Published office truth: the announcement that is evidence.
struct WorkerOpen {
    std::string worker;
    ZEN_SHAPE(WorkerOpen, 1, ZEN_FIELD(worker));
};

/// Drive the worker. `personal` asks for the same two shapes in the personal
/// capacity — the forgery a rogue can also produce.
struct RunJob {
    std::int64_t job = 0;
    bool personal = false;
    ZEN_SHAPE(RunJob, 1, ZEN_FIELD(job), ZEN_FIELD(personal));
};

} // namespace farm

#endif // FOLLOWUP_FARM_VOCABULARY_HPP
