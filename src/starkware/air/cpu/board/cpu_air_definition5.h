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

#ifndef STARKWARE_AIR_CPU_BOARD_CPU_AIR5_H_
#define STARKWARE_AIR_CPU_BOARD_CPU_AIR5_H_

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
class CpuAirDefinition<FieldElementT, 5> : public Air {
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

  static constexpr uint64_t kNumColumnsFirst = 21;
  static constexpr uint64_t kNumColumnsSecond = 1;

  static constexpr uint64_t kPublicMemoryStep = 8;
  static constexpr bool kHasDilutedPool = true;
  static constexpr uint64_t kDilutedSpacing = 4;
  static constexpr uint64_t kDilutedNBits = 16;
  static constexpr uint64_t kPedersenBuiltinRatio = 8;
  static constexpr uint64_t kPedersenBuiltinRepetitions = 4;
  static constexpr uint64_t kRcBuiltinRatio = 8;
  static constexpr uint64_t kRcNParts = 8;
  static constexpr uint64_t kEcdsaBuiltinRatio = 512;
  static constexpr uint64_t kEcdsaBuiltinRepetitions = 1;
  static constexpr uint64_t kEcdsaElementBits = 251;
  static constexpr uint64_t kEcdsaElementHeight = 256;
  static constexpr uint64_t kBitwiseRatio = 64;
  static constexpr uint64_t kBitwiseTotalNBits = 251;
  static constexpr bool kHasOutputBuiltin = true;
  static constexpr bool kHasPedersenBuiltin = true;
  static constexpr bool kHasRangeCheckBuiltin = true;
  static constexpr bool kHasEcdsaBuiltin = true;
  static constexpr bool kHasBitwiseBuiltin = true;
  static constexpr bool kHasEcOpBuiltin = false;
  static constexpr bool kHasKeccakBuiltin = false;
  static constexpr bool kHasPoseidonBuiltin = false;
  static constexpr char kLayoutName[] = "dex_with_bitwise";
  static constexpr BigInt<4> kLayoutCode = 0x6465785f776974685f62697477697365_Z;
  static constexpr uint64_t kConstraintDegree = 2;
  static constexpr uint64_t kCpuComponentHeight = 16;
  static constexpr uint64_t kLogCpuComponentHeight = 4;
  static constexpr uint64_t kMemoryStep = 2;
  static constexpr std::array<std::string_view, 7> kSegmentNames = {
      "program", "execution", "output", "pedersen", "range_check", "ecdsa", "bitwise"};

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
    kColumn21Inter1Column,
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
    kColumn4Row1Neighbor,
    kColumn4Row255Neighbor,
    kColumn4Row256Neighbor,
    kColumn4Row511Neighbor,
    kColumn5Row0Neighbor,
    kColumn5Row1Neighbor,
    kColumn5Row255Neighbor,
    kColumn5Row256Neighbor,
    kColumn6Row0Neighbor,
    kColumn6Row1Neighbor,
    kColumn6Row192Neighbor,
    kColumn6Row193Neighbor,
    kColumn6Row196Neighbor,
    kColumn6Row197Neighbor,
    kColumn6Row251Neighbor,
    kColumn6Row252Neighbor,
    kColumn6Row256Neighbor,
    kColumn7Row0Neighbor,
    kColumn7Row1Neighbor,
    kColumn7Row255Neighbor,
    kColumn7Row256Neighbor,
    kColumn7Row511Neighbor,
    kColumn8Row0Neighbor,
    kColumn8Row1Neighbor,
    kColumn8Row255Neighbor,
    kColumn8Row256Neighbor,
    kColumn9Row0Neighbor,
    kColumn9Row1Neighbor,
    kColumn9Row192Neighbor,
    kColumn9Row193Neighbor,
    kColumn9Row196Neighbor,
    kColumn9Row197Neighbor,
    kColumn9Row251Neighbor,
    kColumn9Row252Neighbor,
    kColumn9Row256Neighbor,
    kColumn10Row0Neighbor,
    kColumn10Row1Neighbor,
    kColumn10Row255Neighbor,
    kColumn10Row256Neighbor,
    kColumn10Row511Neighbor,
    kColumn11Row0Neighbor,
    kColumn11Row1Neighbor,
    kColumn11Row255Neighbor,
    kColumn11Row256Neighbor,
    kColumn12Row0Neighbor,
    kColumn12Row1Neighbor,
    kColumn12Row192Neighbor,
    kColumn12Row193Neighbor,
    kColumn12Row196Neighbor,
    kColumn12Row197Neighbor,
    kColumn12Row251Neighbor,
    kColumn12Row252Neighbor,
    kColumn12Row256Neighbor,
    kColumn13Row0Neighbor,
    kColumn13Row255Neighbor,
    kColumn14Row0Neighbor,
    kColumn14Row255Neighbor,
    kColumn15Row0Neighbor,
    kColumn15Row255Neighbor,
    kColumn16Row0Neighbor,
    kColumn16Row255Neighbor,
    kColumn17Row0Neighbor,
    kColumn17Row1Neighbor,
    kColumn17Row2Neighbor,
    kColumn17Row3Neighbor,
    kColumn17Row4Neighbor,
    kColumn17Row5Neighbor,
    kColumn17Row6Neighbor,
    kColumn17Row7Neighbor,
    kColumn17Row8Neighbor,
    kColumn17Row9Neighbor,
    kColumn17Row12Neighbor,
    kColumn17Row13Neighbor,
    kColumn17Row16Neighbor,
    kColumn17Row22Neighbor,
    kColumn17Row23Neighbor,
    kColumn17Row38Neighbor,
    kColumn17Row39Neighbor,
    kColumn17Row70Neighbor,
    kColumn17Row71Neighbor,
    kColumn17Row102Neighbor,
    kColumn17Row103Neighbor,
    kColumn17Row134Neighbor,
    kColumn17Row135Neighbor,
    kColumn17Row150Neighbor,
    kColumn17Row151Neighbor,
    kColumn17Row167Neighbor,
    kColumn17Row199Neighbor,
    kColumn17Row230Neighbor,
    kColumn17Row263Neighbor,
    kColumn17Row295Neighbor,
    kColumn17Row327Neighbor,
    kColumn17Row391Neighbor,
    kColumn17Row406Neighbor,
    kColumn17Row423Neighbor,
    kColumn17Row455Neighbor,
    kColumn17Row534Neighbor,
    kColumn17Row535Neighbor,
    kColumn17Row663Neighbor,
    kColumn17Row918Neighbor,
    kColumn17Row919Neighbor,
    kColumn17Row1174Neighbor,
    kColumn17Row4118Neighbor,
    kColumn17Row4119Neighbor,
    kColumn17Row8214Neighbor,
    kColumn18Row0Neighbor,
    kColumn18Row1Neighbor,
    kColumn18Row2Neighbor,
    kColumn18Row3Neighbor,
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
    kColumn19Row11Neighbor,
    kColumn19Row12Neighbor,
    kColumn19Row13Neighbor,
    kColumn19Row15Neighbor,
    kColumn19Row17Neighbor,
    kColumn19Row19Neighbor,
    kColumn19Row27Neighbor,
    kColumn19Row28Neighbor,
    kColumn19Row33Neighbor,
    kColumn19Row44Neighbor,
    kColumn19Row49Neighbor,
    kColumn19Row60Neighbor,
    kColumn19Row65Neighbor,
    kColumn19Row76Neighbor,
    kColumn19Row81Neighbor,
    kColumn19Row92Neighbor,
    kColumn19Row97Neighbor,
    kColumn19Row108Neighbor,
    kColumn19Row113Neighbor,
    kColumn19Row124Neighbor,
    kColumn19Row129Neighbor,
    kColumn19Row145Neighbor,
    kColumn19Row161Neighbor,
    kColumn19Row177Neighbor,
    kColumn19Row193Neighbor,
    kColumn19Row209Neighbor,
    kColumn19Row225Neighbor,
    kColumn19Row241Neighbor,
    kColumn19Row257Neighbor,
    kColumn19Row265Neighbor,
    kColumn19Row513Neighbor,
    kColumn19Row521Neighbor,
    kColumn19Row705Neighbor,
    kColumn19Row721Neighbor,
    kColumn19Row737Neighbor,
    kColumn19Row753Neighbor,
    kColumn19Row769Neighbor,
    kColumn19Row777Neighbor,
    kColumn19Row961Neighbor,
    kColumn19Row977Neighbor,
    kColumn19Row993Neighbor,
    kColumn19Row1009Neighbor,
    kColumn20Row0Neighbor,
    kColumn20Row1Neighbor,
    kColumn20Row2Neighbor,
    kColumn20Row3Neighbor,
    kColumn20Row4Neighbor,
    kColumn20Row5Neighbor,
    kColumn20Row6Neighbor,
    kColumn20Row8Neighbor,
    kColumn20Row9Neighbor,
    kColumn20Row10Neighbor,
    kColumn20Row12Neighbor,
    kColumn20Row13Neighbor,
    kColumn20Row14Neighbor,
    kColumn20Row18Neighbor,
    kColumn20Row19Neighbor,
    kColumn20Row20Neighbor,
    kColumn20Row21Neighbor,
    kColumn20Row22Neighbor,
    kColumn20Row26Neighbor,
    kColumn20Row28Neighbor,
    kColumn20Row29Neighbor,
    kColumn20Row37Neighbor,
    kColumn20Row45Neighbor,
    kColumn20Row53Neighbor,
    kColumn20Row83Neighbor,
    kColumn20Row147Neighbor,
    kColumn20Row211Neighbor,
    kColumn20Row4081Neighbor,
    kColumn20Row4082Neighbor,
    kColumn20Row4089Neighbor,
    kColumn20Row4090Neighbor,
    kColumn20Row4094Neighbor,
    kColumn20Row4100Neighbor,
    kColumn20Row4108Neighbor,
    kColumn20Row8163Neighbor,
    kColumn20Row8165Neighbor,
    kColumn20Row8177Neighbor,
    kColumn20Row8178Neighbor,
    kColumn20Row8181Neighbor,
    kColumn20Row8185Neighbor,
    kColumn20Row8186Neighbor,
    kColumn20Row8189Neighbor,
    kColumn21Inter1Row0Neighbor,
    kColumn21Inter1Row1Neighbor,
    kColumn21Inter1Row2Neighbor,
    kColumn21Inter1Row3Neighbor,
    kColumn21Inter1Row5Neighbor,
    kColumn21Inter1Row7Neighbor,
    kColumn21Inter1Row11Neighbor,
    kColumn21Inter1Row15Neighbor,
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
    kPedersenHash1EcSubsetSumBitUnpackingLastOneIsZeroCond,         // Constraint 72.
    kPedersenHash1EcSubsetSumBitUnpackingZeroesBetweenOnes0Cond,    // Constraint 73.
    kPedersenHash1EcSubsetSumBitUnpackingCumulativeBit192Cond,      // Constraint 74.
    kPedersenHash1EcSubsetSumBitUnpackingZeroesBetweenOnes192Cond,  // Constraint 75.
    kPedersenHash1EcSubsetSumBitUnpackingCumulativeBit196Cond,      // Constraint 76.
    kPedersenHash1EcSubsetSumBitUnpackingZeroesBetweenOnes196Cond,  // Constraint 77.
    kPedersenHash1EcSubsetSumBooleanityTestCond,                    // Constraint 78.
    kPedersenHash1EcSubsetSumBitExtractionEndCond,                  // Constraint 79.
    kPedersenHash1EcSubsetSumZerosTailCond,                         // Constraint 80.
    kPedersenHash1EcSubsetSumAddPointsSlopeCond,                    // Constraint 81.
    kPedersenHash1EcSubsetSumAddPointsXCond,                        // Constraint 82.
    kPedersenHash1EcSubsetSumAddPointsYCond,                        // Constraint 83.
    kPedersenHash1EcSubsetSumCopyPointXCond,                        // Constraint 84.
    kPedersenHash1EcSubsetSumCopyPointYCond,                        // Constraint 85.
    kPedersenHash1CopyPointXCond,                                   // Constraint 86.
    kPedersenHash1CopyPointYCond,                                   // Constraint 87.
    kPedersenHash1InitXCond,                                        // Constraint 88.
    kPedersenHash1InitYCond,                                        // Constraint 89.
    kPedersenHash2EcSubsetSumBitUnpackingLastOneIsZeroCond,         // Constraint 90.
    kPedersenHash2EcSubsetSumBitUnpackingZeroesBetweenOnes0Cond,    // Constraint 91.
    kPedersenHash2EcSubsetSumBitUnpackingCumulativeBit192Cond,      // Constraint 92.
    kPedersenHash2EcSubsetSumBitUnpackingZeroesBetweenOnes192Cond,  // Constraint 93.
    kPedersenHash2EcSubsetSumBitUnpackingCumulativeBit196Cond,      // Constraint 94.
    kPedersenHash2EcSubsetSumBitUnpackingZeroesBetweenOnes196Cond,  // Constraint 95.
    kPedersenHash2EcSubsetSumBooleanityTestCond,                    // Constraint 96.
    kPedersenHash2EcSubsetSumBitExtractionEndCond,                  // Constraint 97.
    kPedersenHash2EcSubsetSumZerosTailCond,                         // Constraint 98.
    kPedersenHash2EcSubsetSumAddPointsSlopeCond,                    // Constraint 99.
    kPedersenHash2EcSubsetSumAddPointsXCond,                        // Constraint 100.
    kPedersenHash2EcSubsetSumAddPointsYCond,                        // Constraint 101.
    kPedersenHash2EcSubsetSumCopyPointXCond,                        // Constraint 102.
    kPedersenHash2EcSubsetSumCopyPointYCond,                        // Constraint 103.
    kPedersenHash2CopyPointXCond,                                   // Constraint 104.
    kPedersenHash2CopyPointYCond,                                   // Constraint 105.
    kPedersenHash2InitXCond,                                        // Constraint 106.
    kPedersenHash2InitYCond,                                        // Constraint 107.
    kPedersenHash3EcSubsetSumBitUnpackingLastOneIsZeroCond,         // Constraint 108.
    kPedersenHash3EcSubsetSumBitUnpackingZeroesBetweenOnes0Cond,    // Constraint 109.
    kPedersenHash3EcSubsetSumBitUnpackingCumulativeBit192Cond,      // Constraint 110.
    kPedersenHash3EcSubsetSumBitUnpackingZeroesBetweenOnes192Cond,  // Constraint 111.
    kPedersenHash3EcSubsetSumBitUnpackingCumulativeBit196Cond,      // Constraint 112.
    kPedersenHash3EcSubsetSumBitUnpackingZeroesBetweenOnes196Cond,  // Constraint 113.
    kPedersenHash3EcSubsetSumBooleanityTestCond,                    // Constraint 114.
    kPedersenHash3EcSubsetSumBitExtractionEndCond,                  // Constraint 115.
    kPedersenHash3EcSubsetSumZerosTailCond,                         // Constraint 116.
    kPedersenHash3EcSubsetSumAddPointsSlopeCond,                    // Constraint 117.
    kPedersenHash3EcSubsetSumAddPointsXCond,                        // Constraint 118.
    kPedersenHash3EcSubsetSumAddPointsYCond,                        // Constraint 119.
    kPedersenHash3EcSubsetSumCopyPointXCond,                        // Constraint 120.
    kPedersenHash3EcSubsetSumCopyPointYCond,                        // Constraint 121.
    kPedersenHash3CopyPointXCond,                                   // Constraint 122.
    kPedersenHash3CopyPointYCond,                                   // Constraint 123.
    kPedersenHash3InitXCond,                                        // Constraint 124.
    kPedersenHash3InitYCond,                                        // Constraint 125.
    kPedersenInput0Value0Cond,                                      // Constraint 126.
    kPedersenInput0Value1Cond,                                      // Constraint 127.
    kPedersenInput0Value2Cond,                                      // Constraint 128.
    kPedersenInput0Value3Cond,                                      // Constraint 129.
    kPedersenInput0AddrCond,                                        // Constraint 130.
    kPedersenInitAddrCond,                                          // Constraint 131.
    kPedersenInput1Value0Cond,                                      // Constraint 132.
    kPedersenInput1Value1Cond,                                      // Constraint 133.
    kPedersenInput1Value2Cond,                                      // Constraint 134.
    kPedersenInput1Value3Cond,                                      // Constraint 135.
    kPedersenInput1AddrCond,                                        // Constraint 136.
    kPedersenOutputValue0Cond,                                      // Constraint 137.
    kPedersenOutputValue1Cond,                                      // Constraint 138.
    kPedersenOutputValue2Cond,                                      // Constraint 139.
    kPedersenOutputValue3Cond,                                      // Constraint 140.
    kPedersenOutputAddrCond,                                        // Constraint 141.
    kRcBuiltinValueCond,                                            // Constraint 142.
    kRcBuiltinAddrStepCond,                                         // Constraint 143.
    kRcBuiltinInitAddrCond,                                         // Constraint 144.
    kEcdsaSignature0DoublingKeySlopeCond,                           // Constraint 145.
    kEcdsaSignature0DoublingKeyXCond,                               // Constraint 146.
    kEcdsaSignature0DoublingKeyYCond,                               // Constraint 147.
    kEcdsaSignature0ExponentiateGeneratorBooleanityTestCond,        // Constraint 148.
    kEcdsaSignature0ExponentiateGeneratorBitExtractionEndCond,      // Constraint 149.
    kEcdsaSignature0ExponentiateGeneratorZerosTailCond,             // Constraint 150.
    kEcdsaSignature0ExponentiateGeneratorAddPointsSlopeCond,        // Constraint 151.
    kEcdsaSignature0ExponentiateGeneratorAddPointsXCond,            // Constraint 152.
    kEcdsaSignature0ExponentiateGeneratorAddPointsYCond,            // Constraint 153.
    kEcdsaSignature0ExponentiateGeneratorAddPointsXDiffInvCond,     // Constraint 154.
    kEcdsaSignature0ExponentiateGeneratorCopyPointXCond,            // Constraint 155.
    kEcdsaSignature0ExponentiateGeneratorCopyPointYCond,            // Constraint 156.
    kEcdsaSignature0ExponentiateKeyBooleanityTestCond,              // Constraint 157.
    kEcdsaSignature0ExponentiateKeyBitExtractionEndCond,            // Constraint 158.
    kEcdsaSignature0ExponentiateKeyZerosTailCond,                   // Constraint 159.
    kEcdsaSignature0ExponentiateKeyAddPointsSlopeCond,              // Constraint 160.
    kEcdsaSignature0ExponentiateKeyAddPointsXCond,                  // Constraint 161.
    kEcdsaSignature0ExponentiateKeyAddPointsYCond,                  // Constraint 162.
    kEcdsaSignature0ExponentiateKeyAddPointsXDiffInvCond,           // Constraint 163.
    kEcdsaSignature0ExponentiateKeyCopyPointXCond,                  // Constraint 164.
    kEcdsaSignature0ExponentiateKeyCopyPointYCond,                  // Constraint 165.
    kEcdsaSignature0InitGenXCond,                                   // Constraint 166.
    kEcdsaSignature0InitGenYCond,                                   // Constraint 167.
    kEcdsaSignature0InitKeyXCond,                                   // Constraint 168.
    kEcdsaSignature0InitKeyYCond,                                   // Constraint 169.
    kEcdsaSignature0AddResultsSlopeCond,                            // Constraint 170.
    kEcdsaSignature0AddResultsXCond,                                // Constraint 171.
    kEcdsaSignature0AddResultsYCond,                                // Constraint 172.
    kEcdsaSignature0AddResultsXDiffInvCond,                         // Constraint 173.
    kEcdsaSignature0ExtractRSlopeCond,                              // Constraint 174.
    kEcdsaSignature0ExtractRXCond,                                  // Constraint 175.
    kEcdsaSignature0ExtractRXDiffInvCond,                           // Constraint 176.
    kEcdsaSignature0ZNonzeroCond,                                   // Constraint 177.
    kEcdsaSignature0RAndWNonzeroCond,                               // Constraint 178.
    kEcdsaSignature0QOnCurveXSquaredCond,                           // Constraint 179.
    kEcdsaSignature0QOnCurveOnCurveCond,                            // Constraint 180.
    kEcdsaInitAddrCond,                                             // Constraint 181.
    kEcdsaMessageAddrCond,                                          // Constraint 182.
    kEcdsaPubkeyAddrCond,                                           // Constraint 183.
    kEcdsaMessageValue0Cond,                                        // Constraint 184.
    kEcdsaPubkeyValue0Cond,                                         // Constraint 185.
    kBitwiseInitVarPoolAddrCond,                                    // Constraint 186.
    kBitwiseStepVarPoolAddrCond,                                    // Constraint 187.
    kBitwiseXOrYAddrCond,                                           // Constraint 188.
    kBitwiseNextVarPoolAddrCond,                                    // Constraint 189.
    kBitwisePartitionCond,                                          // Constraint 190.
    kBitwiseOrIsAndPlusXorCond,                                     // Constraint 191.
    kBitwiseAdditionIsXorWithAndCond,                               // Constraint 192.
    kBitwiseUniqueUnpacking192Cond,                                 // Constraint 193.
    kBitwiseUniqueUnpacking193Cond,                                 // Constraint 194.
    kBitwiseUniqueUnpacking194Cond,                                 // Constraint 195.
    kBitwiseUniqueUnpacking195Cond,                                 // Constraint 196.
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

#include "starkware/air/cpu/board/cpu_air_definition5.inl"

#endif  // STARKWARE_AIR_CPU_BOARD_CPU_AIR5_H_
