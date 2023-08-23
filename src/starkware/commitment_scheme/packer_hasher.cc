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

#include "starkware/commitment_scheme/packer_hasher.h"

#include <algorithm>

#include "starkware/commitment_scheme/utils.h"
#include "starkware/crypt_tools/template_instantiation.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {

namespace details {

/*
  Computes the number of elements that go in each package. Designed so that each package contains
  the minimal number of elements it can, without introducing trivial efficiency issues. The
  result will be at most max_n_elements.
*/
size_t ComputeNumElementsInPackage(
    const size_t size_of_element, const size_t size_of_package, const uint64_t max_n_elements) {
  ASSERT_RELEASE(size_of_element > 0, "An element must be at least of length 1 byte.");
  if (size_of_element >= size_of_package) {
    return 1;
  }
  const size_t elements_fit_in_package = (size_of_package - 1) / size_of_element + 1;
  return static_cast<size_t>(std::min(Pow2(Log2Ceil(elements_fit_in_package)), max_n_elements));
}

/*
  Given a sequence of bytes, partitions the sequence to n_elements equal sub-sequences, hashing
  each separately, and returning the resulting sequence of hashes as vector of bytes.
*/
template <typename HashT>
std::vector<std::byte> HashElements(gsl::span<const std::byte> data, size_t n_elements) {
  // If n_elements==0 and data is empty, return an empty vector.
  if (n_elements == 0 && data.empty()) {
    return {};
  }
  const size_t element_size = SafeDiv(data.size(), n_elements);
  std::vector<std::byte> res;
  res.reserve(n_elements * HashT::kDigestNumBytes);
  size_t pos = 0;
  for (size_t i = 0; i < n_elements; ++i, pos += element_size) {
    const auto hash_as_bytes_array =
        (HashT::HashBytesWithLength(data.subspan(pos, element_size))).GetDigest();
    std::copy(hash_as_bytes_array.begin(), hash_as_bytes_array.end(), std::back_inserter(res));
  }
  return res;
}

/*
  Given a sequence of bytes, partitions the sequence to elements and hashes each pair separately.
  Returns the resulting sequence of hashes as a vector of bytes.
*/
template <typename HashT>
std::vector<std::byte> HashElementsTwoToOne(gsl::span<const std::byte> data) {
  // If data is empty, return an empty vector.
  if (data.empty()) {
    return {};
  }
  // Each element in the next layer is hash of 2 elements in current layer.
  const size_t elements_to_hash_size = 2 * HashT::kDigestNumBytes;
  const size_t n_elements_next_layer = SafeDiv(data.size(), elements_to_hash_size);

  std::vector<HashT> bytes_as_hash = BytesAsHash<HashT>(data, HashT::kDigestNumBytes);

  // Compute next hash layer.
  for (size_t i = 0; i < n_elements_next_layer; i++) {
    bytes_as_hash[i] = HashT::Hash(bytes_as_hash[i * 2], bytes_as_hash[i * 2 + 1]);
  }

  // Translate to bytes.
  std::vector<std::byte> res;
  res.reserve(n_elements_next_layer * HashT::kDigestNumBytes);
  size_t pos = 0;
  for (size_t i = 0; i < n_elements_next_layer; ++i, pos += elements_to_hash_size) {
    const auto hash_as_bytes_array = bytes_as_hash[i].GetDigest();
    std::copy(hash_as_bytes_array.begin(), hash_as_bytes_array.end(), std::back_inserter(res));
  }
  return res;
}

}  // namespace details

// Implementation of PackerHasher.

template <typename HashT>
PackerHasher<HashT>::PackerHasher(size_t size_of_element, size_t n_elements)
    : k_size_of_element(size_of_element),
      k_n_elements_in_package(details::ComputeNumElementsInPackage(
          k_size_of_element, 2 * HashT::kDigestNumBytes, n_elements)),
      k_n_packages(SafeDiv(n_elements, k_n_elements_in_package)) {
  ASSERT_RELEASE(
      IsPowerOfTwo(n_elements), "Can only handle total number of elements that is a power of 2.");
  ASSERT_RELEASE(
      IsPowerOfTwo(k_n_elements_in_package),
      "Can only pack number of elements that is a power of 2.");
  // The following may indicate an error in parameters.
  ASSERT_RELEASE(
      n_elements >= k_n_elements_in_package,
      "There are less elements overall than there should be in a single package.");
}

