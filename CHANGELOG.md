# Changelog

All notable changes to this project will be documented in this file.

## [0.5.0] - 2026-10-21

### Features

- Warn at compile time about the deprecated .cvf API
- *(tools)* Let fairship_to_fieldmap set the provenance source
- *(mapio)* Read magnet placements from a JSON manifest

### Bug fixes

- Return NaN for NaN positions instead of indexing the grid with NaN
- *(cmake)* Accept only the requested minor version while at 0.x
- *(tools)* Print full precision in fieldmap_dump, parse plot ranges strictly
- *(mapio)* Compare format_version without narrowing it

### Refactor

- Include the canonical SHiP/FieldService headers internally
- [**breaking**] Drop the pre-0.4.0 spellings

### Documentation

- Use the current CMake target names and SHiP namespace
- Require only a thread-safe evaluator in the G4 adapter comment
- Point to the ship-field-maps package

### Testing

- Compile and check the Geant4 adapter
- Give temporary map files unique names per run

### Miscellaneous

- Add advisory builds against ShipSoft library mains
- Build and test on linux-aarch64

### Build

- Resolve SHiP packages from ship-experiment/stable
## [0.4.0] - 2026-10-13

### Bug fixes

- Ship and use the canonical SHiP/FieldService headers

### Refactor

- [**breaking**] Move the field service into the SHiP namespace

### Build

- *(recipe)* Vendor covfie v0.16.0
- Lock shipdatamodel 0.6.0
## [0.3.0] - 2026-10-13

### Features

- Add placement and domain helpers for field evaluators
- Add an ACTS adapter component

### Bug fixes

- *(release)* Tag the release only after it lands on main

### Documentation

- Say which frame IFieldEvaluator::at takes
- Note the Catch2 requirement for direct CMake builds
- Say which components need their dependencies at configure time

### Styling

- Address clang-tidy findings in placement helpers

### Build

- Get Catch2 from conda-forge instead of FetchContent
## [0.2.0] - 2026-10-01

### Features

- *(tools)* Add plot_field_map field-map plotter
- Add local conda channel build for dev workflows
- Re-point field quantity aliases at the canonical vocabulary
- [**breaking**] Add versioned RNTuple field-map format

### Bug fixes

- *(cmake)* Make the exported mp-units prefix hint relocatable
- [**breaking**] Clamp field queries to the map boundary
- Validate field-map grid inputs in the covfie writer path
- *(release)* Roll back on any failure, fail on unbumpable versions
- Let a failed git diff fail the clang-tidy-diff task

### Documentation

- Link Doxygen API reference from README
- Correct IFieldEvaluator thread-safety documentation
- Document prek hooks in CONTRIBUTING
- Adopt AI policy
- Describe what the clang-tidy gate actually does, and fix the local check

### Performance

- Reuse the covfie view and share field maps across magnets

### Testing

- Migrate from GoogleTest to Catch2

### Miscellaneous

- Lint with prek via pixi
- *(doxygen)* Publish via native GitHub Pages deployment
- Enable Renovate via shared preset
- Remove Dependabot in favour of Renovate
- Update pixi lock file
- Enforce conventional commits in CI via commit-check
- Bump googletest to v1.17.0
- Add shared-config sync
- Remove redundant PR checklist template
- Grant config-sync job explicit permissions
- Sync shared configs
- Update pixi lock file
- Gate merges on uniform aggregator checks
- Check C++ against the Core Guidelines with clang-tidy
- Update pixi lock file

### Build

- Drop redundant Geant4 include dirs from exported adapter target
- Re-lock shipdatamodel to 0.3.0 for SHiP::SHiPUnits
- Add explicit expat dependency for Geant4
## [0.1.0] - 2026-06-18

### Features

- Initial scaffold of SHiP field service
- *(tools)* Add generate_constant_cvf for closure-test field maps

### Bug fixes

- *(cmake)* Export G4 adapter as SHiPFieldService::G4Adapter
- *(pre-commit)* Use upstream committed hook

### Documentation

- Document pixi workflow and add release-ready repo docs

### Styling

- Pre-commit fixes

### Miscellaneous

- Add Pixi build, Doxygen and lock-refresh workflows
- Add pre-commit config and repo hygiene files
- *(cmake)* Make CMAKE_CXX_STANDARD overridable from the caller
- Add release automation via git-cliff

### Build

- *(cmake)* Isolate FetchContent deps and default tests on
- *(pixi)* Add reproducible pixi environment
- *(cmake)* Bump covfie FetchContent pin to v0.15.6
- *(cliff)* Include changelog file header
