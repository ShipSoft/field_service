<!--
SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Contributing to SHiP Field Service

Thank you for your interest in contributing. As part of the SHiP Collaboration, we follow a set of standards to keep the codebase clean and downstream-friendly.

## Development workflow

1. **Fork and clone** the repository.
2. **Environment**: the supported way to obtain build dependencies (Geant4, ROOT, mp-units, a recent CMake/Ninja/compiler) is [pixi](https://pixi.sh):
   ```bash
   pixi install
   pixi run test
   ```
   See `pixi.toml` for the full list of tasks (`configure`, `build`, `install`, `test`, `clean`).
3. **Pre-commit hooks**: we enforce style and licensing via `pre-commit`. Install both the pre-commit and commit-msg hooks:
   ```bash
   pixi run -e dev pre-commit install --hook-type pre-commit --hook-type commit-msg
   ```
4. **Branching**: create a feature branch for your changes.
5. **Coding standards**:
   - C++20; style enforced by `clang-format` (`.clang-format`) and `cpplint` (`CPPLINT.cfg`).
   - CMake formatting enforced by `gersemi`.
   - Every new file must carry an SPDX header (REUSE-compliant; verified by `reuse lint`).
6. **Commits**: we follow [Conventional Commits](https://www.conventionalcommits.org/), validated by [`committed`](https://github.com/crate-ci/committed). Allowed types are listed in `committed.toml` (`feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `revert`).
7. **Testing**: add tests for new behaviour. Run them with `pixi run test`.
8. **Submission**: open a pull request against `main`. CI (`Pixi Build`) must pass.

## Licensing

This project is licensed under **LGPL-3.0-or-later**. All contributions must be compatible with that licence; every file needs an SPDX identifier and copyright notice.
