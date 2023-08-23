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

#include "starkware/commitment_scheme/packaging_commitment_scheme.h"

#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel_mock.h"

#include "starkware/commitment_scheme/commitment_scheme_mock.h"
#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::HasSubstr;
using testing::StrictMock;
using testing::Test;
using HashT = Keccak256;

// Prevents from webasm running mock tests.
#ifndef __EMSCRIPTEN__

// --------- NumOfPackages tests ----------

size_t GetNumOfPackages(
    const size_t size_of_element, const uint64_t n_elements_in_segment, const size_t n_segments) {
  // Prover.
  StrictMock<ProverChannelMock> prover_channel;
  size_t prover_factory_input;
  const PackagingCommitmentSchemeProver<HashT> packaging_prover(
      size_of_element, n_elements_in_segment, n_segments, &prover_channel,
      [&prover_factory_input](
          size_t n_elements_inner_layer) -> std::unique_ptr<CommitmentSchemeProver> {
        prover_factory_input = n_elements_inner_layer;
        return std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
      });
  // Check that packaging_prover calls factory with correct param.
  EXPECT_EQ(prover_factory_input, packaging_prover.GetNumOfPackages());

  // Verifier.
  StrictMock<VerifierChannelMock> verifier_channel;
  size_t verifier_factory_input;
  const PackagingCommitmentSchemeVerifier<HashT> packaging_verifier(
      size_of_element, n_elements_in_segment * n_segments, &verifier_channel,
      [&verifier_factory_input](
          size_t n_elements_inner_layer) -> std::unique_ptr<CommitmentSchemeVerifier> {
        verifier_factory_input = n_elements_inner_layer;
        return std::make_unique<StrictMock<CommitmentSchemeVerifierMock>>();
      },
      /*is_merkle_layer=*/false);
  // Check that packaging_verifier calls factory with correct param.
  EXPECT_EQ(verifier_factory_input, packaging_verifier.GetNumOfPackages());

  EXPECT_EQ(packaging_prover.GetNumOfPackages(), packaging_verifier.GetNumOfPackages());
  return packaging_prover.GetNumOfPackages();
}

void TestGetNumOfPackages(
    const size_t size_of_element, const uint64_t n_elements_in_segment, const size_t n_segments) {
  const size_t n_packages = GetNumOfPackages(size_of_element, n_elements_in_segment, n_segments);
  const size_t n_elements = n_elements_in_segment * n_segments;
  const size_t n_elements_in_package = n_elements / n_packages;
  EXPECT_TRUE(IsPowerOfTwo(n_packages));
  EXPECT_TRUE(IsPowerOfTwo(n_elements_in_package));
  // Checks that n_packages is not too big (equivalently - n_elements_in_package is big enough).
  EXPECT_TRUE(n_elements_in_package * size_of_element >= 2 * HashT::kDigestNumBytes);
  // Checks that n_packages is not too small (equivalently - n_elements_in_package is not too big).
  EXPECT_TRUE(((n_elements_in_package / 2) * size_of_element) < 2 * HashT::kDigestNumBytes);
}

TEST(PackagingCommitmentSchemeProver, GetNumOfPackages) {
  TestGetNumOfPackages(7, 16, 16);
  TestGetNumOfPackages(9, 16, 16);
  TestGetNumOfPackages(32, 16, 8);
  TestGetNumOfPackages(1, 128, 32);
  TestGetNumOfPackages(2 * HashT::kDigestNumBytes, 1, 8);
  // Size of element is zero.
  EXPECT_ASSERT(GetNumOfPackages(0, 32, 20), HasSubstr("least of length"));
  // Num of elements is not power of 2.
  EXPECT_ASSERT(GetNumOfPackages(15, 16, 10), HasSubstr("power of 2"));
  // Size of element greater than size of package.
  TestGetNumOfPackages(100, 1, 64);
  TestGetNumOfPackages(4 * HashT::kDigestNumBytes, 1, 16);
}

// --------- Prover tests ----------

