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

#ifndef STARKWARE_AIR_CPU_BOARD_CPU_AIR2_H_
#define STARKWARE_AIR_CPU_BOARD_CPU_AIR2_H_

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
class CpuAirDefinition<FieldElementT, 2> : public Air {
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
    InteractionParams interaction_params{kNumColumnsFirst, kNumColumnsSecond, 3};
    return interaction_params;
  }

  static constexpr uint64_t kNumColumnsFirst = 9;
  static constexpr uint64_t kNumColumnsSecond = 1;

  static constexpr uint64_t kPublicMemoryStep = 8;
  static constexpr bool kHasDilutedPool = false;
  static constexpr uint64_t kPedersenBuiltinRatio = 32;
  static constexpr uint64_t kPedersenBuiltinRepetitions = 1;
  static constexpr uint64_t kRcBuiltinRatio = 16;
  static constexpr uint64_t kRcNParts = 8;
  static constexpr uint64_t kEcdsaBuiltinRatio = 2048;
  static constexpr uint64_t kEcdsaBuiltinRepetitions = 1;
  static constexpr uint64_t kEcdsaElementBits = 251;
  static constexpr uint64_t kEcdsaElementHeight = 256;
  static constexpr bool kHasOutputBuiltin = true;
  static constexpr bool kHasPedersenBuiltin = true;
  static constexpr bool kHasRangeCheckBuiltin = true;
  static constexpr bool kHasEcdsaBuiltin = true;
  static constexpr bool kHasBitwiseBuiltin = false;
  static constexpr bool kHasEcOpBuiltin = false;
  static constexpr bool kHasKeccakBuiltin = false;
  static constexpr bool kHasPoseidonBuiltin = false;
  static constexpr char kLayoutName[] = "perpetual";
  static constexpr BigInt<4> kLayoutCode = 0x70657270657475616c_Z;
  static constexpr uint64_t kConstraintDegree = 2;
  static constexpr uint64_t kCpuComponentHeight = 16;
  static constexpr uint64_t kLogCpuComponentHeight = 4;
  static constexpr uint64_t kMemoryStep = 2;
  static constexpr std::array<std::string_view, 6> kSegmentNames = {
      "program", "execution", "output", "pedersen", "range_check", "ecdsa"};

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
    kColumn9Inter1Column,
    // Number of columns.
    kNumColumns,
  };

  enum PeriodicColumns {
    kPedersenPointsXPeriodicColumn,
    kPedersenPointsYPeriodicColumn,
    kEcdsaGeneratorPointsXPeriodicColumn,
    kEcdsaGeneratorPointsYPeriodicColumn,
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
    kColumn1Row255Neighbor,
    kColumn1Row256Neighbor,
    kColumn1Row511Neighbor,
    kColumn2Row0Neighbor,
    kColumn2Row1Neighbor,
    kColumn2Row255Neighbor,
    kColumn2Row256Neighbor,
    kColumn3Row0Neighbor,
    kColumn3Row1Neighbor,
    kColumn3Row192Neighbor,
    kColumn3Row193Neighbor,
    kColumn3Row196Neighbor,
    kColumn3Row197Neighbor,
    kColumn3Row251Neighbor,
    kColumn3Row252Neighbor,
    kColumn3Row256Neighbor,
    kColumn4Row0Neighbor,
    kColumn4Row255Neighbor,
    kColumn5Row0Neighbor,
    kColumn5Row1Neighbor,
    kColumn5Row2Neighbor,
    kColumn5Row3Neighbor,
    kColumn5Row4Neighbor,
    kColumn5Row5Neighbor,
    kColumn5Row6Neighbor,
    kColumn5Row7Neighbor,
    kColumn5Row8Neighbor,
    kColumn5Row9Neighbor,
    kColumn5Row12Neighbor,
    kColumn5Row13Neighbor,
    kColumn5Row16Neighbor,
    kColumn5Row70Neighbor,
    kColumn5Row71Neighbor,
    kColumn5Row134Neighbor,
    kColumn5Row135Neighbor,
    kColumn5Row262Neighbor,
    kColumn5Row263Neighbor,
    kColumn5Row326Neighbor,
    kColumn5Row390Neighbor,
    kColumn5Row391Neighbor,
    kColumn5Row518Neighbor,
    kColumn5Row16774Neighbor,
    kColumn5Row16775Neighbor,
    kColumn5Row33158Neighbor,
    kColumn6Row0Neighbor,
    kColumn6Row1Neighbor,
    kColumn6Row2Neighbor,
    kColumn6Row3Neighbor,
    kColumn7Row0Neighbor,
    kColumn7Row1Neighbor,
    kColumn7Row2Neighbor,
    kColumn7Row3Neighbor,
    kColumn7Row4Neighbor,
    kColumn7Row5Neighbor,
    kColumn7Row6Neighbor,
    kColumn7Row7Neighbor,
    kColumn7Row8Neighbor,
    kColumn7Row9Neighbor,
    kColumn7Row11Neighbor,
    kColumn7Row12Neighbor,
    kColumn7Row13Neighbor,
    kColumn7Row15Neighbor,
    kColumn7Row17Neighbor,
    kColumn7Row23Neighbor,
    kColumn7Row25Neighbor,
    kColumn7Row31Neighbor,
    kColumn7Row39Neighbor,
    kColumn7Row44Neighbor,
    kColumn7Row47Neighbor,
    kColumn7Row55Neighbor,
    kColumn7Row63Neighbor,
    kColumn7Row71Neighbor,
    kColumn7Row76Neighbor,
    kColumn7Row79Neighbor,
    kColumn7Row87Neighbor,
    kColumn7Row103Neighbor,
    kColumn7Row108Neighbor,
    kColumn7Row119Neighbor,
    kColumn7Row140Neighbor,
    kColumn7Row172Neighbor,
    kColumn7Row204Neighbor,
    kColumn7Row236Neighbor,
    kColumn7Row16343Neighbor,
    kColumn7Row16351Neighbor,
    kColumn7Row16367Neighbor,
    kColumn7Row16375Neighbor,
    kColumn7Row16383Neighbor,
    kColumn7Row16391Neighbor,
    kColumn7Row16423Neighbor,
    kColumn7Row32727Neighbor,
    kColumn7Row32735Neighbor,
    kColumn7Row32759Neighbor,
    kColumn7Row32767Neighbor,
    kColumn8Row0Neighbor,
    kColumn8Row16Neighbor,
    kColumn8Row32Neighbor,
    kColumn8Row64Neighbor,
    kColumn8Row80Neighbor,
    kColumn8Row96Neighbor,
    kColumn8Row128Neighbor,
    kColumn8Row160Neighbor,
    kColumn8Row192Neighbor,
    kColumn8Row32640Neighbor,
    kColumn8Row32656Neighbor,
    kColumn8Row32704Neighbor,
    kColumn8Row32736Neighbor,
    kColumn9Inter1Row0Neighbor,
    kColumn9Inter1Row1Neighbor,
    kColumn9Inter1Row2Neighbor,
    kColumn9Inter1Row5Neighbor,
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
    kPedersenHash0EcSubsetSumBitUnpackingLastOneIsZeroCond,         // Constraint 47.
    kPedersenHash0EcSubsetSumBitUnpackingZeroesBetweenOnes0Cond,    // Constraint 48.
    kPedersenHash0EcSubsetSumBitUnpackingCumulativeBit192Cond,      // Constraint 49.
    kPedersenHash0EcSubsetSumBitUnpackingZeroesBetweenOnes192Cond,  // Constraint 50.
    kPedersenHash0EcSubsetSumBitUnpackingCumulativeBit196Cond,      // Constraint 51.
    kPedersenHash0EcSubsetSumBitUnpackingZeroesBetweenOnes196Cond,  // Constraint 52.
    kPedersenHash0EcSubsetSumBooleanityTestCond,                    // Constraint 53.
    kPedersenHash0EcSubsetSumBitExtractionEndCond,                  // Constraint 54.
    kPedersenHash0EcSubsetSumZerosTailCond,                         // Constraint 55.
    kPedersenHash0EcSubsetSumAddPointsSlopeCond,                    // Constraint 56.
    kPedersenHash0EcSubsetSumAddPointsXCond,                        // Constraint 57.
    kPedersenHash0EcSubsetSumAddPointsYCond,                        // Constraint 58.
    kPedersenHash0EcSubsetSumCopyPointXCond,                        // Constraint 59.
    kPedersenHash0EcSubsetSumCopyPointYCond,                        // Constraint 60.
    kPedersenHash0CopyPointXCond,                                   // Constraint 61.
    kPedersenHash0CopyPointYCond,                                   // Constraint 62.
    kPedersenHash0InitXCond,                                        // Constraint 63.
    kPedersenHash0InitYCond,                                        // Constraint 64.
    kPedersenInput0Value0Cond,                                      // Constraint 65.
    kPedersenInput0AddrCond,                                        // Constraint 66.
    kPedersenInitAddrCond,                                          // Constraint 67.
    kPedersenInput1Value0Cond,                                      // Constraint 68.
    kPedersenInput1AddrCond,                                        // Constraint 69.
    kPedersenOutputValue0Cond,                                      // Constraint 70.
    kPedersenOutputAddrCond,                                        // Constraint 71.
    kRcBuiltinValueCond,                                            // Constraint 72.
    kRcBuiltinAddrStepCond,                                         // Constraint 73.
    kRcBuiltinInitAddrCond,                                         // Constraint 74.
    kEcdsaSignature0DoublingKeySlopeCond,                           // Constraint 75.
    kEcdsaSignature0DoublingKeyXCond,                               // Constraint 76.
    kEcdsaSignature0DoublingKeyYCond,                               // Constraint 77.
    kEcdsaSignature0ExponentiateGeneratorBooleanityTestCond,        // Constraint 78.
    kEcdsaSignature0ExponentiateGeneratorBitExtractionEndCond,      // Constraint 79.
    kEcdsaSignature0ExponentiateGeneratorZerosTailCond,             // Constraint 80.
    kEcdsaSignature0ExponentiateGeneratorAddPointsSlopeCond,        // Constraint 81.
    kEcdsaSignature0ExponentiateGeneratorAddPointsXCond,            // Constraint 82.
    kEcdsaSignature0ExponentiateGeneratorAddPointsYCond,            // Constraint 83.
    kEcdsaSignature0ExponentiateGeneratorAddPointsXDiffInvCond,     // Constraint 84.
    kEcdsaSignature0ExponentiateGeneratorCopyPointXCond,            // Constraint 85.
    kEcdsaSignature0ExponentiateGeneratorCopyPointYCond,            // Constraint 86.
    kEcdsaSignature0ExponentiateKeyBooleanityTestCond,              // Constraint 87.
    kEcdsaSignature0ExponentiateKeyBitExtractionEndCond,            // Constraint 88.
    kEcdsaSignature0ExponentiateKeyZerosTailCond,                   // Constraint 89.
    kEcdsaSignature0ExponentiateKeyAddPointsSlopeCond,              // Constraint 90.
    kEcdsaSignature0ExponentiateKeyAddPointsXCond,                  // Constraint 91.
    kEcdsaSignature0ExponentiateKeyAddPointsYCond,                  // Constraint 92.
    kEcdsaSignature0ExponentiateKeyAddPointsXDiffInvCond,           // Constraint 93.
    kEcdsaSignature0ExponentiateKeyCopyPointXCond,                  // Constraint 94.
    kEcdsaSignature0ExponentiateKeyCopyPointYCond,                  // Constraint 95.
    kEcdsaSignature0InitGenXCond,                                   // Constraint 96.
    kEcdsaSignature0InitGenYCond,                                   // Constraint 97.
    kEcdsaSignature0InitKeyXCond,                                   // Constraint 98.
    kEcdsaSignature0InitKeyYCond,                                   // Constraint 99.
    kEcdsaSignature0AddResultsSlopeCond,                            // Constraint 100.
    kEcdsaSignature0AddResultsXCond,                                // Constraint 101.
    kEcdsaSignature0AddResultsYCond,                                // Constraint 102.
    kEcdsaSignature0AddResultsXDiffInvCond,                         // Constraint 103.
    kEcdsaSignature0ExtractRSlopeCond,                              // Constraint 104.
    kEcdsaSignature0ExtractRXCond,                                  // Constraint 105.
    kEcdsaSignature0ExtractRXDiffInvCond,                           // Constraint 106.
    kEcdsaSignature0ZNonzeroCond,                                   // Constraint 107.
    kEcdsaSignature0RAndWNonzeroCond,                               // Constraint 108.
    kEcdsaSignature0QOnCurveXSquaredCond,                           // Constraint 109.
    kEcdsaSignature0QOnCurveOnCurveCond,                            // Constraint 110.
    kEcdsaInitAddrCond,                                             // Constraint 111.
    kEcdsaMessageAddrCond,                                          // Constraint 112.
    kEcdsaPubkeyAddrCond,                                           // Constraint 113.
    kEcdsaMessageValue0Cond,                                        // Constraint 114.
    kEcdsaPubkeyValue0Cond,                                         // Constraint 115.
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

#include "starkware/air/cpu/board/cpu_air_definition2.inl"

#endif  // STARKWARE_AIR_CPU_BOARD_CPU_AIR2_H_
