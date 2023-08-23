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

#include "starkware/air/cpu/board/cpu_air.h"

#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/cpu/board/cpu_air_test_instructions_memory.bin.h"
#include "starkware/air/cpu/board/cpu_air_test_instructions_public_input.json.h"
#include "starkware/air/cpu/board/cpu_air_test_instructions_trace.bin.h"
#include "starkware/air/cpu/board/cpu_air_trace_context.h"
#include "starkware/air/test_utils.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/statement/cpu/cpu_air_statement.h"
#include "starkware/utils/json.h"
#include "starkware/utils/json_builder.h"

namespace starkware {
namespace cpu {
namespace {

using testing::ElementsAreArray;

using FieldElementT = PrimeFieldElement<252, 0>;
using AirT = CpuAir<FieldElementT, 3>;

class CpuAirTest : public ::testing::Test {
 public:
  bool run_test = false;
  Prng prng;
  std::stringstream trace_file = GetCpuAirTestInstructionsTraceStream();
  std::stringstream memory_file = GetCpuAirTestInstructionsMemoryStream();
  JsonBuilder public_input = JsonBuilder::FromJsonValue(
      JsonValue::FromString(GetCpuAirTestInstructionsPublicInputString()));
  std::unique_ptr<CpuAirStatement> statement;
  std::unique_ptr<TraceContext> trace_context;

  CpuAirTest() = default;
  ~CpuAirTest() override { EXPECT_TRUE(run_test); }
  CpuAirTest(const CpuAirTest& other) = delete;
  CpuAirTest& operator=(const CpuAirTest& other) = delete;
  CpuAirTest(CpuAirTest&& other) noexcept = delete;
  CpuAirTest& operator=(CpuAirTest&& other) noexcept = delete;

  JsonValue GetPrivateInput() const {
    JsonBuilder private_input;
    private_input["pedersen"] = JsonValue::EmptyArray();
    private_input["range_check"] = JsonValue::EmptyArray();
    private_input["ecdsa"] = JsonValue::EmptyArray();
    return private_input.Build();
  }

  JsonValue GetParams() const {
    JsonBuilder params;
    params["statement"]["page_hash"] = "keccak256";
    return params.Build();
  }

  /*
    Generates the trace of the AIR.
  */
  Trace GenerateTrace(bool disable_assert_in_memory_write_trace = false) {
    statement = std::make_unique<CpuAirStatement>(
        GetParams()["statement"], public_input.Build(), GetPrivateInput());
    statement->GetAir();
    if (disable_assert_in_memory_write_trace) {
      statement->DisableAssertsForTest();
    }
    trace_context = statement->GetTraceContextFromTraceFile(&trace_file, &memory_file);

    // Construct trace.
    std::vector<Trace> traces;
    traces.reserve(2);
    traces.push_back(trace_context->GetTrace());

    trace_context->SetInteractionElements(
        FieldElementVector::Make(prng.RandomFieldElementVector<FieldElementT>(
            trace_context->GetAir().GetInteractionParams()->n_interaction_elements)));

    // Construct interaction trace.
    traces.push_back(trace_context->GetInteractionTrace());
    return MergeTraces<FieldElementT>(traces);
  }

  void ExpectPass(const Air& air, const Trace& trace) {
    // Verify composition degree.
    const auto random_coefficients = FieldElementVector::Make(
        prng.RandomFieldElementVector<FieldElementT>(air.NumRandomCoefficients()));
    EXPECT_LT(
        ComputeCompositionDegree(air, trace, random_coefficients),
        air.GetCompositionPolynomialDegreeBound());
    run_test = true;
  }

  void ExpectFailingConstraints(
      const Air& air, const Trace& trace, const std::vector<size_t>& expected_failing_constraints) {
    EXPECT_THAT(
        GetFailingConstraintsBinarySearch(air, trace, &prng),
        ElementsAreArray(expected_failing_constraints));
    run_test = true;
  }

