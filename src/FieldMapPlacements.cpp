// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "SHiP/FieldService/FieldMapIO.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace SHiP {

namespace {

using nlohmann::json;

[[noreturn]] void fail(std::string const& path, std::string const& what) {
    throw std::runtime_error(path + ": " + what);
}

std::string requireString(json const& obj, char const* key, std::string const& path,
                          std::string const& where) {
    auto const it = obj.find(key);
    if (it == obj.end() || !it->is_string() || it->get_ref<std::string const&>().empty())
        fail(path, where + ": '" + key + "' must be a non-empty string");
    return it->get<std::string>();
}

/// Parse `path` and check the parts every version-1 manifest needs.
json parseManifest(std::string const& path) {
    std::ifstream in{path};
    if (!in)
        fail(path, "cannot open");

    json doc;
    try {
        doc = json::parse(in);
    } catch (json::parse_error const& e) {
        fail(path, std::string("not valid JSON: ") + e.what());
    }
    if (!doc.is_object())
        fail(path, "top level must be an object");

    auto const version = doc.find("format_version");
    if (version == doc.end() || !version->is_number_integer())
        fail(path, "missing integer 'format_version'");
    if (version->get<int>() != kPlacementsFormatVersion)
        fail(path, "unsupported format_version " + std::to_string(version->get<int>()) +
                       " (this library reads " + std::to_string(kPlacementsFormatVersion) + ")");

    if (auto const maps = doc.find("maps"); maps == doc.end() || !maps->is_object())
        fail(path, "missing object 'maps'");
    if (auto const magnets = doc.find("magnets");
        magnets == doc.end() || !magnets->is_array() || magnets->empty())
        fail(path, "missing non-empty array 'magnets'");
    return doc;
}

std::array<double, 3> readTranslation(json const& t, std::string const& path,
                                      std::string const& where) {
    std::array<double, 3> out{};
    if (!t.is_array() || t.size() != out.size())
        fail(path, where + ": 'translation' needs 3 numbers (mm)");
    auto v = t.begin();
    for (auto& component : out) {
        if (!v->is_number() || !std::isfinite(v->get<double>()))
            fail(path, where + ": 'translation' values must be finite numbers");
        component = (v++)->get<double>();
    }
    return out;
}

/// One `magnets` entry; its map file is taken from `dir`.
FieldMapSource::MagnetConfig readMagnet(json const& m, json const& maps,
                                        std::filesystem::path const& dir, std::string const& path,
                                        std::string const& where) {
    if (!m.is_object())
        fail(path, where + " must be an object");

    FieldMapSource::MagnetConfig magnet;
    magnet.name = requireString(m, "name", path, where);
    magnet.volume_pattern = requireString(m, "volume_pattern", path, where);
    magnet.map = requireString(m, "map", path, where);

    auto const entry = maps.find(magnet.map);
    if (entry == maps.end() || !entry->is_object())
        fail(path, where + ": map '" + magnet.map + "' is not listed in 'maps'");
    magnet.file = (dir / requireString(*entry, "file", path, "maps." + magnet.map)).string();

    if (auto const t = m.find("translation"); t != m.end())
        magnet.translation = readTranslation(*t, path, where);
    return magnet;
}

}  // namespace

FieldMapPlacements readFieldMapPlacements(std::string const& path) {
    auto const resolved = resolveFieldMapPath(path);
    auto const doc = parseManifest(resolved);

    FieldMapPlacements out;
    if (auto const geo = doc.find("geometry"); geo != doc.end()) {
        if (!geo->is_object())
            fail(resolved, "'geometry' must be an object");
        out.geometry_package = requireString(*geo, "package", resolved, "geometry");
        out.geometry_version = requireString(*geo, "version", resolved, "geometry");
    }

    // Map files sit next to the manifest, wherever that is.
    auto const dir = std::filesystem::path(resolved).parent_path();
    auto const& magnets = doc.at("magnets");
    std::set<std::string> names;
    for (std::size_t i = 0; i < magnets.size(); ++i) {
        auto const where = "magnets[" + std::to_string(i) + "]";
        auto magnet = readMagnet(magnets.at(i), doc.at("maps"), dir, resolved, where);
        if (!names.insert(magnet.name).second)
            fail(resolved, where + ": duplicate magnet name '" + magnet.name + "'");
        out.magnets.push_back(std::move(magnet));
    }
    return out;
}

}  // namespace SHiP
