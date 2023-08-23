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

#include "starkware/fri/fri_folder.h"

#include <algorithm>
#include <vector>

#include "third_party/cppitertools/range.hpp"

#include "starkware/algebra/utils/invoke_template_version.h"
#include "starkware/utils/task_manager.h"

namespace starkware {
namespace fri {
namespace details {

namespace {

template <typename FieldElementT>
class MultiplicativeFriFolder : public FriFolderBase {
 public:
  FieldElementVector ComputeNextFriLayer(
      const FftDomainBase& domain, const ConstFieldElementSpan& values,
      const FieldElement& eval_point) const override {
    FieldElementVector output_layer =
        FieldElementVector::MakeUninitialized(eval_point.GetField(), values.Size() / 2);
    ComputeNextFriLayer(domain, values, eval_point, output_layer);
    return output_layer;
  }

  void ComputeNextFriLayer(
      const FftDomainBase& domain, const ConstFieldElementSpan& values,
      const FieldElement& eval_point, const FieldElementSpan& output_layer) const override {
    const auto* domain_tmpl =
        dynamic_cast<const FftDomain<FftMultiplicativeGroup<FieldElementT>>*>(&domain);
    ASSERT_RELEASE(
        domain_tmpl != nullptr,
        "The underlying type of domain is wrong. It should be FftDomain<T, true>");

    const auto& vec_tmpl = values.As<FieldElementT>();
    const auto& out_tmpl = output_layer.As<FieldElementT>();
    const auto& eval_point_tmpl = eval_point.As<FieldElementT>();
    return ComputeNextFriLayerImpl(*domain_tmpl, vec_tmpl, eval_point_tmpl, out_tmpl);
  }

  static void ComputeNextFriLayerImpl(
      const FftDomain<FftMultiplicativeGroup<FieldElementT>>& domain,
      const gsl::span<const FieldElementT>& input_layer, const FieldElementT& eval_point,
      const gsl::span<FieldElementT>& output_layer, size_t min_log_n_fri_task_size = 12) {
    ASSERT_RELEASE(input_layer.size() == domain.Size(), "vector size does not match domain size");
    ASSERT_RELEASE(
        output_layer.size() == input_layer.size() / 2,
        "Output layer size must be half than the original");

    // make sure we create tasks that are no shorter than a minimum size unless the domain
    // size is already the size of the minimum task size or smaller, in which case we won't split
    // it.
    const size_t log_n_fri_tasks =
        static_cast<size_t>(std::max<int64_t>(domain.BasisSize() - min_log_n_fri_task_size, 0));

    // Split the domain and parallelize the calculation over multiple offsets after
    // removing the first basis element from domain to iterate over even indices.
    const auto& [inner_domain, outer_domain] =
        domain.RemoveFirstBasisElements(1).Inverse().Split(log_n_fri_tasks);

    // Rather than multiplying each point in the domain by the eval point we shift the entire domain
    // by the eval point. This is more efficient due to the succint representation of the domain.
    // Note that this can be done since the inner domain resulting from a Split() has an offset of
    // the group unit.
    auto shifted_inner_domain = inner_domain.GetShiftedDomain(eval_point);

    std::vector<FieldElementT> outer_vec(outer_domain.begin(), outer_domain.end());
    std::vector<FieldElementT> inner_vec(shifted_inner_domain.begin(), shifted_inner_domain.end());
    TaskManager& task_manager = TaskManager::GetInstance();
    size_t task_size = inner_vec.size();

    task_manager.ParallelFor(
        outer_vec.size(), [&task_size, &inner_vec, &input_layer, &output_layer,
                           &outer_vec](const TaskInfo& task_info) {
          size_t i = task_info.start_idx * task_size * 2;
          FieldElementT& outer = outer_vec[task_info.start_idx];
          for (const FieldElementT& inner : inner_vec) {
            const FieldElementT& f_x = input_layer[i];
            const FieldElementT& f_minus_x = input_layer[i + 1];
            // (outer * inner) == (x_inv * eval_point) so we can use the standard Fold() function.
            output_layer[i / 2] = Fold(f_x, f_minus_x, outer, inner);
            i += 2;
          }
        });
  }

  FieldElement NextLayerElementFromTwoPreviousLayerElements(
      const FieldElement& f_x, const FieldElement& f_minus_x, const FieldElement& eval_point,
      const FieldElement& x) const override {
    return FieldElement(Fold(
        f_x.As<FieldElementT>(), f_minus_x.As<FieldElementT>(), eval_point.As<FieldElementT>(),
        x.As<FieldElementT>().Inverse()));
  }

  /*
    Multiplicative case folding formula:
      f(x)  = g(x^2) + xh(x^2)
      f(-x) = g((-x)^2) - xh((-x)^2) = g(x^2) - xh(x^2)
      =>
      2g(x^2) = f(x) + f(-x)
      2h(x^2) = (f(x) - f(-x))/x
      =>
      2g(x^2) + 2ah(x^2) = f(x) + f(-x) + a(f(x) - f(-x))/x.
  */
  static FieldElementT Fold(
      const FieldElementT& f_x, const FieldElementT& f_minus_x, const FieldElementT& eval_point,
      const FieldElementT& x_inv) {
    FieldElementT outcome = f_x + f_minus_x + eval_point * (f_x - f_minus_x) * x_inv;
    return outcome;
  }
};

}  // namespace

std::unique_ptr<FriFolderBase> FriFolderFromField(const Field& field) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) -> std::unique_ptr<FriFolderBase> {
        using FieldElementT = typename decltype(field_tag)::type;
        return std::make_unique<MultiplicativeFriFolder<FieldElementT>>();
      },
      field);
}

}  // namespace details
}  // namespace fri
}  // namespace starkware
