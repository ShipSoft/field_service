// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "FieldService/IFieldSource.h"

#include <memory>
#include <string>
#include <vector>

namespace ship {

/// Build an `IFieldEvaluator` backed by a covfie `.cvf` file (CPU backend,
/// trilinear interpolation, clamped at the boundary). Bare filenames are
/// resolved against `$SHIPFIELD_ROOT/share/field/`.
///
/// \deprecated `.cvf` is covfie's internal serialisation and changes with
/// covfie versions. Store maps in the SHiP field-map format
/// (docs/field_map_format.md) and load them with `FieldMapSource` from the
/// MapIO component instead. `.cvf` support will be removed in a later release.
[[nodiscard]] std::shared_ptr<IFieldEvaluator> loadCovfieField(std::string const& cvf_path);

/// Concrete `IFieldSource` aggregating one covfie-backed evaluator per magnet.
///
/// \deprecated Reads `.cvf` files; see `loadCovfieField`.
class CovfieFieldSource final : public IFieldSource {
   public:
    struct MagnetConfig {
        std::string name;
        std::string volume_pattern;
        std::string cvf_file;
    };

    explicit CovfieFieldSource(std::vector<MagnetConfig> magnets);
    ~CovfieFieldSource() override = default;

    CovfieFieldSource(CovfieFieldSource const&) = delete;
    CovfieFieldSource& operator=(CovfieFieldSource const&) = delete;
    CovfieFieldSource(CovfieFieldSource&&) = default;
    CovfieFieldSource& operator=(CovfieFieldSource&&) = default;

    [[nodiscard]] std::vector<FieldRegion> const& regions() const override { return regions_; }

   private:
    std::vector<FieldRegion> regions_;
};

}  // namespace ship
