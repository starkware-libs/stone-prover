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

#include "starkware/fri/fri_layer.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "starkware/algebra/lde/lde.h"
#include "starkware/fri/fri_folder.h"
#include "starkware/fri/fri_parameters.h"
#include "starkware/fri/fri_test_utils.h"

namespace starkware {
namespace fri {
namespace layer {
namespace {

using TestedFieldTypes = ::testing::Types<TestFieldElement>;

template <typename FieldElementT>
class FriLayerTest : public ::testing::Test {
 public:
  FriLayerTest()
      : folder_(details::FriFolderFromField(Field::Create<FieldElementT>())),
        offset_(FftMultiplicativeGroup<FieldElementT>::GroupUnit()),
        bases_(log2_eval_domain_, offset_) {}

 protected:
  FieldElementVector CreateEvaluation(Prng* prng, size_t prefix_size = 0) {
    details::TestPolynomial<FieldElementT> test_layer(prng, first_layer_degree_bound_);

    if (prefix_size > 0) {
      std::vector<FieldElementT> domain_eval = test_layer.GetData(bases_[0]);
      witness_data_.assign(domain_eval.begin(), domain_eval.begin() + prefix_size);
    } else {
      witness_data_ = test_layer.GetData(bases_[0]);
    }
    return FieldElementVector::CopyFrom(witness_data_);
  }

  void Init() {
    Prng prng;

    FieldElementVector evaluation = CreateEvaluation(&prng, Pow2(log2_eval_domain_) / 2);

    eval_point_ =
        std::make_optional<FieldElement>(FieldElement(FieldElementT::RandomElement(&prng)));

    layer_0_out_ = std::make_unique<FriLayerOutOfMemory>(std::move(evaluation), UseOwned(&bases_));
    layer_1_proxy_ = std::make_unique<FriLayerProxy>(
        *folder_, UseOwned(layer_0_out_), *eval_point_, &fri_prover_config_);
  }

  void InitMoreLayers() {
    layer_2_out_ = std::make_unique<FriLayerOutOfMemory>(UseOwned(layer_1_proxy_), 256);
    layer_3_proxy_ = std::make_unique<FriLayerProxy>(
        *folder_, UseOwned(layer_2_out_), *eval_point_, &fri_prover_config_);
    layer_4_in_ = std::make_unique<FriLayerInMemory>(UseOwned(layer_3_proxy_));
  }

  std::unique_ptr<FriLayer> layer_0_out_;
  std::unique_ptr<FriLayer> layer_1_proxy_;
  std::unique_ptr<FriLayer> layer_2_out_;
  std::unique_ptr<FriLayer> layer_3_proxy_;
  std::unique_ptr<FriLayer> layer_4_in_;

  std::optional<FieldElement> eval_point_;
  std::vector<FieldElementT> witness_data_;

