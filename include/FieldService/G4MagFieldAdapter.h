// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "FieldService/IFieldSource.h"

#include <G4MagneticField.hh>

#include <CLHEP/Units/SystemOfUnits.h>
#include <memory>
#include <mp-units/systems/si.h>
#include <utility>

namespace ship {

/// Thin `G4MagneticField` wrapping an `IFieldEvaluator`. Geant4 owns the
/// `G4FieldManager`/`ChordFinder`; the evaluator carries the per-thread cache.
class G4MagFieldAdapter final : public G4MagneticField {
   public:
    explicit G4MagFieldAdapter(std::shared_ptr<IFieldEvaluator> eval) : eval_{std::move(eval)} {}

    void GetFieldValue(G4double const point[4], G4double* bField) const override {
        // Geant4 internal length unit is CLHEP::mm; convert to plain mm for the evaluator,
        // which works in mp-units mm-quantity. Fully qualify the mp-units names because
        // CLHEP also defines `tesla` and `metre` as globals via Geant4 headers.
        namespace si = mp_units::si;
        auto const x = (point[0] / CLHEP::mm) * si::milli<si::metre>;
        auto const y = (point[1] / CLHEP::mm) * si::milli<si::metre>;
        auto const z = (point[2] / CLHEP::mm) * si::milli<si::metre>;
        auto const B = eval_->at(x, y, z);
        bField[0] = B[0].numerical_value_in(si::tesla) * CLHEP::tesla;
        bField[1] = B[1].numerical_value_in(si::tesla) * CLHEP::tesla;
        bField[2] = B[2].numerical_value_in(si::tesla) * CLHEP::tesla;
    }

   private:
    std::shared_ptr<IFieldEvaluator> eval_;
};

}  // namespace ship
