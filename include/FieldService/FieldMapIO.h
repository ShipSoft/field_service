// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Reader and writer for the SHiP field-map format (docs/field_map_format.md),
// a ROOT RNTuple named "field_maps" with one entry per map. Part of the MapIO
// component (SHiPFieldService::MapIO), which depends on ROOT; the core
// library does not.

#include "FieldService/FieldMap.h"
#include "FieldService/IFieldSource.h"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ship {

/// Format version written by this library. Readers accept any minor version
/// of a major version they know.
inline constexpr std::uint16_t kFieldMapFormatMajor = 1;
inline constexpr std::uint16_t kFieldMapFormatMinor = 0;

/// Name of the RNTuple holding the maps.
inline constexpr char const* kFieldMapNTupleName = "field_maps";

struct FieldMapWriteOptions {
    /// Store field values with only this many bits (10 to 31) of each float,
    /// dropping low mantissa bits, to shrink the file. 0 keeps full float32.
    /// Readers always get float, whatever the setting.
    unsigned truncated_bits = 0;
};

/// Read every map in `path`. Throws `std::runtime_error` if the file is not a
/// field-map file, uses an unknown major version, or holds a malformed map.
[[nodiscard]] std::vector<FieldMap> readFieldMaps(std::string const& path);

/// Read the map called `name` from `path`; throws if there is none.
[[nodiscard]] FieldMap readFieldMap(std::string const& path, std::string const& name);

/// Write `maps` to a new file at `path`, replacing any existing file. Each map
/// is validated and names must be unique. Empty `provenance.producer` and
/// `provenance.created_utc` are filled in with this library and the current
/// time.
void writeFieldMaps(std::string const& path, std::span<FieldMap const> maps,
                    FieldMapWriteOptions const& options = {});

/// Evaluator for map `map` in `file`. Bare filenames are resolved as in
/// `resolveFieldMapPath`. Evaluators are cached by (file, map), so magnets
/// sharing a map hold its values in memory only once.
[[nodiscard]] std::shared_ptr<IFieldEvaluator> loadFieldMap(std::string const& file,
                                                            std::string const& map);

/// `IFieldSource` with one evaluator per magnet, read from field-map files.
class FieldMapSource final : public IFieldSource {
   public:
    struct MagnetConfig {
        std::string name;
        std::string volume_pattern;
        std::string file;
        std::string map;
        /// Global position (mm) of the map's local origin. Maps are stored in
        /// their own frame; the global field at p is the map's field at
        /// p - translation.
        std::array<double, 3> translation{};
    };

    explicit FieldMapSource(std::vector<MagnetConfig> magnets);

    [[nodiscard]] std::vector<FieldRegion> const& regions() const override { return regions_; }

   private:
    std::vector<FieldRegion> regions_;
};

}  // namespace ship
