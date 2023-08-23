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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"
#include "starkware/commitment_scheme/packaging_commitment_scheme.h"
#include "starkware/crypt_tools/blake2s.h"
#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/crypt_tools/masked_hash.h"
#include "starkware/crypt_tools/pedersen.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using gsl::make_span;
using gsl::span;

using std::map;
using std::set;
using std::vector;

using testing::HasSubstr;

template <typename HashParam>
struct MerkleCommitmentSchemePairT {
  using ProverT = MerkleCommitmentSchemeProver<HashParam>;
  using VerifierT = MerkleCommitmentSchemeVerifier<HashParam>;
  using HashT = HashParam;

  static ProverT CreateProver(
      NoninteractiveProverChannel* prover_channel, size_t /*size_of_element*/,
      size_t n_elements_in_segment, size_t n_segments, size_t /*n_layers*/,
      size_t /*n_verifier_friendly_commitment_layers*/) {
    ASSERT_RELEASE(
        n_elements_in_segment == 1,
        "Wrong number of elements in segment in Merkle commitment scheme prover initialization. "
        "Should be 1, but actually: " +
            std::to_string(n_elements_in_segment));
    return ProverT(n_segments, prover_channel);
  }

  static VerifierT CreateVerifier(
      NoninteractiveVerifierChannel* verifier_channel, size_t /*size_of_element*/,
      size_t n_elements, size_t /*n_verifier_friendly_commitment_layers*/) {
    return VerifierT(n_elements, verifier_channel);
  }

  static size_t DrawSizeOfElement(Prng* /*prng*/) { return HashT::kDigestNumBytes; }

  // Indicator that this is a Merkle commitment scheme layer. Used to set number of segments
  // correctly.
  static constexpr bool kIsMerkle = true;

  static constexpr size_t kMinElementSize = HashT::kDigestNumBytes;
};

template <typename HashParam>
struct PackagingCommitmentSchemePairT {
  using HashT = HashParam;

  static size_t DrawSizeOfElement(Prng* prng) {
    return prng->UniformInt<size_t>(1, sizeof(HashT) * 5);
  }

  // Indicator that this is a merkle commitment scheme layer. Used to set number of segments
  // correctly.
  static constexpr bool kIsMerkle = false;
};

template <typename HashParam>
struct PackagingCommitmentSchemePairSingleHashT : public PackagingCommitmentSchemePairT<HashParam> {
  using ProverT = PackagingCommitmentSchemeProver<HashParam>;
  using VerifierT = PackagingCommitmentSchemeVerifier<HashParam>;
  using HashT = HashParam;

  static ProverT CreateProver(
      NoninteractiveProverChannel* prover_channel, size_t size_of_element,
      size_t n_elements_in_segment, size_t n_segments, size_t n_layers,
      size_t n_verifier_friendly_commitment_layers) {
    ASSERT_RELEASE(
        n_verifier_friendly_commitment_layers == 0,
        "n_verifier_friendly_commitment_layers should be 0, but it is: " +
            std::to_string(n_verifier_friendly_commitment_layers));

    return MakeCommitmentSchemeProver<HashT>(
        size_of_element, n_elements_in_segment, n_segments, prover_channel,
        /*n_verifier_friendly_commitment_layers=*/0, CommitmentHashes(HashT::HashName()), n_layers);
  }

  static VerifierT CreateVerifier(
      NoninteractiveVerifierChannel* verifier_channel, size_t size_of_element, size_t n_elements,
      size_t n_verifier_friendly_commitment_layers) {
    ASSERT_RELEASE(
        n_verifier_friendly_commitment_layers == 0,
        "n_verifier_friendly_commitment_layers should be 0, but it is: " +
            std::to_string(n_verifier_friendly_commitment_layers));

    return MakeCommitmentSchemeVerifier<HashT>(
        size_of_element, n_elements, verifier_channel,
        /*n_verifier_friendly_commitment_layers=*/0, CommitmentHashes(HashT::HashName()));
  }

  static constexpr size_t kMinElementSize = 1;
};

/*
  Uses two hashes, top and bottom, in the commitment.
*/
template <typename TopHash, typename BottomHash>
struct PackagingCommitmentSchemePairTwoHashesT : public PackagingCommitmentSchemePairT<BottomHash> {
  using ProverT = PackagingCommitmentSchemeProver<BottomHash>;
  using VerifierT = PackagingCommitmentSchemeVerifier<BottomHash>;
  using TopHashT = TopHash;
  using BottomHashT = BottomHash;

