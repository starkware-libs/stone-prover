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

namespace starkware {

template <typename FieldElementT>
auto EcdsaComponent<FieldElementT>::Input::FromPartialPublicKey(
    const FieldElementT& public_key_x, const FieldElementT& z, const FieldElementT& r,
    const FieldElementT& w, const Config& config) -> Input {
  const auto& public_key = EcPointT::GetPointFromX(public_key_x, config.alpha, config.beta);
  ASSERT_RELEASE(
      public_key.has_value(), "Given public key (" + public_key_x.ToString() +
                                  ") does not correspond to a valid point on the elliptic curve.");

  // There are two points on the elliptic curve with the given public_key_x. GetPointFromX() returns
  // one of them randomly. Check which of the two points yields a valid signature.
  Input input{*public_key, z, r, w};
  if (Verify(config, input)) {
    return input;
  }

  // Try with -public_key.
  input = {-*public_key, z, r, w};
  ASSERT_RELEASE(Verify(config, input), "Invalid signature.");
  return input;
}

template <typename FieldElementT>
auto EcdsaComponent<FieldElementT>::WriteKeyExponentiationTrace(
    const EcPointT& base_point, const FieldElementT& exponent, uint64_t key_exponentiation_index,
    gsl::span<const gsl::span<FieldElementT>> trace) const -> EcPointT {
  std::vector<FieldElementT> slopes = FieldElementT::UninitializedVector(height_ - 1);
  const auto& key_points = TwosPowersOfPoint(
      base_point, alpha_, height_, std::make_optional(gsl::make_span(slopes)), true);
  const auto& [key_points_x, key_points_y] = EcPointT::ToCoordinatesAndExpand(key_points);
  for (size_t i = 0; i < height_; i++) {
    key_points_x_.SetCell(trace, key_exponentiation_index * height_ + i, key_points_x[i]);
    key_points_y_.SetCell(trace, key_exponentiation_index * height_ + i, key_points_y[i]);
    if (i < height_ - 1) {
      doubling_slope_.SetCell(trace, key_exponentiation_index * height_ + i, slopes[i]);
    }
  }
  r_w_inv_.SetCell(trace, key_exponentiation_index, exponent.Inverse());
  return exponentiate_key_.WriteTrace(
      shift_point_, key_points, exponent, key_exponentiation_index, trace);
}

template <typename FieldElementT>
void EcdsaComponent<FieldElementT>::WriteTrace(
    const Input& input, uint64_t component_index, gsl::span<const gsl::span<FieldElementT>> trace,
    bool check_validity) const {
  // The zG, rQ and wB that appear in the python code are named here z_g, r_q and w_b because of
  // naming conventions.
  const EcPointT z_g = exponentiate_gen_.WriteTrace(
      -shift_point_, generator_points_, input.z, component_index, trace);
  z_inv_.SetCell(trace, component_index, input.z.Inverse());

  const EcPointT r_q =
      WriteKeyExponentiationTrace(input.public_key, input.r, component_index * 2, trace);
  add_results_inv_.SetCell(trace, component_index, (z_g.x - r_q.x).Inverse());
  add_results_slope_.SetCell(trace, component_index, GetSlope(z_g, r_q));
  const EcPointT w_b =
      WriteKeyExponentiationTrace(z_g + r_q, input.w, component_index * 2 + 1, trace);
  extract_r_inv_.SetCell(trace, component_index, (w_b.x - shift_point_.x).Inverse());
  extract_r_slope_.SetCell(trace, component_index, GetSlope(w_b, -shift_point_));

  // Q.x squared.
  q_x_squared_.SetCell(trace, component_index, input.public_key.x * input.public_key.x);

  if (check_validity) {
    ASSERT_RELEASE((w_b - shift_point_).x == input.r, "Invalid signature.");
  }
}

template <typename FieldElementT>
bool EcdsaComponent<FieldElementT>::Verify(const Config& config, const Input& input) {
  ASSERT_RELEASE(input.z != FieldElementT::Zero(), "Message cannot be zero.");
  const EcPointT z_g =
      config.generator_point.MultiplyByScalar(input.z.ToStandardForm(), config.alpha);
  const EcPointT r_q = input.public_key.MultiplyByScalar(input.r.ToStandardForm(), config.alpha);
  const EcPointT w_b = (z_g + r_q).MultiplyByScalar(input.w.ToStandardForm(), config.alpha);
  return w_b.x == input.r;
}

template <typename FieldElementT>
std::pair<FieldElementT, FieldElementT> EcdsaComponent<FieldElementT>::Sign(
    const Config& config, const typename FieldElementT::ValueType& private_key,
    const typename FieldElementT::ValueType& message, Prng* prng) {
  using ValueType = typename FieldElementT::ValueType;

  const ValueType& curve_order = config.curve_order;
  ASSERT_RELEASE(
      curve_order.NumLeadingZeros() > 0, "We require at least one leading zero in the modulus");
  ASSERT_RELEASE(message < curve_order, "message must be less than curve order");
  ASSERT_RELEASE(private_key < curve_order, "private_key must be less than curve order");

  for (;;) {
    ValueType k = prng->UniformBigInt<ValueType>(ValueType::One(), curve_order - ValueType::One());
    const FieldElementT x = config.generator_point.MultiplyByScalar(k, config.alpha).x;
    const ValueType r = x.ToStandardForm();
    if (r >= curve_order || r == ValueType::Zero()) {
      continue;
    }

    const ValueType k_inv = ValueType::Inverse(k, curve_order);
    ValueType s = ValueType::MulMod(r, private_key, curve_order);
    s = ValueType::AddMod(s, message, curve_order);
    s = ValueType::MulMod(s, k_inv, curve_order);
    if (s == ValueType::Zero()) {
      continue;
    }

    const ValueType w = ValueType::Inverse(s, curve_order);
    if (w >= FieldElementT::FieldSize()) {
      continue;
    }
    const FieldElementT w_el = FieldElementT::FromBigInt(w);
    return {x, w_el};
  }
}

}  // namespace starkware
