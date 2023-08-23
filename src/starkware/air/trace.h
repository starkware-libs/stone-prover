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

#ifndef STARKWARE_AIR_TRACE_H_
#define STARKWARE_AIR_TRACE_H_

#include <utility>
#include <vector>

#include "starkware/algebra/polymorphic/field_element_vector.h"

namespace starkware {

class Trace {
 public:
  Trace(const Trace&) = delete;
  Trace(Trace&&) = default;
  ~Trace() = default;
  Trace& operator=(const Trace&) = default;
  Trace& operator=(Trace&&) = default;

  template <typename FieldElementT>
  explicit Trace(std::vector<std::vector<FieldElementT>>&& values) {
    for (auto& column : values) {
      values_.emplace_back(FieldElementVector::Make<FieldElementT>(std::move(column)));
    }
  }

  /*
    Allocates a vector of values that may be passed to the constructor.
  */
  template <typename FieldElementT>
  static std::vector<std::vector<FieldElementT>> Allocate(size_t n_columns, size_t trace_length) {
    std::vector<std::vector<FieldElementT>> values;
    values.reserve(n_columns);
    for (size_t col = 0; col < n_columns; col++) {
      values.emplace_back(FieldElementT::UninitializedVector(trace_length));
    }
    return values;
  }

  static Trace CopyFrom(gsl::span<const ConstFieldElementSpan> values) {
    Trace trace;
    trace.values_.reserve(values.size());
    for (auto& column : values) {
      trace.values_.emplace_back(FieldElementVector::CopyFrom(column));
    }
    return trace;
  }

  Trace Clone() {
    Trace trace;
    trace.values_.reserve(values_.size());
    for (auto& column : values_) {
      trace.values_.emplace_back(FieldElementVector::CopyFrom(column));
    }
    return trace;
  }

  size_t Length() const { return values_[0].Size(); }

  size_t Width() const { return values_.size(); }
  const FieldElementVector& GetColumn(size_t column) const { return values_[column]; }

  std::vector<FieldElementVector> ConsumeAsColumnsVector() && { return std::move(values_); }

  void SetTraceElementForTesting(size_t column, size_t index, const FieldElement& field_element) {
    values_[column].Set(index, field_element);
  }

  template <typename FieldElementT>
  std::vector<gsl::span<const FieldElementT>> As() const {
    std::vector<gsl::span<const FieldElementT>> result;
    result.reserve(values_.size());
    for (const FieldElementVector& v : values_) {
      result.push_back(gsl::make_span(v.As<FieldElementT>()));
    }
    return result;
  }

 private:
  Trace() = default;

  std::vector<FieldElementVector> values_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_TRACE_H_
