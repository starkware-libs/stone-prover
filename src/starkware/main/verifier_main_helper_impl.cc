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

#include "starkware/main/verifier_main_helper_impl.h"

#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fields/field_operations_helper.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/channel/noninteractive_verifier_felt_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/crypt_tools/invoke.h"
#include "starkware/crypt_tools/masked_hash.h"
#include "starkware/proof_system/proof_system.h"
#include "starkware/randomness/prng.h"

namespace starkware {

bool VerifierMainHelperImpl(
    Statement* statement, const std::vector<std::byte>& proof, const JsonValue& parameters,
    const std::string& annotation_file_name, const std::string& extra_output_file_name) {
  try {
    const Air& air = statement->GetAir();
    const bool use_extension_field = parameters["use_extension_field"].AsBool();
    Field field = parameters["field"].AsField();
    if (use_extension_field) {
      ASSERT_RELEASE(
          IsExtensionField(field),
          "use_extension_field is true but the field is not an extension field.");
    }

    StarkParameters stark_params =
        StarkParameters::FromJson(parameters["stark"], field, UseOwned(&air), use_extension_field);

    const std::string commitment_hash = parameters["commitment_hash"].HasValue()
                                            ? parameters["commitment_hash"].AsString()
                                            : "keccak256_masked160_msb";

    const std::string verifier_friendly_commitment_hash =
        parameters["verifier_friendly_commitment_hash"].HasValue()
            ? parameters["verifier_friendly_commitment_hash"].AsString()
            : commitment_hash;

    const size_t n_verifier_friendly_commitment_layers =
        parameters["n_verifier_friendly_commitment_layers"].HasValue()
            ? parameters["n_verifier_friendly_commitment_layers"].AsUint64()
            : 0;

    const bool verifier_friendly_channel_updates =
        parameters["verifier_friendly_channel_updates"].HasValue()
            ? parameters["verifier_friendly_channel_updates"].AsBool()
            : false;

    const std::string channel_hash =
        parameters["channel_hash"].HasValue() ? parameters["channel_hash"].AsString() : "keccak256";

    const std::string public_input_hash =
        verifier_friendly_channel_updates ? verifier_friendly_commitment_hash : channel_hash;

    // If verifier_friendly_channel_updates is true, then the channel initialization is performed by
    // a hash of the verifier friendly hash function, and afterwards the channel continues to
    // produce numbers based on the channel_hash function. Otherwise, the channel_hash is used for
    // the entire process.
    const std::vector<std::byte> public_input_initial_hash =
        InvokeByHashFunc(public_input_hash, [&](auto hash_tag) {
          using HashT = typename decltype(hash_tag)::type;
          std::array<std::byte, HashT::kDigestNumBytes> public_input_initial_hash_array =
              HashT::HashBytesWithLength(statement->GetInitialHashChainSeed()).GetDigest();
          return std::vector<std::byte>(
              public_input_initial_hash_array.begin(), public_input_initial_hash_array.end());
        });

    VerifierChannel* channel;
    if (channel_hash == "poseidon3") {
      const std::string pow_hash =
          parameters["pow_hash"].HasValue() ? parameters["pow_hash"].AsString() : "blake256";

      using FieldElementT = PrimeFieldElement<252, 0>;
      FieldElementT initial_state =
          FieldElementT::FromBigInt(FieldElementT::ValueType::FromBytes(public_input_initial_hash));

      channel = new NoninteractiveVerifierFeltChannel(initial_state, proof, pow_hash);
    } else {
      std::unique_ptr<PrngBase> prng = InvokeByHashFunc(channel_hash, [&](auto hash_tag) {
        using HashT = typename decltype(hash_tag)::type;
        return PrngImpl<HashT>(HashChain<HashT>(HashT::InitDigestTo(public_input_initial_hash)))
            .Clone();
      });
      channel = new NoninteractiveVerifierChannel(std::move(prng), proof);
    }

    if (annotation_file_name.empty()) {
      channel->DisableAnnotations();
    }
    if (extra_output_file_name.empty()) {
      channel->DisableExtraAnnotations();
    }

    TableVerifierFactory table_verifier_factory =
        InvokeByHashFunc(commitment_hash, [&](auto hash_tag) {
          using HashT = typename decltype(hash_tag)::type;
          TableVerifierFactory res = [channel, n_verifier_friendly_commitment_layers,
                                      verifier_friendly_commitment_hash, &commitment_hash](
                                         const Field& field, uint64_t n_rows, size_t n_columns) {
            auto commitment_hashes = CommitmentHashes(
                /*top_hash=*/verifier_friendly_commitment_hash,
                /*bottom_hash=*/commitment_hash);
            auto packaging_commitment_scheme = MakeCommitmentSchemeVerifier(
                n_columns * field.ElementSizeInBytes(), n_rows, channel,
                n_verifier_friendly_commitment_layers, commitment_hashes, n_columns);

            return std::make_unique<TableVerifierImpl>(
                field, n_columns,
                TakeOwnershipFrom<CommitmentSchemeVerifier>(std::move(packaging_commitment_scheme)),
                channel);
          };
          return res;
        });

    AnnotationScope scope(channel, statement->GetName());
    // Create StarkVerifier and verify the proof.
    StarkVerifier stark_verifier(
        UseOwned(channel), UseOwned(&table_verifier_factory), UseOwned(&stark_params),
        verifier_friendly_channel_updates);

    bool result = FalseOnError([&]() { stark_verifier.VerifyStark(); });

    if (!annotation_file_name.empty()) {
      std::ofstream annotation_file(annotation_file_name);
      annotation_file << *channel;
    }

    if (!extra_output_file_name.empty()) {
      std::ofstream extra_output_file(extra_output_file_name);
      channel->DumpExtraAnnotations(extra_output_file);
    }

    return result;
  } catch (const StarkwareException& e) {
    LOG(ERROR) << e.what();
    return false;
  }
}

}  // namespace starkware
