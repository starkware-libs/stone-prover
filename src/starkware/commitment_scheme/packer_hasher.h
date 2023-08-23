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

#ifndef STARKWARE_COMMITMENT_SCHEME_PACKER_HASHER_H_
#define STARKWARE_COMMITMENT_SCHEME_PACKER_HASHER_H_

#include <cstddef>
#include <map>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

/*
  Handles packing elements together and hashing them, to be used e.g. by some Merkle tree.

  Motivation: the rational is that feeding individual elements into such tree is wasteful, when the
  element size is smaller than the hash used by the tree. To minimize this waste - we can group
  elements together into packages approximately the size of the hash (or larger), and use these as
  the basic element for the tree. This is more economic but introduces a slight complication, as
  whenever one wants an authentication path for some element, one needs all the elements in the
  package containing that element. This class provides the necessary methods to handle this case.
*/
template <typename HashT>
class PackerHasher {
 public:
  PackerHasher(size_t size_of_element, size_t n_elements);

  /*
    Groups together elements into packages and returns the sequence of hashes (one has per
    package).
  */
  std::vector<std::byte> PackAndHash(gsl::span<const std::byte> data, bool is_merkle_layer) const;

  /*
    Given a list of elements (elements_known), known to the caller, returns a vector of the
    additional elements that the caller has to provide so that the packer can compute the set of
    hashes for the packages including those known elements.

    A typical use case: when one wants to verify a decommitment for the i-th element. Internally,
    this i-th element is in the same package with a bunch of other elements, which are all hashed
    together. In order to verify the decommitment, the hash for the package containing the i-th
    element has to be computed, as the decommitment is provided with respect to it, and for that -
    one needs to find out who are the i-th element's neighbors in this package.
  */
  std::vector<uint64_t> ElementsRequiredToComputeHashes(
      const std::set<uint64_t>& elements_known) const;

  /*
    Given a vector of packages, returns a vector of the indices of all elements in that package. For
    example, if there are 4 elements in each package and packages equals to {2,4} then the return
    value is {8,9,10,11,16,17,18,19}.
  */
  std::vector<uint64_t> GetElementsInPackages(gsl::span<const uint64_t> packages) const;

  /*
    Given numbered elements, groups them into packages, and returns a map where the key is the
    package's index, and the value is the packages's hash.

    A typical use case: when the caller has a decommitment pretaining to a set of known elements
    and when, after calling ElementsRequiredToComputeHashes, the caller provides the missing
    elements, and thus obtains the necessary data that will be fed - together with the
    decommitment - into a verification method of the commitment scheme.
  */
  std::map<uint64_t, std::vector<std::byte>> PackAndHash(
      const std::map<uint64_t, std::vector<std::byte>>& elements, bool is_merkle_layer) const;

  const size_t k_size_of_element;
  const size_t k_n_elements_in_package;
  const size_t k_n_packages;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_PACKER_HASHER_H_