  // Note: we assume here that the largest layer of the Merkle tree uses the bottom hash (see the
  // return types of CreateProver and CreateVerifier).

  static ProverT CreateProver(
      NoninteractiveProverChannel* prover_channel, size_t size_of_element,
      size_t n_elements_in_segment, size_t n_segments, size_t n_layers,
      size_t n_verifier_friendly_commitment_layers) {
    CommitmentHashes commitment_hashes =
        CommitmentHashes(TopHashT::HashName(), BottomHash::HashName());
    return MakeCommitmentSchemeProver<BottomHashT>(
        size_of_element, n_elements_in_segment, n_segments, prover_channel,
        n_verifier_friendly_commitment_layers, commitment_hashes, n_layers);
  }

  static VerifierT CreateVerifier(
      NoninteractiveVerifierChannel* verifier_channel, size_t size_of_element, size_t n_elements,
      size_t n_verifier_friendly_commitment_layers) {
    CommitmentHashes commitment_hashes =
        CommitmentHashes(TopHashT::HashName(), BottomHash::HashName());
    return MakeCommitmentSchemeVerifier<BottomHashT>(
        size_of_element, n_elements, verifier_channel, n_verifier_friendly_commitment_layers,
        commitment_hashes);
  }
};

using TestTypes = ::testing::Types<
    MerkleCommitmentSchemePairT<Blake2s256>, PackagingCommitmentSchemePairSingleHashT<Blake2s256>,
    MerkleCommitmentSchemePairT<Keccak256>, PackagingCommitmentSchemePairSingleHashT<Keccak256>>;

using TestTypesTwoHashes = ::testing::Types<
    PackagingCommitmentSchemePairTwoHashesT<
        MaskedHash<Blake2s256, 20, true>, MaskedHash<Keccak256, 20, true>>,
    PackagingCommitmentSchemePairTwoHashesT<
        MaskedHash<Keccak256, 20, false>, MaskedHash<Blake2s256, 20, false>>,
    PackagingCommitmentSchemePairTwoHashesT<Pedersen, MaskedHash<Blake2s256, 20, false>>>;

/*
  Returns number of segments to use, N, such that:

  1) N is a power of 2.

  2) size_of_element * n_element  >= N * min_segment_bytes (in particular, each segment is of
     length at least min_segment_bytes).

  3) N <= n_elements.
*/
size_t DrawNumSegments(
    const size_t size_of_element, const size_t n_elements, const size_t min_segment_bytes,
    Prng* prng, bool is_merkle) {
  if (is_merkle) {
    // In case this is a Merkle commitment scheme, each segment contains a single element.
    return n_elements;
  }
  const size_t total_bytes = size_of_element * n_elements;
  const size_t max_n_segments =
      std::min(std::max<size_t>(1, total_bytes / min_segment_bytes), n_elements);
  const size_t max_log_n_segments = Log2Floor(max_n_segments);
  return Pow2(prng->UniformInt<size_t>(0, max_log_n_segments));
}

template <typename T>
class CommitmentScheme : public ::testing::Test {
 public:
  using CommitmentProver = typename T::ProverT;

  CommitmentScheme()
      : size_of_element_(T::DrawSizeOfElement(&prng_)),
        n_elements_(Pow2(prng_.UniformInt<size_t>(0, 10))),
        n_segments_(DrawNumSegments(
            size_of_element_, n_elements_, CommitmentProver::kMinSegmentBytes, &prng_,
            T::kIsMerkle)),
        data_(prng_.RandomByteVector(size_of_element_ * n_elements_)),
        queries_(DrawQueries()) {}

  CommitmentScheme(
      size_t n_elements, size_t n_segments, size_t n_verifier_layers_lower_bound,
      size_t n_verifier_layers_upper_bound)
      : size_of_element_(T::HashT::kDigestNumBytes),
        n_elements_(n_elements),
        n_segments_(n_segments),
        data_(prng_.RandomByteVector(size_of_element_ * n_elements_)),
        queries_(DrawQueries()),
        n_verifier_friendly_commitment_layers_(prng_.UniformInt<size_t>(
            n_verifier_layers_lower_bound, n_verifier_layers_upper_bound)) {}

 protected:
  Prng prng_;
  const Prng channel_prng_{};
  size_t size_of_element_;
  size_t n_elements_;
  size_t n_segments_;
  vector<std::byte> data_;
  set<uint64_t> queries_;
  size_t n_verifier_friendly_commitment_layers_{0};

  NoninteractiveProverChannel GetProverChannel() const {
    return NoninteractiveProverChannel(channel_prng_.Clone());
  }

