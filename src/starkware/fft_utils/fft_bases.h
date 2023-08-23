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

#ifndef STARKWARE_FFT_UTILS_FFT_BASES_H_
#define STARKWARE_FFT_UTILS_FFT_BASES_H_

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/algebra/fft/multiplicative_group_ordering.h"
#include "starkware/fft_utils/fft_domain.h"
#include "starkware/fft_utils/fft_group.h"

namespace starkware {

/*
  Contains information about FFT/FRI layers. For a domain of size 2^N, there are N layers.
  Layer i reduces a domain of size 2^(N-i) to a domain of size  2^(N-i-1). This means we have N+1
  domains (Layer i transform from domain i to domain i+1). The last domain is of size 1, with an
  empty basis.
*/
class FftBases {
 public:
  virtual ~FftBases() = default;

  /*
    Returns the number of layers.
  */
  virtual size_t NumLayers() const = 0;

  virtual Field GetField() const = 0;

  /*
    Same as operator[]. This is more readable when the object is given as a pointer.
  */
  virtual const FftDomainBase& At(size_t idx) const = 0;

  /*
    Returns a copy with idx layers removed from the beginning.
  */
  virtual std::unique_ptr<FftBases> FromLayerAsUniquePtr(size_t idx) const = 0;

  /*
    Returns an FftBasesImpl instance that is derived from the original FftBasesImpl
    by changing the offsets in all the layers.
  */
  virtual std::unique_ptr<FftBases> GetShiftedBasesAsUniquePtr(
      const FieldElement& offset) const = 0;

  /*
    Split to 2^n n_log_cosets. Return a pair of (bases for a smaller coset, offsets for each coset).
  */
  virtual std::tuple<std::unique_ptr<FftBases>, std::vector<FieldElement>> SplitToCosets(
      size_t n_log_cosets) const = 0;

  /*
    Applies the domain transformation of layer <layer_index>.
    For the multiplicative case, this is x^2.
  */
  virtual FieldElement ApplyBasisTransform(const FieldElement& point, size_t layer_index) const = 0;
};

template <typename BasesT, typename GroupT>
class FftBasesCommonImpl : public FftBases {
 public:
  using DomainT = FftDomain<GroupT>;
  using FieldElementT = typename GroupT::FieldElementT;

  explicit FftBasesCommonImpl(std::vector<DomainT> bases) : bases_(std::move(bases)) {
    ASSERT_RELEASE(
        !bases_.empty() && bases_.back().BasisSize() == 0, "bases must end in an empty domain");
  }

  /*
    Returns the number of layers in the instance, not including the last empty domain at the end.
  */
  size_t NumLayers() const override { return bases_.size() - 1; }

  BasesT FromLayer(size_t idx) const {
    ASSERT_RELEASE(idx < bases_.size(), "index out of range");
    return BasesT(bases_[idx]);
  }

  std::unique_ptr<FftBases> FromLayerAsUniquePtr(size_t idx) const override {
    return std::make_unique<BasesT>(FromLayer(idx));
  }

  const DomainT& operator[](size_t idx) const { return bases_.at(idx); }
  Field GetField() const override { return Field::Create<FieldElementT>(); }

  /*
    Same as operator[]. This is more readable when the object is given as a pointer.
  */
  const FftDomainBase& At(size_t idx) const override { return (*this)[idx]; }

  /*
    Returns an FftBasesImpl instance that is derived from the original FftBasesImpl
    by changing the offsets in all the layers.

    The offset at layer i is obtained from the offset at layer i-1 using a 2 to 1 mapping.

    The result is independent of the offset in the original FftBasesImpl instance.
  */
  BasesT GetShiftedBases(const FieldElementT& offset) const {
    return BasesT(bases_[0].GetShiftedDomain(offset));
  }

  std::unique_ptr<FftBases> GetShiftedBasesAsUniquePtr(const FieldElement& offset) const override {
    return std::make_unique<BasesT>(GetShiftedBases(offset.As<FieldElementT>()));
  }

  std::tuple<std::unique_ptr<FftBases>, std::vector<FieldElement>> SplitToCosets(
      size_t n_log_cosets) const override;

  FieldElement ApplyBasisTransform(const FieldElement& point, size_t layer_index) const override {
    ASSERT_RELEASE(layer_index < this->NumLayers(), "Layer index out of range.");
    return FieldElement(
        AsDerived().ApplyBasisTransformTmpl(point.As<FieldElementT>(), layer_index));
  };

 protected:
  FftBasesCommonImpl() = default;
  std::vector<DomainT> bases_;

