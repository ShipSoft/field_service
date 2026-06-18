# Changelog

All notable changes to this project will be documented in this file.

## [0.1.0] - 2026-06-18

### Features

- Initial scaffold of SHiP field service
- *(tools)* Add generate_constant_cvf for closure-test field maps

### Bug Fixes

- *(cmake)* Export G4 adapter as SHiPFieldService::G4Adapter
- *(pre-commit)* Use upstream committed hook

### Documentation

- Document pixi workflow and add release-ready repo docs

### Styling

- Pre-commit fixes

### Build

- *(cmake)* Isolate FetchContent deps and default tests on
- *(pixi)* Add reproducible pixi environment
- *(cmake)* Bump covfie FetchContent pin to v0.15.6
- *(cliff)* Include changelog file header

### Miscellaneous

- Add Pixi build, Doxygen and lock-refresh workflows
- Add pre-commit config and repo hygiene files
- *(cmake)* Make CMAKE_CXX_STANDARD overridable from the caller
- Add release automation via git-cliff
