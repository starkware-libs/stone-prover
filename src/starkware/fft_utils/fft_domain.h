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

#ifndef STARKWARE_FFT_UTILS_FFT_DOMAIN_H_
#define STARKWARE_FFT_UTILS_FFT_DOMAIN_H_

#include <memory>
#include <stack>
#include <tuple>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/fft_utils/fft_group.h"
#include "starkware/math/math.h"

namespace starkware {

class FftDomainBase {
 public:
  virtual ~FftDomainBase() = default;
  virtual FieldElement GetFieldElementAt(uint64_t idx) const = 0;
  virtual uint64_t BasisSize() const = 0;
  virtual uint64_t Size() const = 0;

  /*
    Returns a new domain with the first n basis elements removed.
    For example, iterating on RemoveFirstBasisElements(1) yields the elements of even index.
  */
  virtual std::unique_ptr<FftDomainBase> RemoveFirstBasisElementsAsUniquePtr(size_t n) const = 0;

  /*
    Returns a new domain with the last n basis elements removed.
    For example, iterating on RemoveLastBasisElements(1) yields the elements up to index size/2.
  */
  virtual std::unique_ptr<FftDomainBase> RemoveLastBasisElementsAsUniquePtr(size_t n) const = 0;
};

/*
  Represents a succinct subset of the field that is used as the domain of an FFT/FRI layer.
  FftDomain is defined by a basis (list of field elements) and a start offset (a field element).
  Every element is the product of a subset of the basis * start_offset.
  For example, if the basis is {2, 3} and the offset is 5 then the elements are:
    5, 5 * 2, 5 * 3, 5 * 2 * 3.
*/
template <typename GroupT>
class FftDomain : public FftDomainBase {
 public:
  using FieldElementT = typename GroupT::FieldElementT;
  /*
    We add a single parameter constructor, instead of adding a default value to the two parameter
    constructor. This is because we would have to make the two parameter constructor explicit, which
    will make calls to it less convinient.
  */
  explicit FftDomain(std::vector<FieldElementT> basis) : FftDomain(basis, GroupT::GroupUnit()) {}

  FftDomain(std::vector<FieldElementT> basis, const FieldElementT& start_offset)
      : basis_(std::move(basis)), start_offset_(start_offset) {}

  auto begin() const {  // NOLINT
    return Iterator(gsl::make_span(basis_), start_offset_);
  }

  auto end() const {  // NOLINT
    return Iterator();
  }

  const std::vector<FieldElementT>& Basis() const { return basis_; }
  const FieldElementT& StartOffset() const { return start_offset_; }

  /*
    Returns an FftDomain whose elements are the multiplicative inverses of the elements of the
    original domain (in the same order).
  */
  FftDomain Inverse() const {
    static_assert(
        std::is_same_v<GroupT, FftMultiplicativeGroup<FieldElementT>>,
        "Only multiplicative domains support the Inverse operation");
    std::vector<FieldElementT> new_basis;
    new_basis.reserve(basis_.size());
    for (const FieldElementT& x : basis_) {
      new_basis.push_back(x.Inverse());
    }
    return {std::move(new_basis), start_offset_.Inverse()};
  }

  /*
    Returns a new instance of FftDomain with the same basis as the original domain,
    but with a different offset.

    The offset in the original domain is ignored.
  */
  FftDomain GetShiftedDomain(const FieldElementT& offset) const {
    return FftDomain(basis_, offset);
  }

  FieldElementT operator[](uint64_t index) const {
    FieldElementT ret = start_offset_;
    ASSERT_VERIFIER(index < Size(), "Index out of range.");
    for (const auto& b : basis_) {
      if ((index & 1) == 1) {
        ret = GroupT::GroupOperation(ret, b);
      }
      index >>= 1;
    }
    return ret;
  }

  FieldElement GetFieldElementAt(uint64_t idx) const override {
    ASSERT_RELEASE(idx < Size(), "Index out of range.");
    return FieldElement(operator[](idx));
  }

  uint64_t BasisSize() const override { return basis_.size(); }

  /*
    Returns the number of elements in the domain.
  */
  uint64_t Size() const override { return Pow2(basis_.size()); }

  std::unique_ptr<FftDomainBase> RemoveFirstBasisElementsAsUniquePtr(size_t n) const override {
    return std::make_unique<FftDomain>(RemoveFirstBasisElements(n));
  }

  std::unique_ptr<FftDomainBase> RemoveLastBasisElementsAsUniquePtr(size_t n) const override {
    return std::make_unique<FftDomain>(RemoveLastBasisElements(n));
  }

  /*
    See the documentation of FftDomainBase::RemoveFirstBasisElementsAsUniquePtr.
  */
  FftDomain RemoveFirstBasisElements(size_t n) const {
    ASSERT_DEBUG(n <= basis_.size(), "index out of range");
    return FftDomain({basis_.begin() + n, basis_.end()}, start_offset_);
  }

  /*
    See the documentation of FftDomainBase::RemoveLastBasisElements.
  */
  FftDomain RemoveLastBasisElements(size_t n) const {
    ASSERT_DEBUG(n <= basis_.size(), "index out of range");
    return FftDomain({basis_.begin(), basis_.end() - n}, start_offset_);
  }

  std::tuple<FftDomain, FftDomain> Split(size_t n) const {
    ASSERT_DEBUG(n <= basis_.size(), "index out of range");
    return {
        FftDomain({basis_.begin(), basis_.end() - n}, GroupT::GroupUnit()),
        FftDomain({basis_.end() - n, basis_.end()}, start_offset_)};
  }

