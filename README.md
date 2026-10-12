<!--
SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# SHiP Field Service

[![Pixi Build](https://github.com/ShipSoft/field_service/actions/workflows/pixi-build.yml/badge.svg)](https://github.com/ShipSoft/field_service/actions/workflows/pixi-build.yml)
[![REUSE status](https://api.reuse.software/badge/github.com/ShipSoft/field_service)](https://api.reuse.software/info/github.com/ShipSoft/field_service)

Framework-agnostic C++20 library exposing the SHiP magnetic field maps to
simulation (aegir) and reconstruction.

Maps are stored in the [SHiP field-map format](docs/field_map_format.md), a
versioned ROOT RNTuple layout. The library evaluates them with
[covfie](https://github.com/acts-project/covfie) (trilinear interpolation on a
regular grid).

The library has four parts:

- The core library (`SHiPFieldService`) builds evaluators from in-memory maps. It
  depends on neither ROOT nor Geant4, so reconstruction can use it without
  pulling either in.
- The MapIO component (`SHiPFieldService::MapIO`, `BUILD_MAPIO`) reads and writes map
  files and needs ROOT.
- The G4Adapter component (`BUILD_G4_ADAPTER`) is a small Geant4 adapter,
  `G4MagFieldAdapter`.
- The ActsAdapter component (`BUILD_ACTS_ADAPTER`) is the ACTS counterpart,
  `ActsFieldAdapter`, an `Acts::MagneticFieldProvider`.

## Documentation

An [automatic class reference](https://shipsoft.github.io/field_service/) is built using Doxygen from comments in the C++ code.

## Layout

- `docs/field_map_format.md`: specification of the field-map file format.
- `include/FieldService/IFieldSource.h`: interface (point-query evaluator,
  list of named regions tagged by host-geometry volume name).
- `include/FieldService/FieldMap.h`: in-memory map (grid, symmetry, values,
  provenance), `makeFieldEvaluator`, and the `translated` and `zeroOutside`
  wrappers that place a map and set its field outside the grid. Core, no ROOT.
- `include/FieldService/FieldMapIO.h`: read and write map files, and
  `FieldMapSource`, which loads one map per magnet. MapIO component.
- `include/FieldService/CovfieFieldSource.h`: deprecated source reading covfie
  `.cvf` files, kept for one release while consumers move to
  `FieldMapSource`.
- `include/FieldService/G4MagFieldAdapter.h`: Geant4 adapter, built when
  `BUILD_G4_ADAPTER=ON`.
- `include/FieldService/ActsFieldAdapter.h`: ACTS adapter, built when
  `BUILD_ACTS_ADAPTER=ON`.
- `tools/` (built with MapIO):
  - `fairship_to_fieldmap` converts FairShip's legacy ROOT field-map format;
    use `--symmetry quadrant_dipole` for quadrant-symmetric maps.
  - `fieldmap_dump` prints map metadata, or evaluates a map at points read
    from stdin.
  - `generate_constant_fieldmap` writes a uniform map for closure tests.
  - `plot_field_map` plots a map as ROOT histograms and PDFs (B_y vs z plus
    xz/yz/xy planes).

## Install with pixi

The supported way to set up a build environment is [pixi](https://pixi.sh).
`pixi.toml` pins the toolchain and all required packages
(`mp-units`, `geant4`, `acts-core`, `root`, `catch2`, `cmake`, `ninja`,
`cxx-compiler`).

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

The tests need an installed Catch2 3, which CMake locates with `find_package`.
If it is not under a default search path, add its install prefix to
`CMAKE_PREFIX_PATH`, either as a cache variable or as an environment variable.
Separate several prefixes with `;`.

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/catch2
```

To configure without the tests, and so without Catch2, pass
`-DBUILD_TESTING=OFF`.

## Develop

Install the pre-commit hooks (covers `clang-format`, `gersemi`,
`reuse`, `codespell`, conventional-commit validation, and CITATION.cff
checks):

```bash
pixi run -e dev pre-commit install --hook-type pre-commit --hook-type commit-msg
```

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the full workflow.

## Use from aegir

Aegir consumes this via `find_package(SHiPFieldService REQUIRED COMPONENTS
G4Adapter MapIO)`. The aegir provider plugin `field_map_provider` constructs a
`ship::FieldMapSource` from jsonnet config and publishes it as a phlex Job
product; the aegir Geant4 module installs a per-magnet `G4FieldManager` on each
matching logical volume via the adapter.

## Use from Trout

Reconstruction has no volumes to attach regions to, so it places a single map
itself and hands ACTS one global field:

```cpp
auto const map = ship::readFieldMap(ship::resolveFieldMapPath(file), name);
auto field = std::make_shared<ship::ActsFieldAdapter>(ship::translated(
    ship::zeroOutside(ship::makeFieldEvaluator(map), ship::extent(map)), translation));
```

with `find_package(SHiPFieldService REQUIRED COMPONENTS MapIO ActsAdapter)`.
