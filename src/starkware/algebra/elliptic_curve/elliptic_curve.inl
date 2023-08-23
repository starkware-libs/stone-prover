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

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/fraction_field_element.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

template <typename FieldElementT>
auto EcPoint<FieldElementT>::Double(const FieldElementT& alpha, FieldElementT* slope) const
    -> EcPoint {
  // Doubling a point cannot be done by adding the point to itself with the function AddPoints
  // because this function assumes that it gets distinct points. Usually, in order to sum two
  // points, one should draw a straight line containing these points, find the third point in the
  // intersection of the line and the curve, and then negate the y coordinate. In the special case
  // where the two points are the same point, one should draw the line that intersects the elliptic
  // curve "twice" at that point. This means that the slope of the line should be equal to the slope
  // of the curve at this point. That is, the derivative of the function
  // y = sqrt(x^3 + alpha * x + beta), which is slope = dy/dx = (3 * x^2 + alpha)/(2 * y). Note that
  // if y = 0 then the point is a 2-torsion (doubling it gives infinity). The line is then given by
  // y = slope * x + y_intercept. The third intersection point is found using the equation that is
  // true for all cases: slope^2 = x_1 + x_2 + x_3 (where x_1, x_2 and x_3 are the x coordinates of
  // three points in the intersection of the curve with a line).
  const FieldElementT tangent_slope = GetTangentSlope(alpha);
  const FieldElementT x2 = Pow(tangent_slope, 2) - Times(2, x);
  const FieldElementT y2 = tangent_slope * (x - x2) - y;
  if (slope) {
    *slope = tangent_slope;
  }

  return {x2, y2};
}

template <typename FieldElementT>
auto EcPoint<FieldElementT>::operator+(const EcPoint& rhs) const -> EcPoint {
  return AddPointsGivenSlope(*this, rhs, GetSlope(*this, rhs));
}

template <typename FieldElementT>
bool EcPoint<FieldElementT>::IsOnCurve(
    const FieldElementT& alpha, const FieldElementT& beta) const {
  return Pow(y, 2) == Pow(x, 3) + alpha * x + beta;
}

template <typename FieldElementT>
auto EcPoint<FieldElementT>::GetPointFromX(
    const FieldElementT& x, const FieldElementT& alpha, const FieldElementT& beta)
    -> std::optional<EcPoint> {
  const FieldElementT y_squared = Pow(x, 3) + alpha * x + beta;
  if (!IsSquare(y_squared)) {
    return std::nullopt;
  }
  return {{x, Sqrt(y_squared)}};
}

template <typename FieldElementT>
auto EcPoint<FieldElementT>::Random(
    const FieldElementT& alpha, const FieldElementT& beta, Prng* prng) -> EcPoint {
  // Each iteration has probability of ~1/2 to fail. Thus the probability of failing 100 iterations
  // is negligible.
  for (size_t i = 0; i < 100; ++i) {
    const FieldElementT x = FieldElementT::RandomElement(prng);
    const std::optional<EcPoint> pt = GetPointFromX(x, alpha, beta);
    if (pt.has_value()) {
      return *pt;
    }
  }
  ASSERT_RELEASE(false, "ERROR: No random point found: Unlucky?");
}

template <typename FieldElementT>
auto EcPoint<FieldElementT>::RandomVector(
    const FieldElementT& alpha, const FieldElementT& beta, const size_t size, Prng* prng)
    -> std::vector<EcPoint> {
  std::vector<EcPoint> result;
  result.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    result.push_back(Random(alpha, beta, prng));
  }
  return result;
}

template <typename FieldElementT>
auto EcPoint<FieldElementT>::FromString(const std::string& input_string) -> EcPoint {
  const size_t comma_position = input_string.find(',');
  ASSERT_RELEASE(comma_position != std::string::npos, "Expecting comma delimited pair");
  FieldElementT ec_point_x = FieldElementT::FromString(input_string.substr(0, comma_position));
  FieldElementT ec_point_y = FieldElementT::FromString(input_string.substr(comma_position + 1));
  return {ec_point_x, ec_point_y};
}

