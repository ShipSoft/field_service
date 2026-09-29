// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Writes the maps from reference_maps.h to the given path. Used once per
// format version to create the frozen tests/data/field_map_v<major>.<minor>.root;
// never regenerate an existing reference file.

#include "FieldService/FieldMapIO.h"
#include "reference_maps.h"

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <out.root>\n";
        return 1;
    }
    auto const maps = ship::test::referenceMaps();
    ship::writeFieldMaps(argv[1], maps);
    return 0;
}
