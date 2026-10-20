<!--
SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Placement manifest

The [field-map format](field_map_format.md) stores each map in its own frame
and leaves placement to the consumer. A placement manifest records, in one
JSON file, which map belongs to which magnet and where the map sits in the
geometry. `SHiP::readFieldMapPlacements` (MapIO component) reads it and returns
the magnet list `SHiP::FieldMapSource` takes.

The `ship-field-maps` package
([ShipSoft/field_maps](https://github.com/ShipSoft/field_maps)) installs the
manifest for the SHiP maps as `share/field/placements.json`.

## Example

```json
{
  "format_version": 1,
  "geometry": {"package": "shipgeometry", "version": "0.2.1"},
  "maps": {
    "spectrometer_V21_2455": {"file": "spectrometer_V21_2455.root"}
  },
  "magnets": [
    {
      "name": "Spectrometer",
      "volume_pattern": "/SHiP/magnet",
      "map": "spectrometer_V21_2455",
      "translation": [0, 0, 89570]
    }
  ]
}
```

## Fields

| Key | Type | Required | Meaning |
|---|---|---|---|
| `format_version` | integer | yes | Manifest version, currently 1. |
| `geometry.package`, `geometry.version` | string | no | Geometry the translations were taken from. If `geometry` is present, both are required. |
| `maps` | object | yes | Map name → map entry. The key is the map's `name` inside its file. |
| `maps.<name>.file` | string | yes | Field-map file, relative to the manifest's directory. |
| `magnets` | array | yes, non-empty | One entry per magnet. |
| `magnets[].name` | string | yes | Magnet name, unique within the manifest. |
| `magnets[].volume_pattern` | string | yes | Host-geometry volume pattern, as in `FieldRegion`. |
| `magnets[].map` | string | yes | A key of `maps`. |
| `magnets[].translation` | 3 numbers | no | Global position (mm) of the map's origin. Defaults to `[0, 0, 0]`. |

## Reading

- `readFieldMapPlacements` resolves the manifest path like a map file: a bare
  `placements.json` is looked up under `$SHIPFIELD_ROOT/share/field/`.
- Map files are taken from the manifest's directory, so a manifest and its
  maps can be moved together.
- Readers ignore keys they don't know. Producers may add fields such as map
  provenance (`ship-field-maps` records the FairShip commit and each source
  file's sha256).

## Versioning

Adding an optional key keeps `format_version`. Anything an existing reader
would misinterpret if it ignored the key, such as a new way to place a map,
raises the version. Readers reject versions they don't know.
