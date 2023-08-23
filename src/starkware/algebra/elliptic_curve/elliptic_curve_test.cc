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

#include "starkware/algebra/elliptic_curve/elliptic_curve.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/big_int.h"
#include "starkware/algebra/elliptic_curve/elliptic_curve_constants.h"
#include "starkware/algebra/fields/fraction_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;
using FieldElementT = PrimeFieldElement<252, 0>;
using FractionFieldElementT = FractionFieldElement<FieldElementT>;

TEST(EllipticCurve, GetSlopeTest) {
  Prng prng;

  for (size_t i = 0; i < 100; ++i) {
    FieldElementT x1 = FieldElementT::RandomElement(&prng);
    FieldElementT y1 = FieldElementT::RandomElement(&prng);
    FieldElementT x2 = x1 - FieldElementT::One();
    FieldElementT y2 = FieldElementT::RandomElement(&prng);

    ASSERT_EQ(
        GetSlope(EcPoint<FieldElementT>(x1, y1), EcPoint<FieldElementT>(x2, y1)),
        FieldElementT::Zero());
    ASSERT_EQ(GetSlope(EcPoint<FieldElementT>(x1, y1), EcPoint<FieldElementT>(x2, y2)), (y1 - y2));
    EXPECT_ASSERT(
        GetSlope(EcPoint<FieldElementT>(x1, y1), EcPoint<FieldElementT>(x1, y2)),
        HasSubstr("x values should be different for arbitrary points"));
  }
}

TEST(EllipticCurve, GetGeneralizedSlopesTest) {
  Prng prng;

  const FieldElementT alpha = FieldElementT::RandomElement(&prng);
  const FieldElementT beta = FieldElementT::RandomElement(&prng);
  auto points = EcPoint<FieldElementT>::RandomVector(alpha, beta, 3, &prng);
  const auto& [s2, s3] = GetGeneralizedSlopes(points[0], points[1], points[2]);
  const FieldElementT s0 = -(s2 * points[0].x + s3 * points[0].y + points[0].x * points[0].x);
  // The minus of the sum of points is the fourth root of the function:
  // f(x,y) = s0 + s2 * x + s3 * y + x^2.
  points.push_back(-(points[0] + points[1] + points[2]));
  for (auto point : points) {
    EXPECT_EQ(s0 + s2 * point.x + s3 * point.y + point.x * point.x, FieldElementT::Zero());
  }

  const auto expected = std::make_pair(-(points[0].x + points[1].x), FieldElementT::Zero());
  EXPECT_EQ(GetGeneralizedSlopes(points[0], -points[0], points[1]), expected);
  EXPECT_EQ(GetGeneralizedSlopes(points[0], points[1], -points[0]), expected);
  EXPECT_EQ(GetGeneralizedSlopes(points[1], points[0], -points[0]), expected);

  EXPECT_ASSERT(
      GetGeneralizedSlopes(points[0], points[0], points[1]),
      HasSubstr("The points are collinear."));
  EXPECT_ASSERT(
      GetGeneralizedSlopes(points[0], points[1], points[0]),
      HasSubstr("The points are collinear."));
  EXPECT_ASSERT(
      GetGeneralizedSlopes(points[1], points[0], points[0]),
      HasSubstr("The points are collinear."));
  EXPECT_ASSERT(
      GetGeneralizedSlopes(points[0], points[1], -(points[0] + points[1])),
      HasSubstr("The points are collinear."));
}

TEST(EllipticCurve, IsOnCurveTest) {
  Prng prng;

  for (size_t i = 0; i < 100; ++i) {
    const EcPoint<FieldElementT> point{FieldElementT::RandomElement(&prng),
                                       FieldElementT::RandomElement(&prng)};
    FieldElementT beta0 = Pow(point.y, 2) - Pow(point.x, 3);
    FieldElementT beta1 = Pow(point.y, 2) - Pow(point.x, 3) - point.x;
    ASSERT_TRUE(point.IsOnCurve(FieldElementT::Zero(), beta0));
    ASSERT_TRUE(point.IsOnCurve(FieldElementT::One(), beta1));
  }

}

TEST(EllipticCurve, DoublePointTest) {
  Prng prng;
  FieldElementT x = FieldElementT::RandomElement(&prng);
  FieldElementT y = FieldElementT::RandomElement(&prng);
  FieldElementT alpha = FieldElementT::RandomElement(&prng);
  // There is always a beta such that {x,y} is a point on the curve y^2 = x^3 + alpha * x + beta.
  EcPoint<FieldElementT> point = {x, y};
  EcPoint<FieldElementT> point_times_4 = point.Double(alpha).Double(alpha);
  ASSERT_EQ(point_times_4, (point + point.Double(alpha)) + point);
}

