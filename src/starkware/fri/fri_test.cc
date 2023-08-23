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

#include "starkware/fri/fri_prover.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/domains/multiplicative_group.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/lde/lde.h"
#include "starkware/algebra/polymorphic/field.h"
#include "starkware/algebra/polymorphic/field_element_vector.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel_mock.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/commitment_scheme/table_prover_mock.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/commitment_scheme/table_verifier_mock.h"
#include "starkware/crypt_tools/blake2s.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/fft_utils/fft_domain.h"
#include "starkware/fri/fri_parameters.h"
#include "starkware/fri/fri_test_utils.h"
#include "starkware/fri/fri_verifier.h"
#include "starkware/proof_system/proof_system.h"

namespace starkware {
namespace {

using starkware::fri::details::TestPolynomial;
using testing::_;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::Invoke;
using testing::MockFunction;
using testing::Return;
using testing::StrictMock;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

using TestedFieldTypes = ::testing::Types<TestFieldElement>;

inline size_t CalcLogNInMemoryFriElements(
    size_t witness_size, const std::vector<size_t>& fri_step_list) {
  size_t num_layers = 0;
  for (size_t i = 0; i < fri_step_list.size() - 2; ++i) {
    num_layers += fri_step_list[i];
  }
  return Log2Floor(witness_size * 2 / Pow2(num_layers));
}

#ifndef __EMSCRIPTEN__

template <typename FieldElementT, typename BasesT>
FieldElementT ApplyMultipleBasisTransforms(
    const FieldElementT& point, const BasesT& bases, size_t n_transforms) {
  FieldElementT new_point = point;
  for (size_t i = 0; i < n_transforms; ++i) {
    new_point = bases.ApplyBasisTransformTmpl(new_point, i);
  }
  return new_point;
}

template <typename FieldElementT>
class FriTest : public ::testing::Test {};

TYPED_TEST_CASE(FriTest, TestedFieldTypes);

TYPED_TEST(FriTest, ProverBasicFlowWithMockChannel) {
  using FieldElementT = TypeParam;
  Prng prng;

  const size_t log2_eval_domain = 10;
  const size_t last_layer_degree_bound = 5;
  const size_t proof_of_work_bits = 15;
  const FieldElementT offset =
      FftMultiplicativeGroup<FieldElementT>::GroupUnit();  // RandomElement(&prng);
  auto bases = MakeFftBases(log2_eval_domain, offset);
  FriParameters params{
      /*fri_step_list=*/{2, 3, 1},
      /*last_layer_degree_bound=*/last_layer_degree_bound,
      /*n_queries=*/2,
      /*fft_bases=*/UseOwned(&bases),
      /*field=*/Field::Create<FieldElementT>(),
      /*proof_of_work_bits=*/proof_of_work_bits};
  FriProverConfig fri_prover_config{
      /*max_non_chunked_layer_size=*/FriProverConfig::kDefaultMaxNonChunkedLayerSize,
      /*n_chunks_between_layers=*/FriProverConfig::kDefaultNumberOfChunksBetweenLayers,
      /*log_n_max_in_memory_fri_layer_elements=*/FriProverConfig::kAllInMemoryLayers};
  std::unique_ptr<fri::details::FriFolderBase> folder =
      fri::details::FriFolderFromField(params.field);

  TestPolynomial<FieldElementT> test_layer(&prng, 64 * last_layer_degree_bound);

  // Construct the witness with a prefix of half the size of the entire evaluation domain.
  const size_t prefix_size = Pow2(log2_eval_domain) / 2;
  std::vector<FieldElementT> eval_domain_data = test_layer.GetData(bases[0]);
  std::vector<FieldElementT> witness_data(
      eval_domain_data.begin(), eval_domain_data.begin() + prefix_size);

  const FieldElementT eval_point = FieldElementT::RandomElement(&prng);

  const FieldElementVector second_layer = folder->ComputeNextFriLayer(
      bases[1],
      folder->ComputeNextFriLayer(
          bases[0], FieldElementVector::CopyFrom(eval_domain_data), FieldElement(eval_point)),
      FieldElement(ApplyMultipleBasisTransforms(eval_point, bases, 1)));
  const FieldElementVector third_layer = folder->ComputeNextFriLayer(
      bases[4],
      folder->ComputeNextFriLayer(
          bases[3],
          folder->ComputeNextFriLayer(
              bases[2], second_layer,
              FieldElement(ApplyMultipleBasisTransforms(eval_point, bases, 2))),
          FieldElement(ApplyMultipleBasisTransforms(eval_point, bases, 3))),
      FieldElement(ApplyMultipleBasisTransforms(eval_point, bases, 4)));

  // last_layer_coefs will contain the coefficients of the last layer, as sent on the channel.
  FieldElementVector last_layer_coefs = FieldElementVector::Make<FieldElementT>();

  // Set mock expectations.
  StrictMock<ProverChannelMock> prover_channel;
  TableProverMockFactory table_prover_factory(
      {std::make_tuple(2, 16, 8), std::make_tuple(1, 16, 2)});
  StrictMock<MockFunction<void(const std::vector<uint64_t>& queries)>> first_layer_queries_callback;
  {
    testing::InSequence dummy;

    // Commit phase expectations.

    // The prover will request three evaluation points. Answer with eval_point, eval_point^4 and
    // eval_point^32. This sequence will allow testing the polynomial of the last layer. See below.
    EXPECT_CALL(prover_channel, ReceiveFieldElementImpl(_))
        .WillOnce(Return(FieldElement(eval_point)));
    EXPECT_CALL(table_prover_factory[0], AddSegmentForCommitment(_, 0, 8));
    EXPECT_CALL(table_prover_factory[0], AddSegmentForCommitment(_, 1, 8));
    EXPECT_CALL(table_prover_factory[0], Commit());
    EXPECT_CALL(prover_channel, ReceiveFieldElementImpl(_))
        .WillOnce(Return(FieldElement(ApplyMultipleBasisTransforms(eval_point, bases, 2))));
    EXPECT_CALL(table_prover_factory[1], AddSegmentForCommitment(_, 0, 2));
    EXPECT_CALL(table_prover_factory[1], Commit());
    EXPECT_CALL(prover_channel, ReceiveFieldElementImpl(_))
        .WillOnce(Return(FieldElement(ApplyMultipleBasisTransforms(eval_point, bases, 5))));

    // The prover will send the coefficients of the last layer. Save those in last_layer_coefs.
    EXPECT_CALL(prover_channel, SendFieldElementSpanImpl(_))
        .WillOnce(Invoke([&last_layer_coefs](const ConstFieldElementSpan& x) {
          last_layer_coefs = FieldElementVector::CopyFrom(x);
        }));

    // Query phase expectations.
    // Proof of work.
    EXPECT_CALL(prover_channel, ApplyProofOfWork(proof_of_work_bits));

    // The prover will request two query locations. Answer with 0 and 6.
    EXPECT_CALL(prover_channel, ReceiveNumberImpl(256)).WillOnce(Return(0)).WillOnce(Return(6));
    // The verifier requested indices 0 and 6 which refer to the two cosets (0, 1, 2, 3) and (24,
    // 25, 26, 27) in the first layer (x -> (4 * x, ..., 4 * x + 3)). Hence, the prover will send
    // data[0], ..., data[3], data[24], ..., data[27] from the top layer.
    // Handling the first layer is done using a callback to first_layer_queries_callback.
    EXPECT_CALL(first_layer_queries_callback, Call(ElementsAre(0, 1, 2, 3, 24, 25, 26, 27)));

    // Decommitment phase expectations.

    // As the verifier requested indices 0 and 6 (which refer to (0, 1, 2, 3) and (24, 25, 26,
    // 27) in the first layer), it will be able to compute the values at indices 0 and 6 of the
    // second layer of FRI. The prover will additionally send the values at indices 1...5, 7
    // which will allow the verifier to compute index 0 on the third layer. Then it will send
    // index 1 of the third layer to allow the verifier to continue to the forth (and last)
    // layer.
    // We mock StartDecommitmentPhase() to ask for rows 0,...,9.
    const std::vector<uint64_t> simulated_requested_rows = {0};
    EXPECT_CALL(
        table_prover_factory[0], StartDecommitmentPhase(
                                     UnorderedElementsAreArray(
                                         {RowCol(0, 1), RowCol(0, 2), RowCol(0, 3), RowCol(0, 4),
                                          RowCol(0, 5), RowCol(0, 7)}),
                                     UnorderedElementsAre(RowCol(0, 0), RowCol(0, 6))))
        .WillOnce(Return(simulated_requested_rows));
    EXPECT_CALL(table_prover_factory[0], Decommit(_))
        .WillOnce(Invoke([&](gsl::span<const ConstFieldElementSpan> aa) {
          EXPECT_EQ(aa.size(), 8);
          for (size_t i = 0; i < 8; ++i) {
            EXPECT_EQ(aa[i][0], second_layer[i]);
          }
        }));

    EXPECT_CALL(
        table_prover_factory[1],
        StartDecommitmentPhase(
            UnorderedElementsAre(RowCol(0, 1)), UnorderedElementsAre(RowCol(0, 0))))
        .WillOnce(Return(std::vector<uint64_t>{}));

    EXPECT_CALL(table_prover_factory[1], Decommit(_))
        .WillOnce(Invoke([&](gsl::span<const ConstFieldElementSpan> aa) {
          const FieldElementVector empty_vector = FieldElementVector::Make<FieldElementT>();
          std::vector<ConstFieldElementSpan> v;
          v.push_back(empty_vector.AsSpan());
          v.push_back(empty_vector.AsSpan());
          EXPECT_EQ(aa, gsl::make_span(v));
        }));
  }
  TableProverFactory table_prover_factory_as_factory = table_prover_factory.AsFactory();
  FriProver::FirstLayerCallback first_layer_queries_callback_as_function =
      first_layer_queries_callback.AsStdFunction();
  fri_prover_config.log_n_max_in_memory_fri_layer_elements =
      CalcLogNInMemoryFriElements(witness_data.size(), params.fri_step_list);
  FriProver fri_prover(
      UseOwned(&prover_channel), UseOwned(&table_prover_factory_as_factory), UseOwned(&params),
      FieldElementVector::CopyFrom(witness_data),
      UseOwned(&first_layer_queries_callback_as_function), UseOwned(&fri_prover_config));
  fri_prover.ProveFri();

  // In multiplicative FRI, if the verifier sends a sequence of evaluation points of the form x_0,
  // x_0^{2^fri_step_list[0]}, x_0^{2^{fri_step_list[0] + fri_step_list[1]}}, ..., the polynomial
  // p(y) of the last layer will satisfy p(x_0^{2^{fri_step_list[0] + ...}}) = f(x_0).
  // Since we skip the division by 2 in FRI, the expected result is 2^n f(x_0) where n is the sum
  // of fri_step_list.
  // Another issue is that the coefficients on the last layer are computed without using the offset.
  // So, instead of p(x_0^64) we have to test p((x_0/offset)^64).
  EXPECT_EQ(last_layer_degree_bound, last_layer_coefs.Size());

  FieldElementT correction_factor = FieldElementT::Uninitialized();
  FieldElementT corrected_eval_point = FieldElementT::Uninitialized();

  correction_factor = FieldElementT::FromUint(64);
  corrected_eval_point = eval_point / offset;

  // Evaluate the last layer polynomial at a point.
  FieldElementT test_value = ExtrapolatePointFromCoefficients(
      bases.FromLayer(6), last_layer_coefs.As<FieldElementT>(),
      ApplyMultipleBasisTransforms(corrected_eval_point, bases, 6));

  EXPECT_EQ(correction_factor * test_layer.EvalAt(eval_point), test_value);
}

TYPED_TEST(FriTest, VerifierBasicFlowWithMockChannel) {
  using FieldElementT = TypeParam;
  Prng prng;
  const size_t last_layer_degree_bound = 5;
  const size_t proof_of_work_bits = 15;

  const Field field = Field::Create<FieldElementT>();
  auto bases = MakeFftBases(10, FieldElementT::RandomElement(&prng));
  FriParameters params{
      /*fri_step_list=*/{2, 3, 1},
      /*last_layer_degree_bound=*/last_layer_degree_bound,
      /*n_queries=*/2,
      /*fft_bases=*/UseOwned(&bases),
      /*field=*/Field::Create<FieldElementT>(),
      /*proof_of_work_bits=*/proof_of_work_bits};
  std::unique_ptr<fri::details::FriFolderBase> folder =
      fri::details::FriFolderFromField(params.field);

  TestPolynomial<FieldElementT> test_layer(&prng, 64 * last_layer_degree_bound);
  std::vector<FieldElementT> witness_data = test_layer.GetData(bases[0]);
  // Choose evaluation points for the three layers.
  const std::vector<FieldElementT> eval_points = prng.RandomFieldElementVector<FieldElementT>(3);

  // Set mock expectations.
  StrictMock<VerifierChannelMock> verifier_channel;
  TableVerifierMockFactory table_verifier_factory(
      {std::make_tuple(Field::Create<FieldElementT>(), 32, 8),
       std::make_tuple(Field::Create<FieldElementT>(), 16, 2)});
  StrictMock<MockFunction<FieldElementVector(const std::vector<uint64_t>& queries)>>
      first_layer_queries_callback;

  const FieldElementVector second_layer = folder->ComputeNextFriLayer(
      bases[1],
      folder->ComputeNextFriLayer(
          bases[0], FieldElementVector::CopyFrom(witness_data), FieldElement(eval_points[0])),
      FieldElement(ApplyMultipleBasisTransforms(eval_points[0], bases, 1)));
  const FieldElementVector third_layer = folder->ComputeNextFriLayer(
      bases[4],
      folder->ComputeNextFriLayer(
          bases[3],
          folder->ComputeNextFriLayer(bases[2], second_layer, FieldElement(eval_points[1])),
          FieldElement(ApplyMultipleBasisTransforms(eval_points[1], bases.FromLayer(2), 1))),
      FieldElement(ApplyMultipleBasisTransforms(eval_points[1], bases.FromLayer(2), 2)));

  const FieldElementVector fourth_layer =
      folder->ComputeNextFriLayer(bases[5], third_layer, FieldElement(eval_points[2]));

  auto fourth_layer_bases = bases.FromLayer(6);
  std::unique_ptr<LdeManager> fourth_layer_lde = MakeLdeManager(fourth_layer_bases);
  fourth_layer_lde->AddEvaluation(fourth_layer);
  EXPECT_EQ(fourth_layer_lde->GetEvaluationDegree(0), last_layer_degree_bound - 1);
  const auto fourth_layer_coefs = fourth_layer_lde->GetCoefficients(0);

  {
    testing::InSequence dummy;

    // Commit phase expectations.
    // The verifier will send three elements - eval_points[0], eval_points[1] and eval_points[2].
    EXPECT_CALL(verifier_channel, GetAndSendRandomFieldElementImpl(_))
        .WillOnce(Return(FieldElement(eval_points[0])));
    EXPECT_CALL(table_verifier_factory[0], ReadCommitment());
    EXPECT_CALL(verifier_channel, GetAndSendRandomFieldElementImpl(_))
        .WillOnce(Return(FieldElement(eval_points[1])));
    EXPECT_CALL(table_verifier_factory[1], ReadCommitment());
    EXPECT_CALL(verifier_channel, GetAndSendRandomFieldElementImpl(_))
        .WillOnce(Return(FieldElement(eval_points[2])));

    EXPECT_CALL(verifier_channel, ReceiveFieldElementSpanImpl(_, _))
        .WillOnce(
            Invoke([&fourth_layer_coefs](const Field& /*field*/, const FieldElementSpan& span) {
              ASSERT_RELEASE(
                  span.Size() == last_layer_degree_bound,
                  "span size is not equal to last layer degree bound.");
              for (size_t i = 0; i < last_layer_degree_bound; ++i) {
                span.Set(i, fourth_layer_coefs[i]);
              }
            }));

    // Query phase expectations.
    // Proof of work.
    EXPECT_CALL(verifier_channel, ApplyProofOfWork(proof_of_work_bits));

    // The verifier will send two query locations - 0 and 6.
    EXPECT_CALL(verifier_channel, GetAndSendRandomNumberImpl(256))
        .WillOnce(Return(0))
        .WillOnce(Return(6));

    // First Layer.
    // The received cosets for queries 0 and 6, are (0, 1, 2, 3) and (24, 25, 26, 27) respectively.
    // Upon calling the first_layer_queries_callback, the witness at these 8 indices will be
    // given.
    std::vector<FieldElementT> witness_elements = {
        {(witness_data[0]), (witness_data[1]), (witness_data[2]), (witness_data[3]),
         (witness_data[24]), (witness_data[25]), (witness_data[26]), (witness_data[27])}};

    EXPECT_CALL(first_layer_queries_callback, Call(ElementsAre(0, 1, 2, 3, 24, 25, 26, 27)))
        .WillOnce(Invoke([witness_elements](const std::vector<uint64_t>& /*queries*/) {
          return FieldElementVector::CopyFrom(witness_elements);
        }));

    // Second Layer.
    // Fake response from prover on the data queries.
    std::set<RowCol> data_query_indices{RowCol(0, 1), RowCol(0, 2), RowCol(0, 3),
                                        RowCol(0, 4), RowCol(0, 5), RowCol(0, 7)};
    std::set<RowCol> integrity_query_indices{RowCol(0, 0), RowCol(0, 6)};
    std::map<RowCol, FieldElement> data_queries_response;

    for (const RowCol& query : data_query_indices) {
      data_queries_response.insert({query, second_layer.At(query.GetCol())});
    }

    EXPECT_CALL(
        table_verifier_factory[0], Query(
                                       UnorderedElementsAreArray(data_query_indices),
                                       UnorderedElementsAreArray(integrity_query_indices)))
        .WillOnce(Return(data_queries_response));

    // Add integrity queries to the map, and send this data to verification.
    data_queries_response.insert({RowCol(0, 0), second_layer.At(0)});
    data_queries_response.insert({RowCol(0, 6), second_layer.At(6)});

    EXPECT_CALL(
        table_verifier_factory[0],
        VerifyDecommitment(UnorderedElementsAreArray(data_queries_response)))
        .WillOnce(Return(true));

    // Third Layer.
    data_query_indices = {RowCol(0, 1)};
    integrity_query_indices = {RowCol(0, 0)};
    data_queries_response = {{RowCol(0, 1), third_layer.At(1)}};
    EXPECT_CALL(
        table_verifier_factory[1], Query(
                                       UnorderedElementsAreArray(data_query_indices),
                                       UnorderedElementsAreArray(integrity_query_indices)))
        .WillOnce(Return(data_queries_response));

    // Add integrity queries to the map, and send this data to verification.
    data_queries_response.insert({RowCol(0, 0), third_layer.At(0)});
    EXPECT_CALL(
        table_verifier_factory[1],
        VerifyDecommitment(UnorderedElementsAreArray(data_queries_response)))
        .WillOnce(Return(true));
  }
  TableVerifierFactory table_verifier_factory_as_factory = table_verifier_factory.AsFactory();
  FriVerifier::FirstLayerCallback first_layer_queries_callback_as_function =
      first_layer_queries_callback.AsStdFunction();
  FriVerifier fri_verifier(
      UseOwned(&verifier_channel), UseOwned(&table_verifier_factory_as_factory), UseOwned(&params),
      UseOwned(&first_layer_queries_callback_as_function));
  fri_verifier.VerifyFri();
  LOG(INFO) << verifier_channel;
}
#endif

template <typename FieldElementT>
struct FriTestDefinitions0 {
  using FieldElementT_ = FieldElementT;
  static constexpr std::array<size_t, 4> kFriStepList = {0, 2, 1, 4};
};

template <typename FieldElementT>
struct FriTestDefinitions1 {
  using FieldElementT_ = FieldElementT;
  static constexpr std::array<size_t, 3> kFriStepList = {2, 1, 4};
};

template <typename FieldElementT>
struct FriTestDefinitions2 {
  using FieldElementT_ = FieldElementT;
  static constexpr std::array<size_t, 3> kFriStepList = {0, 4, 3};
};

using EndToEndTestedFieldTypes = ::testing::Types<
    FriTestDefinitions0<TestFieldElement>, FriTestDefinitions1<TestFieldElement>,
    FriTestDefinitions2<TestFieldElement>>;

template <typename TestDefinitionsT>
class FriEndToEndTest : public testing::Test {
 public:
  using FieldElementT = typename TestDefinitionsT::FieldElementT_;
  FriEndToEndTest()
      : bases(MakeFftBases(log2_eval_domain, FieldElementT::RandomElement(&prng))),
        params{fri_step_list,    last_layer_degree_bound,        n_queries,
               UseOwned(&bases), Field::Create<FieldElementT>(), proof_of_work_bits},
        fri_prover_config{
            /*max_non_chunked_layer_size=*/FriProverConfig::kDefaultMaxNonChunkedLayerSize,
            /*n_chunks_between_layers=*/FriProverConfig::kDefaultNumberOfChunksBetweenLayers,
            /*log_n_max_in_memory_fri_layer_elements=*/FriProverConfig::kAllInMemoryLayers} {}