TEST(PackagingCommitmentSchemeProver, NumSegments) {
  Prng prng;
  const size_t n_segments = Pow2(prng.UniformInt<size_t>(1, 10));
  StrictMock<ProverChannelMock> prover_channel;
  const PackagingCommitmentSchemeProver<HashT> packaging_prover(
      prng.UniformInt<size_t>(1, sizeof(HashT) * 5), Pow2(prng.UniformInt<size_t>(1, 10)),
      n_segments, &prover_channel,
      [](size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeProver> {
        return std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
      });
  EXPECT_EQ(packaging_prover.NumSegments(), n_segments);
}

/*
  Given parameters for PackagingCommitmentSchemeProver, creates PackagingCommitmentSchemeProver
  instance and tests the functions AddSegmentForCommitment and Commit.
*/
void TestAddSegmentForCommitmentAndCommit(
    const size_t size_of_element, const uint64_t n_elements_in_segment, const size_t n_segments) {
  Prng prng;
  StrictMock<ProverChannelMock> prover_channel;
  const size_t segment_index = prng.UniformInt<size_t>(0, n_segments - 1);
  const size_t size_of_data_segment = size_of_element * n_elements_in_segment;
  const std::vector<std::byte> data = prng.RandomByteVector(size_of_data_segment);
  const PackerHasher<HashT> packer(size_of_element, n_segments * n_elements_in_segment);
  std::vector<std::byte> packed = packer.PackAndHash(data, false);
  auto inner_commitment_scheme = std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
  EXPECT_CALL(
      *inner_commitment_scheme,
      AddSegmentForCommitment(gsl::span<const std::byte>(packed), segment_index));
  EXPECT_CALL(*inner_commitment_scheme, Commit());
  PackagingCommitmentSchemeProver<HashT> packaging_prover(
      size_of_element, n_elements_in_segment, n_segments, &prover_channel,
      [&inner_commitment_scheme](
          size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeProver> {
        return std::move(inner_commitment_scheme);
      });
  packaging_prover.AddSegmentForCommitment(data, segment_index);
  packaging_prover.Commit();
}

TEST(PackagingCommitmentSchemeProver, AddSegmentForCommitmentAndCommit) {
  TestAddSegmentForCommitmentAndCommit(2 * HashT::kDigestNumBytes, 8, 16);
  TestAddSegmentForCommitmentAndCommit(1, 128, 16);
  TestAddSegmentForCommitmentAndCommit(2 * HashT::kDigestNumBytes, 8, 16);
  TestAddSegmentForCommitmentAndCommit(1, 128, 16);
  TestAddSegmentForCommitmentAndCommit(11, 32, 4);
  TestAddSegmentForCommitmentAndCommit(33, 2, 1);
  TestAddSegmentForCommitmentAndCommit(HashT::kDigestNumBytes, 2, 64);
  TestAddSegmentForCommitmentAndCommit(2 * HashT::kDigestNumBytes + 15, 1, 32);
  TestAddSegmentForCommitmentAndCommit(4 * HashT::kDigestNumBytes, 1, 8);
}

TEST(PackagingCommitmentSchemeProver, AddSegmentForCommitment_AssertsChecks) {
  const size_t size_of_element = 2 * HashT::kDigestNumBytes;
  const uint64_t n_elements_in_segment = 8;
  const size_t n_segments = 16;
  Prng prng;
  StrictMock<ProverChannelMock> prover_channel;
  PackagingCommitmentSchemeProver<HashT> packaging_prover(
      size_of_element, n_elements_in_segment, n_segments, &prover_channel,
      [](size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeProver> {
        return std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
      });

  // Data is too long.
  const size_t size_of_data = size_of_element * packaging_prover.SegmentLengthInElements();
  const std::vector<std::byte> data_too_long(size_of_data + 1);
  const size_t segment_index = prng.UniformInt<size_t>(0, n_segments - 1);
  EXPECT_ASSERT(
      packaging_prover.AddSegmentForCommitment(data_too_long, segment_index),
      HasSubstr("Segment size is"));

  // Segment index is out of range.
  const std::vector<std::byte> data(size_of_data);
  const size_t segment_index_out_of_bound = n_segments;
  EXPECT_ASSERT(
      packaging_prover.AddSegmentForCommitment(data, segment_index_out_of_bound),
      HasSubstr(
          "Segment index " + std::to_string(segment_index_out_of_bound) + " is out of range"));
}

TEST(PackagingCommitmentSchemeProver, StartDecommitmentPhaseAndDecommit) {
  Prng prng;
  StrictMock<ProverChannelMock> prover_channel;
  const size_t element_size = 11;
  const std::set<uint64_t> queries{1, 3, 30};
  auto inner_commitment_scheme = std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
  // Inner_commitment_scheme pack 2 elements in a package. Packaging_prover calls
  // StartDecommitmentPhase of inner_commitment_scheme with a set of packages indices of the
  // packages containing queries. There are 8 elements in each package created by packaging_prover,
  // hence, 1 and 3 are in package no. 0, 30 is in package no. 3.
  std::set<uint64_t> inner_layer_queries{0, 3};
  EXPECT_CALL(*inner_commitment_scheme, StartDecommitmentPhase(inner_layer_queries))
      .WillOnce(testing::Return(std::vector<uint64_t>{1, 2}));
  // Inner_commitment_scheme needs 16 elements, which are 2 packages (there are 8 element in a
  // packages of packaging_prover). Hence, the size of data sent to decommit is twice size of
  // hash in bytes.
  EXPECT_CALL(
      *inner_commitment_scheme,
      Decommit(testing::Property(&gsl::span<const std::byte>::size, 2 * HashT::kDigestNumBytes)));
  PackagingCommitmentSchemeProver<HashT> packaging_prover(
      element_size, 16, 8, &prover_channel,
      [&inner_commitment_scheme](
          size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeProver> {
        return std::move(inner_commitment_scheme);
      });

  // StartDecommitmentPhase phase.
  auto res = packaging_prover.StartDecommitmentPhase(queries);
  std::vector<uint64_t> expected_res = {0,  2,  4,  5,  6,  7,  24, 25, 26, 27, 28, 29, 31, 8, 9,
                                        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
  EXPECT_EQ(res, expected_res);

  // Decommit phase.
  const std::vector<std::byte> data = prng.RandomByteVector(expected_res.size() * element_size);
  // SendBytes is called 13 times - 6 times for missing queries of package no. 0 and 7 times for
  // missing queries of package no. 3.
  const gsl::span<const std::byte> span_data = gsl::make_span(data);
  for (size_t i = 0; i < 13; i++) {
    EXPECT_CALL(prover_channel, SendBytes(span_data.subspan(i * element_size, element_size)));
  }
  packaging_prover.Decommit(data);
}

// ---------- Verifier tests -----------

TEST(PackagingCommitmentSchemeVerifier, ReadCommitment) {
  Prng prng;
  StrictMock<VerifierChannelMock> verifier_channel;
  PackagingCommitmentSchemeVerifier<HashT> packaging_verifier(
      prng.UniformInt<size_t>(1, sizeof(HashT) * 5), Pow2(prng.UniformInt<size_t>(1, 10)),
      &verifier_channel,
      [](size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeVerifier> {
        auto inner_commitment_scheme_verifier =
            std::make_unique<StrictMock<CommitmentSchemeVerifierMock>>();
        EXPECT_CALL(*inner_commitment_scheme_verifier, ReadCommitment());
        return inner_commitment_scheme_verifier;
      },
      /*is_merkle_layer=*/false);
  packaging_verifier.ReadCommitment();
}

TEST(PackagingCommitmentSchemeVerifier, VerifyIntegritySmallElement) {
  Prng prng;
  StrictMock<VerifierChannelMock> verifier_channel;
  // Disable annotation to avoid running part of code in verifier_channel.ReceiveData() (note that
  // verifier_channel is a mock, there is no real interaction between prover and verifier).
  verifier_channel.DisableAnnotations();
  const size_t element_size_small = 17;
  const size_t n_elements = Pow2(prng.UniformInt<size_t>(4, 10));
  PackagingCommitmentSchemeVerifier<HashT> packaging_verifier(
      element_size_small, n_elements, &verifier_channel,
      [&packaging_verifier](
          size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeVerifier> {
        auto inner_commitment_scheme_verifier =
            std::make_unique<StrictMock<CommitmentSchemeVerifierMock>>();
        // NumOfElements() will be called as number of elements that is sent to extra annotations.
        // See implementation on PackagingCommitmentSchemeVerifier.VerifyIntegrity();
        EXPECT_CALL(*inner_commitment_scheme_verifier, NumOfElements())
            .Times(testing::AtLeast(1))
            // #elements of inner commitment scheme layer equals to #packages of current layer.
            .WillRepeatedly(testing::Return(packaging_verifier.GetNumOfPackages()));
        EXPECT_CALL(*inner_commitment_scheme_verifier, VerifyIntegrity(testing::_));

        return inner_commitment_scheme_verifier;
      },
      /*is_merkle_layer=*/false);
  // Create 3 elements to verify.
  std::map<uint64_t, std::vector<std::byte>> elements_to_verify;
  elements_to_verify[1] = prng.RandomByteVector(element_size_small);
  elements_to_verify[10] = prng.RandomByteVector(element_size_small);
  elements_to_verify[11] = prng.RandomByteVector(element_size_small);
  // Get required elements from channel. The verifier gets 5 elements from channel: 3 to calculate
  // element no. 1, and 2 to calculate elements no. 10 and 11.
  // Explanation: number of elements in package is 4 (package size is 2 * HashT::kDigestNumBytes and
  // element size is 17, for full explanation of number of elements in package calculation see
  // Packer_hasher.ComputeNumElementsInPackage). Hence, In order to verify element no. 1 the
  // verifier needs elements 0,2,3; and in order to verify elements no .10 and 11 the verifier
  // needs elements no .8, 9.
  EXPECT_CALL(verifier_channel, ReceiveBytes(element_size_small))
      .WillOnce(testing::Return(std::vector<std::byte>(element_size_small)))
      .WillOnce(testing::Return(std::vector<std::byte>(element_size_small)))
      .WillOnce(testing::Return(std::vector<std::byte>(element_size_small)))
      .WillOnce(testing::Return(std::vector<std::byte>(element_size_small)))
      .WillOnce(testing::Return(std::vector<std::byte>(element_size_small)));
  packaging_verifier.VerifyIntegrity(elements_to_verify);
}

TEST(PackagingCommitmentSchemeVerifier, VerifyIntegrityBigElement) {
  Prng prng;
  StrictMock<VerifierChannelMock> verifier_channel;
  // Disables annotation to avoid running part of code in verifier_channel.ReceiveData() (note that
  // verifier_channel is a mock, there is no real interaction between prover and verifier).
  verifier_channel.DisableAnnotations();
  // Size of element is greater than package size(which is 2 * HashT::kDigestNumBytes), hence each
  // package contains a single element. No extra elements are needed to calculate hash of a single
  // element to verify.
  const size_t element_size_big = prng.UniformInt<size_t>(sizeof(HashT) * 2, sizeof(HashT) * 5);
  const size_t n_elements = Pow2(prng.UniformInt<size_t>(4, 10));

  // Creates 3 elements to verify.
  std::map<uint64_t, std::vector<std::byte>> elements_to_verify;
  elements_to_verify[1] = prng.RandomByteVector(element_size_big);
  elements_to_verify[10] = prng.RandomByteVector(element_size_big);
  elements_to_verify[11] = prng.RandomByteVector(element_size_big);

  const PackerHasher<HashT> packer(element_size_big, n_elements);
  const bool is_merkle_layer = false;
  PackagingCommitmentSchemeVerifier<HashT> packaging_verifier(
      element_size_big, n_elements, &verifier_channel,
      [&packaging_verifier, &packer, &elements_to_verify](
          size_t /*n_elements_inner_layer*/) -> std::unique_ptr<CommitmentSchemeVerifier> {
        auto inner_commitment_scheme_verifier =
            std::make_unique<StrictMock<CommitmentSchemeVerifierMock>>();
        // NumOfElements() will be called as number of elements that is sent to extra annotations.
        // See implementation on PackagingCommitmentSchemeVerifier.VerifyIntegrity();
        EXPECT_CALL(*inner_commitment_scheme_verifier, NumOfElements())
            .Times(testing::AtLeast(1))
            // #elements of inner commitment scheme layer equals to #packages of current layer.
            .WillRepeatedly(testing::Return(packaging_verifier.GetNumOfPackages()));
        EXPECT_CALL(
            *inner_commitment_scheme_verifier,
            VerifyIntegrity(packer.PackAndHash(elements_to_verify, is_merkle_layer)));

        return inner_commitment_scheme_verifier;
      },
      is_merkle_layer);
  packaging_verifier.VerifyIntegrity(elements_to_verify);
}

#endif

}  // namespace
}  // namespace starkware
