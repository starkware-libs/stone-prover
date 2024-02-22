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

#ifndef STARKWARE_AIR_CPU_BOARD_CPU_AIR7_H_
#define STARKWARE_AIR_CPU_BOARD_CPU_AIR7_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/air.h"
#include "starkware/air/compile_time_optional.h"
#include "starkware/air/components/ecdsa/ecdsa.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/cpu/board/memory_segment.h"
#include "starkware/air/cpu/component/cpu_component.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/elliptic_curve/elliptic_curve.h"
#include "starkware/algebra/elliptic_curve/elliptic_curve_constants.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/crypt_tools/hash_context/pedersen_hash_context.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
class CpuAirDefinition<FieldElementT, 7> : public Air {
 public:
  using FieldElementT_ = FieldElementT;
  using Builder = typename CompositionPolynomialImpl<CpuAirDefinition>::Builder;

  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const FieldElement& trace_generator,
      const ConstFieldElementSpan& random_coefficients) const override;

  uint64_t GetCompositionPolynomialDegreeBound() const override {
    return kConstraintDegree * TraceLength();
  }

  std::vector<std::pair<int64_t, uint64_t>> GetMask() const override;

  uint64_t NumRandomCoefficients() const override { return kNumConstraints; }

  uint64_t NumColumns() const override { return kNumColumns; }

  std::vector<std::vector<FieldElementT>> PrecomputeDomainEvalsOnCoset(
      const FieldElementT& point, const FieldElementT& generator,
      gsl::span<const uint64_t> point_exponents, gsl::span<const FieldElementT> shifts) const;

  FractionFieldElement<FieldElementT> ConstraintsEval(
      gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
      gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
      gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const;

  std::vector<FieldElementT> DomainEvalsAtPoint(
      gsl::span<const FieldElementT> point_powers, gsl::span<const FieldElementT> shifts) const;

  TraceGenerationContext GetTraceGenerationContext() const;

  virtual void BuildPeriodicColumns(const FieldElementT& gen, Builder* builder) const = 0;

  std::optional<InteractionParams> GetInteractionParams() const override {
    InteractionParams interaction_params{kNumColumnsFirst, kNumColumnsSecond, 6};
    return interaction_params;
  }

  static constexpr uint64_t kNumColumnsFirst = 10;
  static constexpr uint64_t kNumColumnsSecond = 3;

  static constexpr uint64_t kPublicMemoryStep = 16;
  static constexpr bool kHasDilutedPool = true;
  static constexpr uint64_t kDilutedSpacing = 4;
  static constexpr uint64_t kDilutedNBits = 16;
  static constexpr uint64_t kPedersenBuiltinRatio = 32;
  static constexpr uint64_t kPedersenBuiltinRepetitions = 1;
  static constexpr uint64_t kRcBuiltinRatio = 8;
  static constexpr uint64_t kRcNParts = 8;
  static constexpr uint64_t kBitwiseRatio = 8;
  static constexpr uint64_t kBitwiseTotalNBits = 251;
  static constexpr bool kHasOutputBuiltin = true;
  static constexpr bool kHasPedersenBuiltin = true;
  static constexpr bool kHasRangeCheckBuiltin = true;
  static constexpr bool kHasEcdsaBuiltin = false;
  static constexpr bool kHasBitwiseBuiltin = true;
  static constexpr bool kHasEcOpBuiltin = false;
  static constexpr bool kHasKeccakBuiltin = false;
  static constexpr bool kHasPoseidonBuiltin = false;
  static constexpr char kLayoutName[] = "recursive_large_output";
  static constexpr BigInt<4> kLayoutCode = 0x7265637572736976655f6c617267655f6f7574707574_Z;
  static constexpr uint64_t kConstraintDegree = 2;
  static constexpr uint64_t kCpuComponentHeight = 16;
  static constexpr uint64_t kLogCpuComponentHeight = 4;
  static constexpr uint64_t kMemoryStep = 2;
  static constexpr std::array<std::string_view, 6> kSegmentNames = {
      "program", "execution", "output", "pedersen", "range_check", "bitwise"};

  enum Columns {
    kColumn0Column,
    kColumn1Column,
    kColumn2Column,
    kColumn3Column,
    kColumn4Column,
    kColumn5Column,
    kColumn6Column,
    kColumn7Column,
    kColumn8Column,
    kColumn9Column,
    kColumn10Inter1Column,
    kColumn11Inter1Column,
    kColumn12Inter1Column,
    // Number of columns.
    kNumColumns,
  };

  enum PeriodicColumns {
    kPedersenPointsXPeriodicColumn,
    kPedersenPointsYPeriodicColumn,
    // Number of periodic columns.
    kNumPeriodicColumns,
  };

  enum Neighbors {
    kColumn0Row0Neighbor,
    kColumn0Row1Neighbor,
    kColumn0Row2Neighbor,
    kColumn0Row3Neighbor,
    kColumn0Row4Neighbor,
    kColumn0Row5Neighbor,
    kColumn0Row6Neighbor,
    kColumn0Row7Neighbor,
    kColumn0Row8Neighbor,
    kColumn0Row9Neighbor,
    kColumn0Row10Neighbor,
    kColumn0Row11Neighbor,
    kColumn0Row12Neighbor,
    kColumn0Row13Neighbor,
    kColumn0Row14Neighbor,
    kColumn0Row15Neighbor,
    kColumn1Row0Neighbor,
    kColumn1Row1Neighbor,
    kColumn1Row2Neighbor,
    kColumn1Row4Neighbor,
    kColumn1Row6Neighbor,
    kColumn1Row8Neighbor,
    kColumn1Row10Neighbor,
    kColumn1Row12Neighbor,
    kColumn1Row14Neighbor,
    kColumn1Row16Neighbor,
    kColumn1Row18Neighbor,
    kColumn1Row20Neighbor,
    kColumn1Row22Neighbor,
    kColumn1Row24Neighbor,
    kColumn1Row26Neighbor,
    kColumn1Row28Neighbor,
    kColumn1Row30Neighbor,
    kColumn1Row32Neighbor,
    kColumn1Row33Neighbor,
    kColumn1Row64Neighbor,
    kColumn1Row65Neighbor,
    kColumn1Row88Neighbor,
    kColumn1Row90Neighbor,
    kColumn1Row92Neighbor,
    kColumn1Row94Neighbor,
    kColumn1Row96Neighbor,
    kColumn1Row97Neighbor,
    kColumn1Row120Neighbor,
    kColumn1Row122Neighbor,
    kColumn1Row124Neighbor,
    kColumn1Row126Neighbor,
    kColumn2Row0Neighbor,
    kColumn2Row1Neighbor,
    kColumn3Row0Neighbor,
    kColumn3Row1Neighbor,
    kColumn3Row255Neighbor,
    kColumn3Row256Neighbor,
    kColumn3Row511Neighbor,
    kColumn4Row0Neighbor,
    kColumn4Row1Neighbor,
    kColumn4Row255Neighbor,
    kColumn4Row256Neighbor,
    kColumn5Row0Neighbor,
    kColumn5Row1Neighbor,
    kColumn5Row192Neighbor,
    kColumn5Row193Neighbor,
    kColumn5Row196Neighbor,
    kColumn5Row197Neighbor,
    kColumn5Row251Neighbor,
    kColumn5Row252Neighbor,
    kColumn5Row256Neighbor,
    kColumn6Row0Neighbor,
    kColumn6Row255Neighbor,
    kColumn7Row0Neighbor,
    kColumn7Row1Neighbor,
    kColumn7Row2Neighbor,
    kColumn7Row3Neighbor,
    kColumn7Row4Neighbor,
    kColumn7Row5Neighbor,
    kColumn7Row8Neighbor,
    kColumn7Row9Neighbor,
    kColumn7Row10Neighbor,
    kColumn7Row11Neighbor,
    kColumn7Row12Neighbor,
    kColumn7Row13Neighbor,
    kColumn7Row16Neighbor,
    kColumn7Row26Neighbor,
    kColumn7Row27Neighbor,
    kColumn7Row42Neighbor,
    kColumn7Row43Neighbor,
    kColumn7Row58Neighbor,
    kColumn7Row74Neighbor,
    kColumn7Row75Neighbor,
    kColumn7Row91Neighbor,
    kColumn7Row122Neighbor,
    kColumn7Row123Neighbor,
    kColumn7Row138Neighbor,
    kColumn7Row139Neighbor,
    kColumn7Row154Neighbor,
    kColumn7Row202Neighbor,
    kColumn7Row266Neighbor,
    kColumn7Row267Neighbor,
    kColumn7Row522Neighbor,
    kColumn8Row0Neighbor,
    kColumn8Row1Neighbor,
    kColumn8Row2Neighbor,
    kColumn8Row3Neighbor,
    kColumn9Row0Neighbor,
    kColumn9Row1Neighbor,
    kColumn9Row2Neighbor,
    kColumn9Row3Neighbor,
    kColumn9Row4Neighbor,
    kColumn9Row5Neighbor,
    kColumn9Row6Neighbor,
    kColumn9Row7Neighbor,
    kColumn9Row8Neighbor,
    kColumn9Row9Neighbor,
    kColumn9Row11Neighbor,
    kColumn9Row12Neighbor,
    kColumn9Row13Neighbor,
    kColumn9Row17Neighbor,
    kColumn9Row25Neighbor,
    kColumn9Row28Neighbor,
    kColumn9Row44Neighbor,
    kColumn9Row60Neighbor,
    kColumn9Row76Neighbor,
    kColumn9Row92Neighbor,
    kColumn9Row108Neighbor,
    kColumn9Row124Neighbor,
    kColumn10Inter1Row0Neighbor,
    kColumn10Inter1Row1Neighbor,
    kColumn11Inter1Row0Neighbor,
    kColumn11Inter1Row1Neighbor,
    kColumn12Inter1Row0Neighbor,
    kColumn12Inter1Row1Neighbor,
    kColumn12Inter1Row2Neighbor,
    kColumn12Inter1Row5Neighbor,
    // Number of neighbors.
    kNumNeighbors,
  };

  enum Constraints {
    kCpuDecodeOpcodeRcBitCond,                                      // Constraint 0.
    kCpuDecodeOpcodeRcZeroCond,                                     // Constraint 1.
    kCpuDecodeOpcodeRcInputCond,                                    // Constraint 2.
    kCpuDecodeFlagOp1BaseOp0BitCond,                                // Constraint 3.
    kCpuDecodeFlagResOp1BitCond,                                    // Constraint 4.
    kCpuDecodeFlagPcUpdateRegularBitCond,                           // Constraint 5.
    kCpuDecodeFpUpdateRegularBitCond,                               // Constraint 6.
    kCpuOperandsMemDstAddrCond,                                     // Constraint 7.
    kCpuOperandsMem0AddrCond,                                       // Constraint 8.
    kCpuOperandsMem1AddrCond,                                       // Constraint 9.
    kCpuOperandsOpsMulCond,                                         // Constraint 10.
    kCpuOperandsResCond,                                            // Constraint 11.
    kCpuUpdateRegistersUpdatePcTmp0Cond,                            // Constraint 12.
    kCpuUpdateRegistersUpdatePcTmp1Cond,                            // Constraint 13.
    kCpuUpdateRegistersUpdatePcPcCondNegativeCond,                  // Constraint 14.
    kCpuUpdateRegistersUpdatePcPcCondPositiveCond,                  // Constraint 15.
    kCpuUpdateRegistersUpdateApApUpdateCond,                        // Constraint 16.
    kCpuUpdateRegistersUpdateFpFpUpdateCond,                        // Constraint 17.
    kCpuOpcodesCallPushFpCond,                                      // Constraint 18.
    kCpuOpcodesCallPushPcCond,                                      // Constraint 19.
    kCpuOpcodesCallOff0Cond,                                        // Constraint 20.
    kCpuOpcodesCallOff1Cond,                                        // Constraint 21.
    kCpuOpcodesCallFlagsCond,                                       // Constraint 22.
    kCpuOpcodesRetOff0Cond,                                         // Constraint 23.
    kCpuOpcodesRetOff2Cond,                                         // Constraint 24.
    kCpuOpcodesRetFlagsCond,                                        // Constraint 25.
    kCpuOpcodesAssertEqAssertEqCond,                                // Constraint 26.
    kInitialApCond,                                                 // Constraint 27.
    kInitialFpCond,                                                 // Constraint 28.
    kInitialPcCond,                                                 // Constraint 29.
    kFinalApCond,                                                   // Constraint 30.
    kFinalFpCond,                                                   // Constraint 31.
    kFinalPcCond,                                                   // Constraint 32.
    kMemoryMultiColumnPermPermInit0Cond,                            // Constraint 33.
    kMemoryMultiColumnPermPermStep0Cond,                            // Constraint 34.
    kMemoryMultiColumnPermPermLastCond,                             // Constraint 35.
    kMemoryDiffIsBitCond,                                           // Constraint 36.
    kMemoryIsFuncCond,                                              // Constraint 37.
    kMemoryInitialAddrCond,                                         // Constraint 38.
    kPublicMemoryAddrZeroCond,                                      // Constraint 39.
    kPublicMemoryValueZeroCond,                                     // Constraint 40.
    kRc16PermInit0Cond,                                             // Constraint 41.
    kRc16PermStep0Cond,                                             // Constraint 42.
    kRc16PermLastCond,                                              // Constraint 43.
    kRc16DiffIsBitCond,                                             // Constraint 44.
    kRc16MinimumCond,                                               // Constraint 45.
    kRc16MaximumCond,                                               // Constraint 46.
    kDilutedCheckPermutationInit0Cond,                              // Constraint 47.
    kDilutedCheckPermutationStep0Cond,                              // Constraint 48.
    kDilutedCheckPermutationLastCond,                               // Constraint 49.
    kDilutedCheckInitCond,                                          // Constraint 50.
    kDilutedCheckFirstElementCond,                                  // Constraint 51.
    kDilutedCheckStepCond,                                          // Constraint 52.
    kDilutedCheckLastCond,                                          // Constraint 53.
    kPedersenHash0EcSubsetSumBitUnpackingLastOneIsZeroCond,         // Constraint 54.
    kPedersenHash0EcSubsetSumBitUnpackingZeroesBetweenOnes0Cond,    // Constraint 55.
    kPedersenHash0EcSubsetSumBitUnpackingCumulativeBit192Cond,      // Constraint 56.
    kPedersenHash0EcSubsetSumBitUnpackingZeroesBetweenOnes192Cond,  // Constraint 57.
    kPedersenHash0EcSubsetSumBitUnpackingCumulativeBit196Cond,      // Constraint 58.
    kPedersenHash0EcSubsetSumBitUnpackingZeroesBetweenOnes196Cond,  // Constraint 59.
    kPedersenHash0EcSubsetSumBooleanityTestCond,                    // Constraint 60.
    kPedersenHash0EcSubsetSumBitExtractionEndCond,                  // Constraint 61.
    kPedersenHash0EcSubsetSumZerosTailCond,                         // Constraint 62.
    kPedersenHash0EcSubsetSumAddPointsSlopeCond,                    // Constraint 63.
    kPedersenHash0EcSubsetSumAddPointsXCond,                        // Constraint 64.
    kPedersenHash0EcSubsetSumAddPointsYCond,                        // Constraint 65.
    kPedersenHash0EcSubsetSumCopyPointXCond,                        // Constraint 66.
    kPedersenHash0EcSubsetSumCopyPointYCond,                        // Constraint 67.
    kPedersenHash0CopyPointXCond,                                   // Constraint 68.
    kPedersenHash0CopyPointYCond,                                   // Constraint 69.
    kPedersenHash0InitXCond,                                        // Constraint 70.
    kPedersenHash0InitYCond,                                        // Constraint 71.
    kPedersenInput0Value0Cond,                                      // Constraint 72.
    kPedersenInput0AddrCond,                                        // Constraint 73.
    kPedersenInitAddrCond,                                          // Constraint 74.
    kPedersenInput1Value0Cond,                                      // Constraint 75.
    kPedersenInput1AddrCond,                                        // Constraint 76.
    kPedersenOutputValue0Cond,                                      // Constraint 77.
    kPedersenOutputAddrCond,                                        // Constraint 78.
    kRcBuiltinValueCond,                                            // Constraint 79.
    kRcBuiltinAddrStepCond,                                         // Constraint 80.
    kRcBuiltinInitAddrCond,                                         // Constraint 81.
    kBitwiseInitVarPoolAddrCond,                                    // Constraint 82.
    kBitwiseStepVarPoolAddrCond,                                    // Constraint 83.
    kBitwiseXOrYAddrCond,                                           // Constraint 84.
    kBitwiseNextVarPoolAddrCond,                                    // Constraint 85.
    kBitwisePartitionCond,                                          // Constraint 86.
    kBitwiseOrIsAndPlusXorCond,                                     // Constraint 87.
    kBitwiseAdditionIsXorWithAndCond,                               // Constraint 88.
    kBitwiseUniqueUnpacking192Cond,                                 // Constraint 89.
    kBitwiseUniqueUnpacking193Cond,                                 // Constraint 90.
    kBitwiseUniqueUnpacking194Cond,                                 // Constraint 91.
    kBitwiseUniqueUnpacking195Cond,                                 // Constraint 92.
    kNumConstraints,                                                // Number of constraints.
  };

 public:
  using EcPointT = EcPoint<FieldElementT>;
  using HashContextT = PedersenHashContext<FieldElementT>;
  using SigConfigT = typename EcdsaComponent<FieldElementT>::Config;
  using EcOpCurveConfigT = typename EllipticCurveConstants<FieldElementT>::CurveConfig;

  explicit CpuAirDefinition(
      uint64_t trace_length, const FieldElementT& rc_min, const FieldElementT& rc_max,
      const MemSegmentAddresses& mem_segment_addresses,
      const HashContextT& hash_context)
      : Air(trace_length),
        initial_ap_(
            FieldElementT::FromUint(GetSegment(mem_segment_addresses, "execution").begin_addr)),
        final_ap_(FieldElementT::FromUint(GetSegment(mem_segment_addresses, "execution").stop_ptr)),
        initial_pc_(
            FieldElementT::FromUint(GetSegment(mem_segment_addresses, "program").begin_addr)),
        final_pc_(FieldElementT::FromUint(GetSegment(mem_segment_addresses, "program").stop_ptr)),
        pedersen_begin_addr_(
            kHasPedersenBuiltin ? GetSegment(mem_segment_addresses, "pedersen").begin_addr : 0),
        rc_begin_addr_(
            kHasRangeCheckBuiltin ? GetSegment(mem_segment_addresses, "range_check").begin_addr
                                  : 0),
        ecdsa_begin_addr_(
            kHasEcdsaBuiltin ? GetSegment(mem_segment_addresses, "ecdsa").begin_addr : 0),
        bitwise_begin_addr_(
            kHasBitwiseBuiltin ? GetSegment(mem_segment_addresses, "bitwise").begin_addr : 0),
        ec_op_begin_addr_(
            kHasEcOpBuiltin ? GetSegment(mem_segment_addresses, "ec_op").begin_addr : 0),
        keccak_begin_addr_(
            kHasKeccakBuiltin ? GetSegment(mem_segment_addresses, "keccak").begin_addr : 0),
        poseidon_begin_addr_(
            kHasPoseidonBuiltin ? GetSegment(mem_segment_addresses, "poseidon").begin_addr : 0),
        rc_min_(rc_min),
        rc_max_(rc_max),
        pedersen__shift_point_(hash_context.shift_point),
        ecdsa__sig_config_(EcdsaComponent<FieldElementT>::GetSigConfig()),
        ec_op__curve_config_{
            kPrimeFieldEc0.k_alpha, kPrimeFieldEc0.k_beta, kPrimeFieldEc0.k_order} {}

  static constexpr uint64_t kOffsetBits = CpuComponent<FieldElementT>::kOffsetBits;

 protected:
  const FieldElementT offset_size_ = FieldElementT::FromUint(Pow2(kOffsetBits));
  const FieldElementT half_offset_size_ = FieldElementT::FromUint(Pow2(kOffsetBits - 1));
  const FieldElementT initial_ap_;
  const FieldElementT final_ap_;
  const FieldElementT initial_pc_;
  const FieldElementT final_pc_;

  const CompileTimeOptional<uint64_t, kHasPedersenBuiltin> pedersen_begin_addr_;
  const CompileTimeOptional<FieldElementT, kHasPedersenBuiltin> initial_pedersen_addr_ =
      FieldElementT::FromUint(ExtractHiddenMemberValue(pedersen_begin_addr_));

  const CompileTimeOptional<uint64_t, kHasRangeCheckBuiltin> rc_begin_addr_;
  const CompileTimeOptional<FieldElementT, kHasRangeCheckBuiltin> initial_rc_addr_ =
      FieldElementT::FromUint(ExtractHiddenMemberValue(rc_begin_addr_));

  const CompileTimeOptional<uint64_t, kHasEcdsaBuiltin> ecdsa_begin_addr_;
  const CompileTimeOptional<FieldElementT, kHasEcdsaBuiltin> initial_ecdsa_addr_ =
      FieldElementT::FromUint(ExtractHiddenMemberValue(ecdsa_begin_addr_));

  const CompileTimeOptional<uint64_t, kHasBitwiseBuiltin> bitwise_begin_addr_;
  const CompileTimeOptional<FieldElementT, kHasBitwiseBuiltin> initial_bitwise_addr_ =
      FieldElementT::FromUint(ExtractHiddenMemberValue(bitwise_begin_addr_));

  const CompileTimeOptional<uint64_t, kHasEcOpBuiltin> ec_op_begin_addr_;
  const CompileTimeOptional<FieldElementT, kHasEcOpBuiltin> initial_ec_op_addr_ =
      FieldElementT::FromUint(ExtractHiddenMemberValue(ec_op_begin_addr_));

  const CompileTimeOptional<uint64_t, kHasKeccakBuiltin> keccak_begin_addr_;
  const CompileTimeOptional<FieldElementT, kHasKeccakBuiltin> initial_keccak_addr_ =
      FieldElementT::FromUint(ExtractHiddenMemberValue(keccak_begin_addr_));

  const CompileTimeOptional<uint64_t, kHasPoseidonBuiltin> poseidon_begin_addr_;
  const CompileTimeOptional<FieldElementT, kHasPoseidonBuiltin> initial_poseidon_addr_ =
      FieldElementT::FromUint(ExtractHiddenMemberValue(poseidon_begin_addr_));

  const FieldElementT rc_min_;
  const FieldElementT rc_max_;
  const EcPointT pedersen__shift_point_;
  const SigConfigT ecdsa__sig_config_;
  const EcOpCurveConfigT ec_op__curve_config_;

  // Interaction elements.
  FieldElementT memory__multi_column_perm__perm__interaction_elm_ = FieldElementT::Uninitialized();
  FieldElementT memory__multi_column_perm__hash_interaction_elm0_ = FieldElementT::Uninitialized();
  FieldElementT rc16__perm__interaction_elm_ = FieldElementT::Uninitialized();
  CompileTimeOptional<FieldElementT, kHasDilutedPool> diluted_check__permutation__interaction_elm_ =
      FieldElementT::Uninitialized();
  CompileTimeOptional<FieldElementT, kHasDilutedPool> diluted_check__interaction_z_ =
      FieldElementT::Uninitialized();
  CompileTimeOptional<FieldElementT, kHasDilutedPool> diluted_check__interaction_alpha_ =
      FieldElementT::Uninitialized();

  FieldElementT memory__multi_column_perm__perm__public_memory_prod_ =
      FieldElementT::Uninitialized();
  const FieldElementT rc16__perm__public_memory_prod_ = FieldElementT::One();
  const CompileTimeOptional<FieldElementT, kHasDilutedPool> diluted_check__first_elm_ =
      FieldElementT::Zero();
  const CompileTimeOptional<FieldElementT, kHasDilutedPool>
      diluted_check__permutation__public_memory_prod_ = FieldElementT::One();
  CompileTimeOptional<FieldElementT, kHasDilutedPool> diluted_check__final_cum_val_ =
      FieldElementT::Uninitialized();
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/board/cpu_air_definition7.inl"

#endif  // STARKWARE_AIR_CPU_BOARD_CPU_AIR7_H_