  void InitWitness(size_t degree_bound, size_t prefix_size) {
    TestPolynomial<FieldElementT> test_layer(&prng, degree_bound);

    eval_domain_data = test_layer.GetData(bases[0]);
    ASSERT_LE(prefix_size, eval_domain_data.size());
    std::vector<FieldElementT> prefix_data(
        eval_domain_data.begin(), eval_domain_data.begin() + prefix_size);
    witness = FieldElementVector::Make<FieldElementT>(std::move(prefix_data));
  }

  std::vector<std::byte> GenerateProof() { return GenerateProofWithAnnotations().first; }

  std::pair<std::vector<std::byte>, std::vector<std::string>> GenerateProofWithAnnotations() {
    NoninteractiveProverChannel p_channel(channel_prng.Clone());

    const size_t n_out_of_memory_merkle_layers = 0;
    TableProverFactory table_prover_factory = [&](size_t n_segments, uint64_t n_rows_per_segment,
                                                  size_t n_columns) {
      auto packaging_commitment_scheme = MakeCommitmentSchemeProver<Blake2s256>(
          FieldElementT::SizeInBytes() * n_columns, n_rows_per_segment, n_segments, &p_channel,
          /*n_verifier_friendly_commitment_layers=*/0, CommitmentHashes(Blake2s256::HashName()),
          n_out_of_memory_merkle_layers);

      return std::make_unique<TableProverImpl>(
          n_columns, UseMovedValue(std::move(packaging_commitment_scheme)), &p_channel);
    };

    FriProver::FirstLayerCallback first_layer_queries_callback =
        [](const std::vector<uint64_t>& /* queries */) {};

    fri_prover_config.log_n_max_in_memory_fri_layer_elements =
        CalcLogNInMemoryFriElements(witness->Size(), params.fri_step_list);

    // Create a FRI proof.
    FriProver fri_prover(
        UseOwned(&p_channel), UseOwned(&table_prover_factory), UseOwned(&params),
        std::move(*witness), UseOwned(&first_layer_queries_callback), UseOwned(&fri_prover_config));
    fri_prover.ProveFri();
    return {p_channel.GetProof(), p_channel.GetAnnotations()};
  }

