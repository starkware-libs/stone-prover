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

#include "gflags/gflags.h"

DEFINE_uint64(
    four_step_fft_threshold, 11, "Four step fft is used when log(fft_size) >= this threshold.");
DEFINE_int64(
    log_min_twiddle_shift_task_size, 10,
    "Sets the minimal task_size used by ComputeTwiddleFromOtherTwiddle.");
