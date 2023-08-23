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

#ifndef STARKWARE_FRI_FRI_PARAMETERS_H_
#define STARKWARE_FRI_FRI_PARAMETERS_H_

#include <utility>
#include <vector>

#include "starkware/algebra/polymorphic/field.h"
#include "starkware/fft_utils/fft_bases.h"
#include "starkware/utils/json.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

struct FriParameters {
  /*
    A list of fri_step_i (one per FRI layer). FRI reduction in the i-th layer will be 2^fri_step_i
    and the total reduction factor will be $2^{\sum_i \fri_step_i}$. The size of fri_step_list is
    the number of FRI layers.

    For example, if fri_step_0 = 3, the second layer will be of size N/8 (where N is the size of the
    first layer). It means that the two merkle trees for layers of sizes N/2 and N/4 will be
    skipped. On the other hand, it means that each coset in the first layer is of size 8 instead
    of 2. Also note that in the fri_step_0=1 case we send 2 additional field elements per query (one
    for each of the two layers that we skipped). So, while we send more field elements in the
    fri_step_0=3 case (8 rather than 4), we refrain from sending the authentication paths for the
    two skipped layers.

    For a simple FRI usage, take fri_step_list = {1, 1, ..., 1}.
  */
  std::vector<size_t> fri_step_list;

  /*
    In the original FRI protocol, one has to reduce the degree from N to 1 by using a total of
    log2(N) fri steps (sum of fri_step_list = log2(N)). This has two disadvantages:
      1. The last layers are small but still require Merkle authentication paths which are
         non-negligible.
      2. It requires N to be of the form 2^n.

    In our implementation, we reduce the degree from N to R (last_layer_degree_bound) for a
    relatively small R using log2(N/R) fri steps. To do it we send the R coefficients of the last
    FRI layer instead of continuing with additional FRI layers.

    To reduce proof-length, it is always better to pick last_layer_degree_bound > 1.
  */
  uint64_t last_layer_degree_bound;
  size_t n_queries = 0;
  MaybeOwnedPtr<FftBases> fft_bases = nullptr;
  Field field;

  /*
    If greater than 0, used to apply proof of work right before randomizing the FRI queries. Since
    the probability to draw bad queries is relatively high (~rho for each query), while the
    probability to draw bad x^(0) values is ~1/|F|, the queries are more vulnerable to enumeration.
  */
  size_t proof_of_work_bits;

  static FriParameters FromJson(
      const JsonValue& json, MaybeOwnedPtr<FftBases> fft_bases, const Field& field);
};

inline FriParameters FriParameters::FromJson(
    const JsonValue& json, MaybeOwnedPtr<FftBases> fft_bases, const Field& field) {
  return {
      json["fri_step_list"].AsSizeTVector(),
      json["last_layer_degree_bound"].AsSizeT(),
      json["n_queries"].AsSizeT(),
      std::move(fft_bases),
      field,
      json["proof_of_work_bits"].AsSizeT(),
  };
}

struct FriProverConfig {
  static constexpr uint64_t kDefaultMaxNonChunkedLayerSize = 32768;
  static constexpr size_t kDefaultNumberOfChunksBetweenLayers = 32;
  static constexpr size_t kAllInMemoryLayers = 63;

  /*
    Maximum layer size when quering previous layer between two in-memory layers.
    The goal of this parameter is to reduce memory usage when querying the previous layer vector.
    If the layer size is bigger than this size, the layer is queried in a number of chunks specified
    by n_chunks_between_layers, and not in one chunk of the entire data.
  */
  uint64_t max_non_chunked_layer_size;
  /*
    Number of chunks to query between two in-memory layers when the layer size is larger than
    max_non_chunked_layer_size.
  */
  size_t n_chunks_between_layers;

  /*
    Log(Size) of the biggest in memory fri layer - bigger layers are out of memory.
  */
  size_t log_n_max_in_memory_fri_layer_elements;
};

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_PARAMETERS_H_
