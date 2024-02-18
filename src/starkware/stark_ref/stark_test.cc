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

#include "starkware/stark/stark.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/components/permutation/permutation_dummy_air.h"
#include "starkware/air/components/permutation/permutation_trace_context.h"
#include "starkware/air/degree_three_example/degree_three_example_air.h"
#include "starkware/air/degree_three_example/degree_three_example_trace_context.h"
#include "starkware/air/fibonacci/fibonacci_air.h"
#include "starkware/air/fibonacci/fibonacci_trace_context.h"
#include "starkware/air/test_utils.h"
#include "starkware/air/trace_context.h"
#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/crypt_tools/blake2s.h"
#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/proof_system/proof_system.h"
#include "starkware/stark/test_utils.h"
#include "starkware/stark/utils.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {
namespace {

using testing::HasSubstr;
using FibAirT = FibonacciAir<TestFieldElement>;
using DegThreeAirT = DegreeThreeExampleAir<ExtensionFieldElement<TestFieldElement>>;
using PermutationAirT = PermutationDummyAir<TestFieldElement, 0>;
using FibTraceContextT = FibonacciTraceContext<TestFieldElement>;
using DegThreeTraceContextT =
    DegreeThreeExampleTraceContext<ExtensionFieldElement<TestFieldElement>>;

template <typename HashT, typename FieldElementT>
std::unique_ptr<TableProver> MakeTableProver(
    uint64_t n_segments, uint64_t n_rows_per_segment, size_t n_columns, ProverChannel* channel,
    size_t n_tasks_per_segment, size_t n_layer, size_t n_verifier_friendly_commitment_layers) {
  return GetTableProverFactory<HashT>(
      channel, FieldElementT::SizeInBytes(), n_tasks_per_segment, n_layer,
      n_verifier_friendly_commitment_layers,
      CommitmentHashes(HashT::HashName()))(n_segments, n_rows_per_segment, n_columns);
}

std::vector<size_t> DrawFriStepsList(const size_t log_degree_bound, Prng* prng) {
  if (prng == nullptr) {
    return {3, 3, 1, 1};
  }
  std::vector<size_t> res;
  for (size_t curr_sum = 0; curr_sum < log_degree_bound;) {
    res.push_back(prng->UniformInt<size_t>(1, log_degree_bound - curr_sum));
    curr_sum += res.back();
  }
  ASSERT_RELEASE(
      Sum(res) == log_degree_bound, "The sum of all steps must be the log of the degree bound.");
  return res;
}

template <typename FieldElementT>
StarkParameters GenerateParameters(
    std::unique_ptr<Air> air, Prng* prng, size_t proof_of_work_bits = 15,
    bool use_extension_field = false) {
  // Field used.
  const Field field(Field::Create<FieldElementT>());
  const size_t trace_length = air->TraceLength();
  uint64_t degree_bound = air->GetCompositionPolynomialDegreeBound() / trace_length;

  const auto log_n_cosets =
      (prng == nullptr ? 4 : prng->UniformInt<size_t>(Log2Ceil(degree_bound), 6));
  const size_t log_coset_size = Log2Ceil(trace_length);
  const size_t n_cosets = Pow2(log_n_cosets);
  const size_t log_evaluation_domain_size = log_coset_size + log_n_cosets;

  // Fri steps - hard-coded pattern for this tests suit.
  const size_t log_degree_bound = log_coset_size;
  const std::vector<size_t> fri_step_list = DrawFriStepsList(log_degree_bound, prng);

  // Bases used by FRI.
  const FieldElementT offset =
      (prng == nullptr ? FieldElementT::One() : FieldElementT::RandomElement(prng));
  FftBasesImpl<FftMultiplicativeGroup<FieldElementT>> bases =
      MakeFftBases(log_evaluation_domain_size, offset);

  // FRI parameters.
  FriParameters fri_params{
      /*fri_step_list=*/fri_step_list,
      /*last_layer_degree_bound=*/1,
      /*n_queries=*/30,
      /*fft_bases=*/UseMovedValue(std::move(bases)),
      /*field=*/field,
      /*proof_of_work_bits=*/proof_of_work_bits};

  return StarkParameters(
      field, use_extension_field, n_cosets, trace_length, TakeOwnershipFrom(std::move(air)),
      UseMovedValue(std::move(fri_params)));
}

template <typename HashT, typename FieldElementT>
class StarkTest : public ::testing::Test {
 public:
  explicit StarkTest(bool use_random_values = true)
      : prng(use_random_values ? Prng() : Prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>())),
        channel_prng(use_random_values ? Prng() : Prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>())),
        stark_config(StarkProverConfig::InRam()),
        prover_channel(NoninteractiveProverChannel(channel_prng.Clone())) {
    table_prover_factory = [this](
                               uint64_t n_segments, uint64_t n_rows_per_segment, size_t n_columns) {
      const size_t n_verifier_friendly_commitment_layers = 0;
      return MakeTableProver<HashT, FieldElementT>(
          n_segments, n_rows_per_segment, n_columns, &prover_channel,
          stark_config.table_prover_n_tasks_per_segment, stark_config.n_out_of_memory_merkle_layers,
          n_verifier_friendly_commitment_layers);
    };
  }

  bool VerifyProof(
      const std::vector<std::byte>& proof,
      const std::optional<std::vector<std::string>>& prover_annotations = std::nullopt,
      bool skip_assert_for_extension_field_test = false) {
    NoninteractiveVerifierChannel verifier_channel(channel_prng.Clone(), proof);
    if (prover_annotations) {
      verifier_channel.SetExpectedAnnotations(*prover_annotations);
    }

    const size_t n_verifier_friendly_commitment_layers = 0;
    TableVerifierFactory table_verifier_factory =
        [&verifier_channel](const Field& field, uint64_t n_rows, uint64_t n_columns) {
          return MakeTableVerifier<HashT, FieldElementT>(
              field, n_rows, n_columns, &verifier_channel, n_verifier_friendly_commitment_layers);
        };
    StarkVerifier stark_verifier(
        UseOwned(&verifier_channel), UseOwned(&table_verifier_factory),
        UseOwned(&GetStarkParams()));

    if (skip_assert_for_extension_field_test) {
      stark_verifier.SetSkipAssertForExtensionFieldTest();
      stark_verifier.VerifyStark();
      return true;
    }
    return FalseOnError([&]() { stark_verifier.VerifyStark(); });
  }

  virtual const StarkParameters& GetStarkParams() = 0;

  const uint64_t trace_length = 256;
  Prng prng;
  const Prng channel_prng;
  StarkProverConfig stark_config;

  NoninteractiveProverChannel prover_channel;
  TableProverFactory table_prover_factory;
};

