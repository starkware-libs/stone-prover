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

#include "starkware/algebra/lde/cached_lde_manager.h"

#include <algorithm>
#include <set>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/lde/lde_manager_mock.h"
#include "starkware/algebra/polymorphic/test_utils.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::_;
using testing::AnyNumber;
using testing::ElementsAreArray;
using testing::HasSubstr;
using testing::Invoke;
using testing::StrictMock;

#ifndef __EMSCRIPTEN__

/*
  Used to mock the output of LdeManager::EvalOnCoset().
  Assumes the mocked function second argument (arg1) behaves like span<FieldElementVector>, and
  writes to it.
  Example use: EXPECT_CALL(..., EvalOnCoset(...)).WillOnce(SetEvaluation(mock_evaluation)).
*/
ACTION_P(SetEvaluation, mock_evaluation) {
  auto& result = arg1;
  ASSERT_EQ(result.size(), mock_evaluation.size());

  for (size_t column_index = 0; column_index < result.size(); ++column_index) {
    ASSERT_EQ(result[column_index].Size(), mock_evaluation[column_index].size());
    std::copy(
        mock_evaluation[column_index].begin(), mock_evaluation[column_index].end(),
        result[column_index].template As<TestFieldElement>().begin());
  }
}

/*
  Used to mock the output of LdeManager::EvalAtPoints().
  Example use: EXPECT_CALL(..., EvalAtPoints(...)).WillOnce(SetPointsEvaluation(mock_evaluation)).
*/
ACTION_P(SetPointsEvaluation, eval) {
  const FieldElementSpan& outputs = arg2;
  ASSERT_EQ(outputs.Size(), eval.size());
  std::copy(eval.begin(), eval.end(), outputs.template As<TestFieldElement>().begin());
}

class CachedLdeManagerTest : public ::testing::Test {
 public:
  CachedLdeManagerTest()
      : offsets_(prng_.RandomFieldElementVector<TestFieldElement>(n_cosets_)),
        domain_(MakeFftDomain<TestFieldElement>(
            SafeLog2(coset_size_), TestFieldElement::RandomElement(&prng_))),
        bases_(SafeLog2(coset_size_), TestFieldElement::One()),
        lde_manager_(domain_) {
    EXPECT_CALL(lde_manager_, FftPrecompute(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke([](const FieldElement&) {
          return std::make_unique<DummyFftWithPrecompute>(DummyFftWithPrecompute());
        }));
    EXPECT_CALL(lde_manager_, IfftPrecompute()).Times(1).WillRepeatedly(Invoke([]() {
      return std::make_unique<DummyFftWithPrecompute>(DummyFftWithPrecompute());
    }));
  }

 protected:
  const size_t coset_size_ = 16;
  const size_t n_cosets_ = 10;
  const size_t n_columns_ = 3;
  Prng prng_;
  std::vector<TestFieldElement> offsets_;
  FftDomain<FftMultiplicativeGroup<TestFieldElement>> domain_;
  MultiplicativeFftBases<TestFieldElement> bases_;
  StrictMock<LdeManagerMock> lde_manager_;
  std::optional<CachedLdeManager> cached_lde_manager_;

  /*
    Populates the mocked CachedLdeManager with random evaluations.
    Keeps the evaluations in evaluations_ for comparisons.
  */
  void StartTest(bool store_full_lde, bool use_fft_for_eval);

  void TestEvalAtPointsResult(
      const std::vector<std::pair<size_t, uint64_t>>& coset_point_indices,
      const std::vector<FieldElementVector>& outputs);

  /*
    Indices are: coset_index, column_index, point_index (index within coset).
  */
  std::vector<std::vector<std::vector<TestFieldElement>>> evaluations_;
};

void CachedLdeManagerTest::StartTest(bool store_full_lde, bool use_fft_for_eval) {
  CachedLdeManager::Config config{/*store_full_lde=*/store_full_lde,
                                  /*use_fft_for_eval=*/use_fft_for_eval};
  cached_lde_manager_.emplace(
      config,
      /*lde_manager=*/UseOwned(&lde_manager_),
      /*coset_offsets=*/
      UseMovedValue(FieldElementVector::CopyFrom(offsets_)));

  // AddEvaluation FieldElementVector&& variant.
  EXPECT_CALL(lde_manager_, AddEvaluation_rvr(_, _)).Times(n_columns_);
  for (size_t i = 0; i < n_columns_; ++i) {
    FieldElementVector evaluation =
        FieldElementVector::Make(prng_.RandomFieldElementVector<TestFieldElement>(coset_size_));
    cached_lde_manager_->AddEvaluation(std::move(evaluation));
  }
  cached_lde_manager_->FinalizeAdding();

  // Evaluate cosets once.
  auto storage = cached_lde_manager_->AllocateStorage();
  for (size_t coset_index = 0; coset_index < n_cosets_; ++coset_index) {
    std::vector<std::vector<TestFieldElement>> coset_evaluation;
    FieldElement offset = FieldElement(offsets_[coset_index]);

    // Generate random coset values.
    for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
      coset_evaluation.push_back(prng_.RandomFieldElementVector<TestFieldElement>(coset_size_));
    }
    evaluations_.push_back(coset_evaluation);

    // Setup callback to return that coset.
    EXPECT_CALL(lde_manager_, EvalOnCoset(offset, _, _)).WillOnce(SetEvaluation(coset_evaluation));
    auto result = cached_lde_manager_->EvalOnCoset(coset_index, storage.get());

    // Test that we got the correct evaluation.
    ASSERT_EQ(result->size(), n_columns_);
    for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
      ASSERT_EQ((*result)[column_index].As<TestFieldElement>(), coset_evaluation[column_index]);
    }
  }

  cached_lde_manager_->FinalizeEvaluations();

  testing::Mock::VerifyAndClearExpectations(&lde_manager_);
}