  std::unique_ptr<details::FriFolderBase> folder_;
  const size_t log2_eval_domain_ = 10;
  const size_t first_layer_degree_bound_ = 320;
  const FieldElementT offset_;
  const FriProverConfig fri_prover_config_{
      FriProverConfig::kDefaultMaxNonChunkedLayerSize,
      FriProverConfig::kDefaultNumberOfChunksBetweenLayers,
      FriProverConfig::kAllInMemoryLayers,
  };
  FftBasesDefaultImpl<FieldElementT> bases_;
};

TYPED_TEST_CASE(FriLayerTest, TestedFieldTypes);

TYPED_TEST(FriLayerTest, FriLayerLayerSize) {
  this->Init();
  this->InitMoreLayers();

  EXPECT_EQ(this->layer_0_out_->LayerSize(), 1024);
  EXPECT_EQ(this->layer_1_proxy_->LayerSize(), 512);
  EXPECT_EQ(this->layer_2_out_->LayerSize(), 512);
  EXPECT_EQ(this->layer_3_proxy_->LayerSize(), 256);
  EXPECT_EQ(this->layer_4_in_->LayerSize(), 256);
}

TYPED_TEST(FriLayerTest, FriLayerChunkSize) {
  this->Init();
  this->InitMoreLayers();

  EXPECT_EQ(this->layer_0_out_->ChunkSize(), 512);
  EXPECT_EQ(this->layer_1_proxy_->ChunkSize(), 256);
  EXPECT_EQ(this->layer_2_out_->ChunkSize(), 256);
  EXPECT_EQ(this->layer_3_proxy_->ChunkSize(), 128);
  EXPECT_EQ(this->layer_4_in_->ChunkSize(), 256);
}

TYPED_TEST(FriLayerTest, FirstOutOfMemoryLayerDegreeCheck) {
  this->Init();

  FieldElementVector layer_eval = this->layer_0_out_->GetAllEvaluation();

  auto layer_0_lde_manager = MakeLdeManager(*this->layer_0_out_->GetDomain());
  layer_0_lde_manager->AddEvaluation(layer_eval);
  EXPECT_EQ(layer_0_lde_manager->GetEvaluationDegree(0), this->first_layer_degree_bound_ - 1);
}

TYPED_TEST(FriLayerTest, FirstInMemoryLayer) {
  Prng prng;
  auto original_evaluation = prng.RandomFieldElementVector<TypeParam>(this->bases_[0].Size());
  FieldElementVector evaluation = FieldElementVector::CopyFrom(original_evaluation);

  FriLayerInMemory layer_in_memory(std::move(evaluation), UseOwned(&this->bases_));

  FieldElementVector layer_eval = layer_in_memory.GetAllEvaluation();

  EXPECT_EQ(original_evaluation, layer_eval.As<TypeParam>());
}

TYPED_TEST(FriLayerTest, FirstOutOfMemoryLayerChunk) {
  this->Init();

  auto chunk_size = this->layer_0_out_->ChunkSize();
  EXPECT_EQ(chunk_size, this->witness_data_.size());

  auto storage = this->layer_0_out_->MakeStorage();
  auto chunk = this->layer_0_out_->GetChunk(storage.get(), chunk_size, 0);

  EXPECT_EQ(chunk.template As<TypeParam>(), gsl::span(this->witness_data_));
}

TYPED_TEST(FriLayerTest, ProxyLayer) {
  this->Init();

  FieldElementVector prev_layer_eval = this->layer_0_out_->GetAllEvaluation();

  FieldElementVector layer_eval = this->layer_1_proxy_->GetAllEvaluation();
  auto layer_1_lde_manager = MakeLdeManager(*this->layer_1_proxy_->GetDomain());
  layer_1_lde_manager->AddEvaluation(layer_eval);
  EXPECT_EQ(layer_1_lde_manager->GetEvaluationDegree(0), this->first_layer_degree_bound_ / 2 - 1);

  FieldElementVector folded_layer =
      this->folder_->ComputeNextFriLayer(this->bases_.At(0), prev_layer_eval, *this->eval_point_);

  EXPECT_EQ(layer_eval, folded_layer);
}

TYPED_TEST(FriLayerTest, OutOfMemoryOverOutOfMemory) {
  this->Init();
  FriLayerOutOfMemory layer_2_out(UseOwned(this->layer_1_proxy_), 256);

  FieldElementVector prev_layer_eval = this->layer_0_out_->GetAllEvaluation();

  FieldElementVector layer_eval = layer_2_out.GetAllEvaluation();
  auto layer_2_lde_manager = MakeLdeManager(*layer_2_out.GetDomain());
  layer_2_lde_manager->AddEvaluation(layer_eval);
  EXPECT_EQ(layer_2_lde_manager->GetEvaluationDegree(0), this->first_layer_degree_bound_ / 2 - 1);

  FieldElementVector folded_layer =
      this->folder_->ComputeNextFriLayer(this->bases_.At(0), prev_layer_eval, *this->eval_point_);

  EXPECT_EQ(layer_eval, folded_layer);

  auto chunk_size = layer_2_out.ChunkSize();
  auto storage = layer_2_out.MakeStorage();
  auto chunk = layer_2_out.GetChunk(storage.get(), chunk_size, 0);
  EXPECT_EQ(chunk_size, chunk.Size());
  for (size_t i = 0; i < chunk_size; ++i) {
    EXPECT_EQ(chunk[i], folded_layer[i]);
  }
}

TYPED_TEST(FriLayerTest, InMemory) {
  this->Init();
  FriLayerInMemory layer_2_in(UseOwned(this->layer_1_proxy_));

  FieldElementVector prev_layer_eval = this->layer_0_out_->GetAllEvaluation();

  FieldElementVector layer_eval = layer_2_in.GetAllEvaluation();
  auto layer_2_lde_manager = MakeLdeManager(*layer_2_in.GetDomain());
  layer_2_lde_manager->AddEvaluation(layer_eval);
  EXPECT_EQ(layer_2_lde_manager->GetEvaluationDegree(0), this->first_layer_degree_bound_ / 2 - 1);

  FieldElementVector folded_layer =
      this->folder_->ComputeNextFriLayer(this->bases_.At(0), prev_layer_eval, *this->eval_point_);

  EXPECT_EQ(layer_eval.Size(), folded_layer.Size());
  for (size_t i = 0; i < layer_eval.Size(); ++i) {
    EXPECT_EQ(layer_eval[i], folded_layer[i]);
  }

  auto chunk_size = layer_2_in.ChunkSize();
  EXPECT_EQ(chunk_size, layer_2_in.LayerSize());

  auto storage = layer_2_in.MakeStorage();
  auto chunk = layer_2_in.GetChunk(storage.get(), chunk_size, 0);
  EXPECT_EQ(chunk_size, chunk.Size());
  for (size_t i = 0; i < chunk_size; ++i) {
    EXPECT_EQ(chunk[i], folded_layer[i]);
  }
}

TYPED_TEST(FriLayerTest, EvaluationTest) {
  this->Init();
  this->InitMoreLayers();

  uint64_t index = 42;
  std::vector<uint64_t> required_row_indices{index};
  FieldElementVector layer_eval = this->layer_4_in_->GetAllEvaluation();
  auto evaluation = this->layer_4_in_->EvalAtPoints(required_row_indices);
  EXPECT_EQ(evaluation[0], layer_eval[index]);
}

}  // namespace
}  // namespace layer
}  // namespace fri
}  // namespace starkware
