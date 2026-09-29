<!--
SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# SHiP field-map format, version 1.0

This document specifies the file format SHiP software uses to store magnetic
field maps. The reference reader and writer are in `FieldService/FieldMapIO.h`
(the `SHiPFieldService::MapIO` CMake component).

## Why a SHiP format

Before this format, maps were stored in two ways, and neither holds up over
time.

- A covfie `.cvf` file is a raw dump of one particular covfie backend chain. It
  contains per-layer magic numbers and `sizeof(size_t)` fields, has no
  endianness handling, and records no units, extents, version or provenance.
  It can change with any covfie release, and you cannot inspect it without
  knowing the chain it was written with.
- FairShip's ROOT layout (a `Range` and a `Data` TTree) stores lengths in
  cm and leaves the node order implicit. Symmetry, placement and scale live in
  the code that loads the map, not in the file. The implicit ordering has
  already produced wrongly ordered spectrometer maps.

There is nothing standard to adopt instead. ACTS reads point lists from TTrees
or text, Geant4 has only example-specific text formats, and DD4hep and
EDM4hep define no field-map file at all. ATLAS and CMS use formats internal to
their own software.

## Container

A field-map file is a ROOT file holding one RNTuple named `field_maps`.
Each RNTuple entry is one map, so one file can carry several magnets.

Every field uses only fundamental types, `std::string`, `std::array` and
`std::vector`. Reading a file needs no dictionaries or SHiP libraries: plain
ROOT (≥ 6.34, the first release with the stable RNTuple on-disk format),
PyROOT and uproot all work.

## Fields

