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

#include "starkware/channel/prover_channel.h"

#include <cstddef>
#include <string>
#include <vector>

#include "glog/logging.h"

#include "starkware/utils/serialization.h"

namespace starkware {

void ProverChannel::SendFieldElementImpl(const FieldElement& value) {
  std::vector<std::byte> raw_bytes(value.SizeInBytes());
  value.ToBytes(raw_bytes);
  SendBytes(raw_bytes);
}

void ProverChannel::SendFieldElementSpanImpl(const ConstFieldElementSpan& values) {
  const size_t size_in_bytes = values.GetField().ElementSizeInBytes();
  std::vector<std::byte> raw_bytes(values.Size() * size_in_bytes);
  auto raw_bytes_span = gsl::make_span(raw_bytes);
  for (size_t i = 0; i < values.Size(); i++) {
    values[i].ToBytes(raw_bytes_span.subspan(i * size_in_bytes, size_in_bytes));
  }
  SendBytes(raw_bytes);
}

}  // namespace starkware
