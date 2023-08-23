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

#include "starkware/statement/cpu/cpu_air_statement.h"

#include "starkware/air/cpu/board/cpu_air_definition.h"

#include <fstream>
#include <map>
#include <memory>
#include <utility>

#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/crypt_tools/pedersen.h"
#include "starkware/math/math.h"

namespace starkware {
namespace cpu {

namespace {

using FieldElementT = CpuAirStatement::FieldElementT;
using HashTypes = InvokedTypes<Keccak256, Pedersen>;

std::vector<MemoryAccessUnitData<FieldElementT>> ParsePublicMemory(const JsonValue& public_memory) {
  std::vector<MemoryAccessUnitData<FieldElementT>> res;
  res.reserve(public_memory.ArrayLength());
  for (size_t i = 0; i < public_memory.ArrayLength(); i++) {
    res.push_back(MemoryAccessUnitData<FieldElementT>::FromJson(public_memory[i]));
  }
  return res;
}

MemSegmentAddresses ReadMemorySegments(const JsonValue& json) {
  MemSegmentAddresses addresses;
  for (const auto& name : json.Keys()) {
    addresses[name].begin_addr = json[name]["begin_addr"].AsUint64();
    addresses[name].stop_ptr = json[name]["stop_ptr"].AsUint64();
  }
  return addresses;
}

template <typename Func>
auto InvokeByLayout(const std::string& layout_name, const Func& func) {
  return InvokeGenericTemplateVersion<CpuAirDefinitionInvokedLayoutTypes>(
      func, [&](auto layout_tag) {
        constexpr int kLayoutId = decltype(layout_tag)::type::value;
        return layout_name == CpuAir<FieldElementT, kLayoutId>::kLayoutName;
      });
}

template <typename Func>
auto InvokeByLayout(const std::string& layout_name, Air* air_ptr, const Func& func) {
  return InvokeByLayout(layout_name, [&](auto layout_tag) {
    constexpr int kLayoutId = decltype(layout_tag)::type::value;
    auto* air_derived = dynamic_cast<CpuAir<FieldElementT, kLayoutId>*>(air_ptr);
    ASSERT_RELEASE(air_derived != nullptr, "dynamic_cast failed: air_ptr is of the wrong type.");
    return func(layout_tag, *air_derived);
  });
}

}  // namespace

CpuAirStatement::CpuAirStatement(
    const JsonValue& statement_parameters, const JsonValue& public_input,
    std::optional<JsonValue> private_input)
    : Statement(std::move(private_input)),
      page_hash_(
          statement_parameters.HasValue() && statement_parameters["page_hash"].HasValue()
              ? statement_parameters["page_hash"].AsString()
              : "keccak256"),
      layout_name_(public_input["layout"].AsString()),
      n_steps_(public_input["n_steps"].AsUint64()),
      rc_min_(public_input["rc_min"].AsUint64()),
      rc_max_(public_input["rc_max"].AsUint64()),
      mem_segment_addresses_(ReadMemorySegments(public_input["memory_segments"])),
      public_memory_(ParsePublicMemory(public_input["public_memory"])) {}

const Air& CpuAirStatement::GetAir() {
  air_ = InvokeByLayout(layout_name_, [&](auto layout_tag) -> std::unique_ptr<Air> {
    constexpr int kLayoutId = decltype(layout_tag)::type::value;
    return std::make_unique<CpuAir<FieldElementT, kLayoutId>>(
        n_steps_, public_memory_, rc_min_, rc_max_, mem_segment_addresses_);
  });
  return *air_;
}

const std::vector<std::byte> CpuAirStatement::GetInitialHashChainSeed() const {
  ASSERT_RELEASE(air_ != nullptr, "GetAir() must be called before GetInitialHashChainSeed().");

  std::vector<std::string> segment_names =
      InvokeByLayout(layout_name_, [&](auto layout_tag) -> std::vector<std::string> {
        constexpr int kLayoutId = decltype(layout_tag)::type::value;
        constexpr auto kSegmentNames = CpuAir<FieldElementT, kLayoutId>::kSegmentNames;
        return {kSegmentNames.begin(), kSegmentNames.end()};
      });

  // Get page sizes of the public memory.
  std::map<size_t, size_t> page_sizes;
  for (const auto& unit_data : public_memory_) {
    page_sizes[unit_data.page]++;
  }

  const size_t digest_num_bytes = InvokeGenericTemplateVersion<HashTypes>(
      /*func=*/
      [&](auto hash_tag) {
        using HashT = typename decltype(hash_tag)::type;
        return HashT::kDigestNumBytes;
      },
      /*chooser_func=*/
      [&](auto hash_tag) {
        using HashT = typename decltype(hash_tag)::type;
        return page_hash_ == HashT::HashName();
      });

  PublicInputSerializer serializer(
      (/*n_steps, rc_min, rc_max, layout_name*/ 4) * sizeof(BigInt<4>) +
      segment_names.size() * (/*begin_addr, stop_ptr*/ 2) * sizeof(BigInt<4>) +
      (/*padding_address*/ sizeof(BigInt<4>) + /*padding_value*/ sizeof(FieldElementT)) +
      /*n_pages*/ sizeof(BigInt<4>) +
      (/*First page (no address).*/
       (/*size*/ sizeof(BigInt<4>) + /*hash*/ 1 * digest_num_bytes)) +
      (page_sizes.size() - 1) * ((/*start_addr, size*/ 2) * sizeof(BigInt<4>) +
                                 /*hash*/ 1 * digest_num_bytes));

  serializer.Append(BigInt<4>(SafeLog2(n_steps_)));
  serializer.Append(BigInt<4>(rc_min_));
  serializer.Append(BigInt<4>(rc_max_));

  // Layout.
  serializer.Append(EncodeStringAsBigInt<4>(layout_name_));

  // Serialize segment addresses.
  for (const auto& name : segment_names) {
    ASSERT_RELEASE(
        HasKey(mem_segment_addresses_, name), "Missing segment addresses for '" + name + "'.");
    const auto& segment_addresses = mem_segment_addresses_.at(name);
    serializer.Append(BigInt<4>(segment_addresses.begin_addr));
    serializer.Append(BigInt<4>(segment_addresses.stop_ptr));
  }
  ASSERT_RELEASE(
      mem_segment_addresses_.size() == segment_names.size(),
      "Expected exactly " + std::to_string(segment_names.size()) +
          " items in segment_addresses. Found " + std::to_string(mem_segment_addresses_.size()) +
          ".");

  SerializePublicMemory(&serializer, page_sizes);

  return serializer.GetSerializedVector();
}

void CpuAirStatement::SerializePublicMemory(
    PublicInputSerializer* serializer, const std::map<size_t, size_t>& page_sizes) const {
  // Append the address and the value of the padding cell, which is the first public memory cell.
  // This cell is used to pad the public memory to a power of 2.
  serializer->Append(BigInt<4>(public_memory_[0].address));
  serializer->Append(public_memory_[0].value.ToStandardForm());

  serializer->Append(BigInt<4>(page_sizes.size()));

  // Initialize page serializers with the right size.
  std::map<size_t, PublicInputSerializer> page_serializers;
  for (const auto& [page, size] : page_sizes) {
    if (page == 0) {
      // Page 0 is a non-continuous page (a list of pairs (addr, val)).
      page_serializers.insert({page, PublicInputSerializer(2 * size * sizeof(FieldElementT))});
    } else {
      page_serializers.insert({page, PublicInputSerializer(size * sizeof(FieldElementT))});
    }
  }

  // Store the start address and last seen address for all pages except for page 0.
  std::map<size_t, size_t> page_start_addr;
  std::map<size_t, size_t> page_cur_addr;
  for (const auto& unit_data : public_memory_) {
    PublicInputSerializer& page_serializer = page_serializers.at(unit_data.page);
    if (unit_data.page == 0) {
      page_serializer.Append(BigInt<4>(unit_data.address));
    } else {
      if (HasKey(page_cur_addr, unit_data.page)) {
        ASSERT_RELEASE(
            unit_data.address == page_cur_addr[unit_data.page] + 1,
            "Addresses of public memory must be continuous (address: " +
                std::to_string(unit_data.address) + ").");
        page_cur_addr[unit_data.page]++;
      } else {
        // Initialize page_start_addr and page_cur_addr.
        page_cur_addr[unit_data.page] = page_start_addr[unit_data.page] = unit_data.address;
      }
    }
    page_serializer.Append(unit_data.value.ToStandardForm());
  }

  InvokeGenericTemplateVersion<HashTypes>(
      /*func=*/
      [&](auto hash_tag) {
        using HashT = typename decltype(hash_tag)::type;

        for (const auto& [page, page_serializer] : page_serializers) {
          const auto public_memory_hash =
              HashT::HashBytesWithLength(page_serializer.GetSerializedVector());
          if (page != 0) {
            serializer->Append(BigInt<4>(page_start_addr[page]));
          }
          serializer->Append(BigInt<4>(page_sizes.at(page)));
          serializer->AddBytes(public_memory_hash.GetDigest());
        }
      },
      /*chooser_func=*/
      [&](auto hash_tag) {
        using HashT = typename decltype(hash_tag)::type;
        return page_hash_ == HashT::HashName();
      });
}

std::unique_ptr<TraceContext> CpuAirStatement::GetTraceContext() const {
  ASSERT_RELEASE(private_input_.has_value(), "Missing private input.");

  const std::string& trace_path = (*private_input_)["trace_path"].AsString();
  const std::string& memory_path = (*private_input_)["memory_path"].AsString();
  std::ifstream trace_file(trace_path, std::ios::binary);
  ASSERT_RELEASE(trace_file, "Could not open trace file: \"" + trace_path + "\".");
  std::ifstream memory_file(memory_path, std::ios::binary);
  ASSERT_RELEASE(memory_file, "Could not open memory file: \"" + memory_path + "\".");

  return GetTraceContextFromTraceFile(&trace_file, &memory_file);
}

std::unique_ptr<TraceContext> CpuAirStatement::GetTraceContextFromTraceFile(
    std::istream* trace_file, std::istream* memory_file) const {
  ASSERT_RELEASE(
      air_ != nullptr,
      "Cannot construct trace without a fully initialized AIR instance. Did you forget to "
      "call GetAir()?");

  std::vector<TraceEntry<FieldElementT>> cpu_trace =
      TraceEntry<FieldElementT>::ReadFile(trace_file);

  CpuMemory memory = CpuMemory<FieldElementT>::ReadFile(memory_file);

  ASSERT_RELEASE(private_input_.has_value(), "Missing private input.");
  return InvokeByLayout(
      layout_name_, air_.get(), [&](auto layout_tag, auto& air) -> std::unique_ptr<TraceContext> {
        constexpr int kLayoutId = decltype(layout_tag)::type::value;
        return std::make_unique<CpuAirTraceContext<FieldElementT, kLayoutId>>(
            UseOwned(&air), std::move(cpu_trace), UseMovedValue(std::move(memory)),
            *private_input_);
      });
}

void CpuAirStatement::DisableAssertsForTest() {
  InvokeByLayout(layout_name_, air_.get(), [&](auto /*layout_tag*/, auto& air) {
    air.DisableAssertsForTest();
  });
}

JsonValue CpuAirStatement::FixPublicInput() {
  ASSERT_RELEASE(false, "FixPublicInput() is not implemented for CpuAirStatement.");
}

}  // namespace cpu
}  // namespace starkware