template <typename HashT>
class DegreeThreeStarkTest : public StarkTest<HashT, ExtensionFieldElement<TestFieldElement>> {
 public:
  explicit DegreeThreeStarkTest(
      bool use_extension_field_secret = false, bool use_random_values = true)
      : StarkTest<HashT, ExtensionFieldElement<TestFieldElement>>(use_random_values),
        secret(
            use_extension_field_secret
                ? ExtensionFieldElement<TestFieldElement>::RandomElement(&(this->prng))
                : ExtensionFieldElement<TestFieldElement>::RandomBaseElement(&(this->prng))),
        claimed_res(DegThreeAirT::PublicInputFromPrivateInput(secret, res_claim_index)),
        stark_params(GenerateParameters<ExtensionFieldElement<TestFieldElement>>(
            std::make_unique<DegThreeAirT>(this->trace_length, res_claim_index, claimed_res),
            use_random_values ? &(this->prng) : nullptr, 15, true)) {}

  const StarkParameters& GetStarkParams() override { return stark_params; }

  std::pair<std::vector<std::byte>, std::vector<std::string>> GenerateProofWithAnnotations() {
    auto air = DegThreeAirT(this->trace_length, res_claim_index, claimed_res);
    StarkProver stark_prover(
        UseOwned(&(this->prover_channel)), UseOwned(&(this->table_prover_factory)),
        UseOwned(&GetStarkParams()), UseOwned(&(this->stark_config)));

    stark_prover.ProveStark(
        std::make_unique<DegThreeTraceContextT>(UseOwned(&air), secret, res_claim_index));
    return {this->prover_channel.GetProof(), this->prover_channel.GetAnnotations()};
  }

