// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <CLHEP/Units/SystemOfUnits.h>
#include <G4MagneticField.hh>

#include <memory>
#include <mp-units/systems/si.h>
#include <utility>

#include "FieldService/IFieldSource.h"

namespace ship {

/// Thin `G4MagneticField` wrapping an `IFieldEvaluator`. Geant4 owns the
/// `G4FieldManager`/`ChordFinder`; the evaluator carries the per-thread cache.
class G4MagFieldAdapter final : public G4MagneticField {
   public:
    explicit G4MagFieldAdapter(std::shared_ptr<IFieldEvaluator> eval)
        : eval_{std::move(eval)} {}

    void GetFieldValue(G4double const point[4], G4double* bField) const override {
        using namespace mp_units::si;
        // Geant4 internal length unit is CLHEP::mm; convert to plain mm for the evaluator,
        // which works in mp-units mm-quantity.
        auto const x = (point[0] / CLHEP::mm) * milli<metre>;
        auto const y = (point[1] / CLHEP::mm) * milli<metre>;
        auto const z = (point[2] / CLHEP::mm) * milli<metre>;
        auto const B = eval_->at(x, y, z);
        bField[0] = B[0].numerical_value_in(tesla) * CLHEP::tesla;
        bField[1] = B[1].numerical_value_in(tesla) * CLHEP::tesla;
        bField[2] = B[2].numerical_value_in(tesla) * CLHEP::tesla;
    }

   private:
    std::shared_ptr<IFieldEvaluator> eval_;
};

}  // namespace ship