TEST(EllipticCurve, MulByScalarTest) {
  Prng prng;

  const EcPoint<FieldElementT> point = {FieldElementT::RandomElement(&prng),
                                        FieldElementT::RandomElement(&prng)};
  const FieldElementT alpha = FieldElementT::RandomElement(&prng);
  // There is always a beta such that {x,y} is a point on the curve y^2 = x^3 + alpha * x + beta.

  const EcPoint<FieldElementT> point_times_5_b = point.MultiplyByScalar(0x5_Z, alpha);
  ASSERT_EQ(point_times_5_b, point.Double(alpha).Double(alpha) + point);
}

TEST(EllipticCurve, MulByZeroTest) {
  Prng prng;
  const EcPoint<FieldElementT> point = {FieldElementT::RandomElement(&prng),
                                        FieldElementT::RandomElement(&prng)};
  const FieldElementT alpha = FieldElementT::RandomElement(&prng);
  // There is always a beta such that {x,y} is a point on the curve y^2 = x^3 + alpha * x + beta.

  // Multiply by zero (result should be zero).
  EXPECT_ASSERT(point.MultiplyByScalar(0x0_Z, alpha), HasSubstr("zero element"));
}

TEST(EllipticCurve, MulByCurveOrderTest) {
  Prng prng;
  using Test_value_type = EllipticCurveConstants<FieldElementT>::ValueType;
  // The elliptic curve we use over PrimeFieldElement<252,0>.
  const EcPoint<FieldElementT> generator = kPrimeFieldEc0.k_points[1];
  const Test_value_type random_power = Test_value_type(prng.UniformInt(1, 1000));
  // Multiply by the group order (result should be zero).
  EXPECT_ASSERT(
      generator.MultiplyByScalar(kPrimeFieldEc0.k_order, kPrimeFieldEc0.k_alpha),
      HasSubstr("zero element"));
  EXPECT_EQ(
      generator.MultiplyByScalar(random_power, kPrimeFieldEc0.k_alpha),
      generator.MultiplyByScalar(kPrimeFieldEc0.k_order + random_power, kPrimeFieldEc0.k_alpha));
}

TEST(EllipticCurve, MulPowerBecomesZeroTest) {
  Prng prng;
  // Start with a point P such that P+P = zero.
  const EcPoint<FieldElementT> point = {FieldElementT::RandomElement(&prng),
                                        FieldElementT::FromUint(0)};
  const FieldElementT alpha = FieldElementT::FromUint(1);

  // Check that multiplying P by an odd scalar gives back P.
  ASSERT_EQ(point.MultiplyByScalar(0x3_Z, alpha), point);
  ASSERT_EQ(point.MultiplyByScalar(0x12431234234121_Z, alpha), point);

  // Check that multiplying P by an even scalar gives zero.
  EXPECT_ASSERT(point.MultiplyByScalar(0x4_Z, alpha), HasSubstr("zero element"));
}

TEST(EllipticCurve, MinusPointTest) {
  Prng prng;
  FieldElementT x1 = FieldElementT::RandomElement(&prng);
  FieldElementT y1 = FieldElementT::RandomElement(&prng);
  FieldElementT alpha = FieldElementT::RandomElement(&prng);
  FieldElementT x2 = FieldElementT::RandomElement(&prng);
  FieldElementT y2 = FieldElementT::RandomElement(&prng);
  // There is always a beta such that {x,y} is a point on the curve y^2 = x^3 + alpha * x + beta.
  EcPoint<FieldElementT> point1 = {x1, y1};
  EcPoint<FieldElementT> point2 = {x2, y2};
  EcPoint<FieldElementT> point1_times_2 = point1.Double(alpha);
  ASSERT_EQ(point1_times_2 - point1, point1);
  ASSERT_EQ((point1 + point2) - point1, point2);
}

TEST(EllipticCurve, AddPointsTestCurve) {
  Prng prng;
  auto point = EcPoint<FieldElementT>::Random(kPrimeFieldEc0.k_alpha, kPrimeFieldEc0.k_beta, &prng);
  std::vector<EcPoint<FieldElementT>> twos_powers =
      TwosPowersOfPoint(point, kPrimeFieldEc0.k_alpha, FieldElementT::FieldSize().Log2Floor() + 1);

  auto curve_size_as_bool_vector = kPrimeFieldEc0.k_order.ToBoolVector();
  ASSERT_TRUE(curve_size_as_bool_vector[0]);
  ASSERT_TRUE(curve_size_as_bool_vector.size() >= twos_powers.size());
  ASSERT_TRUE(curve_size_as_bool_vector[twos_powers.size() - 1]);
  for (size_t i = 1; i < twos_powers.size() - 1; ++i) {
    if (curve_size_as_bool_vector[i]) {
      point = point + twos_powers[i];
    }
  }
  EXPECT_EQ(twos_powers.back(), EcPoint<FieldElementT>(point.x, -point.y));
}

TEST(EllipticCurve, ToFromString) {
  Prng prng;
  FieldElementT x = FieldElementT::RandomElement(&prng);
  FieldElementT y = FieldElementT::RandomElement(&prng);
  EcPoint<FieldElementT> point = {x, y};
  std::string point_str = point.ToString();
  auto point_from_str = EcPoint<FieldElementT>::FromString(point_str);
  EXPECT_EQ(point_from_str, point);
  std::string point_str_from_point_from_str = point_from_str.ToString();
  EXPECT_EQ(point_str, point_str_from_point_from_str);
}

