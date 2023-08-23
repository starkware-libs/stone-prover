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

#ifndef STARKWARE_ALGEBRA_ELLIPTIC_CURVE_ELLIPTIC_CURVE_H_
#define STARKWARE_ALGEBRA_ELLIPTIC_CURVE_ELLIPTIC_CURVE_H_

#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/big_int.h"
#include "starkware/randomness/prng.h"

namespace starkware {

using std::size_t;

/*
  Represents a point on an elliptic curve of the form: y^2 = x^3 + alpha*x + beta.
*/
template <typename FieldElementT>
class EcPoint {
 public:
  constexpr EcPoint(const FieldElementT& x, const FieldElementT& y) : x(x), y(y) {}

  bool operator==(const EcPoint& rhs) const { return x == rhs.x && y == rhs.y; }
  bool operator!=(const EcPoint& rhs) const { return !(*this == rhs); }

  /*
    Computes the point added to itself.
    Optionally returns the tangent slope to the curve at the point.
  */
  EcPoint Double(const FieldElementT& alpha, FieldElementT* slope = nullptr) const;

  /*
    Returns the sum of two points. The added point must be different than both the original point
    and its negation.
  */
  EcPoint operator+(const EcPoint& rhs) const;
  EcPoint& operator+=(const EcPoint& rhs) { return *this = ((*this) + rhs); }
  EcPoint operator-() const { return EcPoint(x, -y); }
  EcPoint operator-(const EcPoint& rhs) const { return (*this) + (-rhs); }

  bool IsOnCurve(const FieldElementT& alpha, const FieldElementT& beta) const;

  /*
    Returns one of the two points with the given x coordinate or nullopt if there is no such point.
  */
  static std::optional<EcPoint> GetPointFromX(
      const FieldElementT& x, const FieldElementT& alpha, const FieldElementT& beta);

  /*
    Returns a random point on the curve: y^2 = x^3 + alpha*x + beta.
  */
  static EcPoint Random(const FieldElementT& alpha, const FieldElementT& beta, Prng* prng);

  static std::vector<EcPoint> RandomVector(
      const FieldElementT& alpha, const FieldElementT& beta, size_t size, Prng* prng);

  static EcPoint FromString(const std::string& input_string);

  std::string ToString() const;

  static EcPoint<FieldElementT> FromBytes(gsl::span<std::byte> bytes, bool use_big_endian = true);

  template <typename OtherFieldElementT>
  EcPoint<OtherFieldElementT> ConvertTo() const;

  void ToBytes(gsl::span<std::byte> span_out, bool use_big_endian = true) const;

  /*
    Given the bool vector representing a scalar, and the alpha of the elliptic curve "y^2 = x^3 +
    alpha * x + beta" the point is on, returns scalar*point.
  */
  template <size_t N>
  EcPoint<FieldElementT> MultiplyByScalar(
      const BigInt<N>& scalar, const FieldElementT& alpha) const;

  /*
    Returns two vectors, one with the x coordinate of the points and one with the y coordinate.
    Optionally expands the vectors to desired length by repeating the last point.
  */
  static std::pair<std::vector<FieldElementT>, std::vector<FieldElementT>> ToCoordinatesAndExpand(
      gsl::span<const EcPoint> points, const std::optional<size_t>& length = std::nullopt);

  FieldElementT x;
  FieldElementT y;

 private:
  /*
    Computes the slope of the tangent to the curve at the point. This slope is used for the
    computation of 2*P.
  */
  FieldElementT GetTangentSlope(const FieldElementT& alpha) const;

  /*
    Returns the sum of this point with a point in the form of std::optional, where std::nullopt
    represents the curve's zero element.
  */
  std::optional<EcPoint<FieldElementT>> AddOptionalPoint(
      const std::optional<EcPoint<FieldElementT>>& point, const FieldElementT& alpha) const;
};

/*
  Returns the slope of the straight line passing through the points p1 and p2.
  If the x coordinate in p1 is equal to that of p2, behavior is undefined.
*/
template <typename FieldElementT>
FieldElementT GetSlope(const EcPoint<FieldElementT>& p1, const EcPoint<FieldElementT>& p2);

/*
  Returns the sum of two distinct points, given the slope of the straight line
  passing through them.
*/
template <typename FieldElementT>
EcPoint<FieldElementT> AddPointsGivenSlope(
    const EcPoint<FieldElementT>& p1, const EcPoint<FieldElementT>& p2, const FieldElementT& slope);

/*
  Given three distinct, non-zero points that do not lie on the same straight line, computes the
  function of the form: f(x,y) = s0 + s2 * x + s3 * y + x^2, that vanishes on the three points and
  returns the pair <s2, s3>.
  If the points are collinear or not distinct, behavior is undefined.
*/
template <typename FieldElementT>
std::pair<FieldElementT, FieldElementT> GetGeneralizedSlopes(
    const EcPoint<FieldElementT>& p0, const EcPoint<FieldElementT>& p1,
    const EcPoint<FieldElementT>& p2);

/*
  Recieves a base point g and returns a vector of 2^i * g by repeated doubling.
  Optionally returns the slopes appearing in the computation.
  It is insecure to use more than log2(#E) points (where #E is the order of the curve),
  to bypass this assert set allow_more_points = true.
*/
template <typename FieldElementT>
std::vector<EcPoint<FieldElementT>> TwosPowersOfPoint(
    const EcPoint<FieldElementT>& base, const FieldElementT& alpha, size_t num_points,
    const std::optional<gsl::span<FieldElementT>>& slopes = std::nullopt,
    bool allow_more_points = false);

template <typename FieldElementT>
bool IsValidCurve(const FieldElementT& alpha, const FieldElementT& beta);

template <typename FieldElementT>
std::ostream& operator<<(std::ostream& out, const EcPoint<FieldElementT>& point) {
  return out << "(" << point.ToString() << ")";
}

}  // namespace starkware

#include "starkware/algebra/elliptic_curve/elliptic_curve.inl"

#endif  // STARKWARE_ALGEBRA_ELLIPTIC_CURVE_ELLIPTIC_CURVE_H_