template <typename FieldElementT>
std::string EcPoint<FieldElementT>::ToString() const {
  std::ostringstream ecpoint_str;
  ecpoint_str << x << "," << y;
  return ecpoint_str.str();
}

template <typename FieldElementT>
auto EcPoint<FieldElementT>::FromBytes(gsl::span<std::byte> bytes, bool use_big_endian) -> EcPoint {
  ASSERT_RELEASE(
      bytes.size() == 2 * FieldElementT::SizeInBytes(),
      "Expected output bytes span of size " + std::to_string(2 * FieldElementT::SizeInBytes()) +
          " but got " + std::to_string(bytes.size()));
  return EcPoint<FieldElementT>(
      FieldElementT::FromBytes(bytes.subspan(0, FieldElementT::SizeInBytes()), use_big_endian),
      FieldElementT::FromBytes(
          bytes.subspan(FieldElementT::SizeInBytes(), FieldElementT::SizeInBytes()),
          use_big_endian));
}

template <typename FieldElementT>
template <typename OtherFieldElementT>
EcPoint<OtherFieldElementT> EcPoint<FieldElementT>::ConvertTo() const {
  return EcPoint<OtherFieldElementT>(OtherFieldElementT(x), OtherFieldElementT(y));
}

template <typename FieldElementT>
void EcPoint<FieldElementT>::ToBytes(gsl::span<std::byte> span_out, bool use_big_endian) const {
  ASSERT_RELEASE(
      span_out.size() == 2 * FieldElementT::SizeInBytes(),
      "Expected output bytes span of size " + std::to_string(2 * FieldElementT::SizeInBytes()) +
          " but got " + std::to_string(span_out.size()));
  x.ToBytes(span_out.subspan(0, FieldElementT::SizeInBytes()), use_big_endian);
  y.ToBytes(
      span_out.subspan(FieldElementT::SizeInBytes(), FieldElementT::SizeInBytes()), use_big_endian);
}

template <typename FieldElementT>
std::pair<std::vector<FieldElementT>, std::vector<FieldElementT>>
EcPoint<FieldElementT>::ToCoordinatesAndExpand(
    gsl::span<const EcPoint> points, const std::optional<size_t>& length) {
  size_t size = length.value_or(points.size());
  ASSERT_RELEASE(size >= points.size(), "Too many points were given.");
  std::vector<FieldElementT> x_values;
  std::vector<FieldElementT> y_values;
  x_values.reserve(size);
  y_values.reserve(size);
  for (const EcPoint& point : points) {
    x_values.push_back(point.x);
    y_values.push_back(point.y);
  }
  for (size_t i = points.size(); i < size; ++i) {
    x_values.push_back(x_values.back());
    y_values.push_back(y_values.back());
  }
  return {std::move(x_values), std::move(y_values)};
}

template <typename FieldElementT>
FieldElementT EcPoint<FieldElementT>::GetTangentSlope(const FieldElementT& alpha) const {
  // For detailed documentation see EcPoint<FieldElementT>::Double.
  ASSERT_RELEASE(y != FieldElementT::Zero(), "Tangent slope of 2 torsion point is infinite.");
  return (Times(3, Pow(x, 2)) + alpha) / Times(2, y);
}

template <typename FieldElementT>
FieldElementT GetSlope(const EcPoint<FieldElementT>& p1, const EcPoint<FieldElementT>& p2) {
  ASSERT_RELEASE(p1.x != p2.x, "x values should be different for arbitrary points");
  return (p1.y - p2.y) / (p1.x - p2.x);
}