  bool VerifyProof(
      gsl::span<const std::byte> proof,
      const std::optional<std::vector<std::string>>& prover_annotations = std::nullopt) {
    NoninteractiveVerifierChannel v_channel(channel_prng.Clone(), proof);
    if (prover_annotations) {
      v_channel.SetExpectedAnnotations(*prover_annotations);
    }

    TableVerifierFactory table_verifier_factory = [&](const Field& field, uint64_t n_rows,
                                                      size_t n_columns) {
      auto packaging_commitment_scheme = MakeCommitmentSchemeVerifier<Blake2s256>(
          field.ElementSizeInBytes() * n_columns, n_rows, &v_channel,
          /*n_verifier_friendly_commitment_layers=*/0, CommitmentHashes(Blake2s256::HashName()));

      return std::make_unique<TableVerifierImpl>(
          field, n_columns, UseMovedValue(std::move(packaging_commitment_scheme)), &v_channel);
    };

    FriVerifier::FirstLayerCallback first_layer_queries_callback =
        [&](const std::vector<uint64_t>& queries) {
          std::vector<FieldElementT> res;
          res.reserve(queries.size());
          for (uint64_t query : queries) {
            res.emplace_back(eval_domain_data.at(query));
          }
          return FieldElementVector::Make(std::move(res));
        };

    return FalseOnError([&]() {
      FriVerifier fri_verifier(
          UseOwned(&v_channel), UseOwned(&table_verifier_factory), UseOwned(&params),
          UseOwned(&first_layer_queries_callback));
      fri_verifier.VerifyFri();
    });
  }

