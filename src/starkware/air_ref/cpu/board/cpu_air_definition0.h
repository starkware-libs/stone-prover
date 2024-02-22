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

#ifndef STARKWARE_AIR_CPU_BOARD_CPU_AIR0_H_
#define STARKWARE_AIR_CPU_BOARD_CPU_AIR0_H_

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
class CpuAirDefinition<FieldElementT, 0> : public Air {
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

  static constexpr uint64_t kNumColumnsFirst = 23;
  static constexpr uint64_t kNumColumnsSecond = 2;

  static constexpr uint64_t kPublicMemoryStep = 8;
  static constexpr bool kHasDilutedPool = false;
  static constexpr uint64_t kPedersenBuiltinRatio = 8;
  static constexpr uint64_t kPedersenBuiltinRepetitions = 4;
  static constexpr uint64_t kRcBuiltinRatio = 8;
  static constexpr uint64_t kRcNParts = 8;
  static constexpr uint64_t kEcdsaBuiltinRatio = 512;
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
  static constexpr char kLayoutName[] = "small";
  static constexpr BigInt<4> kLayoutCode = 0x736d616c6c_Z;
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
    kColumn9Column,
    kColumn10Column,
    kColumn11Column,
    kColumn12Column,
    kColumn13Column,
    kColumn14Column,
    kColumn15Column,
    kColumn16Column,
    kColumn17Column,
    kColumn18Column,
    kColumn19Column,
    kColumn20Column,
    kColumn21Column,
    kColumn22Column,
    kColumn23Inter1Column,
    kColumn24Inter1Column,
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
    kColumn0Row4Neighbor,
    kColumn0Row8Neighbor,
    kColumn0Row12Neighbor,
    kColumn0Row28Neighbor,
    kColumn0Row44Neighbor,
    kColumn0Row60Neighbor,
    kColumn0Row76Neighbor,
    kColumn0Row92Neighbor,
    kColumn0Row108Neighbor,
    kColumn0Row124Neighbor,
    kColumn1Row0Neighbor,
    kColumn1Row1Neighbor,
    kColumn1Row2Neighbor,
    kColumn1Row3Neighbor,
    kColumn1Row4Neighbor,
    kColumn1Row5Neighbor,
    kColumn1Row6Neighbor,
    kColumn1Row7Neighbor,
    kColumn1Row8Neighbor,
    kColumn1Row9Neighbor,
    kColumn1Row10Neighbor,
    kColumn1Row11Neighbor,
    kColumn1Row12Neighbor,
    kColumn1Row13Neighbor,
    kColumn1Row14Neighbor,
    kColumn1Row15Neighbor,
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
    kColumn6Row1Neighbor,
    kColumn6Row255Neighbor,
    kColumn6Row256Neighbor,
    kColumn6Row511Neighbor,
    kColumn7Row0Neighbor,
    kColumn7Row1Neighbor,
    kColumn7Row255Neighbor,
    kColumn7Row256Neighbor,
    kColumn8Row0Neighbor,
    kColumn8Row1Neighbor,
    kColumn8Row192Neighbor,
    kColumn8Row193Neighbor,
    kColumn8Row196Neighbor,
    kColumn8Row197Neighbor,
    kColumn8Row251Neighbor,
    kColumn8Row252Neighbor,
    kColumn8Row256Neighbor,
    kColumn9Row0Neighbor,
    kColumn9Row1Neighbor,
    kColumn9Row255Neighbor,
    kColumn9Row256Neighbor,
    kColumn9Row511Neighbor,
    kColumn10Row0Neighbor,
    kColumn10Row1Neighbor,
    kColumn10Row255Neighbor,
    kColumn10Row256Neighbor,
    kColumn11Row0Neighbor,
    kColumn11Row1Neighbor,
    kColumn11Row192Neighbor,
    kColumn11Row193Neighbor,
    kColumn11Row196Neighbor,
    kColumn11Row197Neighbor,
    kColumn11Row251Neighbor,
    kColumn11Row252Neighbor,
    kColumn11Row256Neighbor,
    kColumn12Row0Neighbor,
    kColumn12Row1Neighbor,
    kColumn12Row255Neighbor,
    kColumn12Row256Neighbor,
    kColumn12Row511Neighbor,
    kColumn13Row0Neighbor,
    kColumn13Row1Neighbor,
    kColumn13Row255Neighbor,
    kColumn13Row256Neighbor,
    kColumn14Row0Neighbor,
    kColumn14Row1Neighbor,
    kColumn14Row192Neighbor,
    kColumn14Row193Neighbor,
    kColumn14Row196Neighbor,
    kColumn14Row197Neighbor,
    kColumn14Row251Neighbor,
    kColumn14Row252Neighbor,
    kColumn14Row256Neighbor,
    kColumn15Row0Neighbor,
    kColumn15Row255Neighbor,
    kColumn16Row0Neighbor,
    kColumn16Row255Neighbor,
    kColumn17Row0Neighbor,
    kColumn17Row255Neighbor,
    kColumn18Row0Neighbor,
    kColumn18Row255Neighbor,
    kColumn19Row0Neighbor,
    kColumn19Row1Neighbor,
    kColumn19Row2Neighbor,
    kColumn19Row3Neighbor,
    kColumn19Row4Neighbor,
    kColumn19Row5Neighbor,
    kColumn19Row6Neighbor,
    kColumn19Row7Neighbor,
    kColumn19Row8Neighbor,
    kColumn19Row9Neighbor,
    kColumn19Row12Neighbor,
    kColumn19Row13Neighbor,
    kColumn19Row16Neighbor,
    kColumn19Row22Neighbor,
    kColumn19Row23Neighbor,
    kColumn19Row38Neighbor,
    kColumn19Row39Neighbor,
    kColumn19Row70Neighbor,
    kColumn19Row71Neighbor,
    kColumn19Row102Neighbor,
    kColumn19Row103Neighbor,
    kColumn19Row134Neighbor,
    kColumn19Row135Neighbor,
    kColumn19Row167Neighbor,
    kColumn19Row199Neighbor,
    kColumn19Row230Neighbor,
    kColumn19Row263Neighbor,
    kColumn19Row295Neighbor,
    kColumn19Row327Neighbor,
    kColumn19Row391Neighbor,
    kColumn19Row423Neighbor,
    kColumn19Row455Neighbor,
    kColumn19Row4118Neighbor,
    kColumn19Row4119Neighbor,
    kColumn19Row8214Neighbor,
    kColumn20Row0Neighbor,
    kColumn20Row1Neighbor,
    kColumn20Row2Neighbor,
    kColumn20Row3Neighbor,
    kColumn21Row0Neighbor,
    kColumn21Row1Neighbor,
    kColumn21Row2Neighbor,
    kColumn21Row3Neighbor,
    kColumn21Row4Neighbor,
    kColumn21Row5Neighbor,
    kColumn21Row6Neighbor,
    kColumn21Row7Neighbor,
    kColumn21Row8Neighbor,
    kColumn21Row9Neighbor,
    kColumn21Row10Neighbor,
    kColumn21Row11Neighbor,
    kColumn21Row12Neighbor,
    kColumn21Row13Neighbor,
    kColumn21Row14Neighbor,
    kColumn21Row15Neighbor,
    kColumn21Row16Neighbor,
    kColumn21Row17Neighbor,
    kColumn21Row21Neighbor,
    kColumn21Row22Neighbor,
    kColumn21Row23Neighbor,
    kColumn21Row24Neighbor,
    kColumn21Row25Neighbor,
    kColumn21Row30Neighbor,
    kColumn21Row31Neighbor,
    kColumn21Row39Neighbor,
    kColumn21Row47Neighbor,
    kColumn21Row55Neighbor,
    kColumn21Row4081Neighbor,
    kColumn21Row4083Neighbor,
    kColumn21Row4089Neighbor,
    kColumn21Row4091Neighbor,
    kColumn21Row4093Neighbor,
    kColumn21Row4102Neighbor,
    kColumn21Row4110Neighbor,
    kColumn21Row8167Neighbor,
    kColumn21Row8177Neighbor,
    kColumn21Row8179Neighbor,
    kColumn21Row8183Neighbor,
    kColumn21Row8185Neighbor,
    kColumn21Row8187Neighbor,
    kColumn21Row8191Neighbor,
    kColumn22Row0Neighbor,
    kColumn22Row16Neighbor,
    kColumn22Row80Neighbor,
    kColumn22Row144Neighbor,
    kColumn22Row208Neighbor,
    kColumn22Row8160Neighbor,
    kColumn23Inter1Row0Neighbor,
    kColumn23Inter1Row1Neighbor,
    kColumn24Inter1Row0Neighbor,
    kColumn24Inter1Row2Neighbor,
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
    kPedersenHash1EcSubsetSumBitUnpackingLastOneIsZeroCond,         // Constraint 65.
    kPedersenHash1EcSubsetSumBitUnpackingZeroesBetweenOnes0Cond,    // Constraint 66.
    kPedersenHash1EcSubsetSumBitUnpackingCumulativeBit192Cond,      // Constraint 67.
    kPedersenHash1EcSubsetSumBitUnpackingZeroesBetweenOnes192Cond,  // Constraint 68.
    kPedersenHash1EcSubsetSumBitUnpackingCumulativeBit196Cond,      // Constraint 69.
    kPedersenHash1EcSubsetSumBitUnpackingZeroesBetweenOnes196Cond,  // Constraint 70.
    kPedersenHash1EcSubsetSumBooleanityTestCond,                    // Constraint 71.
    kPedersenHash1EcSubsetSumBitExtractionEndCond,                  // Constraint 72.
    kPedersenHash1EcSubsetSumZerosTailCond,                         // Constraint 73.
    kPedersenHash1EcSubsetSumAddPointsSlopeCond,                    // Constraint 74.
    kPedersenHash1EcSubsetSumAddPointsXCond,                        // Constraint 75.
    kPedersenHash1EcSubsetSumAddPointsYCond,                        // Constraint 76.
    kPedersenHash1EcSubsetSumCopyPointXCond,                        // Constraint 77.
    kPedersenHash1EcSubsetSumCopyPointYCond,                        // Constraint 78.
    kPedersenHash1CopyPointXCond,                                   // Constraint 79.
    kPedersenHash1CopyPointYCond,                                   // Constraint 80.
    kPedersenHash1InitXCond,                                        // Constraint 81.
    kPedersenHash1InitYCond,                                        // Constraint 82.
    kPedersenHash2EcSubsetSumBitUnpackingLastOneIsZeroCond,         // Constraint 83.
    kPedersenHash2EcSubsetSumBitUnpackingZeroesBetweenOnes0Cond,    // Constraint 84.
    kPedersenHash2EcSubsetSumBitUnpackingCumulativeBit192Cond,      // Constraint 85.
    kPedersenHash2EcSubsetSumBitUnpackingZeroesBetweenOnes192Cond,  // Constraint 86.
    kPedersenHash2EcSubsetSumBitUnpackingCumulativeBit196Cond,      // Constraint 87.
    kPedersenHash2EcSubsetSumBitUnpackingZeroesBetweenOnes196Cond,  // Constraint 88.
    kPedersenHash2EcSubsetSumBooleanityTestCond,                    // Constraint 89.
    kPedersenHash2EcSubsetSumBitExtractionEndCond,                  // Constraint 90.
    kPedersenHash2EcSubsetSumZerosTailCond,                         // Constraint 91.
    kPedersenHash2EcSubsetSumAddPointsSlopeCond,                    // Constraint 92.
    kPedersenHash2EcSubsetSumAddPointsXCond,                        // Constraint 93.
    kPedersenHash2EcSubsetSumAddPointsYCond,                        // Constraint 94.
    kPedersenHash2EcSubsetSumCopyPointXCond,                        // Constraint 95.
    kPedersenHash2EcSubsetSumCopyPointYCond,                        // Constraint 96.
    kPedersenHash2CopyPointXCond,                                   // Constraint 97.
    kPedersenHash2CopyPointYCond,                                   // Constraint 98.
    kPedersenHash2InitXCond,                                        // Constraint 99.
    kPedersenHash2InitYCond,                                        // Constraint 100.
    kPedersenHash3EcSubsetSumBitUnpackingLastOneIsZeroCond,         // Constraint 101.
    kPedersenHash3EcSubsetSumBitUnpackingZeroesBetweenOnes0Cond,    // Constraint 102.
    kPedersenHash3EcSubsetSumBitUnpackingCumulativeBit192Cond,      // Constraint 103.
    kPedersenHash3EcSubsetSumBitUnpackingZeroesBetweenOnes192Cond,  // Constraint 104.
    kPedersenHash3EcSubsetSumBitUnpackingCumulativeBit196Cond,      // Constraint 105.
    kPedersenHash3EcSubsetSumBitUnpackingZeroesBetweenOnes196Cond,  // Constraint 106.
    kPedersenHash3EcSubsetSumBooleanityTestCond,                    // Constraint 107.
    kPedersenHash3EcSubsetSumBitExtractionEndCond,                  // Constraint 108.
    kPedersenHash3EcSubsetSumZerosTailCond,                         // Constraint 109.
    kPedersenHash3EcSubsetSumAddPointsSlopeCond,                    // Constraint 110.
    kPedersenHash3EcSubsetSumAddPointsXCond,                        // Constraint 111.
    kPedersenHash3EcSubsetSumAddPointsYCond,                        // Constraint 112.
    kPedersenHash3EcSubsetSumCopyPointXCond,                        // Constraint 113.
    kPedersenHash3EcSubsetSumCopyPointYCond,                        // Constraint 114.
    kPedersenHash3CopyPointXCond,                                   // Constraint 115.
    kPedersenHash3CopyPointYCond,                                   // Constraint 116.
    kPedersenHash3InitXCond,                                        // Constraint 117.
    kPedersenHash3InitYCond,                                        // Constraint 118.
    kPedersenInput0Value0Cond,                                      // Constraint 119.
    kPedersenInput0Value1Cond,                                      // Constraint 120.
    kPedersenInput0Value2Cond,                                      // Constraint 121.
    kPedersenInput0Value3Cond,                                      // Constraint 122.
    kPedersenInput0AddrCond,                                        // Constraint 123.
    kPedersenInitAddrCond,                                          // Constraint 124.
    kPedersenInput1Value0Cond,                                      // Constraint 125.
    kPedersenInput1Value1Cond,                                      // Constraint 126.
    kPedersenInput1Value2Cond,                                      // Constraint 127.
    kPedersenInput1Value3Cond,                                      // Constraint 128.
    kPedersenInput1AddrCond,                                        // Constraint 129.
    kPedersenOutputValue0Cond,                                      // Constraint 130.
    kPedersenOutputValue1Cond,                                      // Constraint 131.
    kPedersenOutputValue2Cond,                                      // Constraint 132.
    kPedersenOutputValue3Cond,                                      // Constraint 133.
    kPedersenOutputAddrCond,                                        // Constraint 134.
    kRcBuiltinValueCond,                                            // Constraint 135.
    kRcBuiltinAddrStepCond,                                         // Constraint 136.
    kRcBuiltinInitAddrCond,                                         // Constraint 137.
    kEcdsaSignature0DoublingKeySlopeCond,                           // Constraint 138.
    kEcdsaSignature0DoublingKeyXCond,                               // Constraint 139.
    kEcdsaSignature0DoublingKeyYCond,                               // Constraint 140.
    kEcdsaSignature0ExponentiateGeneratorBooleanityTestCond,        // Constraint 141.
    kEcdsaSignature0ExponentiateGeneratorBitExtractionEndCond,      // Constraint 142.
    kEcdsaSignature0ExponentiateGeneratorZerosTailCond,             // Constraint 143.
    kEcdsaSignature0ExponentiateGeneratorAddPointsSlopeCond,        // Constraint 144.
    kEcdsaSignature0ExponentiateGeneratorAddPointsXCond,            // Constraint 145.
    kEcdsaSignature0ExponentiateGeneratorAddPointsYCond,            // Constraint 146.
    kEcdsaSignature0ExponentiateGeneratorAddPointsXDiffInvCond,     // Constraint 147.
    kEcdsaSignature0ExponentiateGeneratorCopyPointXCond,            // Constraint 148.
    kEcdsaSignature0ExponentiateGeneratorCopyPointYCond,            // Constraint 149.
    kEcdsaSignature0ExponentiateKeyBooleanityTestCond,              // Constraint 150.
    kEcdsaSignature0ExponentiateKeyBitExtractionEndCond,            // Constraint 151.
    kEcdsaSignature0ExponentiateKeyZerosTailCond,                   // Constraint 152.
    kEcdsaSignature0ExponentiateKeyAddPointsSlopeCond,              // Constraint 153.
    kEcdsaSignature0ExponentiateKeyAddPointsXCond,                  // Constraint 154.
    kEcdsaSignature0ExponentiateKeyAddPointsYCond,                  // Constraint 155.
    kEcdsaSignature0ExponentiateKeyAddPointsXDiffInvCond,           // Constraint 156.
    kEcdsaSignature0ExponentiateKeyCopyPointXCond,                  // Constraint 157.
    kEcdsaSignature0ExponentiateKeyCopyPointYCond,                  // Constraint 158.
    kEcdsaSignature0InitGenXCond,                                   // Constraint 159.
    kEcdsaSignature0InitGenYCond,                                   // Constraint 160.
    kEcdsaSignature0InitKeyXCond,                                   // Constraint 161.
    kEcdsaSignature0InitKeyYCond,                                   // Constraint 162.
    kEcdsaSignature0AddResultsSlopeCond,                            // Constraint 163.
    kEcdsaSignature0AddResultsXCond,                                // Constraint 164.
    kEcdsaSignature0AddResultsYCond,                                // Constraint 165.
    kEcdsaSignature0AddResultsXDiffInvCond,                         // Constraint 166.
    kEcdsaSignature0ExtractRSlopeCond,                              // Constraint 167.
    kEcdsaSignature0ExtractRXCond,                                  // Constraint 168.
    kEcdsaSignature0ExtractRXDiffInvCond,                           // Constraint 169.
    kEcdsaSignature0ZNonzeroCond,                                   // Constraint 170.
    kEcdsaSignature0RAndWNonzeroCond,                               // Constraint 171.
    kEcdsaSignature0QOnCurveXSquaredCond,                           // Constraint 172.
    kEcdsaSignature0QOnCurveOnCurveCond,                            // Constraint 173.
    kEcdsaInitAddrCond,                                             // Constraint 174.
    kEcdsaMessageAddrCond,                                          // Constraint 175.
    kEcdsaPubkeyAddrCond,                                           // Constraint 176.
    kEcdsaMessageValue0Cond,                                        // Constraint 177.
    kEcdsaPubkeyValue0Cond,                                         // Constraint 178.
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

#include "starkware/air/cpu/board/cpu_air_definition0.inl"

#endif  // STARKWARE_AIR_CPU_BOARD_CPU_AIR0_H_
