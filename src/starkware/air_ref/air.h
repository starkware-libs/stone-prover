// Copyright 2023 StarkWare Industries Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License").
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.starkware.co/open-source-license/
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.

#ifndef STARKWARE_AIR_AIR_H_
#define STARKWARE_AIR_AIR_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/trace.h"
#include "starkware/algebra/domains/multiplicative_group.h"
#include "starkware/algebra/fields/fraction_field_element.h"
#include "starkware/algebra/polymorphic/field_element_vector.h"
#include "starkware/composition_polynomial/composition_polynomial.h"

namespace starkware {

class CompositionPolynomial;

class Air {
 public:
  virtual ~Air() = default;

  explicit Air(uint64_t trace_length) : trace_length_(trace_length) {
    ASSERT_RELEASE(IsPowerOfTwo(trace_length), "trace_length must be a power of 2.");
  }

  /*
    Creates a CompositionPolynomial object based on the given (verifier-chosen) coefficients.
  */
  virtual std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const FieldElement& trace_generator,
      const ConstFieldElementSpan& random_coefficients) const = 0;

  uint64_t TraceLength() const { return trace_length_; }

  virtual uint64_t GetCompositionPolynomialDegreeBound() const = 0;

  /*
    Number of random coefficients that are chosen by the verifier and affect the constraint.
    They are the coefficients of the linear combination of the constraints and must be random in
    order to maintain soundness.
  */
  virtual uint64_t NumRandomCoefficients() const = 0;

  virtual uint64_t GetNumConstraints() const { return NumRandomCoefficients(); }

  /*
    Returns a list of pairs (relative_row, col) that define the neighbors needed for the constraint.
  */
  virtual std::vector<std::pair<int64_t, uint64_t>> GetMask() const = 0;

  virtual uint64_t NumColumns() const = 0;

  /*
    When the AIR has interaction, clones the AIR and updates its interaction elements. Returns the
    cloned AIR. Otherwise, this function shouldn't be used.
  */
  virtual std::unique_ptr<const Air> WithInteractionElements(
      const FieldElementVector& /*interaction_elms*/) const {
    ASSERT_RELEASE(false, "Calling WithInteractionElements in an air with no interaction.");
  }

  /*
    Stores data relevant to the interaction.
  */
  struct InteractionParams {
    // Number of columns in first and second trace.
    size_t n_columns_first;
    size_t n_columns_second;
    // Number of interaction random elements.
    size_t n_interaction_elements;
  };

  /*
    Returns the interaction parameters. If there is no interaction, returns nullopt.
  */
  virtual std::optional<InteractionParams> GetInteractionParams() const = 0;

  /*
    If air has interaction, returns the number of columns in the first trace, otherwise, returns the
    total number of columns.
  */
  size_t GetNColumnsFirst() const {
    const auto interaction_params = GetInteractionParams();
    if (interaction_params.has_value()) {
      return interaction_params->n_columns_first;
    }
    return this->NumColumns();
  }

 protected:
  uint64_t trace_length_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_AIR_H_
