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

#ifndef STARKWARE_STL_UTILS_CONTAINERS_H_
#define STARKWARE_STL_UTILS_CONTAINERS_H_

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "starkware/error_handling/error_handling.h"

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

template <typename K, typename V>
std::set<K> Keys(const std::map<K, V>& m) {
  std::set<K> res;
  for (const auto& p : m) {
    res.insert(p.first);
  }
  return res;
}

template <typename T, typename V>
size_t Count(const T& container, const V& val) {
  return std::count(container.begin(), container.end(), val);
}

template <typename T>
typename T::value_type Sum(
    const T& container, const typename T::value_type& init = typename T::value_type(0)) {
  return std::accumulate(container.begin(), container.end(), init);
}

template <typename C, typename K>
bool HasKey(const C& container, const K& key) {
  return container.count(key) != 0;
}

template <typename T>
std::set<T> SetUnion(const std::set<T>& set1, const std::set<T>& set2) {
  std::set<T> res;
  std::set_union(
      set1.begin(), set1.end(), set2.begin(), set2.end(), std::inserter(res, res.begin()));
  return res;
}

template <typename T>
bool AreDisjoint(const std::set<T>& set1, const std::set<T>& set2) {
  const std::set<T>& large = (set1.size() < set2.size()) ? set1 : set2;
  const std::set<T>& small = (set1.size() >= set2.size()) ? set1 : set2;
  for (const T& el : small) {
    if (large.count(el) > 0) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool HasDuplicates(gsl::span<const T> values) {
  std::set<T> s;
  for (const T& x : values) {
    if (s.count(x) != 0) {
      return true;
    }
    s.insert(x);
  }
  return false;
}

/*
  Constructs an array of std::byte from integers.
  Usage:
    std::array<std::byte, 2> array = MakeByteArray<0x01, 0x02>();
*/
template <unsigned char... Bytes>
constexpr std::array<std::byte, sizeof...(Bytes)> MakeByteArray() {
  return {std::byte{Bytes}...};
}

/*
  Converts a span of unique_ptr<T> to a vector of raw pointers.
  Note that the unique_ptrs still have ownership of the pointees.
*/
template <typename T>
std::vector<const T*> UniquePtrsToRawPointers(gsl::span<const std::unique_ptr<T>> unique_ptrs) {
  std::vector<const T*> res;
  res.reserve(unique_ptrs.size());
  for (const std::unique_ptr<T>& ptr : unique_ptrs) {
    res.push_back(ptr.get());
  }
  return res;
}

/*
  An adapter from vector<vector<T>> to span<span<T>>, through vector<span<T>>.
  Usage:
    vector<vector<int>> v;
    void f(span<span<int>> s) {
      ...
    }
    ...
    f(SpanAdapter(v));
*/
template <typename T>
class ConstSpanAdapter {
 public:
  explicit ConstSpanAdapter(const std::vector<std::vector<T>>& vec)
      : inner_(vec.begin(), vec.end()) {}

  template <size_t N>
  explicit ConstSpanAdapter(gsl::span<const std::array<T, N>> vec)
      : inner_(vec.begin(), vec.end()) {}

  operator gsl::span<const gsl::span<const T>>() const {  // NOLINT: implicit cast.
    return gsl::span<const gsl::span<const T>>(inner_);
  }

  const gsl::span<const T>& operator[](size_t i) const { return inner_[i]; }

  size_t Size() const { return inner_.size(); }

 private:
  const std::vector<gsl::span<const T>> inner_;
};

template <typename T>
class SpanAdapter {
 public:
  explicit SpanAdapter(std::vector<std::vector<T>>& vec) : inner_(vec.begin(), vec.end()) {}

  operator gsl::span<const gsl::span<T>>() const {  // NOLINT: implicit cast.
    return gsl::span<const gsl::span<T>>(inner_);
  }

 private:
  const std::vector<gsl::span<T>> inner_;
};

/*
  Workaround non-constexpr non-const array<>::operator[] in c++14
  We use the const version and then const_cast away the const.
*/
template <class T, size_t N>
constexpr T& constexpr_at(  // NOLINT: readability-identifier-naming.
    std::array<T, N>& arr,  // NOLINT: non-const reference.
    size_t index) {
  return const_cast<T&>(  // NOLINT: const_cast.
      gsl::at(static_cast<const std::array<T, N>&>(arr), index));
}

/*
  Unsafe span at opearator for performance critical code.
*/
template <typename T>
static inline auto UncheckedAt(gsl::span<T> span, size_t index) -> decltype(span[0]) {
  ASSERT_DEBUG(index < span.size(), "index is out of range");
  return span.data()[index];
}

template <typename C>
inline void WriteCommaSeparatedValues(const C& c, std::ostream* out) {
  for (auto it = c.begin(); it != c.end(); ++it) {
    if (it != c.begin()) {
      *out << ", ";
    }
    *out << *it;
  }
}

/*
  Consumes the inner value from an r-value of optional<T>.
  Updates the value of optional<T> to nullopt. Using only std:move() instead won't reset its
  value.
*/
template <typename T>
T ConsumeOptional(std::optional<T>&& optional) {
  ASSERT_RELEASE(optional.has_value(), "The optional object doesn't have a value.");
  T value = std::move(*optional);
  optional = std::nullopt;
  return value;
}

}  // namespace starkware

namespace std {  // NOLINT: modifying std is discouraged by clang-tidy.

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const std::set<T>& s) {
  out << "{";
  starkware::WriteCommaSeparatedValues(s, &out);
  out << "}";
  return out;
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const std::vector<T>& v) {
  out << "[";
  starkware::WriteCommaSeparatedValues(v, &out);
  out << "]";
  return out;
}

template <typename K, typename V>
inline std::ostream& operator<<(std::ostream& out, const std::map<K, V>& m) {
  out << "{";
  for (auto it = m.begin(); it != m.end(); ++it) {
    if (it != m.begin()) {
      out << ", ";
    }
    out << it->first << ": " << it->second;
  }
  out << "}";
  return out;
}

}  // namespace std

#endif  // STARKWARE_STL_UTILS_CONTAINERS_H_