  NoninteractiveVerifierChannel GetVerifierChannel(const span<const std::byte> proof) const {
    return NoninteractiveVerifierChannel(channel_prng_.Clone(), proof);
  }

  set<uint64_t> DrawQueries() {
    const auto n_queries = prng_.UniformInt<size_t>(1, 10);
    set<uint64_t> queries;
    for (size_t i = 0; i < n_queries; ++i) {
      queries.insert(prng_.UniformInt<uint64_t>(0, n_elements_ - 1));
    }

    return queries;
  }

  size_t GetNumSegments() const { return n_segments_; }
  size_t GetNumElementsInSegment() const { return SafeDiv(n_elements_, n_segments_); }

  span<const std::byte> GetSegment(const size_t index) const {
    const size_t n_segment_bytes = size_of_element_ * GetNumElementsInSegment();
    return make_span(data_).subspan(index * n_segment_bytes, n_segment_bytes);
  }

  vector<std::byte> GetElement(const size_t index) const {
    return vector<std::byte>(
        data_.begin() + index * size_of_element_, data_.begin() + (index + 1) * size_of_element_);
  }

  map<uint64_t, vector<std::byte>> GetSparseData() const {
    map<uint64_t, vector<std::byte>> elements_to_verify;
    for (const uint64_t& q : queries_) {
      elements_to_verify[q] = GetElement(q);
    }
    return elements_to_verify;
  }

  const vector<std::byte> GenerateProof(
      const size_t n_out_of_memory_layers, bool include_decommitment = true) {
    using CommitmentProver = typename T::ProverT;

    // Initialize prover channel.
    NoninteractiveProverChannel prover_channel = GetProverChannel();

    // Initialize the commitment-scheme prover side.
    CommitmentProver committer = T::CreateProver(
        &prover_channel, size_of_element_, GetNumElementsInSegment(), GetNumSegments(),
        n_out_of_memory_layers, n_verifier_friendly_commitment_layers_);

    // Commit on the data_.
    for (size_t i = 0; i < GetNumSegments(); ++i) {
      const auto segment = GetSegment(i);
      committer.AddSegmentForCommitment(segment, i);
    }
    committer.Commit();

    if (include_decommitment) {
      // Generate decommitment for queries_ already drawn.
      const vector<uint64_t> element_idxs = committer.StartDecommitmentPhase(queries_);
      vector<std::byte> elements_data;
      for (const uint64_t index : element_idxs) {
        const auto element = GetElement(index);
        std::copy(element.begin(), element.end(), std::back_inserter(elements_data));
      }
      committer.Decommit(elements_data);
    }

    // Return proof.
    return prover_channel.GetProof();
  }

  bool VerifyProof(
      const vector<std::byte>& proof, const map<uint64_t, vector<std::byte>>& elements_to_verify) {
    using CommitmentVerifier = typename T::VerifierT;

    // Initialize verifier channel.
    NoninteractiveVerifierChannel verifier_channel = GetVerifierChannel(proof);

    // Initialize commitment-scheme verifier.
    CommitmentVerifier verifier = T::CreateVerifier(
        &verifier_channel, size_of_element_, n_elements_, n_verifier_friendly_commitment_layers_);

    // Read commitment.
    verifier.ReadCommitment();

    // Verify consistency of data_ with commitment.
    return verifier.VerifyIntegrity(elements_to_verify);
  }
};

/*
  A derived class to set up specific parameters for commitment scheme that uses a single hash.
*/
template <typename T>
class CommitmentSchemeSingleHash : public CommitmentScheme<T> {
 public:
  CommitmentSchemeSingleHash() : CommitmentScheme<T>() {}
};

/*
  A derived class to set up specific parameters for commitment scheme that uses 2 hashes.
*/
template <typename T>
class CommitmentSchemeTwoHashes : public CommitmentScheme<T> {
  using TopHashT = typename T::TopHashT;
  using BottomHashT = typename T::BottomHashT;

 public:
  CommitmentSchemeTwoHashes() : CommitmentScheme<T>(64, 4, 2, 5) {}

