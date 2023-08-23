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

#include "starkware/channel/verifier_channel.h"

#include <string>
#include <vector>

#include "glog/logging.h"

#include "starkware/utils/serialization.h"

namespace starkware {

void VerifierChannel::SendNumber(uint64_t number) {
  std::array<std::byte, sizeof(uint64_t)> bytes{};
  Serialize<uint64_t>(number, bytes);
  SendBytes(bytes);
}

void VerifierChannel::SendFieldElement(const FieldElement& value) {
  std::vector<std::byte> raw_bytes(value.SizeInBytes());
  value.ToBytes(raw_bytes);
  SendBytes(raw_bytes);
}

FieldElement VerifierChannel::ReceiveFieldElementImpl(const Field& field) {
  return field.FromBytes(ReceiveBytes(field.ElementSizeInBytes()));
}

void VerifierChannel::ReceiveFieldElementSpanImpl(
    const Field& field, const FieldElementSpan& span) {
  const size_t size_in_bytes = field.ElementSizeInBytes();
  const size_t n_elements = span.Size();
  std::vector<std::byte> field_element_vector_bytes = ReceiveBytes(size_in_bytes * n_elements);
  auto span_bytes = gsl::make_span(field_element_vector_bytes);
  for (size_t i = 0; i < n_elements; i++) {
    span.Set(i, field.FromBytes(span_bytes.subspan(i * size_in_bytes, size_in_bytes)));
  }
}

/*
  Generates a random number, sends it to the prover and returns it to the caller.
  The number should be chosen uniformly in the range [0, upper_bound).
*/
uint64_t VerifierChannel::GetAndSendRandomNumberImpl(uint64_t upper_bound) {
  uint64_t number = GetRandomNumber(upper_bound);
  // NOTE: Must be coupled with GetRandomNumber (for the non-interactive hash chain).
  SendNumber(number);
  return number;
}

FieldElement VerifierChannel::GetAndSendRandomFieldElementImpl(const Field& field) {
  FieldElement field_element = GetRandomFieldElement(field);
  SendFieldElement(field_element);
  return field_element;
}

}  // namespace starkware
