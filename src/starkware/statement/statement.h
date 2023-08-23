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

#ifndef STARKWARE_STATEMENT_STATEMENT_H_
#define STARKWARE_STATEMENT_STATEMENT_H_

#include <algorithm>
#include <cstddef>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/air.h"
#include "starkware/air/trace.h"
#include "starkware/air/trace_context.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/json.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

/*
  Represents a combinatorial statement that the integrity of its computation can be proven using an
  AIR.

  Each subclass can be constructed from the public input (JsonValue) whose contents depend on the
  specific statement.
*/
class Statement {
 public:
  explicit Statement(std::optional<JsonValue> private_input)
      : private_input_(std::move(private_input)) {}
  virtual ~Statement() = default;

  /*
    Generates and returns an AIR for the statement. The AIR is stored as a local member of the
    Statement class.
  */
  virtual const Air& GetAir() = 0;

  /*
    Returns the default initial seed to be used in the hash chain. This is obtained from the public
    parameters in some deterministic way.
  */
  virtual const std::vector<std::byte> GetInitialHashChainSeed() const = 0;

  /*
    Builds and returns a trace context for the given private input. The content of the private
    input depends on the specific statement.
  */
  virtual std::unique_ptr<TraceContext> GetTraceContext() const = 0;

  /*
    Fixes the public input according to the private input, and returns a new public input JSON.
    For example, if there is a mismatch between a commitment in the public input and a secret in the
    private input, a new commitment will be computed from the secret.
  */
  virtual JsonValue FixPublicInput() = 0;

  /*
    Returns the name of the statement for annotation purposes.
  */
  virtual std::string GetName() const = 0;

  /*
    Statement File name conversion to protocol name that appears in the annotations.
    For example:
    pedersen_merkle_statement.h --> "pedersen merkle" will appear in the title of the annotations.
  */
  std::string ConvertFileNameToProverName(const std::string& file_name) const {
    std::regex proof_name_regex("([^/]+)_statement.h");
    std::smatch matches;
    std::regex_search(file_name, matches, proof_name_regex);
    std::string proof_name = matches.str(1);
    std::regex underscore("_");
    return std::regex_replace(proof_name, underscore, " ");
  }

  const JsonValue& GetPrivateInput() const {
    ASSERT_RELEASE(private_input_.has_value(), "Missing private input.");
    return private_input_.value();
  }

 protected:
  const std::optional<JsonValue> private_input_;
};

/*
  Helper class for serializing the public input to be used as initial hash chain seed.
*/
class PublicInputSerializer {
 public:
  explicit PublicInputSerializer(size_t data_size) : public_input_vector_(data_size) {}

  template <typename T>
  void Append(const T& data) {
    ASSERT_RELEASE(offset_ + sizeof(T) <= public_input_vector_.size(), "Not enough space.");
    gsl::span<std::byte> bytes_span = gsl::make_span(public_input_vector_);
    Serialize<T>(data, bytes_span.subspan(offset_, sizeof(T)), /*use_big_endian=*/true);
    offset_ += sizeof(T);
  }

  void AddBytes(gsl::span<const std::byte> data) {
    ASSERT_RELEASE(offset_ + data.size() <= public_input_vector_.size(), "Not enough space.");
    std::copy(data.begin(), data.end(), public_input_vector_.begin() + offset_);
    offset_ += data.size();
  }

  const std::vector<std::byte>& GetSerializedVector() const {
    ASSERT_RELEASE(
        offset_ == public_input_vector_.size(), "offset is not equal to size of vector.");
    return public_input_vector_;
  }

 private:
  size_t offset_ = 0;
  std::vector<std::byte> public_input_vector_;
};

}  // namespace starkware

#endif  // STARKWARE_STATEMENT_STATEMENT_H_
