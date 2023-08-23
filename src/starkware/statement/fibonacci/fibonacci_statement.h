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

#ifndef STARKWARE_STATEMENT_FIBONACCI_FIBONACCI_STATEMENT_H_
#define STARKWARE_STATEMENT_FIBONACCI_FIBONACCI_STATEMENT_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/fibonacci/fibonacci_air.h"
#include "starkware/air/fibonacci/fibonacci_trace_context.h"
#include "starkware/air/trace.h"
#include "starkware/statement/statement.h"
#include "starkware/utils/json.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

/*
  Public input:
    * fibonacci_claim_index (size_t).
    * claimed_fib (field element).
  Private input:
    * witness (field element).
*/
template <typename FieldElementT>
class FibonacciStatement : public Statement {
 public:
  explicit FibonacciStatement(
      const JsonValue& public_input, std::optional<JsonValue> private_input);

  const Air& GetAir() override;

  /*
    Returns the serialization of: [path_length, root, leaf].
    Where path_length is written as 8 bytes and root and leaf are written as
    FieldElementT::SizeInBytes() bytes.
  */
  const std::vector<std::byte> GetInitialHashChainSeed() const override;

  std::unique_ptr<TraceContext> GetTraceContext() const override;

  JsonValue FixPublicInput() override;

  std::string GetName() const override {
    std::string file_name = __FILE__;
    return ConvertFileNameToProverName(file_name);
  }

 private:
  size_t fibonacci_claim_index_;
  std::optional<FieldElementT> claimed_fib_;
  std::unique_ptr<FibonacciAir<FieldElementT>> air_;
};

}  // namespace starkware

#include "starkware/statement/fibonacci/fibonacci_statement.inl"

#endif  // STARKWARE_STATEMENT_FIBONACCI_FIBONACCI_STATEMENT_H_
