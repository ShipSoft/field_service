<!--
SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Changelog

All notable changes to this project will be documented in this file.

## [unreleased]

### Features

- `IFieldSource` / `IFieldEvaluator` interfaces for point-query magnetic field access
- `CovfieFieldSource` loading `.cvf` field maps via covfie
- Optional `G4MagFieldAdapter` Geant4 integration (header-only, separate CMake component)
- `fairship_to_cvf` converter from FairShip's legacy ROOT field-map format
- `cvf_to_text` inspection tool
- `generate_constant_cvf` closure-test field-map generator
- Exported CMake package (`SHiPFieldService::Core`, `SHiPFieldService::G4Adapter`)
