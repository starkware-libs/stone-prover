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

__start__:
[ap] = 17, ap++;

// Test filling holes in the offsets range check component, by accessing offsets 0 (in the line
// above) and -10 (below), without accessing offsets between the two. Note that the offsets -1 and 1
// appear naturally in some instructions.
ap += 10;
[ap - 10] = 0;

__end__:
jmp rel 0;
