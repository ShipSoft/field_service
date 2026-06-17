<!--
SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# SHiP Field Service

[![Pixi Build](https://github.com/ShipSoft/field_service/actions/workflows/pixi-build.yml/badge.svg)](https://github.com/ShipSoft/field_service/actions/workflows/pixi-build.yml)
[![REUSE status](https://api.reuse.software/badge/github.com/ShipSoft/field_service)](https://api.reuse.software/info/github.com/ShipSoft/field_service)

Framework-agnostic C++20 library exposing the SHiP magnetic field maps to
simulation (aegir) and reconstruction.

The library wraps [covfie](https://github.com/acts-project/covfie) for storage
and interpolation of regular-grid field maps. A small optional Geant4 adapter
(`G4MagFieldAdapter`) is built when `BUILD_G4_ADAPTER` is enabled; the core
library has no Geant4 dependency, so reconstruction can consume it without
pulling Geant4 in.

## Layout

- `include/FieldService/IFieldSource.h` — interface (point-query evaluator,
  list of named regions tagged by host-geometry volume name).
- `include/FieldService/CovfieFieldSource.h` + `src/CovfieFieldSource.cpp` —
  concrete source that loads one `.cvf` file per magnet via covfie.
- `include/FieldService/G4MagFieldAdapter.h` — Geant4 adapter, built when
  `BUILD_G4_ADAPTER=ON`.
- `tools/fairship_to_cvf` — convert FairShip's legacy ROOT field-map format
  to covfie `.cvf`. Built when ROOT is available.
- `tools/cvf_to_text` — dump a `.cvf` to whitespace-separated text for
  inspection and closure tests.
- `tools/generate_constant_cvf` — synthetic uniform field-map generator for
  closure tests.

## Install with pixi

The supported way to set up a build environment is [pixi](https://pixi.sh).
`pixi.toml` pins the toolchain and all required packages
(`mp-units`, `geant4`, `root`, `cmake`, `ninja`, `cxx-compiler`).

```bash
git clone https://github.com/ShipSoft/field_service
cd field_service
pixi install
pixi run test
```

Available tasks: `configure`, `build`, `install`, `test`, `clean`.
`pixi run install` deploys the library, headers and CMake package files under
the pixi environment prefix so downstream consumers can `find_package`
`SHiPFieldService`.

Map files are resolved relative to `$SHIPFIELD_ROOT/share/field/` when a bare
filename is passed; the pixi activation script sets `SHIPFIELD_ROOT` to the
pixi environment prefix by default.

## Build directly with CMake

If you prefer to bring your own dependencies:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Develop

Install the pre-commit hooks (covers `clang-format`, `cpplint`, `gersemi`,
`reuse`, `codespell`, conventional-commit validation, and CITATION.cff
checks):

```bash
pixi run -e dev pre-commit install --hook-type pre-commit --hook-type commit-msg
```

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the full workflow.

## Use from aegir

Aegir consumes this via `find_package(SHiPFieldService REQUIRED COMPONENTS Core
G4Adapter)`. The aegir provider plugin `field_covfie_provider` constructs a
`ship::CovfieFieldSource` from jsonnet config and publishes it as a phlex Job
product; the aegir Geant4 module installs a per-magnet `G4FieldManager` on each
matching logical volume via the adapter.
