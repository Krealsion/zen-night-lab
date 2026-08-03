#ifndef WORKSHOP_BUNDLE_HPP
#define WORKSHOP_BUNDLE_HPP

// The smallest honest sharing story (Gate 7): a bundle is a directory —
//
//   <name>-bundle/
//       BUNDLE.json      the gated BundleInfo (provenance, truth-labelled)
//       project.json     the spec, CANONICALIZED through the gate (what was
//                        admitted, not whatever bytes the author had)
//       artifacts/*.so   the parts, fingerprinted
//
// Import verifies: both files admit through the gate, and every artifact's
// recomputed fingerprint matches the declared one — a tampered artifact
// refuses by NAME. What import does NOT do: believe `author`, believe
// `exported_from`, or grant anything (an imported toy runs with exactly the
// grants the Workshop gives any toy — importing confers no reach).
//
// v1 ships ARTIFACTS, not source: a consumer gets the schematic/knob height
// but not the code height. Recorded as the bundle's honest boundary.

#include "vocabulary.hpp"

#include <zen/serialize.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace workshop {

namespace bundlefs = std::filesystem;

inline std::string fnv64_hex(const std::string& bytes) {
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : bytes) {
        h ^= c;
        h *= 1099511628211ull;
    }
    char buf[20];
    std::snprintf(buf, sizeof buf, "0x%016llx", static_cast<unsigned long long>(h));
    return buf;
}

inline std::optional<std::string> slurp(const bundlefs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

struct BundleOutcome {
    bool ok = false;
    std::string error;
    std::string dir; ///< the bundle dir (export) or the toy dir (import)
    BundleInfo info;
};

/// Resolve a stem to an artifact path — supplied by the caller (the shell's
/// search path, or a test's explicit map).
using StemResolver = std::function<std::optional<std::string>(const std::string& stem)>;

inline BundleOutcome export_bundle(const ProjectSpec& spec, const std::string& spec_dir,
                                   const std::string& dest_root, const std::string& author,
                                   const std::string& loom_pin, const std::string& zengine_pin,
                                   const std::string& abi, const StemResolver& resolve) {
    BundleOutcome out;
    const bundlefs::path bundle = bundlefs::path(dest_root) / (spec.name + "-bundle");
    if (bundlefs::exists(bundle)) {
        out.error = bundle.string() + " already exists";
        return out;
    }
    bundlefs::create_directories(bundle / "artifacts");

    BundleInfo info;
    info.project = spec.name;
    info.author = author; // UNVERIFIED, and the format says so
    info.exported_from = spec_dir;
    info.loom_pin = loom_pin;
    info.zengine_pin = zengine_pin;
    info.abi = abi;
    info.needs = spec.needs;

    for (const PartSpec& part : spec.parts) {
        bool already = false;
        for (const ArtifactInfo& a : info.artifacts) {
            already = already || a.stem == part.stem;
        }
        if (already) {
            continue; // one artifact may serve many parts (the pond, the sky)
        }
        auto path = resolve(part.stem);
        if (!path) {
            out.error = "artifact for stem '" + part.stem + "' not found";
            return out;
        }
        auto bytes = slurp(*path);
        if (!bytes) {
            out.error = "cannot read " + *path;
            return out;
        }
        std::ofstream copy(bundle / "artifacts" / (part.stem + ".so"), std::ios::binary);
        copy << *bytes;
        info.artifacts.push_back(ArtifactInfo{part.stem,
                                              static_cast<std::int64_t>(bytes->size()),
                                              fnv64_hex(*bytes)});
    }

    {
        std::ofstream spec_out(bundle / "project.json");
        spec_out << loom::compat::serialize(loom::to_value(spec)) << "\n";
        std::ofstream info_out(bundle / "BUNDLE.json");
        info_out << loom::compat::serialize(loom::to_value(info)) << "\n";
    }
    out.ok = true;
    out.dir = bundle.string();
    out.info = info;
    return out;
}

/// `as_name` (optional) receives the toy under a different name — the honest
/// answer to "I already have one of those", found by the cold user. The
/// RENAME IS RECORDED: the received project's name changes, so `describe`
/// shows the new name and the bundle's own `project` field (kept verbatim in
/// BUNDLE.json beside it) still shows what the sender called it.
inline BundleOutcome import_bundle(const std::string& bundle_dir,
                                   const std::string& toys_root,
                                   const std::string& as_name = "") {
    BundleOutcome out;
    const bundlefs::path bundle(bundle_dir);

    const auto info_bytes = slurp(bundle / "BUNDLE.json");
    if (!info_bytes) {
        out.error = "no BUNDLE.json in " + bundle_dir;
        return out;
    }
    loom::Admission info_adm =
        loom::admit(loom::compat::parse(*info_bytes), loom::schema_of<BundleInfo>());
    if (!info_adm.ok()) {
        out.error = "BUNDLE.json refused at the gate: " + info_adm.first_error().message();
        return out;
    }
    const BundleInfo info = loom::from_value<BundleInfo>(info_adm.value());

    const auto spec_bytes = slurp(bundle / "project.json");
    if (!spec_bytes) {
        out.error = "no project.json in " + bundle_dir;
        return out;
    }
    loom::Admission spec_adm =
        loom::admit(loom::compat::parse(*spec_bytes), loom::schema_of<ProjectSpec>());
    if (!spec_adm.ok()) {
        out.error = "project.json refused at the gate: " + spec_adm.first_error().message();
        return out;
    }
    ProjectSpec spec = loom::from_value<ProjectSpec>(spec_adm.value());
    if (!as_name.empty()) {
        spec.name = as_name;
    }

    // Verify every declared fingerprint against the bytes actually shipped.
    for (const ArtifactInfo& a : info.artifacts) {
        const auto bytes = slurp(bundle / "artifacts" / (a.stem + ".so"));
        if (!bytes) {
            out.error = "declared artifact '" + a.stem + "' is missing from the bundle";
            return out;
        }
        const std::string actual = fnv64_hex(*bytes);
        if (actual != a.fnv64 || static_cast<std::int64_t>(bytes->size()) != a.bytes) {
            out.error = "artifact '" + a.stem + "' does not match its declared fingerprint (" +
                        actual + " != " + a.fnv64 + ") - refusing the import";
            return out;
        }
    }

    const bundlefs::path toy = bundlefs::path(toys_root) / spec.name;
    if (bundlefs::exists(toy)) {
        out.error = toy.string() + " already exists - not overwriting";
        return out;
    }
    bundlefs::create_directories(toy / "artifacts");
    {
        std::ofstream spec_out(toy / "project.json");
        spec_out << loom::compat::serialize(loom::to_value(spec)) << "\n";
    }
    for (const ArtifactInfo& a : info.artifacts) {
        const auto bytes = slurp(bundle / "artifacts" / (a.stem + ".so"));
        std::ofstream copy(toy / "artifacts" / (a.stem + ".so"), std::ios::binary);
        copy << *bytes;
    }
    {
        // Keep the provenance beside the toy, verbatim — labelled data, not
        // trusted truth.
        std::ofstream info_out(toy / "BUNDLE.json");
        info_out << *info_bytes;
    }

    out.ok = true;
    out.dir = toy.string();
    out.info = info;
    return out;
}

} // namespace workshop

#endif // WORKSHOP_BUNDLE_HPP
