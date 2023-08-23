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

#include "starkware/fri/fri_committed_layer.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "starkware/algebra/lde/lde.h"
#include "starkware/channel/prover_channel_mock.h"
#include "starkware/crypt_tools/blake2s.h"
#include "starkware/fri/fri_folder.h"
#include "starkware/fri/fri_parameters.h"
#include "starkware/fri/fri_test_utils.h"
#include "starkware/stark/utils.h"

namespace starkware {
namespace fri {
namespace layer {
namespace {

using testing::_;
using testing::AtLeast;

using TestedFieldTypes = ::testing::Types<TestFieldElement>;
using FieldElementT = TestFieldElement;

template <typename FieldElementT>
class FriCommittedLayerTest : public ::testing::Test {};

TYPED_TEST_CASE(FriCommittedLayerTest, TestedFieldTypes);

TYPED_TEST(FriCommittedLayerTest, FriCommittedLayerCallback) {
  auto callback = FirstLayerCallback([](const std::vector<uint64_t>& queries) {
    EXPECT_EQ(queries, std::vector<uint64_t>({4, 5, 8, 9, 12, 13}));
    return queries;
  });
  auto layer = FriCommittedLayerByCallback(1, UseOwned(&callback));
  const std::vector<uint64_t> queries{2, 4, 6};
  layer.Decommit(queries);
}

#ifndef __EMSCRIPTEN__

TYPED_TEST(FriCommittedLayerTest, FriCommittedLayerByTableProver) {
  Prng prng;
  std::unique_ptr<details::FriFolderBase> folder(
      details::FriFolderFromField(Field::Create<TypeParam>()));
  const size_t log2_eval_domain = 10;
  const size_t last_layer_degree_bound = 5;
  const size_t proof_of_work_bits = 15;
  const TypeParam offset(FftMultiplicativeGroup<TypeParam>::GroupUnit());
  FftBasesDefaultImpl<TypeParam> bases = MakeFftBases(log2_eval_domain, offset);

  FriParameters params(
      {{2, 3, 1} /*fri_step_list=*/,
       last_layer_degree_bound /*last_layer_degree_bound=*/,
       2 /*n_queries=*/,
       UseOwned(&bases) /*fft_bases=*/,
       Field::Create<TypeParam>() /*field=*/,
       proof_of_work_bits /*proof_of_work_bits=*/});

  details::TestPolynomial<TypeParam> test_layer(&prng, 64 * last_layer_degree_bound);

  // Construct the witness with a prefix of half the size of the entire evaluation domain.
  const size_t prefix_size = SafeDiv(Pow2(log2_eval_domain), 2);
  std::vector<TypeParam> eval_domain_data = test_layer.GetData(bases[0]);
  std::vector<TypeParam> witness_data(
      eval_domain_data.begin(), eval_domain_data.begin() + prefix_size);
  FieldElementVector evaluation(FieldElementVector::CopyFrom(witness_data));

  ProverChannelMock prover_channel;
  EXPECT_CALL(prover_channel, SendBytes(_)).Times(AtLeast(10));
  EXPECT_CALL(prover_channel, SendFieldElementImpl(_)).Times(1);

  TableProverFactory table_prover_factory = [&](uint64_t n_segments, uint64_t n_rows_per_segment,
                                                size_t n_columns) {
    EXPECT_EQ(n_segments, 1);
    EXPECT_EQ(n_rows_per_segment, 256);
    EXPECT_EQ(n_columns, 2);
    return GetTableProverFactory<Blake2s256>(
        &prover_channel, TypeParam::SizeInBytes(), 32, 1, 0,
        CommitmentHashes(Blake2s256::HashName()))(n_segments, n_rows_per_segment, n_columns);
  };

  FieldElement eval_point = FieldElement(TypeParam::RandomElement(&prng));
  const FriProverConfig fri_prover_config{
      FriProverConfig::kDefaultMaxNonChunkedLayerSize,
      FriProverConfig::kDefaultNumberOfChunksBetweenLayers, FriProverConfig::kAllInMemoryLayers};

  FriLayerOutOfMemory layer_0_out(std::move(evaluation), UseOwned(&bases));
  FriLayerProxy layer_1_proxy(*folder, UseOwned(&layer_0_out), eval_point, &fri_prover_config);
  FriLayerInMemory layer_2_in(UseOwned(&layer_1_proxy));

  FriCommittedLayerByTableProver committed_layer(
      1, UseOwned(&layer_2_in), table_prover_factory, params, 2);

  const std::vector<uint64_t> queries{2, 4, 6};
  committed_layer.Decommit(queries);
}

#endif  // #ifndef __EMSCRIPTEN__.

}  // namespace
}  // namespace layer
}  // namespace fri
}  // namespace starkware
