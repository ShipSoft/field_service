// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FieldService/FieldMapIO.h"

#include <ROOT/RError.hxx>
#include <ROOT/RField.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleWriter.hxx>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ship {

namespace {

// Enumerated string values defined by format 1.x.
constexpr char const* kCartesian = "cartesian";
constexpr char const* kMillimetre = "mm";
constexpr char const* kTesla = "T";
constexpr char const* kZFastest = "xyz_z_fastest";

[[noreturn]] void fail(std::string const& path, std::string const& what) {
    throw std::runtime_error(path + ": " + what);
}

std::unique_ptr<ROOT::RNTupleReader> open_reader(std::string const& path) {
    try {
        return ROOT::RNTupleReader::Open(kFieldMapNTupleName, path);
    } catch (ROOT::RException const& e) {
        fail(path, std::string("not a SHiP field-map file: ") + e.what());
    }
}

/// Read all maps in `path`, or only the one named `*only`.
std::vector<FieldMap> read_maps(std::string const& path, std::string const* only) {
    auto reader = open_reader(path);
    try {
        // Check the version before touching anything else: a future major
        // version may have renamed or dropped the other fields.
        auto format_major = reader->GetView<std::uint16_t>("format_major");
        for (auto i : reader->GetEntryRange())
            if (format_major(i) != kFieldMapFormatMajor)
                fail(path, "unsupported field-map format major version " +
                               std::to_string(format_major(i)) + " (this reader supports " +
                               std::to_string(kFieldMapFormatMajor) + ")");

        auto name = reader->GetView<std::string>("name");
        auto coordinate_system = reader->GetView<std::string>("coordinate_system");
        auto length_unit = reader->GetView<std::string>("length_unit");
        auto field_unit = reader->GetView<std::string>("field_unit");
        auto index_order = reader->GetView<std::string>("index_order");
        auto axis_min = reader->GetView<std::array<double, 3>>("axis_min");
        auto axis_max = reader->GetView<std::array<double, 3>>("axis_max");
        auto axis_n = reader->GetView<std::array<std::uint32_t, 3>>("axis_n");
        auto mirror = reader->GetView<std::array<bool, 3>>("mirror");
        auto parity = reader->GetView<std::array<std::int8_t, 9>>("parity");
        auto bx = reader->GetView<std::vector<float>>("bx");
        auto by = reader->GetView<std::vector<float>>("by");
        auto bz = reader->GetView<std::vector<float>>("bz");
        auto source = reader->GetView<std::string>("source");
        auto comment = reader->GetView<std::string>("comment");
        auto producer = reader->GetView<std::string>("producer");
        auto created_utc = reader->GetView<std::string>("created_utc");

        std::vector<FieldMap> maps;
        for (auto i : reader->GetEntryRange()) {
            if (only && name(i) != *only)
                continue;
            auto expect = [&](std::string const& field, std::string const& value,
                              char const* wanted) {
                if (value != wanted)
                    fail(path, "map '" + name(i) + "': " + field + " is '" + value +
                                   "', this reader supports only '" + wanted + "'");
            };
            expect("coordinate_system", coordinate_system(i), kCartesian);
            expect("length_unit", length_unit(i), kMillimetre);
            expect("field_unit", field_unit(i), kTesla);
            expect("index_order", index_order(i), kZFastest);

            FieldMap m;
            m.name = name(i);
            m.grid = {axis_min(i), axis_max(i), axis_n(i)};
            m.symmetry = {mirror(i), parity(i)};
            m.bx = bx(i);
            m.by = by(i);
            m.bz = bz(i);
            m.provenance = {source(i), comment(i), producer(i), created_utc(i)};
            validate(m);
            maps.push_back(std::move(m));
        }
        return maps;
    } catch (ROOT::RException const& e) {
        fail(path, std::string("malformed field-map file: ") + e.what());
    } catch (std::invalid_argument const& e) {
        fail(path, e.what());
    }
}

/// Places a map's local frame at `offset` (mm) in the global frame.
class TranslatedEvaluator final : public IFieldEvaluator {
   public:
    TranslatedEvaluator(std::shared_ptr<IFieldEvaluator> inner, std::array<double, 3> const& offset)
        : inner_{std::move(inner)} {
        using namespace mp_units::si;
        for (std::size_t a = 0; a < 3; ++a)
            offset_[a] = offset[a] * milli<metre>;
    }

    [[nodiscard]] std::array<field_q, 3> at(pos_q x, pos_q y, pos_q z) const override {
        return inner_->at(x - offset_[0], y - offset_[1], z - offset_[2]);
    }