template <typename FieldElementT>
EcPoint<FieldElementT> AddPointsGivenSlope(
    const EcPoint<FieldElementT>& p1, const EcPoint<FieldElementT>& p2,
    const FieldElementT& slope) {
  // In order to sum two points, one should draw a straight line containing these points, find the
  // third point in the intersection of the line and the curve, and then negate the y coordinate.
  // Notice that if x_1 = x_2 then either they are the same point or their sum is infinity. This
  // function doesn't deal with these cases. The straight line is given by the equation:
  // y = slope * x + y_intercept. The x coordinate of the third point is found by solving the system
  // of equations:
  //
  // y = slope * x + y_intercept.
  // y^2 = x^3 + alpha * x + beta.
  //
  // These equations yield:
  // (slope * x + y_intercept)^2 = x^3 + alpha * x + beta
  // ==> x^3 - slope^2 * x^2 + (alpha  - 2 * slope * y_intercept) * x + (beta - y_intercept^2) = 0.
  //
  // This is a monic polynomial in x whose roots are exactly the x coordinates of the three
  // intersection points of the line with the curve. Thus it is equal to the polynomial:
  // (x - x_1) * (x - x_2) * (x - x_3)
  // where x1,x2,x3 are the x coordinates of those points.
  // Notice that the equality of the coefficient of the x^2 term yields:
  // slope^2 = x_1 + x_2 + x_3.
  FieldElementT x3 = Pow(slope, 2) - p1.x - p2.x;
  FieldElementT y3 = slope * (p1.x - x3) - p1.y;
  return {x3, y3};
}

template <typename FieldElementT>
std::pair<FieldElementT, FieldElementT> GetGeneralizedSlopes(
    const EcPoint<FieldElementT>& p0, const EcPoint<FieldElementT>& p1,
    const EcPoint<FieldElementT>& p2) {
  const FieldElementT delta_x1 = p0.x - p1.x;
  const FieldElementT delta_y1 = p0.y - p1.y;
  const FieldElementT delta_x_squared1 = (p0.x * p0.x) - (p1.x * p1.x);
  const FieldElementT delta_x2 = p0.x - p2.x;
  const FieldElementT delta_y2 = p0.y - p2.y;
  const FieldElementT delta_x_squared2 = (p0.x * p0.x) - (p2.x * p2.x);
  const FieldElementT det = ((delta_x1 * delta_y2) - (delta_x2 * delta_y1));
  ASSERT_RELEASE(det != FieldElementT::Zero(), "The points are collinear.");
  const FieldElementT det_inverse = det.Inverse();
  const FieldElementT s2 =
      ((delta_y1 * delta_x_squared2) - (delta_y2 * delta_x_squared1)) * det_inverse;
  const FieldElementT s3 =
      ((delta_x2 * delta_x_squared1) - (delta_x1 * delta_x_squared2)) * det_inverse;
  return {s2, s3};
}

