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

#include "starkware/air/cpu/board/cpu_air_definition2.h"

#include "starkware/air/cpu/board/cpu_air_definition2.inl"

namespace starkware {
namespace cpu {
template class CpuAirDefinition<PrimeFieldElement<252, 0>, 2>;
}  // namespace cpu
}  // namespace starkware