  // This iterator below iterates over all the subset sums or subset products
  // (when Multiplicative == True).
  // The subsets are ordered by their binary representation, i.e. at the k'th
  // place the iterator returns the sum of the subset selected by the binary representation
  // of k.
  //
  // The algorithm follows the recursive algorithm below:
  // def subsetsums_iter(basis, depth, offset):
  //     if depth == 0:
  //         yield offset
  //         return
  //
  //         for element in subsetsums_iter(basis, depth - 1, offset):
  //           yield element
  //
  //         for element in subsetsums_iter(basis, depth - 1, basis[depth] + offset):
  //           yield element
  //
  // When called with
  //  subsetsums_iter(basis_, len(basis_), start_offset_)
  //
  // Since yield is not supported we implement it using a stack. At the first recursive
  // call we push (depth - 1, offset) onto the stack. At the second recursive call we don't push
  // anything into the stack since its a "tail recursion".

  // To understand why it is a "tail recursion", observe that we don't need a stack to implement
  // the following iterator:
  //   def iter1():
  //     for element in iter2():
  //        yield element.
  class Iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = FieldElementT;
    using difference_type = void;
    using pointer = FieldElementT*;
    using reference = FieldElementT&;

    Iterator() : data_(FieldElementT::Uninitialized()) {}

    explicit Iterator(FieldElementT data) : data_(data) {}

    Iterator(gsl::span<const FieldElementT> basis, const FieldElementT& start_offset)
        : basis_(basis), data_(start_offset), done_(false) {
      size_t len = basis.size();
      stack_.reserve(len);
      RefillStack(len);
    }

    Iterator& operator++() {
      if (stack_.empty()) {
        done_ = true;
        return *this;
      }

      std::pair<int, FieldElementT> depth_elem_pair = PopStack();
      const int curr_depth = depth_elem_pair.first;
      const FieldElementT& curr_elem = depth_elem_pair.second;
      data_ = GroupT::GroupOperation(curr_elem, basis_[curr_depth]);
      RefillStack(curr_depth);

      return *this;
    }

    const Iterator operator++(int) {
      const Iterator tmp = Iterator(data_);
      (*this)++;
      return tmp;
    }

    bool operator==(const Iterator& rhs) const {
      ASSERT_DEBUG(done_ || rhs.done_, "one of the iterators is expected to point to the end");
      return done_ == rhs.done_;
    }

    bool operator!=(const Iterator& rhs) const { return !((*this) == rhs); }
    FieldElementT operator*() const { return data_; }

   private:
    gsl::span<const FieldElementT> basis_;
    std::vector<std::pair<int, FieldElementT>> stack_;
    FieldElementT data_;
    bool done_ = true;

    auto PopStack() {
      auto top = stack_.back();
      stack_.pop_back();
      return top;
    }

    void RefillStack(int depth) {
      while (depth > 0) {
        stack_.emplace_back(--depth, data_);
      }
    }
  };

 private:
  const std::vector<FieldElementT> basis_;
  const FieldElementT start_offset_;

  using BasisIteratorType = decltype(basis_.begin());
};

template <typename GroupT>
FftDomain<GroupT> MakeFftDomain(
    std::vector<typename GroupT::FieldElementT> basis,
    const typename GroupT::FieldElementT& start_offset) {
  return FftDomain<GroupT>(std::move(basis), start_offset);
}

template <typename FieldElementT>
FftDomain<FftMultiplicativeGroup<FieldElementT>> MakeMultiplicativeFftDomain(
    std::vector<FieldElementT> basis, const FieldElementT& start_offset) {
  return FftDomain<FftMultiplicativeGroup<FieldElementT>>(std::move(basis), start_offset);
}

/*
  Returns a multiplicative domain corresponding to a bit reversed order coset of a cyclic group of
  size 2^log_n.
  If reversed_order is true, reverse basis. true by default for historical reasons.
*/
template <typename FieldElementT, typename GroupT = FftMultiplicativeGroup<FieldElementT>>
typename std::enable_if<
    std::is_same_v<GroupT, FftMultiplicativeGroup<FieldElementT>>, FftDomain<GroupT>>::type
MakeFftDomain(
    const FieldElementT& generator, size_t log_n, const FieldElementT& start_offset,
    bool reversed_order = true) {
  std::vector<FieldElementT> basis;
  if (log_n > 0) {
    basis = GetSquares(generator, log_n);
    ASSERT_RELEASE(basis.back() == -FieldElementT::One(), "generator order is not Pow2(log_n)");
    if (reversed_order) {
      std::reverse(basis.begin(), basis.end());
    }
  }
  return MakeMultiplicativeFftDomain(std::move(basis), start_offset);
}

template <typename FieldElementT, typename GroupT = FftMultiplicativeGroup<FieldElementT>>
FftDomain<GroupT> MakeFftDomain(size_t log_n, const FieldElementT& start_offset) {
  FieldElementT generator = std::is_same_v<GroupT, FftMultiplicativeGroup<FieldElementT>>
                                ? GetSubGroupGenerator<FieldElementT>(Pow2(log_n))
                                : FieldElementT::Generator();

  return MakeFftDomain(generator, log_n, start_offset);
}

}  // namespace starkware

#endif  // STARKWARE_FFT_UTILS_FFT_DOMAIN_H_
