// SPDX-FileCopyrightText: CERN for the benefit of the SHiP Collaboration
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Forwarding header for the pre-0.4.0 layout, removed in 0.5.0.
// FieldService/ActsFieldAdapter.h is now SHiP/FieldService/ActsFieldAdapter.h, and the types it
// declares moved from the `ship` namespace to `SHiP`.
#pragma once

#include "SHiP/FieldService/ActsFieldAdapter.h"

#ifndef SHIP_NO_DEPRECATED_NAMESPACE_ALIAS
namespace ship = SHiP;
#endif
