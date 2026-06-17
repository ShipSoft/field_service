// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <array>
#include <memory>
#include <mp-units/systems/si.h>
#include <string>
#include <vector>

namespace ship {

/// Framework-agnostic point-query evaluator for one magnetic field region.
///
/// Implementations must be thread-safe; covfie-backed evaluators use a
/// per-worker thread_local view to keep `at()` allocation-free on the hot path.
class IFieldEvaluator {
   public:
    using pos_q = mp_units::quantity<mp_units::si::milli<mp_units::si::metre>, double>;
    using field_q = mp_units::quantity<mp_units::si::tesla, double>;

    virtual ~IFieldEvaluator() = default;

    /// Return {Bx, By, Bz} in Tesla at global position (x, y, z) in mm.
    /// Behaviour outside the mapped region is implementation-defined; the
    /// covfie-backed evaluator clamps to the boundary value.
    [[nodiscard]] virtual std::array<field_q, 3> at(pos_q x, pos_q y, pos_q z) const = 0;
};

/// One magnet's worth of field, tagged by a host-geometry volume pattern.
///
/// `volume_pattern` is a substring match against the host's volume name store
/// (Geant4 `G4LogicalVolumeStore`, or equivalent for reco). The host is
/// responsible for installing the field on each matching volume.
struct FieldRegion {
    std::string name;
    std::string volume_pattern;
    std::shared_ptr<IFieldEvaluator> field;
};

/// Source of one or more named field regions. Mirrors the shape of
/// `IGeometrySource`/`IGeometryService` in the geometry repo.
class IFieldSource {
   public:
    virtual ~IFieldSource() = default;

    /// Idempotent. Ownership of the evaluators stays with the source.
    [[nodiscard]] virtual std::vector<FieldRegion> const& regions() const = 0;
};

}  // namespace ship
