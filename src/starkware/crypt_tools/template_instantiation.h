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

#ifndef STARKWARE_CRYPT_TOOLS_TEMPLATE_INSTANTIATION_H_
#define STARKWARE_CRYPT_TOOLS_TEMPLATE_INSTANTIATION_H_

#include "starkware/crypt_tools/blake2s.h"
#include "starkware/crypt_tools/hash_context/pedersen_hash_context.h"
#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/crypt_tools/masked_hash.h"
#include "starkware/crypt_tools/pedersen.h"

#define INSTANTIATE_FOR_ALL_HASH_FUNCTIONS(ClassName)                     \
  /* NOLINTNEXTLINE */                                                    \
  template class ClassName<starkware::Blake2s256>;                        \
  /* NOLINTNEXTLINE */                                                    \
  template class ClassName<starkware::Keccak256>;                         \
  /* NOLINTNEXTLINE */                                                    \
  template class ClassName<starkware::Pedersen>;                          \
  /* NOLINTNEXTLINE */                                                    \
  template class ClassName<starkware::MaskedHash<Blake2s256, 20, true>>;  \
  /* NOLINTNEXTLINE */                                                    \
  template class ClassName<starkware::MaskedHash<Blake2s256, 20, false>>; \
  /* NOLINTNEXTLINE */                                                    \
  template class ClassName<starkware::MaskedHash<Keccak256, 20, true>>;   \
  /* NOLINTNEXTLINE */                                                    \
  template class ClassName<starkware::MaskedHash<Keccak256, 20, false>>;

#endif  // STARKWARE_CRYPT_TOOLS_TEMPLATE_INSTANTIATION_H_
