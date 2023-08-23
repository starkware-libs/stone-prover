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

#include "starkware/fri/fri_details.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/lde/lde.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/fri/fri_test_utils.h"

namespace starkware {
namespace fri {
namespace details {
namespace {

using testing::ElementsAreArray;

template <typename T>
std::vector<FieldElement> AsFieldElements(const std::vector<T>& vec) {
  std::vector<FieldElement> res;
  res.reserve(vec.size());
  for (const T& x : vec) {
    res.push_back(FieldElement(x));
  }
  return res;
}

template <typename FieldElementT>
FieldElementT GetCorrectionFactor([[maybe_unused]] size_t n_layers) {
  return Pow(FieldElementT::FromUint(2), n_layers);
}

template <typename BasesT>
int64_t GetEvaluationDegree(const FieldElementVector& vec, const BasesT& bases) {
  auto lde_manager = MakeLdeManager(bases);
  lde_manager->AddEvaluation(vec);

  return lde_manager->GetEvaluationDegree(0);
}

using TestedFieldTypes = ::testing::Types<TestFieldElement>;

template <typename FieldElementT>
class FriDetailsTest : public ::testing::Test {
 public:
  FriDetailsTest() { folder_ = FriFolderFromField(Field::Create<FieldElementT>()); }

 protected:
  std::unique_ptr<FriFolderBase> folder_;
};

TYPED_TEST_CASE(FriDetailsTest, TestedFieldTypes);

TYPED_TEST(FriDetailsTest, ComputeNextFriLayerBasicTest) {
  using FieldElementT = TypeParam;
  Prng prng;

  const size_t log_domain_size = 5;
  auto bases = MakeFftBases(log_domain_size, FieldElementT::RandomElement(&prng));

  auto eval_point = FieldElementT::RandomElement(&prng);

  // Start with a random polynomial evaluated at a domain of 16 points.
  auto coefs = prng.RandomFieldElementVector<FieldElementT>(4);

  std::vector<FieldElementT> first_layer_eval;
  for (const FieldElementT& x : bases[0]) {
    // Multiplicative case: a_3*x^3 + a_2*x^2 + a_1*x + a_0.
    first_layer_eval.push_back(coefs[0] + coefs[1] * x + coefs[2] * x * x + coefs[3] * x * x * x);
  }

  // The expected result is the polynomial 2 * ((a_2 * x + a_0) + eval_point * (a_3 * x + a_1)).
  std::vector<FieldElementT> expected_res;
  for (const FieldElementT& x : bases[1]) {
    expected_res.push_back(
        GetCorrectionFactor<FieldElementT>(1) *
        ((coefs[2] * x + coefs[0]) + eval_point * (coefs[3] * x + coefs[1])));
  }

  auto res = this->folder_->ComputeNextFriLayer(
      bases[0], FieldElementVector::Make(std::move(first_layer_eval)), FieldElement(eval_point));
  EXPECT_THAT(res.template As<FieldElementT>(), ElementsAreArray(expected_res));
}

TYPED_TEST(FriDetailsTest, ComputeNextFriLayerDegreeBound800) {
  using FieldElementT = TypeParam;
  Prng prng;
  const size_t log_domain_size = 13;

  auto bases = MakeFftBases(log_domain_size, FieldElementT::RandomElement(&prng));
  const auto& domain = bases[0];

  auto eval_point = FieldElementT::RandomElement(&prng);

  // Start with a polynomial polynomial of degree bound 800 (degree 799), evaluated at a domain of
  // 8192 points.
  TestPolynomial<FieldElementT> test_layer(&prng, 800);

  auto first_layer = FieldElementVector::Make(test_layer.GetData(domain));
  EXPECT_EQ(799, GetEvaluationDegree(first_layer, bases));

  // The expected result is a polynomial of degree <400.
  auto res = this->folder_->ComputeNextFriLayer(domain, first_layer, FieldElement(eval_point));

  EXPECT_EQ(399, GetEvaluationDegree(res, bases.FromLayer(1)));
}

/*
  This test checks that if the evaluation points are x0, x0^2, x0^4, x0^8, ...
  then f_i(x0^(2^i)) = f(x0) where f_i is the i-th layer in FRI.
*/
TYPED_TEST(FriDetailsTest, ComputeNextFriLayerUseFriToEvaluateAtPoint) {
  using FieldElementT = TypeParam;
  Prng prng;
  const size_t log_domain_size = 13;
  const size_t degree = 16;

  auto bases = MakeFftBases(log_domain_size, FftMultiplicativeGroup<FieldElementT>::GroupUnit());

  const FieldElementT original_eval_point = FieldElementT::RandomElement(&prng);
  FieldElementT eval_point = original_eval_point;

  TestPolynomial<FieldElementT> test_layer(&prng, degree + 1);
  auto current_layer = FieldElementVector::Make(test_layer.GetData(bases[0]));
  FieldElementT f_e = ExtrapolatePoint(bases, current_layer, eval_point);

  for (size_t i = 1; i < log_domain_size; ++i) {
    current_layer =
        this->folder_->ComputeNextFriLayer(bases[i - 1], current_layer, FieldElement(eval_point));
    eval_point = bases.ApplyBasisTransformTmpl(eval_point, i - 1);

    FieldElementT res = ExtrapolatePoint(bases.FromLayer(i), current_layer, eval_point);

    FieldElementT correction_factor = GetCorrectionFactor<FieldElementT>(i);
    EXPECT_EQ(correction_factor * f_e, res) << "i = " << i;
  }
}

TYPED_TEST(FriDetailsTest, ApplyFriLayersCorrectness) {
  using FieldElementT = TypeParam;
  Prng prng;
  auto bases = MakeFftBases(5, FieldElementT::RandomElement(&prng));
  FriParameters params{
      /*fri_step_list=*/{1, 2},
      /*last_layer_degree_bound=*/1,
      /*n_queries=*/1,
      /*fft_bases=*/UseOwned(&bases),
      /*field=*/Field::Create<FieldElementT>(),
      /*proof_of_work_bits=*/15};
  FieldElement eval_point(FieldElementT::RandomElement(&prng));

  // fri_step = 1.
  FieldElementVector elements =
      FieldElementVector::Make(prng.RandomFieldElementVector<FieldElementT>(2));
  const size_t coset_offset = 4;
  EXPECT_EQ(
      this->folder_->NextLayerElementFromTwoPreviousLayerElements(
          elements[0], elements[1], eval_point, bases[0].GetFieldElementAt(coset_offset)),
      ApplyFriLayers(elements, eval_point, params, 0, coset_offset, *this->folder_));

  // fri_step = 2.
  FieldElementVector elements2 =
      FieldElementVector::Make(prng.RandomFieldElementVector<FieldElementT>(4));
  const size_t coset_offset2 = 12;
  EXPECT_EQ(
      this->folder_->NextLayerElementFromTwoPreviousLayerElements(
          this->folder_->NextLayerElementFromTwoPreviousLayerElements(
              elements2[0], elements2[1], eval_point, bases[1].GetFieldElementAt(coset_offset2)),
          this->folder_->NextLayerElementFromTwoPreviousLayerElements(
              elements2[2], elements2[3], eval_point,
              bases[1].GetFieldElementAt(coset_offset2 + 2)),
          bases.ApplyBasisTransform(eval_point, 1), bases[2].GetFieldElementAt(coset_offset2 / 2)),
      ApplyFriLayers(elements2, eval_point, params, 1, coset_offset2, *this->folder_));
}

TYPED_TEST(FriDetailsTest, ApplyFriLayersPolynomial) {
  using FieldElementT = TypeParam;
  const size_t fri_step = 3;
  Prng prng;
  auto bases = MakeFftBases(fri_step, FieldElementT::RandomElement(&prng));
  FriParameters params{
      /*fri_step_list=*/{fri_step},
      /*last_layer_degree_bound=*/1,
      /*n_queries=*/1,
      /*fft_bases=*/UseOwned(&bases),
      /*field=*/Field::Create<FieldElementT>(),
      /*proof_of_work_bits=*/15};
  const auto eval_point = FieldElementT::RandomElement(&prng);

  // Take a polynomial of degree 7 evaluated at a domain of 8 points.
  TestPolynomial<FieldElementT> test_layer(&prng, Pow2(fri_step));
  const FieldElement res = ApplyFriLayers(
      FieldElementVector::Make(test_layer.GetData(bases[0])), FieldElement(eval_point), params, 0,
      0, *this->folder_);
  FieldElementT correction_factor = GetCorrectionFactor<FieldElementT>(fri_step);
  EXPECT_EQ(test_layer.EvalAt(eval_point) * correction_factor, res.As<FieldElementT>());
}

}  // namespace
}  // namespace details
}  // namespace fri
}  // namespace starkware
