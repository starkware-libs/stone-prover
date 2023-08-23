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

#include "starkware/algebra/fft/fft_with_precompute.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fft/multiplicative_fft.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/fft_utils/fft_bases.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {
namespace {

template <typename T>
class FftTestReversed : public ::testing::Test {};

using ReversedBasesTypes = ::testing::Types<
    MultiplicativeFftBases<LongFieldElement, MultiplicativeGroupOrdering::kBitReversedOrder>,
    MultiplicativeFftBases<
        PrimeFieldElement<252, 0>, MultiplicativeGroupOrdering::kBitReversedOrder>>;
TYPED_TEST_CASE(FftTestReversed, ReversedBasesTypes);

TYPED_TEST(FftTestReversed, FftNaturalToReverseTest) {
  using BasesT = TypeParam;
  using FieldElementT = typename BasesT::FieldElementT;

  size_t log_n = 3;
  size_t n = Pow2(log_n);
  Prng prng;
  BasesT bases = BasesT(log_n, FieldElementT::RandomElement(&prng));

  std::vector<FieldElementT> res = FieldElementT::UninitializedVector(n);
  auto coefs = prng.RandomFieldElementVector<FieldElementT>(n);
  FftNaturalToReverse<FieldElementT>(coefs, res, bases);
  res = BitReverseVector(std::move(res));

  ASSERT_EQ(res.size(), coefs.size());

  auto x = bases[0].StartOffset();
  auto w = bases[0].Basis().back();
  for (auto y : res) {
    ASSERT_EQ(y, HornerEval(x, coefs));
    x *= w;
  }
}

template <typename T>
class FftTest : public ::testing::Test {};

template <typename BasesT, bool UseFourStepFft>
struct FftTestDefinitions {
  using BasesT_ = BasesT;
  static constexpr bool kUseFourStepFft = UseFourStepFft;
};

using BasesTypes = ::testing::Types<
    FftTestDefinitions<
        MultiplicativeFftBases<LongFieldElement, MultiplicativeGroupOrdering::kNaturalOrder>,
        false>,
    FftTestDefinitions<
        MultiplicativeFftBases<LongFieldElement, MultiplicativeGroupOrdering::kNaturalOrder>, true>,

    FftTestDefinitions<
        MultiplicativeFftBases<LongFieldElement, MultiplicativeGroupOrdering::kBitReversedOrder>,
        false>,
    FftTestDefinitions<
        MultiplicativeFftBases<LongFieldElement, MultiplicativeGroupOrdering::kBitReversedOrder>,
        true>,

    FftTestDefinitions<
        MultiplicativeFftBases<
            PrimeFieldElement<252, 0>, MultiplicativeGroupOrdering::kNaturalOrder>,
        false>,
    FftTestDefinitions<
        MultiplicativeFftBases<
            PrimeFieldElement<252, 0>, MultiplicativeGroupOrdering::kNaturalOrder>,
        true>,