/*
  Tests the correct AddEvaluation version is called.
*/
TEST_F(CachedLdeManagerTest, AddEvaluationVariations) {
  CachedLdeManager::Config config{/*store_full_lde=*/false,
                                  /*use_fft_for_eval=*/false};
  cached_lde_manager_.emplace(
      config,
      /*lde_manager=*/UseOwned(&lde_manager_),
      /*coset_offsets=*/UseMovedValue(FieldElementVector::CopyFrom(offsets_)));

  auto vec = prng_.RandomFieldElementVector<TestFieldElement>(coset_size_);
  FieldElementVector evaluation =
      FieldElementVector::Make(prng_.RandomFieldElementVector<TestFieldElement>(coset_size_));
  ConstFieldElementSpan const_evaluation =
      static_cast<const FieldElementVector&>(evaluation).AsSpan();

  // AddEvaluation ConstFieldElementSpan variant.
  EXPECT_CALL(lde_manager_, AddEvaluation(const_evaluation, _));
  cached_lde_manager_->AddEvaluation(const_evaluation);

  // AddEvaluation FieldElementVector&& variant.
  EXPECT_CALL(lde_manager_, AddEvaluation_rvr(_, _));
  cached_lde_manager_->AddEvaluation(std::move(evaluation));
}

TEST_F(CachedLdeManagerTest, EvalOnCoset_Cache) {
  StartTest(/*store_full_lde=*/true, /*use_fft_for_eval=*/false);

  // Evaluate again. Expect No additional computation, and same coset values as before.
  auto storage = cached_lde_manager_->AllocateStorage();
  for (size_t coset_index = 0; coset_index < n_cosets_; ++coset_index) {
    EXPECT_CALL(lde_manager_, EvalOnCoset(FieldElement(offsets_[coset_index]), _, _)).Times(0);
    auto result = cached_lde_manager_->EvalOnCoset(coset_index, storage.get());
    ASSERT_EQ(result->size(), n_columns_);
    for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
      ASSERT_EQ(
          (*result)[column_index].As<TestFieldElement>(), evaluations_[coset_index][column_index]);
    }
  }
}

TEST_F(CachedLdeManagerTest, EvalOnCoset_NoCache) {
  StartTest(/*store_full_lde=*/false, /*use_fft_for_eval=*/false);

  // Evaluate again. Expect recomputation.
  auto storage = cached_lde_manager_->AllocateStorage();
  for (size_t coset_index = 0; coset_index < n_cosets_; ++coset_index) {
    EXPECT_CALL(lde_manager_, EvalOnCoset(FieldElement(offsets_[coset_index]), _, _))
        .WillOnce(SetEvaluation(evaluations_[coset_index]));

    auto result = cached_lde_manager_->EvalOnCoset(coset_index, storage.get());
    ASSERT_EQ(result->size(), n_columns_);
    for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
      ASSERT_EQ(
          (*result)[column_index].As<TestFieldElement>(), evaluations_[coset_index][column_index]);
    }
  }
}

