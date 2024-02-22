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

#ifndef STARKWARE_AIR_COMPONENTS_PEDERSEN_HASH_PEDERSEN_HASH_H_
#define STARKWARE_AIR_COMPONENTS_PEDERSEN_HASH_PEDERSEN_HASH_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/ec_subset_sum/ec_subset_sum.h"
#include "starkware/air/components/hash/hash_component.h"
#include "starkware/air/components/hash/hash_factory.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/air/trace.h"
#include "starkware/crypt_tools/hash_context/pedersen_hash_context.h"

namespace starkware {

template <typename FieldElementT>
class PedersenHashComponent : public HashComponent<FieldElementT> {
 public:
  /*
    See starkware/air/components/pedersen_hash/pedersen_hash.py for documentation.
  */
  PedersenHashComponent(
      const std::string& name, const TraceGenerationContext& ctx, bool use_x_diff_inv,
      const PedersenHashContext<FieldElementT>& hash_ctx)
      : ec_subset_sum_(
            name + "/ec_subset_sum", ctx, hash_ctx.ec_subset_sum_height, hash_ctx.n_element_bits,
            use_x_diff_inv, true),
        hash_ctx_(hash_ctx) {
    ASSERT_RELEASE(
        hash_ctx_.points.size() == hash_ctx_.n_element_bits * hash_ctx_.n_inputs,
        "Wrong number of points.");
  }

  /*
    Writes the trace for one instance of the component.
    points_x and points_y should be two lists of ec_subset_sum_height_ * n_inputs_ points each.
    Returns the result of the hash on the given inputs.
    component_index is the index of the component instance.
  */
  FieldElementT WriteTrace(
      gsl::span<const FieldElementT> inputs, uint64_t component_index,
      gsl::span<const gsl::span<FieldElementT>> trace) const override;

  /*
    Returns the configuration of this instance.
  */
  const PedersenHashContext<FieldElementT>& GetHashContext() const override { return hash_ctx_; }

 private:
  /*
    The inner SubsetSum component.
  */
  EcSubsetSumComponent<FieldElementT> ec_subset_sum_;

  /*
    The hash configuration.
  */
  PedersenHashContext<FieldElementT> hash_ctx_;
};

template <typename FieldElementT>
class PedersenHashFactory : public HashFactory<FieldElementT> {
 public:
  PedersenHashFactory(
      const std::string& name, bool use_x_diff_inv, PedersenHashContext<FieldElementT> hash_ctx)
      : HashFactory<FieldElementT>(name),
        use_x_diff_inv_(use_x_diff_inv),
        hash_ctx_(std::move(hash_ctx)) {}

  std::unique_ptr<PedersenHashComponent<FieldElementT>> CreatePedersenHashComponent(
      const std::string& name, const TraceGenerationContext& ctx) const {
    return std::make_unique<PedersenHashComponent<FieldElementT>>(
        name, ctx, use_x_diff_inv_, hash_ctx_);
  }

  std::unique_ptr<HashComponent<FieldElementT>> CreateComponent(
      const std::string& name, const TraceGenerationContext& ctx) const override {
    return CreatePedersenHashComponent(name, ctx);
  }

  std::map<const std::string, std::vector<FieldElementT>> ComputePeriodicColumnValues()
      const override;

 private:
  /*
    See PedersenHashComponent for documentation on the following members.
  */
  bool use_x_diff_inv_;
  PedersenHashContext<FieldElementT> hash_ctx_;
};

}  // namespace starkware

#include "starkware/air/components/pedersen_hash/pedersen_hash.inl"

#endif  // STARKWARE_AIR_COMPONENTS_PEDERSEN_HASH_PEDERSEN_HASH_H_