    FftTestDefinitions<
        MultiplicativeFftBases<
            PrimeFieldElement<252, 0>, MultiplicativeGroupOrdering::kBitReversedOrder>,
        false>,
    FftTestDefinitions<
        MultiplicativeFftBases<
            PrimeFieldElement<252, 0>, MultiplicativeGroupOrdering::kBitReversedOrder>,
        true>>;

TYPED_TEST_CASE(FftTest, BasesTypes);

template <typename BasesT>
void TestMultiplicativeFft(const size_t log_n) {
  using FieldElementT = typename BasesT::FieldElementT;
  size_t n = Pow2(log_n);
  Prng prng;

  FieldElementT w = GetSubGroupGenerator<FieldElementT>(n);
  FieldElementT offset = FieldElementT::RandomElement(&prng);
  BasesT bases(w, log_n, offset);

  std::vector<FieldElementT> res = FieldElementT::UninitializedVector(n);
  auto coefs = prng.RandomFieldElementVector<FieldElementT>(n);

  // Fft expects to get the coefficents in BitReversal order.
  std::vector<FieldElementT> maybe_rev_coefs =
      BasesT::kOrder == MultiplicativeGroupOrdering::kNaturalOrder ? BitReverseVector(coefs)
                                                                   : coefs;
  MultiplicativeFft(bases, maybe_rev_coefs, res);
  if (BasesT::kOrder == MultiplicativeGroupOrdering::kBitReversedOrder) {
    res = BitReverseVector(res);
  }

  ASSERT_EQ(res.size(), coefs.size());

  FieldElementT x = offset;
  for (auto y : res) {
    ASSERT_EQ(y, HornerEval(x, coefs));
    x *= w;
  }
}

TYPED_TEST(FftTest, NormalFftTest) {
  using BasesT = typename TypeParam::BasesT_;
  if (TypeParam::kUseFourStepFft) {
    FLAGS_four_step_fft_threshold = 0;
  }
  TestMultiplicativeFft<BasesT>(3);
  TestMultiplicativeFft<BasesT>(0);
}

template <typename BasesT>
void TestMultiplicativeIfft(const size_t log_n) {
  using FieldElementT = typename BasesT::FieldElementT;
  const size_t n = Pow2(log_n);
  Prng prng;
  const FieldElementT offset = FieldElementT::RandomElement(&prng);

  std::vector<FieldElementT> coefs = prng.RandomFieldElementVector<FieldElementT>(n);
  std::vector<FieldElementT> values;
  auto x = offset;
  auto w = GetSubGroupGenerator<FieldElementT>(n);
  for (size_t i = 0; i < coefs.size(); ++i) {
    values.push_back(HornerEval(x, coefs));
    x *= w;
  }

  std::vector<FieldElementT> res = FieldElementT::UninitializedVector(n);
  BasesT bases(w, log_n, offset);

  if (BasesT::kOrder == MultiplicativeGroupOrdering::kBitReversedOrder) {
    values = BitReverseVector(std::move(values));
  }
  MultiplicativeIfft(bases, values, res);
  FieldElementT normalizer = Pow((FieldElementT::FromUint(2)).Inverse(), log_n);

  if (BasesT::kOrder == MultiplicativeGroupOrdering::kNaturalOrder) {
    res = BitReverseVector(std::move(res));
  }
  for (size_t i = 0; i < n; ++i) {
    // Output of the Ifft is multiplied by Pow(2, log_n), we fix this with the normalizer.
    EXPECT_EQ(coefs[i], res[i] * normalizer);
  }
}

template <typename BasesT>
void TestMultiplicativeIfftPartial(const size_t log_n) {
  using FieldElementT = typename BasesT::FieldElementT;
  const size_t n = Pow2(log_n);
  Prng prng;
  const FieldElementT offset = FieldElementT::RandomElement(&prng);
  FieldElementT normalizer = Pow((FieldElementT::FromUint(2)).Inverse(), log_n);

  // Generate coefficients and evaluation.
  std::vector<FieldElementT> coefs = prng.RandomFieldElementVector<FieldElementT>(n);
  std::vector<FieldElementT> values;
  auto w = GetSubGroupGenerator<FieldElementT>(n);
  auto x = offset;
  BasesT bases(w, log_n, offset);

  for (size_t i = 0; i < coefs.size(); ++i) {
    values.push_back(HornerEval(x, coefs));
    x *= w;
  }

  coefs = BitReverseVector(std::move(coefs));
  if (BasesT::kOrder == MultiplicativeGroupOrdering::kBitReversedOrder) {
    values = BitReverseVector(std::move(values));
  }

  // Test different values for n_layers.
  for (size_t n_layers = 0; n_layers <= log_n; ++n_layers) {
    const size_t chunk_size = Pow2(log_n - n_layers);

    // Run partial Ifft.
    std::vector<FieldElementT> res = FieldElementT::UninitializedVector(n);
    MultiplicativeIfft(bases, values, res, n_layers);
    if (BasesT::kOrder == MultiplicativeGroupOrdering::kBitReversedOrder) {
      res = BitReverseVector(std::move(res));
    }

    std::vector<FieldElementT> sub_res;
    sub_res.resize(chunk_size, FieldElementT::Uninitialized());
    for (size_t chunk_i = 0; chunk_i < Pow2(n_layers); ++chunk_i) {
      // Complete the partial n_layers Ifft we did earlier by doing the last (log_n - n_layers) Ifft
      // layers.
      // The last layers are done as Pow2(n_layers) Iffts of size chunk_size=Pow2(log_n -
      // n_layers)).
      auto chunk_span = gsl::make_span(res).subspan(chunk_i * chunk_size, chunk_size);
      std::vector<FieldElementT> extracted_chunk{chunk_span.begin(), chunk_span.end()};
      if (BasesT::kOrder == MultiplicativeGroupOrdering::kBitReversedOrder) {
        extracted_chunk = BitReverseVector(std::move(extracted_chunk));
      }
      MultiplicativeIfft(bases.FromLayer(n_layers), extracted_chunk, sub_res);
      if (BasesT::kOrder == MultiplicativeGroupOrdering::kBitReversedOrder) {
        sub_res = BitReverseVector(std::move(sub_res));
      }

      for (size_t i = 0; i < chunk_size; ++i) {
        // Output of the Ifft is multiplied by Pow(2, log_n), we fix this with the normalizer.
        EXPECT_EQ(coefs[chunk_i * chunk_size + i], sub_res[i] * normalizer);
      }
    }
  }
}

TYPED_TEST(FftTest, MultiplicativeIfftTest) {
  using BasesT = typename TypeParam::BasesT_;
  TestMultiplicativeIfft<BasesT>(3);
  TestMultiplicativeIfft<BasesT>(0);
  TestMultiplicativeIfftPartial<BasesT>(3);
  TestMultiplicativeIfftPartial<BasesT>(0);
}

TYPED_TEST(FftTest, FftZeroPrecomputeDepth) {
  using BasesT = typename TypeParam::BasesT_;
  using FieldElementT = typename BasesT::FieldElementT;
  size_t log_n = 2;
  size_t n = Pow2(log_n);
  Prng prng;

  FieldElementT w = GetSubGroupGenerator<FieldElementT>(n);
  FieldElementT offset = FieldElementT::RandomElement(&prng);

  std::vector<FieldElementT> res = FieldElementT::UninitializedVector(n);
  auto coefs = prng.RandomFieldElementVector<FieldElementT>(n);

  // Fft expects to get the coefficents in BitReversal order.
  std::vector<FieldElementT> rev_coefs = BitReverseVector(coefs);

  fft::details::FftNoPrecompute(
      rev_coefs, MakeFftBases<MultiplicativeGroupOrdering::kNaturalOrder>(w, log_n, offset), 0,
      res);

  FieldElementT x = offset;
  for (auto y : res) {
    ASSERT_EQ(y, HornerEval(x, coefs));
    x *= w;
  }
}

template <typename BasesT>
void TestfftWithPrecompute(const size_t log_n, const size_t log_precompute_depth) {
  using FieldElementT = typename BasesT::FieldElementT;
  static const auto kOrder = BasesT::kOrder;
  const size_t n = Pow2(log_n);
  Prng prng;

  const FieldElementT w = GetSubGroupGenerator<FieldElementT>(n);
  const FieldElementT offset = FieldElementT::RandomElement(&prng);

  const std::vector<FieldElementT> sub_groups_generators =
      log_n > 0 ? GetSquares(w, log_n - 1) : std::vector<FieldElementT>{};

  auto bases = BasesT(
      sub_groups_generators.empty() ? -FieldElementT::One() : sub_groups_generators.front(),
      sub_groups_generators.size() + 1, offset);

  auto fft_precompute = FftWithPrecompute<BasesT>(bases, log_precompute_depth);

  std::vector<FieldElementT> res = FieldElementT::UninitializedVector(n);

  auto coefs = prng.RandomFieldElementVector<FieldElementT>(n);

  fft_precompute.Fft(
      kOrder == MultiplicativeGroupOrdering::kNaturalOrder ? BitReverseVector(coefs) : coefs, res);

  if (kOrder == MultiplicativeGroupOrdering::kBitReversedOrder) {
    BitReverseInPlace<FieldElementT>(res);
  }

  FieldElementT x = offset;
  for (auto y : res) {
    ASSERT_EQ(y, HornerEval(x, coefs));
    x *= w;
  }
}

TYPED_TEST(FftTest, FftWithPrecompute) {
  using BasesT = typename TypeParam::BasesT_;
  if (TypeParam::kUseFourStepFft) {
    FLAGS_four_step_fft_threshold = 0;
  }
  TestfftWithPrecompute<BasesT>(4, 0);
  TestfftWithPrecompute<BasesT>(4, 1);
  TestfftWithPrecompute<BasesT>(4, 4);

  TestfftWithPrecompute<BasesT>(0, 0);
  TestfftWithPrecompute<BasesT>(0, 1);
}

TYPED_TEST(FftTest, Identity) {
  using BasesT = typename TypeParam::BasesT_;
  using FieldElementT = typename BasesT::FieldElementT;
  size_t log_n = 4;
  size_t n = Pow2(log_n);
  Prng prng;

  FieldElementT offset = FieldElementT::RandomElement(&prng);
  auto bases = MakeFftBases(log_n, offset);

  std::vector<FieldElementT> values = prng.RandomFieldElementVector<FieldElementT>(n);
  std::vector<FieldElementT> res = FieldElementT::UninitializedVector(n);
  MultiplicativeFft(bases, values, res);
  MultiplicativeIfft(bases, res, res);

  FieldElementT normalizer = FieldElementT::FromUint(n).Inverse();

  ASSERT_EQ(res.size(), n);
  for (size_t i = 0; i < n; ++i) {
    // Output of the Ifft is multiplied by Pow(2, log_n), we fix this with the normalizer.
    EXPECT_EQ(res[i] * normalizer, values[i]);
  }
}

template <typename BasesT>
void TestTwiddleShiftByElement(const BasesT& bases) {
  using FieldElementT = typename BasesT::FieldElementT;
  Prng prng;

  const FieldElementT offset_1 = FieldElementT::RandomElement(&prng);
  const FieldElementT offset_2 = FieldElementT::RandomElement(&prng);

  auto bases_1 = bases.GetShiftedBases(offset_1);
  auto bases_2 = bases.GetShiftedBases(offset_2);

  auto fft_precompute_1 = FftWithPrecompute<BasesT>(bases_1, bases.NumLayers());
  auto fft_precompute_2 = FftWithPrecompute<BasesT>(bases_2, bases.NumLayers());

  // Shift the first precompute twiddle factors by offset2, to get a total offset of
  // offset1*offset2.
  fft_precompute_1.ShiftTwiddleFactors(FieldElement(offset_2), FieldElement(offset_1));

  auto twiddle_1 = fft_precompute_1.GetTwiddleFactors();
  auto twiddle_2 = fft_precompute_2.GetTwiddleFactors();

  // Compare both twiddle factors.
  ASSERT_EQ(twiddle_2.size(), twiddle_1.size());
  for (size_t i = 0; i < twiddle_1.size(); ++i) {
    ASSERT_EQ(twiddle_1[i], twiddle_2[i]);
  }
}

TYPED_TEST(FftTest, TwiddleShiftByConstantMult) {
  using BasesT = typename TypeParam::BasesT_;
  auto bases = MakeFftBases<BasesT::kOrder, typename BasesT::FieldElementT>(8);
  TestTwiddleShiftByElement<BasesT>(bases);
}

}  // namespace
}  // namespace starkware