| Field | Type | Meaning |
|---|---|---|
| `format_major` | `std::uint16_t` | Format major version, `1`. |
| `format_minor` | `std::uint16_t` | Format minor version, `0`. |
| `name` | `std::string` | Map identifier. Non-empty and unique within the file. |
| `coordinate_system` | `std::string` | `"cartesian"`. `"cylindrical_rz"` is reserved for a later version. |
| `length_unit` | `std::string` | `"mm"`. |
| `field_unit` | `std::string` | `"T"`. |
| `axis_min` | `std::array<double,3>` | Position of the first node on x, y, z, in mm. |
| `axis_max` | `std::array<double,3>` | Position of the last node on x, y, z, in mm. `axis_max > axis_min`. |
| `axis_n` | `std::array<std::uint32_t,3>` | Nodes per axis, at least 2. |
| `index_order` | `std::string` | `"xyz_z_fastest"`. |
| `bx`, `by`, `bz` | `std::vector<float>` | Field components at the nodes, in T. Each has `nx·ny·nz` entries. |
| `mirror` | `std::array<bool,3>` | Whether the map is reflected across the plane where the x, y or z coordinate is 0. See [Symmetry](#symmetry). |
| `parity` | `std::array<std::int8_t,9>` | Sign of each component under each reflection. See [Symmetry](#symmetry). |
| `source` | `std::string` | Where the values came from, for example the file a map was converted from. |
| `comment` | `std::string` | Free text. |
| `producer` | `std::string` | Tool and version that wrote the entry. |
| `created_utc` | `std::string` | ISO 8601 UTC time the entry was written, e.g. `2026-09-29T12:00:00Z`. |

The string-valued fields `coordinate_system`, `length_unit`, `field_unit` and
`index_order` fix the interpretation of the data. Version 1 allows exactly the
values above, and a reader must reject any other value rather than guess. They
are stored anyway so that a file states its own conventions, and so that a
later version can add new values without changing the schema.

The provenance fields `source`, `comment`, `producer` and `created_utc` never
affect how a map is evaluated.

## Grid and node order

All grids are equidistant. On axis `a`, node `i` (0 ≤ i < n) sits at

```
p_a(i) = axis_min[a] + i · (axis_max[a] − axis_min[a]) / (axis_n[a] − 1)
```

Both `axis_min` and `axis_max` are nodes. Spacing is not stored; it follows
from the three values above.

The value at node (i, j, k) is at flat index

```
index(i, j, k) = (i · ny + j) · nz + k
```

This is x-major order with z varying fastest. covfie's `strided` backend,
ACTS's XYZ map helpers, FairShip and Opera table exports all use the same
order, so none of them need a reshuffle.

## Symmetry

A map may store only one half of the field along an axis and obtain the other
half by reflection. For each axis `a` with `mirror[a]` set:

- `axis_min[a]` must be exactly 0: the file stores coordinates ≥ 0 only.
- A query with coordinate `p_a < 0` is evaluated at `−p_a`, and each field
  component `c` is multiplied by `parity[3·a + c]`.

`parity` is a 3×3 matrix stored row by row. The row is the mirrored axis
(x, y, z) and the column is the component (Bx, By, Bz). Each entry is +1 or −1.
Rows of axes that are not mirrored are ignored, but must still hold ±1. When
several axes are mirrored, the signs multiply.

The signs depend on the field's physics and cannot be inferred from the grid, so
each file stores `parity` explicitly instead of naming a symmetry. ACTS's
`firstOctant` option, for comparison, reflects coordinates without flipping
any component, which is only correct for fields that are even in every
coordinate.

### Example: FairShip quadrant-symmetric dipole

FairShip's `ShipBFieldMap` with quadrant symmetry (the muon shield maps)
stores x ≥ 0, y ≥ 0. Its evaluation code (`ShipBFieldMap::Field`) flips Bx for
x < 0, flips Bx and Bz for y < 0, and never flips By. In this format that is:

```
mirror = {true, true, false}
parity = {-1, +1, +1,    // reflection in x: Bx odd, By even, Bz even
          -1, +1, -1,    // reflection in y: Bx odd, By even, Bz odd
          +1, +1, +1}    // z not mirrored, row ignored
```

At (x, y) = (−1 m, −0.5 m) both reflections apply, so Bx picks up
(−1)·(−1) = +1, By stays +1, and Bz picks up (+1)·(−1) = −1.

`FieldMapSymmetry::quadrantDipole()` returns this configuration, and
`fairship_to_fieldmap --symmetry quadrant_dipole` writes it.

## What the format leaves to the consumer

A map describes the field in its own local frame. The format does not say
where the map sits in the detector, how it is scaled, which volumes it
applies to, or what happens outside the grid. Those depend on the geometry
and the job, and one map may be used in several places. They belong in the
consumer's configuration: `FieldMapSource::MagnetConfig` takes a volume
pattern and a translation, for example.

The reference evaluator (`makeFieldEvaluator`) interpolates trilinearly and
returns the boundary value outside the grid. FairShip's `ShipBFieldMap`
returns 0 outside instead, and the format allows either.

## Reduced precision

Values are always read as `float`. A writer may store them with fewer bits
using an RNTuple column encoding: `Real32Trunc` (mantissa truncation, see
`FieldMapWriteOptions::truncated_bits`), or `Real32Quant` or `Real16`. RNTuple
decodes these transparently, so readers do not need to know which encoding a
file uses.

## Versioning

- A reader supports one major version and every minor version of it.
- A reader rejects an entry whose `format_major` it does not know. It checks
  `format_major` before reading any other field, because a new major version
  may rename or remove fields.
- A reader ignores fields it does not know. This lets older readers open files
  from newer minor versions.
- Adding a field bumps the minor version. Readers of the new minor version
  must still accept files without the field, and must document what its
  absence means.
- Removing a field bumps the major version, as does changing a field's type,
  meaning or allowed values in a way an older reader would misread. Adding a
  new allowed value to an enumerated string field, such as
  `"cylindrical_rz"`, only needs a minor bump: older readers already reject
  values they do not know.
- A field name is never reused with a different meaning.

Each format version has a frozen reference file in `tests/data/`, and the
test suite reads every one of them with the current reader. See
[`tests/data/README.md`](../tests/data/README.md).

## Notes for consumers

### Geant4

`G4MagneticField::GetFieldValue` receives positions in CLHEP
units (mm) and must return the field in CLHEP units. Multiply the tesla value
by `CLHEP::tesla` (10⁻³ in CLHEP's internal units); `G4MagFieldAdapter` does
this. `G4MagneticField` objects are per worker thread. The map values can be
shared read-only across threads.

### ACTS

ACTS works in its own natural units, where 1 T is
`Acts::UnitConstants::T` (≈ 2.998·10⁻⁴). Build an `Acts::Grid` with
equidistant axes directly from `axis_min`, `axis_max` and `axis_n`, or go
through covfie. Do not feed nodes through `Acts::fieldMapXYZ`. That helper
rebuilds the axes from a point list and adds one extra bin past the last node,
which it leaves at zero, so the top cell of every axis interpolates towards
zero. ACTS has no notion of per-component parity, so expand mirrored maps
before handing them over.

### covfie

Interleave `bx`, `by`, `bz` into a `float3` array in the same
index order and adopt it with `array::owning_data_t(size, std::unique_ptr)`.
Wrap it in `strided` → `linear` → `clamp` → `affine`. `makeFieldEvaluator`
(`src/detail/covfie_chains.h`) does exactly this.

## Reading a file without SHiP software

C++ (ROOT ≥ 6.34):

```cpp
auto reader = ROOT::RNTupleReader::Open("field_maps", "map.root");
auto name = reader->GetView<std::string>("name");
auto n = reader->GetView<std::array<std::uint32_t, 3>>("axis_n");
auto by = reader->GetView<std::vector<float>>("by");
for (auto i : reader->GetEntryRange())
    std::cout << name(i) << ": " << n(i)[0] << "x" << n(i)[1] << "x" << n(i)[2]
              << ", By[0] = " << by(i)[0] << " T\n";
```

PyROOT:

```python
import ROOT

reader = ROOT.RNTupleReader.Open("field_maps", "map.root")
entry = reader.CreateEntry()
for i in range(reader.GetNEntries()):
    reader.LoadEntry(i, entry)
    print(entry["name"], list(entry["axis_n"]), entry["by"][0])
```

uproot:

```python
import uproot

for m in uproot.open("map.root")["field_maps"].arrays():
    print(m["name"], m["axis_n"].tolist(), m["by"][0])
```
