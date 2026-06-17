// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Smoke test: this target links only SHiPFieldService (no Geant4). If the
// Core library accidentally exposes a Geant4 dependency through its public
// interface, the link step here fails.

#include "FieldService/CovfieFieldSource.h"
#include "FieldService/IFieldSource.h"

int main() {
    // Touching the public types is sufficient — we don't need to load a map.
    ship::CovfieFieldSource src({});
    return src.regions().empty() ? 0 : 1;
}
