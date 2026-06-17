// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "FieldService/IFieldSource.h"

namespace ship {

/// Build an `IFieldEvaluator` backed by a covfie `.cvf` file (CPU backend,
/// trilinear interpolation, per-thread view). Bare filenames are resolved
/// against `$SHIPFIELD_ROOT/share/field/`.
[[nodiscard]] std::shared_ptr<IFieldEvaluator> loadCovfieField(std::string const& cvf_path);

/// Concrete `IFieldSource` aggregating one covfie-backed evaluator per magnet.
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