  const uint64_t res_claim_index = 251;
  const ExtensionFieldElement<TestFieldElement> secret;
  const ExtensionFieldElement<TestFieldElement> claimed_res;
  StarkParameters stark_params;
};

template <typename HashT>
class FibonacciStarkTest : public StarkTest<HashT, TestFieldElement> {
 public:
  explicit FibonacciStarkTest(bool use_random_values = true)
      : StarkTest<HashT, TestFieldElement>(use_random_values),
        secret(TestFieldElement::RandomElement(&(this->prng))),
        claimed_fib(FibAirT::PublicInputFromPrivateInput(secret, fibonacci_claim_index)),
        stark_params(GenerateParameters<TestFieldElement>(
            std::make_unique<FibAirT>(this->trace_length, fibonacci_claim_index, claimed_fib),
            use_random_values ? &(this->prng) : nullptr)) {}

  const StarkParameters& GetStarkParams() override { return stark_params; }

  std::vector<std::byte> GenerateProof() { return GenerateProofWithAnnotations().first; }

  std::pair<std::vector<std::byte>, std::vector<std::string>> GenerateProofWithAnnotations() {
    auto air = FibAirT(this->trace_length, fibonacci_claim_index, claimed_fib);
    StarkProver stark_prover(
        UseOwned(&(this->prover_channel)), UseOwned(&(this->table_prover_factory)),
        UseOwned(&GetStarkParams()), UseOwned(&(this->stark_config)));

    stark_prover.ProveStark(
        std::make_unique<FibTraceContextT>(UseOwned(&air), secret, fibonacci_claim_index));
    return {this->prover_channel.GetProof(), this->prover_channel.GetAnnotations()};
  }

  void SetAir(FibAirT air) { stark_params.air = UseMovedValue<FibAirT>(std::move(air)); }

  const uint64_t fibonacci_claim_index = 251;
  const TestFieldElement secret;
  const TestFieldElement claimed_fib;
  StarkParameters stark_params;
};

template <typename HashT>
class PermutationStarkTest : public StarkTest<HashT, TestFieldElement> {
 public:
  PermutationStarkTest()
      : StarkTest<HashT, TestFieldElement>(),
        stark_params(GenerateParameters<TestFieldElement>(
            std::make_unique<PermutationAirT>(this->trace_length, &(this->prng)), &(this->prng))) {}

  const StarkParameters& GetStarkParams() override { return stark_params; }

  std::pair<std::vector<std::byte>, std::vector<std::string>>
  GeneratePermutationProofWithAnnotations() {
    PermutationAirT air(this->trace_length, &(this->prng));
    StarkProver stark_prover(
        UseOwned(&(this->prover_channel)), UseOwned(&(this->table_prover_factory)),
        UseOwned(&(this->stark_params)), UseOwned(&(this->stark_config)));

    stark_prover.ProveStark(
        std::make_unique<PermutationTraceContext<TestFieldElement>>(UseOwned(&air)));

    return {this->prover_channel.GetProof(), this->prover_channel.GetAnnotations()};
  }

  StarkParameters stark_params;
};

using TestedChannelTypes = ::testing::Types<Keccak256>;

TYPED_TEST_CASE(FibonacciStarkTest, TestedChannelTypes);
TYPED_TEST_CASE(DegreeThreeStarkTest, TestedChannelTypes);
TYPED_TEST_CASE(PermutationStarkTest, TestedChannelTypes);

TYPED_TEST(FibonacciStarkTest, Correctness) {
  // Generate proof.
  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();

  // Verify proof.
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
}

TYPED_TEST(DegreeThreeStarkTest, Correctness) {
  // Generate proof.
  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();

  // Verify proof.
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
}

// Derive from DegreeThreeStarkTest to call the constructor with use_extension_field_secret=true.
template <typename HashT>
class DegreeThreeStarkTestExtensionFieldSecret : public DegreeThreeStarkTest<HashT> {
 public:
  DegreeThreeStarkTestExtensionFieldSecret()
      : DegreeThreeStarkTest<HashT>(/*use_extension_field_secret=*/true) {}
};

