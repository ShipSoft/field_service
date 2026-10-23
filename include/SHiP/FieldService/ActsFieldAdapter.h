// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "SHiP/FieldService/IFieldSource.h"

#include <Acts/Definitions/Algebra.hpp>
#include <Acts/Definitions/Units.hpp>
#include <Acts/MagneticField/MagneticFieldContext.hpp>
#include <Acts/MagneticField/MagneticFieldError.hpp>
#include <Acts/MagneticField/MagneticFieldProvider.hpp>
#include <Acts/Utilities/Result.hpp>
#include <SHiP/Units.hpp>
#include <array>
#include <atomic>
#include <cmath>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace SHiP {

/// `Acts::MagneticFieldProvider` wrapping an `IFieldEvaluator`, the ACTS
/// counterpart of `G4MagFieldAdapter`. Positions go to the evaluator as they
/// come, so pass one in the global frame: wrap map evaluators in `translated`,
/// and in `zeroOutside` if lookups past the grid should see no field.
///
/// A non-finite position, a non-finite field or an exception from the
/// evaluator fails the lookup with `MagneticFieldError::OutOfBounds`, so the
/// propagator aborts instead of carrying on with a wrong field. The first
/// such failure is reported to `on_error`, if given.
class ActsFieldAdapter final : public Acts::MagneticFieldProvider {
   public:
    /// May own state that cannot be copied; it is called at most once.
    using ErrorHandler = std::move_only_function<void(std::string_view) const>;

    explicit ActsFieldAdapter(std::shared_ptr<IFieldEvaluator> eval, ErrorHandler on_error = {})
        : eval_{std::move(eval)}, on_error_{std::move(on_error)} {
        if (!eval_) {
            throw std::invalid_argument("ActsFieldAdapter: null field evaluator");
        }
    }

    [[nodiscard]] Cache makeCache(Acts::MagneticFieldContext const& /*mctx*/) const override {
        return Cache{Empty{}};
    }

    [[nodiscard]] Acts::Result<Acts::Vector3> getField(Acts::Vector3 const& position,
                                                       Cache& /*cache*/) const override {
        using Result = Acts::Result<Acts::Vector3>;
        namespace si = mp_units::si;
        if (!position.allFinite()) {
            return fail("non-finite position queried");
        }

        std::array<IFieldEvaluator::field_q, 3> B{};
        try {
            B = eval_->at((position[0] / Acts::UnitConstants::mm) * si::milli<si::metre>,
                          (position[1] / Acts::UnitConstants::mm) * si::milli<si::metre>,
                          (position[2] / Acts::UnitConstants::mm) * si::milli<si::metre>);
        } catch (std::exception const& e) {
            return fail(e.what());
        } catch (...) {
            return fail("unknown exception from evaluator");
        }

        Acts::Vector3 const field{B[0].numerical_value_in(si::tesla) * Acts::UnitConstants::T,
                                  B[1].numerical_value_in(si::tesla) * Acts::UnitConstants::T,
                                  B[2].numerical_value_in(si::tesla) * Acts::UnitConstants::T};
        if (!field.allFinite()) {
            return fail("non-finite field value from evaluator");
        }
        return Result::success(field);
    }

   private:
    struct Empty {};

    [[nodiscard]] Acts::Result<Acts::Vector3> fail(std::string_view what) const {
        if (on_error_ && !reported_.exchange(true)) {
            on_error_(what);
        }
        return Acts::Result<Acts::Vector3>::failure(Acts::MagneticFieldError::OutOfBounds);
    }

    std::shared_ptr<IFieldEvaluator> eval_;
    ErrorHandler on_error_;
    mutable std::atomic<bool> reported_{false};
};

}  // namespace SHiP