TEST(EllipticCurve, ToFromBytes) {
  Prng prng;
  FieldElementT x = FieldElementT::RandomElement(&prng);
  FieldElementT y = FieldElementT::RandomElement(&prng);
  EcPoint<FieldElementT> point(x, y);
  std::array<std::byte, 2 * FieldElementT::SizeInBytes()> bytes{};
  point.ToBytes(bytes);
  auto point_from_bytes = EcPoint<FieldElementT>::FromBytes(bytes);
  EXPECT_EQ(point_from_bytes, point);
  std::array<std::byte, 2 * FieldElementT::SizeInBytes()> bytes_from_point_from_bytes{};
  point_from_bytes.ToBytes(bytes_from_point_from_bytes);
  EXPECT_EQ(bytes, bytes_from_point_from_bytes);
}

TEST(EllipticCurve, RandomPointOnCurve) {
  Prng prng;
  const FieldElementT alpha = FieldElementT::RandomElement(&prng);
  const FieldElementT beta = FieldElementT::RandomElement(&prng);
  const auto point = EcPoint<FieldElementT>::Random(alpha, beta, &prng);
  EXPECT_TRUE(point.IsOnCurve(alpha, beta));

  const auto point2 = EcPoint<FieldElementT>::GetPointFromX(point.x, alpha, beta);
  ASSERT_TRUE(point2.has_value());
  EXPECT_EQ(point.x, point2->x);
  EXPECT_TRUE(point.y == point2->y || point.y == -point2->y)
      << "point: " << point << ", point2: " << *point2;
}

TEST(EllipticCurve, RandomVector) {
  Prng prng;
  const size_t size = 10;
  const FieldElementT alpha = FieldElementT::RandomElement(&prng);
  const FieldElementT beta = FieldElementT::RandomElement(&prng);
  const auto v = EcPoint<FieldElementT>::RandomVector(alpha, beta, size, &prng);
  const auto w = EcPoint<FieldElementT>::RandomVector(alpha, beta, size, &prng);
  EXPECT_EQ(v.size(), size);
  EXPECT_EQ(w.size(), size);
  EXPECT_NE(v, w);
  for (const auto& p : v) {
    EXPECT_TRUE(p.IsOnCurve(alpha, beta));
  }
  for (const auto& p : w) {
    EXPECT_TRUE(p.IsOnCurve(alpha, beta));
  }
}

TEST(EllipticCurve, ToCoordinatesAndExpand) {
  Prng prng;
  const size_t size = 10;
  const size_t length = 20;
  const FieldElementT alpha = FieldElementT::RandomElement(&prng);
  const FieldElementT beta = FieldElementT::RandomElement(&prng);
  const auto points = EcPoint<FieldElementT>::RandomVector(alpha, beta, size, &prng);
  const auto coordinates1 = EcPoint<FieldElementT>::ToCoordinatesAndExpand(points, length);
  const auto coordinates2 = EcPoint<FieldElementT>::ToCoordinatesAndExpand(points, size);
  const auto coordinates3 = EcPoint<FieldElementT>::ToCoordinatesAndExpand(points);
  EXPECT_ASSERT(
      EcPoint<FieldElementT>::ToCoordinatesAndExpand(points, size - 1),
      HasSubstr("Too many points were given."));

  ASSERT_EQ(coordinates1.first.size(), length);
  ASSERT_EQ(coordinates1.second.size(), length);
  ASSERT_EQ(coordinates2.first.size(), size);
  ASSERT_EQ(coordinates2.second.size(), size);
  ASSERT_EQ(coordinates3.first.size(), size);
  ASSERT_EQ(coordinates3.second.size(), size);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(EcPoint(coordinates1.first[i], coordinates1.second[i]), points[i]);
    EXPECT_EQ(EcPoint(coordinates2.first[i], coordinates2.second[i]), points[i]);
    EXPECT_EQ(EcPoint(coordinates3.first[i], coordinates3.second[i]), points[i]);
  }
  for (size_t i = size; i < length; ++i) {
    EXPECT_EQ(EcPoint(coordinates1.first[i], coordinates1.second[i]), points.back());
  }
}

TEST(EllipticCurve, TestConvertToFunction) {
  Prng prng;
  const FieldElementT first_element = FieldElementT::RandomElement(&prng);
  const FieldElementT second_element = FieldElementT::RandomElement(&prng);
  const auto random_ec_point = EcPoint<FieldElementT>(first_element, second_element);
  const auto ec_point_converted = random_ec_point.template ConvertTo<FractionFieldElementT>();
  EXPECT_EQ(random_ec_point, ec_point_converted.template ConvertTo<FieldElementT>());
}

}  // namespace
}  // namespace starkware