TYPED_TEST_CASE(DegreeThreeStarkTestExtensionFieldSecret, TestedChannelTypes);

TYPED_TEST(DegreeThreeStarkTestExtensionFieldSecret, StarkWithFriProverBadTrace) {
  // Generate proof.
  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();

  // Verify proof, verifier should reject because the secret is not from the base field.
  EXPECT_FALSE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
}

TYPED_TEST(DegreeThreeStarkTestExtensionFieldSecret, StarkWithFriProverBadTraceSkipAssert) {
  // Generate proof.
  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();

  // Verify proof, verifier should reject because the secret is not from the base field.
  const auto expected_error_msg = this->stark_params.fri_params->fri_step_list.size() < 2
                                      ? "is not consistent with the coefficients"
                                      : "Layer 0 failed decommitment";
  EXPECT_ASSERT(
      this->VerifyProof(
          proof_annotations_pair.first, proof_annotations_pair.second,
          /*skip_assert_for_extension_field_test=*/true),
      testing::HasSubstr(expected_error_msg));
}

TYPED_TEST(PermutationStarkTest, PermutationAirCorrectness) {
  // Generate proof.
  const auto proof_annotations_pair = this->GeneratePermutationProofWithAnnotations();

  // Verify proof.
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
}

TYPED_TEST(FibonacciStarkTest, CorrectnessDontStoreFullLde) {
  this->stark_config.cached_lde_config = CachedLdeManager::Config{
      /*store_full_lde=*/false,
      /*use_fft_for_eval=*/false};

  // Generate proof.
  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();

  // Verify proof.
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
}

TYPED_TEST(FibonacciStarkTest, CorrectnessRecomputeLdeWithFft) {
  this->stark_config.cached_lde_config = CachedLdeManager::Config{
      /*store_full_lde=*/false,
      /*use_fft_for_eval=*/true};

  // Generate proof.
  const auto proof_annotations_pair = this->GenerateProofWithAnnotations();

  // Verify proof.
  EXPECT_TRUE(this->VerifyProof(proof_annotations_pair.first, proof_annotations_pair.second));
}

// Derive from StarkTest to call the constructor with use_random_values=false.
template <typename HashT>
class StarkTestConstSeed : public FibonacciStarkTest<HashT> {
 public:
  StarkTestConstSeed() : FibonacciStarkTest<HashT>(false) {}
};

TYPED_TEST_CASE(StarkTestConstSeed, TestedChannelTypes);

TYPED_TEST(StarkTestConstSeed, Regression_Statistical) {
  StarkProver stark_prover(
      UseOwned(&this->prover_channel), UseOwned(&this->table_prover_factory),
      UseOwned(&(this->GetStarkParams())), UseOwned(&this->stark_config));

  AnnotationScope scope(&this->prover_channel, "Fibonacci");

  auto air =
      std::make_unique<FibAirT>(this->trace_length, this->fibonacci_claim_index, this->claimed_fib);
  stark_prover.ProveStark(std::make_unique<FibTraceContextT>(
      TakeOwnershipFrom(std::move(air)), this->secret, this->fibonacci_claim_index));

  std::ofstream ofs("/tmp/stark_test.proof.txt");
  ofs << this->prover_channel;

  ChannelStatistics stats = this->prover_channel.GetStatistics();

  EXPECT_EQ(stats.commitment_count, 5U);

  const size_t hash_id = typeid(TypeParam).hash_code();
  const size_t blake_id = typeid(Blake2s256).hash_code();
  const size_t keccak_id = typeid(Keccak256).hash_code();

  // The values below where measured by running this (deterministic) test. This test allows us to
  // keep track on changes to these values.

  const std::map<size_t, size_t> expected_field_element_count = {
      std::make_pair(blake_id, 920), std::make_pair(keccak_id, 927)};
  EXPECT_EQ(expected_field_element_count.at(hash_id), stats.field_element_count);
  const std::map<size_t, size_t> expected_hash_count = {
      std::make_pair(blake_id, 189), std::make_pair(keccak_id, 192)};
  EXPECT_EQ(expected_hash_count.at(hash_id), stats.hash_count);
  const std::map<size_t, size_t> expected_data_count = {
      std::make_pair(blake_id, 357), std::make_pair(keccak_id, 254)};
  EXPECT_EQ(expected_data_count.at(hash_id), stats.data_count);
  const std::map<size_t, size_t> expected_byte_count = {
      std::make_pair(blake_id, 11160), std::make_pair(keccak_id, 11372)};

  EXPECT_EQ(expected_byte_count.at(hash_id), stats.byte_count);
}

