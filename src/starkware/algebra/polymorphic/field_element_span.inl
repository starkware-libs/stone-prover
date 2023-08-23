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

#include "starkware/algebra/polymorphic/field_element_vector.h"

namespace starkware {

template <bool IsConst, typename Subclass>
class FieldElementSpanImpl<IsConst, Subclass>::WrapperBase {
 public:
  virtual ~WrapperBase() = default;

  virtual size_t Size() const = 0;
  virtual FieldElement operator[](size_t index) const = 0;
  virtual void Set(size_t index, const FieldElement& elt) const = 0;
  virtual void CopyDataFrom(const ConstFieldElementSpan& other) const = 0;
  virtual Field GetField() const = 0;
  virtual bool IsEqual(const Subclass& other) const = 0;
  virtual std::unique_ptr<FieldElementSpanImpl::WrapperBase> Clone() const = 0;

  virtual Subclass SubSpan(size_t offset) const = 0;
  virtual Subclass SubSpan(size_t offset, size_t count) const = 0;
};

namespace field_element_span {
namespace details {
template <typename FieldElementT>
void AssignOrAssert(const FieldElementT& val, FieldElementT* dst) {
  *dst = val;
}

template <typename FieldElementT>
void AssignOrAssert(const FieldElementT& /*val*/, const FieldElementT* /*dst*/) {
  ASSERT_RELEASE(false, "Set function is disabled for const destination");
}
}  // namespace details
}  // namespace field_element_span

template <bool IsConst, typename Subclass>
template <typename FieldElementT>
class FieldElementSpanImpl<IsConst, Subclass>::Wrapper : public WrapperBase {
 public:
  explicit Wrapper(gsl::span<FieldElementT> value) : value_(value) {}

  size_t Size() const override { return value_.size(); }

  FieldElement operator[](size_t index) const override { return FieldElement(value_[index]); }

  void Set(size_t index, const FieldElement& value) const override {
    field_element_span::details::AssignOrAssert(value.As<FieldElementT>(), &value_[index]);
  }

  void CopyDataFrom(const ConstFieldElementSpan& other) const override {
    if constexpr (IsConst) {  // NOLINT
      ASSERT_RELEASE(false, "Cannot copy to const");
    } else {  // NOLINT: Suppress readability-else-after-return after if constexpr.
      ASSERT_RELEASE(Size() == other.Size(), "Cannot copy from different size");
      const gsl::span<const FieldElementT> other_span = other.template As<FieldElementT>();
      std::copy(other_span.begin(), other_span.end(), value_.begin());
    }
  }

  Field GetField() const override {
    return Field::Create<typename std::remove_cv<FieldElementT>::type>();
  }

  gsl::span<FieldElementT> Value() const { return value_; }

  bool IsEqual(const Subclass& other) const override {
    return value_ == other.template As<FieldElementT>();
  }

  std::unique_ptr<FieldElementSpanImpl::WrapperBase> Clone() const override {
    return std::make_unique<Wrapper>(value_);
  }

  Subclass SubSpan(size_t offset) const override { return Subclass(value_.subspan(offset)); }

  Subclass SubSpan(size_t offset, size_t count) const override {
    return Subclass(value_.subspan(offset, count));
  }

 private:
  gsl::span<FieldElementT> value_;
};

template <typename FieldElementT>
FieldElementSpan::FieldElementSpan(gsl::span<FieldElementT> value)
    : FieldElementSpanImpl(std::make_unique<Wrapper<FieldElementT>>(value)) {
  static_assert(
      !std::is_const<FieldElementT>::value,
      "Cannot initialize FieldElementSpan with a const span.");
}

template <typename FieldElementT>
ConstFieldElementSpan::ConstFieldElementSpan(gsl::span<const FieldElementT> value)
    : FieldElementSpanImpl(std::make_unique<Wrapper<const FieldElementT>>(value)) {}

template <typename FieldElementT>
gsl::span<FieldElementT> FieldElementSpan::As() const {
  auto* ptr = dynamic_cast<const Wrapper<FieldElementT>*>(wrapper_.get());
  ASSERT_RELEASE(ptr != nullptr, "The underlying type of FieldElementSpanImpl is wrong");

  return ptr->Value();
}

template <typename FieldElementT>
gsl::span<const FieldElementT> ConstFieldElementSpan::As() const {
  auto* ptr = dynamic_cast<const Wrapper<const FieldElementT>*>(wrapper_.get());
  ASSERT_RELEASE(ptr != nullptr, "The underlying type of FieldElementSpanImpl is wrong");

  return ptr->Value();
}

}  // namespace starkware