  const size_t log2_eval_domain = 10;
  const size_t eval_domain_size = Pow2(log2_eval_domain);
  const size_t n_layers = 7;
  const std::vector<size_t> fri_step_list{
      TestDefinitionsT::kFriStepList.begin(), TestDefinitionsT::kFriStepList.end()};
  const uint64_t last_layer_degree_bound = 3;
  const uint64_t degree_bound = Pow2(n_layers) * last_layer_degree_bound;
  const size_t n_queries = 4;
  const size_t proof_of_work_bits = 15;
  Prng prng;
  const Prng channel_prng;
  FftBasesDefaultImpl<FieldElementT> bases;
  FriParameters params;
  FriProverConfig fri_prover_config;
  std::optional<FieldElementVector> witness;
  std::vector<FieldElementT> eval_domain_data;
};

TYPED_TEST_CASE(FriEndToEndTest, EndToEndTestedFieldTypes);

TYPED_TEST(FriEndToEndTest, Correctness) {
  this->InitWitness(this->degree_bound, this->eval_domain_size);

  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();
  // VerifyProof should return true with or without checking the annotations.
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first));
}

TYPED_TEST(FriEndToEndTest, CorrectnessSmallerDomain) {
  const uint64_t minimal_domain_size =
      Pow2(Log2Ceil(this->last_layer_degree_bound) + this->n_layers);
  ASSERT_LT(minimal_domain_size, this->eval_domain_size);
  this->InitWitness(this->degree_bound, minimal_domain_size);

  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first));
}

TYPED_TEST(FriEndToEndTest, NegativeTestTooSmallDomain) {
  this->InitWitness(this->degree_bound, Pow2(this->n_layers - 1));

  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();
  EXPECT_FALSE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
  EXPECT_FALSE(this->VerifyProof(proof_annotations_pair.first));
}

TYPED_TEST(FriEndToEndTest, NegativeTestLargerDegree) {
  this->InitWitness(this->degree_bound + 1, this->eval_domain_size);

  EXPECT_ASSERT(this->GenerateProof(), HasSubstr("Last FRI layer"));
}

TYPED_TEST(FriEndToEndTest, ChangeByte) {
  this->InitWitness(this->degree_bound, this->eval_domain_size);

  std::vector<std::byte> proof = this->GenerateProof();
  EXPECT_TRUE(this->VerifyProof(proof));

  const auto byte_idx = this->prng.template UniformInt<size_t>(0, proof.size() - 1);
  proof[byte_idx] ^= std::byte(this->prng.template UniformInt<uint8_t>(1, 255));

  EXPECT_FALSE(this->VerifyProof(proof));
}

}  // namespace
}  // namespace starkware