 private:
  const BasesT& AsDerived() const { return static_cast<const BasesT&>(*this); }
  BasesT& AsDerived() { return static_cast<BasesT&>(*this); }
};

/*
  Creates a series of FFT domains.
  The i'th domain is start_offset^i*<g^i>.

  The domains are ordered according to the Order paramater.
*/
template <
    typename FieldElementT,
    MultiplicativeGroupOrdering Order = MultiplicativeGroupOrdering::kBitReversedOrder>
class MultiplicativeFftBases
    : public FftBasesCommonImpl<
          MultiplicativeFftBases<FieldElementT, Order>, FftMultiplicativeGroup<FieldElementT>> {
  friend class FftBasesCommonImpl<
      MultiplicativeFftBases<FieldElementT, Order>, FftMultiplicativeGroup<FieldElementT>>;

 public:
  using GroupT = FftMultiplicativeGroup<FieldElementT>;
  using DomainT = FftDomain<GroupT>;
  static const MultiplicativeGroupOrdering kOrder = Order;

  MultiplicativeFftBases(
      const FieldElementT& generator, size_t log_n, const FieldElementT& start_offset)
      : MultiplicativeFftBases(MakeFftDomain<FieldElementT, GroupT>(
            generator, log_n, start_offset,
            Order == MultiplicativeGroupOrdering::kBitReversedOrder)) {}

  MultiplicativeFftBases(size_t log_n, const FieldElementT& start_offset)
      : MultiplicativeFftBases(
            GetSubGroupGenerator<FieldElementT>(Pow2(log_n)), log_n, start_offset) {}

  bool IsNaturalOrder() const { return Order == MultiplicativeGroupOrdering::kNaturalOrder; }

  static bool IsNaturalOrder(DomainT domain) {
    return domain.Basis().empty() || domain.Basis().front() != -FieldElementT::One();
  }

  /*
    Assume n < domain.BasisSize()
    Every domain can be split to a smaller domain that can have FftBases, and the complement domain
    that generated the offsets (cannot have FftBases).
    Note that returned offsets domain is not a coset - cannot be used to make FftBases.
  */
  static std::tuple<MultiplicativeFftBases<FieldElementT, Order>, DomainT> SplitDomain(
      const DomainT& domain, size_t n) {
    ASSERT_RELEASE(n <= domain.BasisSize(), "Domain not big enough.");
    auto basis = domain.Basis();
    auto offset = domain.StartOffset();
    auto coset_basis = !IsNaturalOrder(domain)
                           ? std::vector<FieldElementT>{basis.begin(), basis.end() - n}
                           : std::vector<FieldElementT>{basis.begin() + n, basis.end()};
    auto offsets_basis = !IsNaturalOrder(domain)
                             ? std::vector<FieldElementT>{basis.end() - n, basis.end()}
                             : std::vector<FieldElementT>{basis.begin(), basis.begin() + n};
    return {
        MultiplicativeFftBases<FieldElementT, Order>(DomainT(std::move(coset_basis))),
        DomainT(std::move(offsets_basis), offset)};
  }

  FieldElementT ApplyBasisTransformTmpl(const FieldElementT& point, size_t /*layer_index*/) const {
    return point * point;
  }

 private:
  /*
    Private so no one will give us a bad domain.
  */
  explicit MultiplicativeFftBases(DomainT domain);

  explicit MultiplicativeFftBases(std::vector<DomainT> bases)
      : FftBasesCommonImpl<
            MultiplicativeFftBases<FieldElementT, Order>, FftMultiplicativeGroup<FieldElementT>>(
            bases) {}
};

namespace fft_bases {
template <typename GroupT>
class FftBasesDefaultImplHelper {
  static_assert(
      sizeof(GroupT) == 0, "FftDomainImpl is not implemented for the given FieldElementT");
};

template <typename FieldElementT>
class FftBasesDefaultImplHelper<FftMultiplicativeGroup<FieldElementT>> {
 public:
  using T = MultiplicativeFftBases<FieldElementT, MultiplicativeGroupOrdering::kBitReversedOrder>;
};
}  // namespace fft_bases

template <typename GroupT>
using FftBasesImpl = typename fft_bases::FftBasesDefaultImplHelper<GroupT>::T;

template <typename FieldElementT>
using FftBasesDefaultImpl = FftBasesImpl<FftMultiplicativeGroup<FieldElementT>>;

/*
  Calculates the vanishing polynomial x*(x+basis_element).

  This polynomial is useful in characteristic 2 fields where {0, basis_element} ==
  span(basis_element).
*/
template <typename FieldElementT>
FieldElementT ApplyDimOneVanishingPolynomial(
    const FieldElementT& point, const FieldElementT& basis_element);

/*
  Returns multiplicative FftBases.
*/
template <
    MultiplicativeGroupOrdering Order = MultiplicativeGroupOrdering::kBitReversedOrder,
    typename FieldElementT>
auto MakeFftBases(const FieldElementT& generator, size_t log_n, const FieldElementT& start_offset) {
  return MultiplicativeFftBases<FieldElementT, Order>(generator, log_n, start_offset);
}

/*
  Returns multiplicative FftBases.
*/
template <
    MultiplicativeGroupOrdering Order = MultiplicativeGroupOrdering::kBitReversedOrder,
    typename FieldElementT>
auto MakeFftBases(
    size_t log_n,
    const FieldElementT& start_offset = FftMultiplicativeGroup<FieldElementT>::GroupUnit()) {
  return MakeFftBases<Order>(GetSubGroupGenerator<FieldElementT>(Pow2(log_n)), log_n, start_offset);
}

/*
  Invoke func(templatic_bases) where templatic_bases is the underlying templatic version of bases.
  This is similar to InvokeFieldTemplateVersion(), only for fft_bases instead of field.
  Note: func() will be compiled only for BasesT types for which IsSupportFftBases<BasesT>::value ==
  true.

  Example usage:
    FftBases& bases_orig = ...;
    InvokeBasesTemplateVersion(
        [&](const auto& bases) {
          using BasesT = std::remove_const_t<std::remove_reference_t<decltype(bases)>>;
          ...
        },
        bases_orig);
*/
template <typename Func>
auto InvokeBasesTemplateVersion(const Func& func, const FftBases& bases);

}  // namespace starkware

#include "starkware/fft_utils/fft_bases.inl"

#endif  // STARKWARE_FFT_UTILS_FFT_BASES_H_
