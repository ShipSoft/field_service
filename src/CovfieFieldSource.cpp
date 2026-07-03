// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FieldService/CovfieFieldSource.h"

#include "detail/covfie_chains.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ship {

namespace {

using field_t = detail::reader_field_t;

/// Filename resolution: bare names against `$SHIPFIELD_ROOT/share/field/`.
[[nodiscard]] std::string resolve_cvf_path(std::string const& path) {
    if (std::filesystem::exists(path))
        return path;
    auto bare = std::filesystem::path(path).filename();
    if (auto const* root = std::getenv("SHIPFIELD_ROOT")) {
        auto resolved = std::filesystem::path(root) / "share" / "field" / bare;
        if (std::filesystem::exists(resolved))
            return resolved.string();
    }
    throw std::runtime_error("Cannot locate field map '" + path +
                             "'; set SHIPFIELD_ROOT or provide an absolute path");
}

class CovfieEvaluator final : public IFieldEvaluator {
   public:
    explicit CovfieEvaluator(field_t field) : field_{std::move(field)} {}

    // view_ references field_; moving would dangle it. Instances only ever
    // live behind a shared_ptr, so immovability costs nothing.
    CovfieEvaluator(CovfieEvaluator const&) = delete;
    CovfieEvaluator& operator=(CovfieEvaluator const&) = delete;

    [[nodiscard]] std::array<field_q, 3> at(pos_q x, pos_q y, pos_q z) const override {
        using namespace mp_units::si;
        auto const v = view_.at(static_cast<float>(x.numerical_value_in(milli<metre>)),
                                static_cast<float>(y.numerical_value_in(milli<metre>)),
                                static_cast<float>(z.numerical_value_in(milli<metre>)));
        return {v[0] * tesla, v[1] * tesla, v[2] * tesla};
    }

   private:
    field_t field_;
    // Field data is immutable post-load and view_t::at is const and stateless,
    // so one view shared across threads is safe and keeps at() allocation-free.
    field_t::view_t view_{field_};
};

}  // namespace

std::shared_ptr<IFieldEvaluator> loadCovfieField(std::string const& cvf_path) {
    auto resolved = std::filesystem::canonical(resolve_cvf_path(cvf_path)).string();

    // Magnets may share a map file; hand out one evaluator per file so the
    // (potentially large) grid is held in memory only once.
    static std::mutex cache_mutex;
    static std::map<std::string, std::weak_ptr<IFieldEvaluator>> cache;
    std::lock_guard lock{cache_mutex};
    if (auto it = cache.find(resolved); it != cache.end())
        if (auto cached = it->second.lock())
            return cached;

    std::ifstream is(resolved, std::ios::binary);
    if (!is.good())
        throw std::runtime_error("Failed to open field map: " + resolved);
    field_t field(is);
    auto eval = std::make_shared<CovfieEvaluator>(std::move(field));
    cache[resolved] = eval;
    return eval;
}

CovfieFieldSource::CovfieFieldSource(std::vector<MagnetConfig> magnets) {
    regions_.reserve(magnets.size());
    for (auto& m : magnets) {
        FieldRegion r;
        r.name = std::move(m.name);
        r.volume_pattern = std::move(m.volume_pattern);
        r.field = loadCovfieField(m.cvf_file);
        regions_.push_back(std::move(r));
    }
}

}  // namespace ship
