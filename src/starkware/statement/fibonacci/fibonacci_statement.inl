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

#include "starkware/air/fibonacci/fibonacci_trace_context.h"

namespace starkware {

template <typename FieldElementT>
FibonacciStatement<FieldElementT>::FibonacciStatement(
    const JsonValue& public_input, std::optional<JsonValue> private_input)
    : Statement(std::move(private_input)),
      fibonacci_claim_index_(public_input["fibonacci_claim_index"].AsSizeT()),
      claimed_fib_(
          public_input["claimed_fib"].HasValue()
              ? std::make_optional(public_input["claimed_fib"].AsFieldElement<FieldElementT>())
              : std::nullopt) {}

template <typename FieldElementT>
const Air& FibonacciStatement<FieldElementT>::GetAir() {
  ASSERT_RELEASE(claimed_fib_.has_value(), "Can't construct air, claimed Fibonacci value not set.");
  size_t trace_length = Pow2(Log2Ceil(fibonacci_claim_index_ + 1));

  air_ = std::make_unique<FibonacciAir<FieldElementT>>(
      trace_length, fibonacci_claim_index_, *claimed_fib_);
  return *air_;
}

template <typename FieldElementT>
const std::vector<std::byte> FibonacciStatement<FieldElementT>::GetInitialHashChainSeed() const {
  ASSERT_RELEASE(
      claimed_fib_.has_value(),
      "Can't calculate initial hash chain seed, claimed Fibonacci value not set.");
  const size_t elem_bytes = FieldElementT::SizeInBytes();
  const size_t fibonacci_claim_index_bytes = sizeof(uint64_t);
  std::vector<std::byte> randomness_seed(elem_bytes + fibonacci_claim_index_bytes);
  Serialize(
      uint64_t(fibonacci_claim_index_),
      gsl::make_span(randomness_seed).subspan(0, fibonacci_claim_index_bytes));
  (*claimed_fib_).ToBytes(gsl::make_span(randomness_seed).subspan(fibonacci_claim_index_bytes));
  return randomness_seed;
}

template <typename FieldElementT>
std::unique_ptr<TraceContext> FibonacciStatement<FieldElementT>::GetTraceContext() const {
  using TraceContextT = FibonacciTraceContext<FieldElementT>;
  ASSERT_RELEASE(
      air_ != nullptr,
      "cant construct trace without a fully initialized AIR instance. Did you forget to "
      "call GetAir()");
  ASSERT_RELEASE(
      private_input_.has_value(), "Can't calculate trace, claimed Fibonacci value not set.");
  const FieldElementT witness =
      (*private_input_)["witness"].template AsFieldElement<FieldElementT>();

  return std::make_unique<TraceContextT>(UseOwned(air_), witness, fibonacci_claim_index_);
}

template <typename FieldElementT>
JsonValue FibonacciStatement<FieldElementT>::FixPublicInput() {
  ASSERT_RELEASE(private_input_.has_value(), "Missing private input.");
  Json::Value root;
  root["fibonacci_claim_index"] = static_cast<uint64_t>(fibonacci_claim_index_);

  const FieldElementT witness =
      (*private_input_)["witness"].template AsFieldElement<FieldElementT>();
  claimed_fib_ =
      FibonacciAir<FieldElementT>::PublicInputFromPrivateInput(witness, fibonacci_claim_index_);
  root["claimed_fib"] = claimed_fib_->ToString();
  return JsonValue::FromJsonCppValue(root);
}

}  // namespace starkware