  TopHashT CalculateTreeRoot() {
    // Get leaves as span of bottom hash.
    gsl::span<BottomHashT> data_hash_bottom =
        gsl::make_span(this->data_).template as_span<BottomHashT>();

    // First layer hashes 2 leaves to one node.
    size_t sub_layer_length = data_hash_bottom.size();
    // Note that the smallest layer of the bottom tree is the largest layer of top tree.
    const size_t barrier_layer_length = Pow2(this->n_verifier_friendly_commitment_layers_);
    const size_t n_bottom_layers =
        SafeLog2(data_hash_bottom.size()) - this->n_verifier_friendly_commitment_layers_;
    // Compute bottom layers with BottomHash.
    for (size_t j = 0; j < n_bottom_layers; j++) {
      sub_layer_length = SafeDiv(sub_layer_length, 2);
      ASSERT_RELEASE(sub_layer_length >= barrier_layer_length, "Layer length is wrong.");
      for (size_t i = 0; i < sub_layer_length; i++) {
        // Compute next sub-layer.
        data_hash_bottom[i] =
            BottomHashT::Hash(data_hash_bottom[i * 2], data_hash_bottom[i * 2 + 1]);
      }
    }

    // View BottomHashT elements as TopHashT elements- first TopHash layer.
    std::vector<TopHashT> data_hash_top;
    data_hash_top.reserve(sub_layer_length);
    ASSERT_RELEASE(sub_layer_length == barrier_layer_length, "Layer length is wrong.");
    for (size_t i = 0; i < sub_layer_length; i++) {
      auto curr_hash_as_bytes = data_hash_bottom[i].GetDigest();
      data_hash_top.push_back(TopHashT::InitDigestTo(curr_hash_as_bytes));
    }

    // Compute the rest of the layers - top tree.
    for (size_t j = 0; j < this->n_verifier_friendly_commitment_layers_; j++) {
      sub_layer_length = SafeDiv(sub_layer_length, 2);
      for (size_t i = 0; i < sub_layer_length; i++) {
        data_hash_top[i] = TopHashT::Hash(data_hash_top[i * 2], data_hash_top[i * 2 + 1]);
      }
    }
    ASSERT_RELEASE(
        sub_layer_length == 1,
        "Last layer should be of size 1, but got : " + std::to_string(sub_layer_length));

    // Return the root.
    return data_hash_top[0];
  }

  /*
    Assuming proof contains only the commitment phase (see 'GenerateProof' function with
    include_decommitment=false), converts the proof to the (single) hash it represents.
  */
  TopHashT FromProofToRoot(const vector<std::byte> proof) { return TopHashT::InitDigestTo(proof); }
};

TYPED_TEST_CASE(CommitmentSchemeSingleHash, TestTypes);
TYPED_TEST_CASE(CommitmentSchemeTwoHashes, TestTypesTwoHashes);

TYPED_TEST(CommitmentSchemeTwoHashes, Completeness) {
  const size_t n_out_of_memory_layers = this->prng_.template UniformInt<size_t>(0, 6);
  auto proof = this->GenerateProof(n_out_of_memory_layers);

  // Fetch data_ in queried locations.
  const map<uint64_t, vector<std::byte>> elements_to_verify = this->GetSparseData();

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_TRUE(this->VerifyProof(proof, elements_to_verify));
}

TYPED_TEST(CommitmentSchemeTwoHashes, TreeCheck) {
  const size_t n_out_of_memory_layers = this->prng_.template UniformInt<size_t>(0, 6);
  const auto proof = this->GenerateProof(n_out_of_memory_layers, false);
  const auto actual_root = this->FromProofToRoot(proof);
  const auto expected_root = this->CalculateTreeRoot();
  EXPECT_EQ(actual_root, expected_root);
}

TYPED_TEST(CommitmentSchemeSingleHash, Completeness) {
  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto n_out_of_memory_layers = this->prng_.template UniformInt<size_t>(0, 6);
  const auto proof = this->GenerateProof(n_out_of_memory_layers);

  // Fetch data_ in queried locations.
  const map<uint64_t, vector<std::byte>> elements_to_verify = this->GetSparseData();

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_TRUE(this->VerifyProof(proof, elements_to_verify));
}

/*
  Generates two proofs: first with 0 out of memory merkle layers and then with a random number of
  layers between 1 to the minimum between tree height and 10, and checks that they are equal.
*/
TYPED_TEST(CommitmentSchemeSingleHash, MultipleOutOfMemoryLayers) {
  const auto proof_full_merkle = this->GenerateProof(0);
  const auto proof_out_of_memory_merkle =
      this->GenerateProof(this->prng_.template UniformInt<size_t>(1, 10));
  EXPECT_EQ(proof_full_merkle, proof_out_of_memory_merkle);
}

TYPED_TEST(CommitmentSchemeSingleHash, CorruptedProof) {
  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof(this->prng_.template UniformInt<size_t>(0, 6));

  // Fetch data_ in queried locations.
  const map<uint64_t, vector<std::byte>> elements_to_verify = this->GetSparseData();

  // Construct corrupted proof.
  vector<std::byte> corrupted_proof = proof;
  corrupted_proof[this->prng_.template UniformInt<size_t>(0, proof.size() - 1)] ^= std::byte(1);

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_FALSE(this->VerifyProof(corrupted_proof, elements_to_verify));
}

