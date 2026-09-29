// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FieldService/CovfieFieldSource.h"

#include "FieldService/FieldMap.h"
#include "detail/covfie_chains.h"

#include <array>
#include <cstddef>
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

class CovfieEvaluator final : public IFieldEvaluator {
   public:
    explicit CovfieEvaluator(field_t field, FieldMapSymmetry symmetry = {})
        : field_{std::move(field)}, symmetry_{symmetry} {}

    // view_ references field_; moving would dangle it. Instances only ever
    // live behind a shared_ptr, so immovability costs nothing.
    CovfieEvaluator(CovfieEvaluator const&) = delete;
    CovfieEvaluator& operator=(CovfieEvaluator const&) = delete;

    [[nodiscard]] std::array<field_q, 3> at(pos_q x, pos_q y, pos_q z) const override {
        using namespace mp_units::si;
        std::array<float, 3> p{static_cast<float>(x.numerical_value_in(milli<metre>)),
                               static_cast<float>(y.numerical_value_in(milli<metre>)),
                               static_cast<float>(z.numerical_value_in(milli<metre>))};
        // Fold negative coordinates on mirrored axes into the stored half and
        // collect the per-component sign flips that reflection induces.
        std::array<float, 3> sign{1.0f, 1.0f, 1.0f};
        if (symmetry_.any()) {
            for (std::size_t a = 0; a < 3; ++a) {
                if (symmetry_.mirror[a] && p[a] < 0.0f) {
                    p[a] = -p[a];
                    for (std::size_t c = 0; c < 3; ++c)
                        sign[c] *= symmetry_.parity[3 * a + c];
                }
            }
        }
        auto const v = view_.at(p[0], p[1], p[2]);
        return {sign[0] * v[0] * tesla, sign[1] * v[1] * tesla, sign[2] * v[2] * tesla};
    }

   private:
    field_t field_;
    FieldMapSymmetry symmetry_;
    // Field data is immutable post-load and view_t::at is const and stateless,
    // so one view shared across threads is safe and keeps at() allocation-free.
    field_t::view_t view_{field_};
};

}  // namespace

std::shared_ptr<IFieldEvaluator> makeFieldEvaluator(FieldMap const& map) {
    validate(map);
    auto const& g = map.grid;
    detail::GridSpec const spec{
        {static_cast<float>(g.min[0]), static_cast<float>(g.min[1]), static_cast<float>(g.min[2])},
        {static_cast<float>(g.max[0]), static_cast<float>(g.max[1]), static_cast<float>(g.max[2])},
        {g.n[0], g.n[1], g.n[2]}};
    // The format stores components as separate arrays; covfie wants one
    // interleaved {Bx, By, Bz} per node. Both use the same node order.
    auto const size = g.size();
    auto values = std::make_unique<detail::stored_vector_t[]>(size);
    for (std::size_t i = 0; i < size; ++i) {
        values[i][0] = map.bx[i];
        values[i][1] = map.by[i];
        values[i][2] = map.bz[i];
    }
    return std::make_shared<CovfieEvaluator>(detail::make_reader_field(spec, std::move(values)),
                                             map.symmetry);
}

std::shared_ptr<IFieldEvaluator> loadCovfieField(std::string const& cvf_path) {
    auto resolved = resolveFieldMapPath(cvf_path);

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
