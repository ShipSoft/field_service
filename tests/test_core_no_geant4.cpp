// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Smoke test: this target links only SHiPFieldService (no Geant4, no ROOT).
// If the Core library accidentally exposes a Geant4 or ROOT dependency
// through its public interface, the link step here fails.

#include "FieldService/CovfieFieldSource.h"
#include "FieldService/FieldMap.h"
#include "FieldService/IFieldSource.h"

int main() {
    // Touching the public types is sufficient — we don't need to load a map.
    ship::CovfieFieldSource src({});
    ship::FieldMap map;
    map.name = "smoke";
    map.grid = {{0, 0, 0}, {1, 1, 1}, {2, 2, 2}};
    map.bx.assign(8, 0.0f);
    map.by.assign(8, 1.0f);
    map.bz.assign(8, 0.0f);
    auto const eval = ship::makeFieldEvaluator(map);
    return src.regions().empty() && eval ? 0 : 1;
}