void CachedLdeManagerTest::TestEvalAtPointsResult(
    const std::vector<std::pair<size_t, uint64_t>>& coset_point_indices,
    const std::vector<FieldElementVector>& outputs) {
  // Compare result.
  for (size_t column_index = 0; column_index < n_columns_; column_index++) {
    for (size_t i = 0; i < coset_point_indices.size(); ++i) {
      const auto& [coset_index, point_index] = coset_point_indices[i];
      ASSERT_EQ(
          outputs[column_index][i].As<TestFieldElement>(),
          evaluations_[coset_index][column_index][point_index]);
    }
  }
}

TEST_F(CachedLdeManagerTest, EvalAtPoints_Cache) {
  StartTest(/*store_full_lde=*/true, /*use_fft_for_eval=*/false);

  const size_t n_points = 30;
  std::vector<std::pair<size_t, uint64_t>> coset_point_indices;
  coset_point_indices.reserve(n_points);
  for (size_t i = 0; i < n_points; ++i) {
    coset_point_indices.emplace_back(
        prng_.UniformInt(0, 4), prng_.UniformInt(0, static_cast<int>(coset_size_ - 1)));
  }

  // Allocate outputs.
  std::vector<FieldElementVector> outputs;
  for (size_t column_index = 0; column_index < n_columns_; column_index++) {
    outputs.push_back(
        FieldElementVector::MakeUninitialized(Field::Create<TestFieldElement>(), n_points));
  }

  // Evaluate points.
  std::vector<FieldElementSpan> outputs_spans = {outputs.begin(), outputs.end()};
  cached_lde_manager_->EvalAtPoints(coset_point_indices, outputs_spans);

  // Compare result.
  TestEvalAtPointsResult(coset_point_indices, outputs);
}

TEST_F(CachedLdeManagerTest, EvalAtPoints_NoCacheNoFft) {
  StartTest(/*store_full_lde=*/false, /*use_fft_for_eval=*/false);

  const size_t n_points = 30;
  std::vector<std::pair<size_t, uint64_t>> coset_point_indices;
  coset_point_indices.reserve(n_points);
  for (size_t i = 0; i < n_points; ++i) {
    coset_point_indices.emplace_back(
        prng_.UniformInt(0, 4), prng_.UniformInt(0, static_cast<int>(coset_size_ - 1)));
  }

  // Compute the expected points parameter for EvalAtPoints.
  std::vector<TestFieldElement> expected_points;
  for (const auto& [coset_index, point_index] : coset_point_indices) {
    auto coset_domain = domain_.GetShiftedDomain(offsets_[coset_index]);
    expected_points.push_back(coset_domain[point_index]);
  }

  // Compute the expected evaluations at these points.
  std::vector<std::vector<TestFieldElement>> expected_outputs;
  for (size_t column_index = 0; column_index < n_columns_; column_index++) {
    std::vector<TestFieldElement> expected_eval;
    expected_eval.reserve(coset_point_indices.size());
    for (const auto& [coset_index, point_index] : coset_point_indices) {
      expected_eval.push_back(evaluations_[coset_index][column_index][point_index]);
    }
    expected_outputs.push_back(std::move(expected_eval));
  }

  // Allocate outputs.
  std::vector<FieldElementVector> outputs;
  for (size_t column_index = 0; column_index < n_columns_; column_index++) {
    outputs.push_back(
        FieldElementVector::MakeUninitialized(Field::Create<TestFieldElement>(), n_points));
  }

  // Evaluate points.
  for (size_t column_index = 0; column_index < n_columns_; column_index++) {
    EXPECT_CALL(
        lde_manager_,
        EvalAtPoints(
            column_index, IsFieldElementVector<TestFieldElement>(ElementsAreArray(expected_points)),
            _))
        .WillOnce(SetPointsEvaluation(expected_outputs[column_index]));
  }
  std::vector<FieldElementSpan> outputs_spans = {outputs.begin(), outputs.end()};
  cached_lde_manager_->EvalAtPoints(coset_point_indices, outputs_spans);

  // Compare result.
  TestEvalAtPointsResult(coset_point_indices, outputs);
}

