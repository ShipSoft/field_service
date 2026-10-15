// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Compile check for the pre-0.4.0 forwarding headers and the `ship` namespace
// alias they provide. Remove together with them in 0.5.0.

#include "FieldService/CovfieFieldSource.h"
#include "FieldService/FieldMap.h"
#include "FieldService/IFieldSource.h"

#include <type_traits>

static_assert(std::is_same_v<ship::FieldMap, SHiP::FieldMap>);
static_assert(std::is_same_v<ship::IFieldEvaluator, SHiP::IFieldEvaluator>);

int main() {
    return 0;
}