template <typename HashT>
std::vector<std::byte> PackerHasher<HashT>::PackAndHash(
    gsl::span<const std::byte> data, bool is_merkle_layer) const {
  if (data.empty()) {
    return {};
  }
  size_t n_elements_in_data = SafeDiv(data.size(), k_size_of_element);
  size_t n_packages = SafeDiv(n_elements_in_data, k_n_elements_in_package);
  if (is_merkle_layer) {
    ASSERT_RELEASE(
        SafeDiv(data.size(), n_packages) == 2 * HashT::kDigestNumBytes, "Data size is wrong.");
    return details::HashElementsTwoToOne<HashT>(data);
  }
  return details::HashElements<HashT>(data, n_packages);
}

template <typename HashT>
std::vector<uint64_t> PackerHasher<HashT>::GetElementsInPackages(
    gsl::span<const uint64_t> packages) const {
  // Finds all elements that belong to the required packages.
  std::vector<uint64_t> elements_needed;
  elements_needed.reserve(packages.size() * k_n_elements_in_package);
  for (uint64_t package : packages) {
    for (uint64_t i = package * k_n_elements_in_package;
         i < (package + 1) * k_n_elements_in_package; ++i) {
      elements_needed.push_back(i);
    }
  }
  return elements_needed;
}

template <typename HashT>
std::vector<uint64_t> PackerHasher<HashT>::ElementsRequiredToComputeHashes(
    const std::set<uint64_t>& elements_known) const {
  std::set<uint64_t> packages;

  // Get package indices of known_elements.
  for (const uint64_t el : elements_known) {
    const uint64_t package_id = el / k_n_elements_in_package;
    ASSERT_RELEASE(
        package_id < k_n_packages, "Query out of range. range: [0, " +
                                       std::to_string(k_n_packages) +
                                       "), query: " + std::to_string(package_id));

    packages.insert(package_id);
  }
  // Return only elements that belong to packages but are not known.
  const auto all_packages_elements =
      GetElementsInPackages(std::vector<uint64_t>{packages.begin(), packages.end()});
  std::vector<uint64_t> required_elements;
  std::set_difference(
      all_packages_elements.begin(), all_packages_elements.end(), elements_known.begin(),
      elements_known.end(), std::inserter(required_elements, required_elements.begin()));
  return required_elements;
}

template <typename HashT>
std::map<uint64_t, std::vector<std::byte>> PackerHasher<HashT>::PackAndHash(
    const std::map<uint64_t, std::vector<std::byte>>& elements, const bool is_merkle_layer) const {
  std::set<uint64_t> packages;
  // Deduce required packages.
  for (const auto& key_val : elements) {
    packages.insert(key_val.first / k_n_elements_in_package);
  }

  // Hash packages and returns the results as map of element indices with their hash value.
  std::map<uint64_t, std::vector<std::byte>> hashed_packages;
  for (const uint64_t package : packages) {
    const size_t first = package * k_n_elements_in_package;
    const size_t last = (package + 1) * k_n_elements_in_package;
    std::vector<std::byte> packed_elements(k_size_of_element * k_n_elements_in_package);
    // Hash package.
    for (size_t i = first, pos = 0; i < last; ++i, pos += k_size_of_element) {
      const std::vector<std::byte>& element_data = elements.at(i);
      ASSERT_RELEASE(
          element_data.size() == k_size_of_element, "Element size mismatches the one declared.");
      std::copy(element_data.begin(), element_data.end(), packed_elements.begin() + pos);
    }
    // Store the results in the returned map.
    auto bytes_array = PackAndHash(packed_elements, is_merkle_layer);
    hashed_packages[package] = {bytes_array.begin(), bytes_array.end()};
  }

  return hashed_packages;
}

INSTANTIATE_FOR_ALL_HASH_FUNCTIONS(PackerHasher);

}  // namespace starkware