   private:
    std::shared_ptr<IFieldEvaluator> inner_;
    std::array<pos_q, 3> offset_;
};

std::string now_utc() {
    return std::format("{:%FT%TZ}",
                       std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

}  // namespace

std::vector<FieldMap> readFieldMaps(std::string const& path) {
    return read_maps(path, nullptr);
}

FieldMap readFieldMap(std::string const& path, std::string const& name) {
    auto maps = read_maps(path, &name);
    if (maps.empty())
        fail(path, "no field map named '" + name + "'");
    return std::move(maps.front());
}

void writeFieldMaps(std::string const& path, std::span<FieldMap const> maps,
                    FieldMapWriteOptions const& options) {
    std::set<std::string> names;
    for (auto const& m : maps) {
        validate(m);
        if (!names.insert(m.name).second)
            throw std::invalid_argument("duplicate field map name '" + m.name + "'");
    }

    auto model = ROOT::RNTupleModel::Create();
    model->SetDescription("SHiP field map format " + std::to_string(kFieldMapFormatMajor) + "." +
                          std::to_string(kFieldMapFormatMinor) +
                          " (see field_service docs/field_map_format.md)");
    auto format_major = model->MakeField<std::uint16_t>("format_major");
    auto format_minor = model->MakeField<std::uint16_t>("format_minor");
    auto name = model->MakeField<std::string>("name");
    auto coordinate_system = model->MakeField<std::string>("coordinate_system");
    auto length_unit = model->MakeField<std::string>("length_unit");
    auto field_unit = model->MakeField<std::string>("field_unit");
    auto axis_min = model->MakeField<std::array<double, 3>>("axis_min");
    auto axis_max = model->MakeField<std::array<double, 3>>("axis_max");
    auto axis_n = model->MakeField<std::array<std::uint32_t, 3>>("axis_n");
    auto index_order = model->MakeField<std::string>("index_order");
    auto bx = model->MakeField<std::vector<float>>("bx");
    auto by = model->MakeField<std::vector<float>>("by");
    auto bz = model->MakeField<std::vector<float>>("bz");
    auto mirror = model->MakeField<std::array<bool, 3>>("mirror");
    auto parity = model->MakeField<std::array<std::int8_t, 9>>("parity");
    auto source = model->MakeField<std::string>("source");
    auto comment = model->MakeField<std::string>("comment");
    auto producer = model->MakeField<std::string>("producer");
    auto created_utc = model->MakeField<std::string>("created_utc");

    if (options.truncated_bits != 0) {
        for (auto const* component : {"bx", "by", "bz"}) {
            auto* item = model->GetMutableField(component).GetMutableSubfields().front();
            // Throws for bit counts outside what Real32Trunc supports.
            dynamic_cast<ROOT::RField<float>&>(*item).SetTruncated(options.truncated_bits);
        }
    }

    auto writer = ROOT::RNTupleWriter::Recreate(std::move(model), kFieldMapNTupleName, path);
    auto const created = now_utc();
    for (auto const& m : maps) {
        *format_major = kFieldMapFormatMajor;
        *format_minor = kFieldMapFormatMinor;
        *name = m.name;
        *coordinate_system = kCartesian;
        *length_unit = kMillimetre;
        *field_unit = kTesla;
        *axis_min = m.grid.min;
        *axis_max = m.grid.max;
        *axis_n = m.grid.n;
        *index_order = kZFastest;
        *bx = m.bx;
        *by = m.by;
        *bz = m.bz;
        *mirror = m.symmetry.mirror;
        *parity = m.symmetry.parity;
        *source = m.provenance.source;
        *comment = m.provenance.comment;
        *producer = m.provenance.producer.empty() ? "SHiPFieldService " SHIPFIELD_VERSION
                                                  : m.provenance.producer;
        *created_utc = m.provenance.created_utc.empty() ? created : m.provenance.created_utc;
        writer->Fill();
    }
}

std::shared_ptr<IFieldEvaluator> loadFieldMap(std::string const& file, std::string const& map) {
    auto const resolved = resolveFieldMapPath(file);

    // Magnets may share a map; hand out one evaluator per (file, map) so the
    // (potentially large) grid is held in memory only once.
    static std::mutex cache_mutex;
    static std::map<std::pair<std::string, std::string>, std::weak_ptr<IFieldEvaluator>> cache;
    std::lock_guard lock{cache_mutex};
    auto const key = std::pair{resolved, map};
    if (auto it = cache.find(key); it != cache.end())
        if (auto cached = it->second.lock())
            return cached;

    auto eval = makeFieldEvaluator(readFieldMap(resolved, map));
    cache[key] = eval;
    return eval;
}

FieldMapSource::FieldMapSource(std::vector<MagnetConfig> magnets) {
    regions_.reserve(magnets.size());
    for (auto& m : magnets) {
        FieldRegion r;
        r.field = loadFieldMap(m.file, m.map);
        if (m.translation != std::array<double, 3>{})
            r.field = std::make_shared<TranslatedEvaluator>(std::move(r.field), m.translation);
        r.name = std::move(m.name);
        r.volume_pattern = std::move(m.volume_pattern);
        regions_.push_back(std::move(r));
    }
}

}  // namespace ship