  void ExpectTraceGenerationAssert(const std::string& message) {
    EXPECT_ASSERT(GenerateTrace(), testing::HasSubstr(message));
    run_test = true;
  }
};

TEST_F(CpuAirTest, Completeness) {
  Trace trace = GenerateTrace();
  ExpectPass(trace_context->GetAir(), trace);
}

TEST_F(CpuAirTest, WrongInitialAp) {
  auto&& initial_ap = public_input["memory_segments"]["execution"]["begin_addr"];
  initial_ap = initial_ap.Value().AsUint64() + 1;
  Trace trace = GenerateTrace();
  ExpectFailingConstraints(
      trace_context->GetAir(), trace,
      {AirT::kInitialApCond, AirT::kInitialFpCond, AirT::kFinalFpCond});
}

TEST_F(CpuAirTest, WrongFinalAp) {
  auto&& final_ap = public_input["memory_segments"]["execution"]["stop_ptr"];
  final_ap = final_ap.Value().AsUint64() + 1;
  Trace trace = GenerateTrace();
  ExpectFailingConstraints(trace_context->GetAir(), trace, {AirT::kFinalApCond});
}

TEST_F(CpuAirTest, WrongInitialPc) {
  auto&& begin_addr = public_input["memory_segments"]["program"]["begin_addr"];
  begin_addr = begin_addr.Value().AsUint64() + 1;
  Trace trace = GenerateTrace();
  ExpectFailingConstraints(trace_context->GetAir(), trace, {AirT::kInitialPcCond});
}

TEST_F(CpuAirTest, WrongFinalPc) {
  auto&& stop_ptr = public_input["memory_segments"]["program"]["stop_ptr"];
  stop_ptr = stop_ptr.Value().AsUint64() + 1;
  Trace trace = GenerateTrace();
  ExpectFailingConstraints(trace_context->GetAir(), trace, {AirT::kFinalPcCond});
}

TEST_F(CpuAirTest, WrongPublicMemoryExpectAssert) {
  public_input["public_memory"][0]["address"] = 2;
  ExpectTraceGenerationAssert("Problem with memory in row");
}

TEST_F(CpuAirTest, WrongPublicMemoryAssertsDisabled) {
  public_input["public_memory"][0]["address"] = 2;
  Trace trace = GenerateTrace(true);
  ExpectFailingConstraints(trace_context->GetAir(), trace, {AirT::kMemoryIsFuncCond});
}

TEST_F(CpuAirTest, WrongRcMin) {
  public_input["rc_min"] = public_input["rc_min"].Value().AsUint64() + 1;
  ExpectTraceGenerationAssert("Out of range value: 32758, min=32759, max=32769");
}

TEST_F(CpuAirTest, WrongRcMin2) {
  public_input["rc_min"] = public_input["rc_min"].Value().AsUint64() - 500;
  // The range size is rc_max - rc_min + 1 = (32769 - (32758 - 500)) + 1 = 512.
  // There are 16 - 3 free range-checks for each instructions, so the number of filled holes is
  // (16 - 3) * 32 = 416.
  // There are 4 offsets that appear in the trace, so the number of remaining holes is:
  // 512 - 4 - 416 = 92.
  ExpectTraceGenerationAssert(
      "Trace size is not large enough for range-check values. "
      "Range size: 512. Filled Holes: 416. Remaining holes: 92.");
}

TEST_F(CpuAirTest, WrongRcMax) {
  public_input["rc_max"] = public_input["rc_max"].Value().AsUint64() - 1;
  ExpectTraceGenerationAssert("Out of range value: 32769, min=32758, max=32768");
}

TEST_F(CpuAirTest, WrongRcMaxOutOfRange) {
  public_input["rc_max"] = Pow2(AirT::kOffsetBits);
  ExpectTraceGenerationAssert("Invalid value for rc_max: Must be less than 65536.");
}

TEST_F(CpuAirTest, WrongRcMinRcMax) {
  public_input["rc_min"] = public_input["rc_max"].Value().AsUint64() + 1;
  ExpectTraceGenerationAssert("Invalid value for rc_max: Must be >= rc_min.");
}

}  // namespace
}  // namespace cpu
}  // namespace starkware