TYPED_TEST(CommitmentSchemeSingleHash, CorruptedData) {
  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof(this->prng_.template UniformInt<size_t>(0, 6));

  // Fetch data_ in queried locations.
  const map<uint64_t, vector<std::byte>> elements_to_verify = this->GetSparseData();

  // Construct corrupted data_.
  map<uint64_t, vector<std::byte>> corrupted_data = elements_to_verify;
  auto iter = corrupted_data.begin();
  std::advance(iter, this->prng_.template UniformInt<size_t>(0, corrupted_data.size() - 1));
  iter->second[this->prng_.template UniformInt<size_t>(0, this->size_of_element_ - 1)] ^=
      std::byte(1);

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_FALSE(this->VerifyProof(proof, corrupted_data));
}

TYPED_TEST(CommitmentSchemeSingleHash, ShortDataOneSegment) {
  // Make shortest possible input.
  this->n_segments_ = 1;
  this->size_of_element_ = TypeParam::kMinElementSize;
  this->n_elements_ = 1;
  this->data_ = this->prng_.RandomByteVector(this->size_of_element_ * this->n_elements_);
  this->queries_ = this->DrawQueries();

  // Verify completeness.

  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof(this->prng_.template UniformInt<size_t>(0, 6));

  // Fetch data_ in queried locations.
  const map<uint64_t, vector<std::byte>> elements_to_verify = this->GetSparseData();

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_TRUE(this->VerifyProof(proof, elements_to_verify));
}

TYPED_TEST(CommitmentSchemeSingleHash, OutOfRangeQuery) {
  // Set queries to be a single query, out of range ( >= n_elements_).
  const size_t query = this->n_elements_ + this->prng_.template UniformInt<size_t>(0, 10);
  this->queries_ = {query};

  // Try to generate proof.
  EXPECT_ASSERT(
      this->GenerateProof(this->prng_.template UniformInt<size_t>(0, 6)),
      HasSubstr("out of range"));

  // Fetch data_ in queried locations.
  const map<uint64_t, vector<std::byte>> elements_to_verify = {
      std::make_pair(query, vector<std::byte>(this->size_of_element_))};

  // Verify to verify consistency.
  const vector<std::byte> dummy_proof(
      100);  // Proof string must be long enough to contain the commitment.
  EXPECT_ASSERT(this->VerifyProof(dummy_proof, elements_to_verify), HasSubstr("out of range"));
}

TYPED_TEST(CommitmentSchemeSingleHash, ShortElement) {
  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof(this->prng_.template UniformInt<size_t>(0, 6));

  // Fetch data_ in queried locations.
  const map<uint64_t, vector<std::byte>> elements_to_verify = this->GetSparseData();

  // Construct corrupted data_ (one element shorter than expected).
  map<uint64_t, vector<std::byte>> corrupted_data = elements_to_verify;
  auto iter = corrupted_data.begin();
  std::advance(iter, this->prng_.template UniformInt<size_t>(0, corrupted_data.size() - 1));
  iter->second.resize(this->prng_.template UniformInt<size_t>(0, this->size_of_element_ - 1));

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_ASSERT(this->VerifyProof(proof, corrupted_data), HasSubstr("Element size mismatch"));
}

TYPED_TEST(CommitmentSchemeSingleHash, LongElement) {
  // Generate decommitment, using CommitmentSchemeProver instance.
  const auto proof = this->GenerateProof(this->prng_.template UniformInt<size_t>(0, 6));

  // Fetch data_ in queried locations.
  const map<uint64_t, vector<std::byte>> elements_to_verify = this->GetSparseData();

  // Construct corrupted data_ (one element to longer than expected).
  map<uint64_t, vector<std::byte>> corrupted_data = elements_to_verify;
  auto iter = corrupted_data.begin();
  std::advance(iter, this->prng_.template UniformInt<size_t>(0, corrupted_data.size() - 1));
  iter->second.resize(this->prng_.template UniformInt<size_t>(
      this->size_of_element_ + 1, this->size_of_element_ + 10));

  // Verify integrity of queried data_ with commitment using CommitmentSchemeVerifier instance.
  EXPECT_ASSERT(this->VerifyProof(proof, corrupted_data), HasSubstr("Element size mismatches"));
}

}  // namespace
}  // namespace starkware
