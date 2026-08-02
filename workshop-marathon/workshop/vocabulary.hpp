#ifndef WORKSHOP_VOCABULARY_HPP
#define WORKSHOP_VOCABULARY_HPP

// The Workshop's message vocabulary — the local truths this experiment mints.
//
// Two tiers, deliberately separate:
//
//   DESCRIPTION — what a creation SAYS it is. `ProjectSpec` is admitted from a
//   project file through the one gate (loom::compat::parse -> admit), so a
//   project file is a Value like everything else: a malformed project is a
//   GateRefused with a field path, never a parser stack trace. Everything in
//   this tier is DECLARED truth: the file said so, nobody verified it.
//
//   RUNTIME FACT — what the Workshop OBSERVED happen. `PartUp` / `PartFailed`
//   are published by the operator from inside the handler of the Weave
//   Manager's own answer (the answer, not the wish, is what publishes).
//   The registry accumulates them and answers `QueryRunning` asks.
//
// Nothing here is proposed for Loom or Zengine. Workshop-local until toys vote
// otherwise (see reports/PRESSURE.md).

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace workshop {

// ---- description tier (DECLARED: admitted from project.json) ---------------

/// One loadable part of a creation. `stem` names the artifact
/// (`<stem>.so`, resolved by the launcher's search path); `role` may be empty.
struct PartSpec {
    std::string name;
    std::string stem;
    std::string role;
    ZEN_SHAPE(PartSpec, 1, ZEN_FIELD(name), ZEN_FIELD(stem), ZEN_FIELD(role));
};

/// A creation, as its project file describes it. `needs` lists the service
/// roles the creation expects to exist (e.g. "zengine.timer", "zengine.skin");
/// the Workshop maps each need to a service artifact it trusts. A need the
/// Workshop cannot supply is an honest launch failure, not a silent shrug.
struct ProjectSpec {
    std::string name;
    std::string description;
    std::vector<PartSpec> parts;
    std::vector<std::string> needs;
    ZEN_SHAPE(ProjectSpec, 1, ZEN_FIELD(name), ZEN_FIELD(description), ZEN_FIELD(parts),
              ZEN_FIELD(needs));
};

// ---- runtime-fact tier (published from the Manager's answers) --------------

/// A part (or service) the Manager confirmed loaded. Published by the
/// operator from the answer handler; DERIVED truth resting on the Manager's
/// own answer, one step from FACT.
struct PartUp {
    std::string project;
    std::string part;
    std::string stem;
    std::string role;
    ZEN_SHAPE(PartUp, 1, ZEN_FIELD(project), ZEN_FIELD(part), ZEN_FIELD(stem), ZEN_FIELD(role));
};

/// A part the Manager refused, with the substrate's own words. The refusal
/// reason is carried verbatim — re-deriving refusal-ness from prose downstream
/// would be a second, weaker authority.
struct PartFailed {
    std::string project;
    std::string part;
    std::string stem;
    std::string reason;
    ZEN_SHAPE(PartFailed, 1, ZEN_FIELD(project), ZEN_FIELD(part), ZEN_FIELD(stem),
              ZEN_FIELD(reason));
};

// ---- asks ------------------------------------------------------------------

/// Ask the registry what is running. Answer: `RunningReport`.
struct QueryRunning {
    ZEN_SHAPE(QueryRunning, 1);
};

/// The registry's answer: everything it witnessed come up or fail, in
/// witness order. The registry counts what it HEARD — a registry loaded late
/// honestly knows less than the world does (the score-weave stance).
struct RunningReport {
    std::vector<PartUp> up;
    std::vector<PartFailed> failed;
    ZEN_SHAPE(RunningReport, 1, ZEN_FIELD(up), ZEN_FIELD(failed));
};

/// A wish that the current run end, spoken as ordinary intent. The operator
/// honors it; anyone granted the shape may wish (the governor does; a toy
/// could). A wish, not a command — the operator is the one with the lever.
struct StopWish {
    std::string reason;
    ZEN_SHAPE(StopWish, 1, ZEN_FIELD(reason));
};

// ---- the addresses ---------------------------------------------------------

/// The registry's role: ask the OFFICE what is running, so the question
/// survives the registry weave being replaced.
inline constexpr const char* kRegistryRole = "workshop.registry";

} // namespace workshop

#endif // WORKSHOP_VOCABULARY_HPP
