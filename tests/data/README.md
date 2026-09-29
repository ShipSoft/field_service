<!--
SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Frozen field-map reference files

Each file here was written by the reader/writer of one format version
(`field_map_v<major>.<minor>.root`) and holds the maps defined in
`tests/reference_maps.h`. A `test_compat_v<major>_<minor>` test reads it with
the current code.

The rules:

- Never modify or regenerate an existing reference file. If a compat test
  fails, the change broke files people already have: fix the reader.
- When the format version changes, add a new file and a new compat test next
  to the old ones. Build the tree, then run

  ```bash
  pixi run ./build/tests/make_reference_field_map tests/data/field_map_v<major>.<minor>.root
  ```

  If the new version adds fields, extend `reference_maps.h` so the new file
  exercises them. Keep the old compat tests checking only what their version
  defines.
- The format and its versioning rules are specified in
  [`docs/field_map_format.md`](../../docs/field_map_format.md).
