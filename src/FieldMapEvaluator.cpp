// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// makeFieldEvaluator: the covfie-backed evaluator for an in-memory FieldMap.

#include "SHiP/FieldService/FieldMap.h"
#include "detail/covfie_chains.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <ranges>
#include <utility>

namespace SHiP {

namespace {

using field_t = detail::reader_field_t;

class MapEvaluator final : public IFieldEvaluator {
   public:
    MapEvaluator(field_t field, FieldMapSymmetry symmetry)
        : field_{std::move(field)}, symmetry_{symmetry} {}

    // view_ references field_; moving would dangle it. Instances only ever
    // live behind a shared_ptr, so immovability costs nothing.
    MapEvaluator(MapEvaluator const&) = delete;
    MapEvaluator& operator=(MapEvaluator const&) = delete;
    MapEvaluator(MapEvaluator&&) = delete;
    MapEvaluator& operator=(MapEvaluator&&) = delete;
    ~MapEvaluator() override = default;

    [[nodiscard]] std::array<field_q, 3> at(pos_q x, pos_q y, pos_q z) const override {
        using namespace mp_units::si;
        std::array<float, 3> p{static_cast<float>(x.numerical_value_in(milli<metre>)),
                               static_cast<float>(y.numerical_value_in(milli<metre>)),
                               static_cast<float>(z.numerical_value_in(milli<metre>))};
        // covfie's clamp passes NaN through and the interpolator would then
        // cast it to a grid index, which is undefined behaviour.
        if (std::isnan(p[0]) || std::isnan(p[1]) || std::isnan(p[2])) {
            auto const nan = std::numeric_limits<float>::quiet_NaN() * tesla;
            return {nan, nan, nan};
        }
        // Fold negative coordinates on mirrored axes into the stored half and
        // collect the per-component sign flips that reflection induces.
        std::array<float, 3> sign{1.0f, 1.0f, 1.0f};
        if (symmetry_.any()) {
            for (auto&& [mirrored, row, coord] :
                 std::views::zip(symmetry_.mirror, symmetry_.parity, p)) {
                if (mirrored && coord < 0.0f) {
                    coord = -coord;
                    for (auto&& [s, parity] : std::views::zip(sign, row)) {
                        s *= std::to_underlying(parity);
                    }
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
    return std::make_shared<MapEvaluator>(detail::make_reader_field(spec, std::move(values)),
                                          map.symmetry);
}

}  // namespace SHiP