template <typename FieldElementT>
std::vector<EcPoint<FieldElementT>> TwosPowersOfPoint(
    const EcPoint<FieldElementT>& base, const FieldElementT& alpha, size_t num_points,
    const std::optional<gsl::span<FieldElementT>>& slopes, bool allow_more_points) {
  using FractionFieldElementT = FractionFieldElement<FieldElementT>;

  // To avoid repeated inverse computations we use a fraction field optimization.
  // We compute the point doubling (and slopes if needed) in the fraction field, and then convert
  // back to the original field using BatchToBaseFieldElement().

  LOG_IF(ERROR, num_points > FieldElementT::FieldSize().Log2Floor() && !allow_more_points)
      << "It is insecure to request " << num_points << " points which is more than "
      << std::to_string(FieldElementT::FieldSize().Log2Floor()) << " powers-of-two points.";
  ASSERT_RELEASE(num_points > 0, "No points requested.");
  if (slopes.has_value()) {
    ASSERT_RELEASE(slopes->size() == num_points - 1, "Incorrect number of slopes requested.");
  }

  // Initialize fraction field values.
  FractionFieldElementT ff_alpha = FractionFieldElementT(alpha);
  EcPoint<FractionFieldElementT> point =
      EcPoint<FractionFieldElementT>(FractionFieldElementT(base.x), FractionFieldElementT(base.y));

  // Initialize fraction field columns.
  // A column for x, y, and possibly slope if we need to compute it.
  const size_t n_cols_ff = 2;
  const size_t col_slope = 2;
  std::vector<std::vector<FractionFieldElementT>> ff_cols(
      n_cols_ff + (slopes.has_value() ? 1 : 0),
      std::vector<FractionFieldElementT>(num_points, FractionFieldElementT::Uninitialized()));
  auto& ff_cols_x = ff_cols[0];
  auto& ff_cols_y = ff_cols[1];
  ff_cols_x[0] = point.x;
  ff_cols_y[0] = point.y;

  // Make sure extra element is invertible.
  if (slopes.has_value()) {
    ff_cols[2][0] = FractionFieldElementT::One();
  }

  // Repeated doubling in fraction field.
  for (size_t i = 0; i < num_points - 1; ++i) {
    ASSERT_RELEASE(
        point.y != FractionFieldElementT::Zero(), "Base is of order 2^" + std::to_string(i + 1));
    if (slopes.has_value()) {
      point = point.Double(ff_alpha, &ff_cols[col_slope][i + 1]);
    } else {
      point = point.Double(ff_alpha, nullptr);
    }
    ff_cols_x[i + 1] = point.x;
    ff_cols_y[i + 1] = point.y;
  }

  // Convert back to original field.
  std::vector<std::vector<FieldElementT>> cols(
      n_cols_ff + (slopes.has_value() ? 1 : 0),
      std::vector<FieldElementT>(num_points, FieldElementT::Uninitialized()));

  FractionFieldElementT::BatchToBaseFieldElement(
      ConstSpanAdapter(std::move(ff_cols)), SpanAdapter(cols));

  // Build EC points to return.
  std::vector<EcPoint<FieldElementT>> points_ret;
  points_ret.reserve(num_points);
  for (size_t i = 0; i < num_points; ++i) {
    points_ret.emplace_back(cols[0][i], cols[1][i]);
  }

  // Write back slopes to if needed.
  if (slopes.has_value()) {
    for (size_t i = 0; i < num_points - 1; ++i) {
      (*slopes)[i] = cols[col_slope][i + 1];
    }
  }

  return points_ret;
}

template <typename FieldElementT>
bool IsValidCurve(const FieldElementT& alpha, const FieldElementT& beta) {
  return Times(4, Pow(alpha, 3)) != -Times(27, Pow(beta, 2));
}

template <typename FieldElementT>
template <size_t N>
EcPoint<FieldElementT> EcPoint<FieldElementT>::MultiplyByScalar(
    const BigInt<N>& scalar, const FieldElementT& alpha) const {
  std::optional<EcPoint<FieldElementT>> res;
  EcPoint<FieldElementT> power = *this;
  for (const auto& b : scalar.ToBoolVector()) {
    if (b) {
      res = power.AddOptionalPoint(res, alpha);
    }
    // if power == -power, then power + power == zero, and will remain zero (so res will not change)
    // until the end of the for loop. Therefore there is no point to keep looping.
    if (power == -power) {
      break;
    }
    power = power.Double(alpha);
  }
  ASSERT_RELEASE(res.has_value(), "Result of multiplication is the curve's zero element.");
  return *res;
}

template <typename FieldElementT>
std::optional<EcPoint<FieldElementT>> EcPoint<FieldElementT>::AddOptionalPoint(
    const std::optional<EcPoint<FieldElementT>>& point, const FieldElementT& alpha) const {
  if (!point) {
    return *this;
  }
  // If a == -b, then a+b == zero element.
  if (*point == -*this) {
    return std::nullopt;
  }
  if (*point == *this) {
    return point->Double(alpha);
  }
  return *point + *this;
}

}  // namespace starkware