TEST_F(CachedLdeManagerTest, EvalAtPoints_NoCacheFft) {
  StartTest(/*store_full_lde=*/false, /*use_fft_for_eval=*/true);

  const size_t n_points = 30;
  std::vector<std::pair<size_t, uint64_t>> coset_point_indices;
  coset_point_indices.reserve(n_points);
  for (size_t i = 0; i < n_points; ++i) {
    coset_point_indices.emplace_back(
        prng_.UniformInt(0, 4), prng_.UniformInt(0, static_cast<int>(coset_size_ - 1)));
  }

  // Allocate outputs.
  std::vector<FieldElementVector> outputs;
  for (size_t column_index = 0; column_index < n_columns_; column_index++) {
    outputs.push_back(
        FieldElementVector::MakeUninitialized(Field::Create<TestFieldElement>(), n_points));
  }

  // Expect calls.
  std::set<size_t> unique_coset_indices;
  for (const auto& [coset_index, point_index] : coset_point_indices) {
    (void)point_index;  // Unused.
    unique_coset_indices.insert(coset_index);
  }
  for (size_t coset_index : unique_coset_indices) {
    EXPECT_CALL(lde_manager_, EvalOnCoset(FieldElement(offsets_[coset_index]), _, _))
        .WillOnce(SetEvaluation(evaluations_[coset_index]));
  }
  // Evaluate points.
  std::vector<FieldElementSpan> outputs_spans = {outputs.begin(), outputs.end()};
  cached_lde_manager_->EvalAtPoints(coset_point_indices, outputs_spans);

  // Compare result.
  TestEvalAtPointsResult(coset_point_indices, outputs);
}

TEST_F(CachedLdeManagerTest, AddAfterEvalOnCoset) {
  StartTest(/*store_full_lde=*/true, /*use_fft_for_eval=*/false);

  FieldElementVector evaluation =
      FieldElementVector::Make(prng_.RandomFieldElementVector<TestFieldElement>(coset_size_));

  EXPECT_ASSERT(
      cached_lde_manager_->AddEvaluation(evaluation), HasSubstr("Cannot call AddEvaluation after"));
}

TEST_F(CachedLdeManagerTest, AddAfterEvalAtPoints) {
  CachedLdeManager::Config config{/*store_full_lde=*/false,
                                  /*use_fft_for_eval=*/false};
  cached_lde_manager_.emplace(
      config,
      /*lde_manager=*/UseOwned(&lde_manager_),
      /*coset_offsets=*/
      UseMovedValue(FieldElementVector::CopyFrom(offsets_)));

  FieldElementVector evaluation =
      FieldElementVector::Make(prng_.RandomFieldElementVector<TestFieldElement>(coset_size_));

  EXPECT_CALL(lde_manager_, AddEvaluation(_, _)).Times(AnyNumber());
  EXPECT_CALL(lde_manager_, EvalAtPoints(_, _, _)).Times(AnyNumber());

  cached_lde_manager_->AddEvaluation(evaluation);
  cached_lde_manager_->FinalizeAdding();

  const size_t n_points = 30;
  std::vector<std::pair<size_t, uint64_t>> coset_point_indices;
  coset_point_indices.reserve(n_points);
  for (size_t i = 0; i < n_points; ++i) {
    coset_point_indices.emplace_back(
        prng_.UniformInt(0, 4), prng_.UniformInt(0, static_cast<int>(coset_size_ - 1)));
  }
  // Allocate outputs.
  std::vector<FieldElementVector> outputs;
  outputs.push_back(
      FieldElementVector::MakeUninitialized(Field::Create<TestFieldElement>(), n_points));
  std::vector<FieldElementSpan> outputs_spans = {outputs.begin(), outputs.end()};
  cached_lde_manager_->EvalAtPoints(coset_point_indices, outputs_spans);
  EXPECT_ASSERT(
      cached_lde_manager_->AddEvaluation(evaluation), HasSubstr("Cannot call AddEvaluation after"));
}

TEST_F(CachedLdeManagerTest, ComputeAfterFinalizeEvaluations) {
  StartTest(/*store_full_lde=*/true, /*use_fft_for_eval=*/false);

  const size_t n_points = 30;
  FieldElementVector points =
      FieldElementVector::Make(prng_.RandomFieldElementVector<TestFieldElement>(n_points));
  FieldElementVector output =
      FieldElementVector::MakeUninitialized(Field::Create<TestFieldElement>(), n_points);

  EXPECT_ASSERT(
      cached_lde_manager_->EvalAtPointsNotCached(0, points, output),
      HasSubstr("FinalizeEvaluations()"));
}

TEST_F(CachedLdeManagerTest, SecondFinalizeAdding) {
  StartTest(/*store_full_lde=*/false, /*use_fft_for_eval=*/true);
  EXPECT_ASSERT(cached_lde_manager_->FinalizeAdding(), HasSubstr("FinalizeAdding called twice."));
}

#endif

}  // namespace
}  // namespace starkware
