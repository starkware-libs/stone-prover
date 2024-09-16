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

#ifndef STARKWARE_AIR_CPU_BUILTIN_MODULO_MOD_BUILTIN_PROVER_CONTEXT_H_
#define STARKWARE_AIR_CPU_BUILTIN_MODULO_MOD_BUILTIN_PROVER_CONTEXT_H_

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/memory/memory.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT, size_t NWords>
class ModBuiltinProverContext {
 public:
  using ValueType = typename FieldElementT::ValueType;
  // The maximum number of bits in a big integer is 384. Increase the type below if more is needed.
  using BigInteger = class BigInt<SafeDiv(384, std::numeric_limits<uint64_t>::digits)>;
  using BigIntegerMult = class BigInt<2 * SafeDiv(384, std::numeric_limits<uint64_t>::digits)>;

  struct BatchSlice {
    uint64_t a_offset;
    uint64_t b_offset;
    uint64_t c_offset;
    std::vector<ValueType> a;
    std::vector<ValueType> b;
    std::vector<ValueType> c;
  };
  struct Input {
    std::vector<ValueType> p;
    uint64_t values_ptr;
    uint64_t offsets_ptr;
    uint64_t n;
    std::vector<BatchSlice> batch;

    Input(
        std::vector<ValueType> p, uint64_t values_ptr, uint64_t offsets_ptr, uint64_t n,
        std::vector<BatchSlice> batch)
        : p(p), values_ptr(values_ptr), offsets_ptr(offsets_ptr), n(n), batch(batch) {}
  };

  ModBuiltinProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>* memory_pool, const uint64_t begin_addr, const size_t n_instances,
      const size_t batch_size, const size_t word_bit_len, const std::map<uint64_t, Input>& inputs)
      : begin_addr_(begin_addr),
        n_instances_(n_instances),
        batch_size_(batch_size),
        word_bit_len_(word_bit_len),
        inputs_(std::move(inputs)),
        mem_p_(InitValue(memory_pool, name + "/p", ctx)),
        mem_values_ptr_(memory_pool, name + "/values_ptr", ctx),
        mem_offsets_ptr_(memory_pool, name + "/offsets_ptr", ctx),
        mem_n_(memory_pool, name + "/n", ctx),
        mem_a_offset_(memory_pool, name + "/a_offset", ctx),
        mem_b_offset_(memory_pool, name + "/b_offset", ctx),
        mem_c_offset_(memory_pool, name + "/c_offset", ctx),
        mem_a_(InitValue(memory_pool, name + "/a", ctx)),
        mem_b_(InitValue(memory_pool, name + "/b", ctx)),
        mem_c_(InitValue(memory_pool, name + "/c", ctx)) {
    ASSERT_RELEASE(
        NWords * word_bit_len_ <= BigInteger::kDigits, "Number of bits larger than " +
                                                           std::to_string(BigInteger::kDigits) +
                                                           " is not implemented.");
  }

  /*
    Parses the private input for the add_mod builtin. private_input should be of the form
    {
      "instances": [
        {
          "index": <index of instance>,
          "p0": <value of p0>,
          ...,
          "values_ptr": <value of values_ptr>,
          "offsets_ptr": <value of offsets_ptr>,
          "n": <value of n>,
          "batch": [
            {
              "a_offset": <value of a_offset>,
              "b_offset": <value of b_offset>,
              "c_offset": <value of c_offset>,
              "a0": <value of a0>,
              ...,
              "b0": <value of b0>,
              ...,
              "c0": <value of c0>,
              ...,
            },
            {...},
            ...
          ]
        },
        {...},
        ...
      ],
      "zero_value_address": address
    }

    Returns an Input object for each instance and an additional zero input at the end.
  */
  static std::map<uint64_t, Input> ParsePrivateInput(
      const JsonValue& private_input, const size_t batch_size);

 protected:
  /*
    Initializes a vector of memory cells.
  */
  static const std::vector<MemoryCellView<FieldElementT>> InitValue(
      MemoryCell<FieldElementT>* memory_pool, const std::string& name,
      const TraceGenerationContext& ctx) {
    std::vector<MemoryCellView<FieldElementT>> res;
    res.reserve(NWords);
    for (size_t i = 0; i < NWords; ++i) {
      res.emplace_back(memory_pool, name + std::to_string(i), ctx);
    }
    return res;
  }

  /*
    Initializes a vector of virtual columns.
  */
  static const std::vector<VirtualColumn> InitVirtualColumns(
      const std::string& name, const std::string& suffix, const TraceGenerationContext& ctx,
      const size_t start, const size_t end) {
    std::vector<VirtualColumn> res;
    res.reserve(end - start);
    for (size_t i = start; i < end; ++i) {
      res.push_back(ctx.GetVirtualColumn(name + std::to_string(i) + suffix));
    }
    return res;
  }

  /*
    Parses a big integer from the input, represented as words.
  */
  static std::vector<ValueType> ParseBigInts(const JsonValue& input, const std::string& name) {
    std::vector<ValueType> res;
    res.reserve(NWords);
    for (size_t i = 0; i < NWords; ++i) {
      res.push_back(ValueType::FromString(input[name + std::to_string(i)].AsString()));
    }
    return res;
  }

  /*
    Used in WriteTrace to write the trace cells for the input of one instance.
    The output is the written input.
  */
  const Input& WriteInput(
      gsl::span<const gsl::span<FieldElementT>> trace, const size_t instance) const;

  inline static const BatchSlice zero_batch_slice_{
      /*a_offset=*/0,
      /*b_offset=*/0,
      /*c_offset=*/0,
      /*a=*/std::vector<ValueType>(NWords, ValueType::Zero()),
      /*b=*/std::vector<ValueType>(NWords, ValueType::Zero()),
      /*c=*/std::vector<ValueType>(NWords, ValueType::Zero())};
  const uint64_t begin_addr_;
  const size_t n_instances_;
  const size_t batch_size_;
  const size_t word_bit_len_;
  const std::map<uint64_t, Input> inputs_;

  /*
    Builtin segment memory cells.
  */
  const std::vector<MemoryCellView<FieldElementT>> mem_p_;
  const MemoryCellView<FieldElementT> mem_values_ptr_;
  const MemoryCellView<FieldElementT> mem_offsets_ptr_;
  const MemoryCellView<FieldElementT> mem_n_;

  /*
    Offset memory cells.
  */
  const MemoryCellView<FieldElementT> mem_a_offset_;
  const MemoryCellView<FieldElementT> mem_b_offset_;
  const MemoryCellView<FieldElementT> mem_c_offset_;

  /*
    Value memory cells.
  */
  const std::vector<MemoryCellView<FieldElementT>> mem_a_;
  const std::vector<MemoryCellView<FieldElementT>> mem_b_;
  const std::vector<MemoryCellView<FieldElementT>> mem_c_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/builtin/modulo/mod_builtin_prover_context.inl"

#endif  // STARKWARE_AIR_CPU_BUILTIN_MODULO_MOD_BUILTIN_PROVER_CONTEXT_H_
