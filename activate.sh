# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Pixi activation script for field_service.
# Sourced automatically by `pixi run` / `pixi shell`.

export SHIPFIELD_ROOT="${SHIPFIELD_ROOT:-$CONDA_PREFIX}"

# Allow running the freshly-built shared library before `pixi run install`.
export LD_LIBRARY_PATH="$PIXI_PROJECT_ROOT/build${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