TYPED_TEST(FibonacciStarkTest, StarkWithFriProverBadTrace) {
  // Create a trace and corrupt it at exactly one location by incrementing it by one.
  auto bad_column = this->prng.template UniformInt<size_t>(0, 1);
  auto bad_index = this->prng.template UniformInt<size_t>(0, this->fibonacci_claim_index - 1);
  Trace trace = FibAirT::GetTrace(this->secret, this->trace_length, this->fibonacci_claim_index);
  trace.SetTraceElementForTesting(
      bad_column, bad_index,
      trace.GetColumn(bad_column).At(bad_index) + FieldElement(TestFieldElement::One()));

  StarkProver stark_prover(
      UseOwned(&this->prover_channel), UseOwned(&this->table_prover_factory),
      UseOwned(&(this->GetStarkParams())), UseOwned(&this->stark_config));

  // Initialize trace context with the corrupted trace and send it to ProveStark.
  stark_prover.ProveStark(std::make_unique<TestTraceContext>(std::move(trace)));
  EXPECT_FALSE(this->VerifyProof(this->prover_channel.GetProof()));
}

TYPED_TEST(FibonacciStarkTest, StarkWithFriProverPublicInputInconsistentWithWitness) {
  auto air =
      std::make_unique<FibAirT>(this->trace_length, this->fibonacci_claim_index, this->claimed_fib);

  // Bad claimed Fibonacci element.
  this->stark_params = GenerateParameters<TestFieldElement>(
      std::make_unique<FibAirT>(
          this->trace_length, this->fibonacci_claim_index,
          this->claimed_fib + TestFieldElement::One()),
      &this->prng);

  StarkProver stark_prover(
      UseOwned(&this->prover_channel), UseOwned(&this->table_prover_factory),
      UseOwned(&this->stark_params), UseOwned(&this->stark_config));

  stark_prover.ProveStark(
      std::make_unique<FibTraceContextT>(UseOwned(air), this->secret, this->fibonacci_claim_index));
  EXPECT_FALSE(this->VerifyProof(this->prover_channel.GetProof()));
}

TYPED_TEST(FibonacciStarkTest, StarkWithFriProverWrongNumberOfLayers) {
  this->GetStarkParams().fri_params->fri_step_list = std::vector<size_t>(5, 1);
  EXPECT_ASSERT(this->GenerateProof(), HasSubstr("Fri parameters do not match"));
}

TYPED_TEST(FibonacciStarkTest, ChangeProofContent) {
  // Generate proof.
  const std::vector<std::byte> proof = this->GenerateProof();

  // Modify proof.
  std::vector<std::byte> modified_proof = proof;
  modified_proof[this->prng.template UniformInt<size_t>(0, modified_proof.size() - 1)] ^=
      std::byte(1);

  // Verify proof.
  EXPECT_FALSE(this->VerifyProof(modified_proof));
}

TYPED_TEST(FibonacciStarkTest, ShortenProof) {
  // Generate proof.
  const std::vector<std::byte> proof = this->GenerateProof();

  // Modify proof.
  std::vector<std::byte> modified_proof = proof;
  modified_proof.pop_back();

  // Verify proof.
  EXPECT_FALSE(this->VerifyProof(modified_proof));
}

TYPED_TEST(FibonacciStarkTest, ChangeAir) {
  // Generate proof.
  const std::vector<std::byte> proof = this->GenerateProof();

  // Modify AIR.
  this->SetAir(FibAirT(
      this->trace_length, this->fibonacci_claim_index,
      this->claimed_fib + TestFieldElement::One()));

  // Verify proof.
  EXPECT_FALSE(this->VerifyProof(proof));
}

}  // namespace
}  // namespace starkware
