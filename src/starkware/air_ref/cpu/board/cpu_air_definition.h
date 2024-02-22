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

#ifndef STARKWARE_AIR_CPU_BOARD_CPU_AIR_DEFINITION_H_
#define STARKWARE_AIR_CPU_BOARD_CPU_AIR_DEFINITION_H_

#include "starkware/algebra/utils/invoke_template_version.h"

namespace starkware {
namespace cpu {

using CpuAirDefinitionInvokedLayoutTypes = InvokedTypes<
    std::integral_constant<int, 0>, std::integral_constant<int, 1>, std::integral_constant<int, 2>,
    std::integral_constant<int, 3>, std::integral_constant<int, 4>, std::integral_constant<int, 5>,
    std::integral_constant<int, 6>, std::integral_constant<int, 7>, std::integral_constant<int, 8>,
    std::integral_constant<int, 9>, std::integral_constant<int, 10>>;

template <typename FieldElementT, int LayoutId = 0>
class CpuAirDefinition {
  // Workaround for static_assert(false).
  static_assert(
      sizeof(FieldElementT) == 0,
      "CpuAirDefinition is not implemented for the given field and layout.");
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/board/cpu_air_definition0.h"
#include "starkware/air/cpu/board/cpu_air_definition1.h"
#include "starkware/air/cpu/board/cpu_air_definition10.h"
#include "starkware/air/cpu/board/cpu_air_definition2.h"
#include "starkware/air/cpu/board/cpu_air_definition3.h"
#include "starkware/air/cpu/board/cpu_air_definition4.h"
#include "starkware/air/cpu/board/cpu_air_definition5.h"
#include "starkware/air/cpu/board/cpu_air_definition6.h"
#include "starkware/air/cpu/board/cpu_air_definition7.h"
#include "starkware/air/cpu/board/cpu_air_definition8.h"
#include "starkware/air/cpu/board/cpu_air_definition9.h"

#endif  // STARKWARE_AIR_CPU_BOARD_CPU_AIR_DEFINITION_H_
