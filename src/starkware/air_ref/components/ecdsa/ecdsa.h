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

#ifndef STARKWARE_AIR_COMPONENTS_ECDSA_ECDSA_H_
#define STARKWARE_AIR_COMPONENTS_ECDSA_ECDSA_H_

#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/ec_subset_sum/ec_subset_sum.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/elliptic_curve/elliptic_curve.h"
#include "starkware/algebra/elliptic_curve/elliptic_curve_constants.h"

namespace starkware {

template <typename FieldElementT>
class EcdsaComponent {
 public:
  using EcPointT = EcPoint<FieldElementT>;
  using ValueType = typename FieldElementT::ValueType;

  struct Config {
    FieldElementT alpha;
    FieldElementT beta;
    ValueType curve_order;
    EcPointT shift_point;
    EcPointT generator_point;

    EcPointT PublicKeyFromPrivateKey(const ValueType& private_key) const {
      return generator_point.MultiplyByScalar(private_key, alpha);
    }

    ValueType RandomPrivateKey(Prng* prng) const {
      return prng->UniformBigInt(ValueType::One(), curve_order - ValueType::One());
    }
  };

  static const Config& GetSigConfig() {
    static const gsl::owner<const Config*> kSigConfig =
        new Config{kPrimeFieldEc0.k_alpha, kPrimeFieldEc0.k_beta, kPrimeFieldEc0.k_order,
                   kPrimeFieldEc0.k_points[0], kPrimeFieldEc0.k_points[1]};
    return *kSigConfig;
  }

  /*
    See starkware/air/components/ecdsa/ecdsa.py for documentation.
  */
  EcdsaComponent(
      const std::string& name, const TraceGenerationContext& ctx, uint64_t height,
      uint64_t n_hash_bits, const Config& sig_config)
      : height_(height),
        n_hash_bits_(n_hash_bits),
        alpha_(sig_config.alpha),
        shift_point_(sig_config.shift_point),
        generator_points_(TwosPowersOfPoint(sig_config.generator_point, alpha_, n_hash_bits)),
        key_points_x_(ctx.GetVirtualColumn(name + "/key_points/x")),
        key_points_y_(ctx.GetVirtualColumn(name + "/key_points/y")),
        doubling_slope_(ctx.GetVirtualColumn(name + "/doubling_slope")),
        z_inv_(ctx.GetVirtualColumn(name + "/z_inv")),
        r_w_inv_(ctx.GetVirtualColumn(name + "/r_w_inv")),
        add_results_inv_(ctx.GetVirtualColumn(name + "/add_results_inv")),
        add_results_slope_(ctx.GetVirtualColumn(name + "/add_results_slope")),
        extract_r_inv_(ctx.GetVirtualColumn(name + "/extract_r_inv")),
        extract_r_slope_(ctx.GetVirtualColumn(name + "/extract_r_slope")),
        q_x_squared_(ctx.GetVirtualColumn(name + "/q_x_squared")),
        exponentiate_gen_(name + "/exponentiate_generator", ctx, height, n_hash_bits, true, false),
        exponentiate_key_(name + "/exponentiate_key", ctx, height, n_hash_bits, true, false) {}

  /*
    This struct represents one input to the ecdsa verification protocol.
  */
  struct Input {
    EcPointT public_key;  // The public key of the signer.
    FieldElementT z;      // The hash of the signed message.
    FieldElementT r;      // The first element of the signature.
    FieldElementT w;      // The second element of the signature.

    /*
      Constructs an instance from the x coordinate of the public key instead of the full public key.
    */
    static Input FromPartialPublicKey(
        const FieldElementT& public_key_x, const FieldElementT& z, const FieldElementT& r,
        const FieldElementT& w, const Config& config);
  };

  /*
    Writes the trace for one instance of the component.
    input includes:
      public_key is assumed to be on the curve (not checked).
      z is the hash of the message.
      r, w are the signature.
    component_index - the index of the component instance.
    check_validity - if true, assert that the signature is valid.
  */
  void WriteTrace(
      const Input& input, uint64_t component_index, gsl::span<const gsl::span<FieldElementT>> trace,
      bool check_validity = true) const;

  /*
    Verifies a message with our modified ECDSA algorithm.
    WARNING: This function can pass even though GetTrace will fail, in edge cases that cannot be
    written in the trace (e.g. Infinity within subset sum).
  */
  static bool Verify(const Config& config, const Input& input);

  /*
    Signs a message with our modified ECDSA algorithm.
    message should be the range [0, curve_order).
  */
  static std::pair<FieldElementT, FieldElementT> Sign(
      const Config& config, const typename FieldElementT::ValueType& private_key,
      const typename FieldElementT::ValueType& message, Prng* prng);

 private:
  /*
    Helper function that handles the key exponentiation part of writing the trace.
    Writes the trace of the computation of (exponent * base_point) in the
    key_exponentiation_index instance of exponentiate_key_ subcomponent in trace. Note that
    since each ecdsa component includes two instances of exponentiate_key_, the component_index
    ecdsa instance includes the (component_index * 2) and (component_index * 2 + 1) instances of
    exponentiate_key_. It returns the result of the exponentiation.
  */
  EcPointT WriteKeyExponentiationTrace(
      const EcPointT& base_point, const FieldElementT& exponent, uint64_t key_exponentiation_index,
      gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    The number of rows for each exponentiation instance.
  */
  const uint64_t height_;

  /*
    The number of bits in the message hash (z).
  */
  const uint64_t n_hash_bits_;

  /*
    The alpha of the curve (beta is not needed).
  */
  const FieldElementT alpha_;

  /*
    The initial curve point for the summation (C).
  */
  const EcPointT shift_point_;

  /*
    The generator of the curve (G) times powers of 2.
  */
  const std::vector<EcPoint<FieldElementT>> generator_points_;

  /*
    The virtual columns (see ecdsa.py).
  */
  const VirtualColumn key_points_x_;
  const VirtualColumn key_points_y_;
  const VirtualColumn doubling_slope_;
  const VirtualColumn z_inv_;
  const VirtualColumn r_w_inv_;
  const VirtualColumn add_results_inv_;
  const VirtualColumn add_results_slope_;
  const VirtualColumn extract_r_inv_;
  const VirtualColumn extract_r_slope_;
  const VirtualColumn q_x_squared_;

  /*
    A SubsetSum component that computes z * G - C (from z, G and C).
  */
  const EcSubsetSumComponent<FieldElementT> exponentiate_gen_;

  /*
    A SubsetSum component with two instances such that:
    The first instance computes r * Q + C.
    The second instance computes w * B + C.
  */
  const EcSubsetSumComponent<FieldElementT> exponentiate_key_;
};

}  // namespace starkware

#include "starkware/air/components/ecdsa/ecdsa.inl"

#endif  // STARKWARE_AIR_COMPONENTS_ECDSA_ECDSA_H_
