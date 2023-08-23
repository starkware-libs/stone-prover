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

/*
  x and y are instances of BigInt<4> represented in montgomery form. This means that a number [x] mod M is
  represented as x = [x] * 2**256 (mod M), over 4 words of 64 bit: x[0], x[1], x[2], x[3] (from least
  significant to most significant).
  This representation is not unique, but if we require x < M, it is.
  y < M, but x can be unreduced. We only need x < 2**256.
  The representation of [x]*[y] is: ([x] * [y] * 2**256) % M =
     ( ([x] * 2**256) * ([y] * 2**256) * 2**(-256) ) % M =
     (x * y * 2**(-256)) % M

  A montgomery round computes a number congruent mod M to:
    (val + x_i*y) * 2**(-64)
  using the following steps:
  MontgomeryRound(val, x_i, y):
    Step 1. val += x_i * y
    Step 2. u = (-val * M^-1) % 2**64
    Step 3. val += u * M
    Step 4. return val >> 64

  Remarks:
  1. u's purpose is to make val divisible by 2**64, while keeping it the same modulo M:
       (val + u * M) % M = val % M
       (val + u * M) % 2**64 = (val + -val * M^-1 * M) % 2**64 = 0 % 2**64
  2. Instead of (4.), we will consider the 64bit words cyclically.

  To see why 4 MontgomeryRounds give a full montgomery multiplication, we follow res % M across
  iterations:
    res_0 = 0  (mod M)
    res_1 = (x_0 * y) * 2**(-64)  (mod M)
    res_2 = (x_0 * y) * 2**(-128) + (x_1 * y) * 2**(-64)  (mod M)
          = ((x_0 + x_1 * 2**64) * y) * 2**(-128)  (mod M)
    res_3 = ((x_0 + x_1 * 2**64 + x_2 * 2**128) * y) * 2**(-192)  (mod M)
    res_4 = ((x_0 + x_1 * 2**64 + x_2 * 2**128 + x_3 * 2**192) * y) * 2**(-256)  (mod M)
          = x * y * 2**(-256)  (mod M)

  This is exactly what we wanted.
*/

#define temp0 %rax
#define temp1 %rbx
#define temp2 %rcx
#define temp3 %rdx

#define res0 %r15
#define res1 %rbp
#define res2 %r8
#define res3 %r9
#define m3 %r10
#define y0 %r11
#define y1 %r12
#define y2 %r13
#define y3 %r14

#define twiddle_ptr m3  // Note register sharing.

/*
  MontgomeryRound(val, x_i, y) computed a single round as explained above. Used for every
  round except the first.
  x_i is passed in rdx.
  ASSUMPTION: CF and OF are off here.
  This macro keeps this invariant when it exits (CF and OF are off)
*/
.macro MontgomeryRound val0 val1 val2 val3 val4
      // Step 1. val += x_i * y
      // We actually have addition of three numbers here, since x_i * y_j is 2 words:
      //      0               val3             val2             val1             val0
      //      0           (x_i * y_3)_L    (x_i * y_2)_L    (x_i * y_1)_L    (x_i * y_0)_L
      // (x_i * y_3)_H    (x_i * y_2)_H    (x_i * y_1)_H    (x_i * y_0)_H         0
      // To add three numbers "together" (since we don't want to compute the multiplications more
      // than once), we use two carry chains:
      // * adcx is addition that does not affect overflow flag.
      // * adox is addition with overflow flag instead of carry flag, and doesn't affect the carry
      // flag.

      // [temp1 : temp0] = (x_i * y0)_H, (x_i * y0)_L .
      mulxq y0, temp0, temp1
      // val0 += (x_i * y_0)_L  (c carry chain) .
      adcxq temp0, \val0
      // val1 += (x_i * y_0)_H  (o carry chain) .
      adoxq temp1, \val1

      // [temp1 : temp0] = (x_i * y1)_H, (x_i * y1)_L .
      mulxq y1, temp0, temp1
      // val1 += (x_i * y_1)_L  (c carry chain) .
      adcxq temp0, \val1
      // val2 += (x_i * y_1)_H  (o carry chain) .
      adoxq temp1, \val2

      // [temp1 : temp0] = (x_i * y2)_H, (x_i * y2)_L .
      mulxq y2, temp0, temp1
      // val2 += (x_i * y_2)_L  (c carry chain) .
      adcxq temp0, \val2
      // val3 += (x_i * y_2)_H  (o carry chain) .
      adoxq temp1, \val3

      // [temp1 : temp0] = (x_i * y3)_H, (x_i * y3)_L .
      mulxq y3, temp0, temp1
      // val3 += (x_i * y_3)_L  (c carry chain) .
      adcxq temp0, \val3
      // val4 += (x_i * y_3)_H  (o carry chain) .
      adoxq temp1, \val4
      // add last carry from other carry chain (c) .
      adcq $0, \val4

      // The last two additions to val4 have no carry because:
      // val + x_i * y <= (2**256-1) + (2**64-1)*(2**256-1)
      //                = 2**64 * (2**256-1) < 2**(256+64)
      // Hence, CF is off

      // Step 2. u = (-val * M^-1) % 2**64
      //           = ( val0 * (-M^-1 % 2**64) ) % 2**64
      //           = ( val0 * (mprime) ) % 2**64
      // For our specific M, we have mprime = -1:
      //      u = (-val0) % 2**64

      // rdx = -val0 .
      movq \val0, %rdx
      negq %rdx

      // Step 3. val += u * M
      // Our specific M looks like [m3:0:0:1], and the situation looks like this:
      //     val4             val3             val2             val1             val0
      //      0             (u * m3)_L          0                0                u
      //   (u * m3)_H          0                0                0                0

      // [temp1 : temp0] = (u * m3)_H, (u * m3)_L .
      mulxq m3, temp0, temp1
      // val0 += u (carry in c) .
      addq %rdx, \val0
      // Note that val0 now is (val_previous + u * M) % 2**64 which is zero! (See first Remark at
      // the beginning)

      // val1 += 0 (carry in c) .
      adcq \val0, \val1
      // val2 += 0 (carry in c) .
      adcq \val0, \val2
      // val3 += (u * m3)_L (carry in c) .
      adcq temp0, \val3
      // val4 += (u * m3)_H (carry in c) .
      adcq temp1, \val4

      // We have kept the invariant that CF and OF are off. Indeed:
      // We want to have no carry and no overflow here. This requires
      //   prev_val + x_i*y + u*M <= 2**(256+64-1)-1
      // What we have:
      //   prev_val + x_i*y + u*M <=
      //   2**256-1 + (2**64-1)*(M-1) + (2**64-1)*M =
      //   2**256 + (2**65-2)*M -2**64+1
      // We get the requirement:
      //   2**256 + (2**65-2)*M -2**64+1 <= 2**(256+64-1)-1
      //   (2**65-2)*M <= 2**256*(2**63 - 1) + 2**64 -2
      // Since our M holds that inequality, we have no carry nor overflow here.
.endm

      // MontgomeryRound_first is for the very first round.
.macro MontgomeryRound_first val0 val1 val2 val3 val4
      // Similar action to regular Round.
      // However, here we only to overwrite val, not add to it.
      // This is more efficient, so we have a different implementation for the first round.
      // Step 1. val = x_i * y
      mulxq y0, \val0, \val1
      mulxq y1, temp0, \val2
      addq temp0, \val1
      mulxq y2, temp0, \val3
      adcq temp0, \val2
      mulxq y3, temp0, \val4
      adcq temp0, \val3
      adcq $0, \val4

      // Step 2 + 3.
      // Currently, identical to regular Round, see above.
      movq \val0, %rdx
      negq %rdx
      mulxq m3, temp0, temp1
      addq %rdx, \val0
      adcq \val0, \val1
      adcq \val0, \val2
      adcq temp0, \val3
      adcq temp1, \val4
.endm


.text
        .globl  UnreducedMontMulPrime0 # -- Begin function UnreducedMontMulPrime0
        .p2align        4, 0x90
        .type   UnreducedMontMulPrime0,@function
UnreducedMontMulPrime0: # @UnreducedMontMulPrime0
        .cfi_startproc

      // According to "System V AMD64 ABI", the registers %rbp, %rbx and %r12 through %r15 belong
      // to the calling function, and thus should be stored on the local stack.
      // Arguments are passed in rdi(result), rsi (x) and rdx (y). The result should be in %rax.
      pushq %rbp
      .cfi_def_cfa_offset 16
      pushq %r15
      .cfi_def_cfa_offset 24
      pushq %r14
      .cfi_def_cfa_offset 32
      pushq %r13
      .cfi_def_cfa_offset 40
      pushq %r12
      .cfi_def_cfa_offset 48
      pushq %rbx
      .cfi_def_cfa_offset 56
      .cfi_offset %rbx, -56
      .cfi_offset %r12, -48
      .cfi_offset %r13, -40
      .cfi_offset %r14, -32
      .cfi_offset %r15, -24
      .cfi_offset %rbp, -16
      movq (%rdx), y0
      movq 8(%rdx), y1
      movq 16(%rdx), y2
      movq 24(%rdx), y3
      movq (%rsi), %rdx  // x0
      movabsq $0x800000000000011, m3

      MontgomeryRound_first res1, res2, res3, temp2, res0
      mov 8(%rsi), %rdx
      MontgomeryRound       res2, res3, temp2, res0, res1
      mov 16(%rsi), %rdx
      MontgomeryRound       res3, temp2, res0, res1, res2
      mov 24(%rsi), %rdx
      MontgomeryRound       temp2, res0, res1, res2, res3

      movq res0, (%rdi)
      movq res1, 8(%rdi)
      movq res2, 16(%rdi)
      movq res3, 24(%rdi)
      movq %rdi, %rax
      popq %rbx
      .cfi_def_cfa_offset 48
      popq %r12
      .cfi_def_cfa_offset 40
      popq %r13
      .cfi_def_cfa_offset 32
      popq %r14
      .cfi_def_cfa_offset 24
      popq %r15
      .cfi_def_cfa_offset 16
      popq %rbp
      .cfi_def_cfa_offset 8
      retq
      .cfi_endproc




.text
        .globl  Prime0FftLoop # -- Begin Prime0FftLoop
        .p2align        4, 0x90
        .type   Prime0FftLoop,@function
Prime0FftLoop: # @Prime0FftLoop
        .cfi_startproc

      // According to "System V AMD64 ABI", the registers %rbp, %rbx and %r12 through %r15 belong
      // to the calling function, and thus should be stored on the local stack.
      // Arguments are passed in rdi(src_plus_distance), rsi (src_end), rdx (src_to_dst),
      // rcx (distance), r8(twiddle_array) r9 (twiddle_shift),
      // and twiddle_mask is passed at the top of the stack.
      pushq %rbp
      .cfi_def_cfa_offset 16
      pushq %r15
      .cfi_def_cfa_offset 24
      pushq %r14
      .cfi_def_cfa_offset 32
      pushq %r13
      .cfi_def_cfa_offset 40
      pushq %r12
      .cfi_def_cfa_offset 48
      pushq %rbx
      .cfi_def_cfa_offset 56
      .cfi_offset %rbx, -56
      .cfi_offset %r12, -48
      .cfi_offset %r13, -40
      .cfi_offset %r14, -32
      .cfi_offset %r15, -24
      .cfi_offset %rbp, -16

      // Local variables (on the stack).
      // Currently they are all read-only (After the initialization).
      #define s_src_end -8(%rsp)
      #define s_src_plus_distance -16(%rsp)
      #define s_src_to_dst -24(%rsp)
      #define s_twiddle_array -32(%rsp)
      #define s_distance -40(%rsp)
      #define s_twiddle_shift -56(%rsp)

      // Paramater on the stack. We can read it but not modify it.
      #define s_twiddle_mask 56(%rsp)


      // Initalize Local variables.
      movq %rsi, s_src_end
      movq %rdx, s_src_to_dst
      movq %r8, s_twiddle_array
      movq %r9, s_twiddle_shift
      movq %rcx, s_distance
      movq %rdi, s_src_plus_distance

      // Prepare inputs for fft_loop.
      // Note that rdi already holds in_2.
      movq %r8, twiddle_ptr
      movq %rcx, temp0

      .p2align 4
fft_loop:
      /*
      Each iteration of the loop computes the FftButterfly:
            out_1 = in_1 + twiddle_factor * in_2
            out_2 = in_1 - twiddle_factor * in_2

      Each iteration of the loop starts with:
           %rdi = &in_2
           %twiddle_ptr = &twiddle_factor
           temp0 = distance
      */

      // Start fetching the cachelines we need for in_2 * twiddle_factor as early as possible.

      movq (%rdi), %rdx  // x0

      // y = twiddle_factor
      movq (twiddle_ptr), y0

      // Compute the address of second element (in_2) in the next butterfly.
      // This needs to be done as early as possible to get the best performance.
      lea 0x20(%rdi), %rsi
      sub s_src_plus_distance, %rsi // substract s_src_plus_distance to get an index in the array.

      // rsi += rsi & distance, this moves us to the next fft chunk in case we are done with the
      // the current one.
      and %rsi, temp0
      add temp0, %rsi

      movq 8(twiddle_ptr), y1
      movq 16(twiddle_ptr), y2
      movq 24(twiddle_ptr), y3

      // Note that twiddle_ptr and m3 are the same register.
      movabsq $0x800000000000011, m3

      MontgomeryRound_first res1, res2, res3, temp2, res0
      movq 8(%rdi), %rdx
      MontgomeryRound       res2, res3, temp2, res0, res1
      movq 16(%rdi), %rdx
      MontgomeryRound       res3, temp2, res0, res1, res2
      movq 24(%rdi), %rdx
      sub s_distance, %rdi // %rdi = &in_1
      MontgomeryRound       temp2, res0, res1, res2, res3

      // temp = in_1
      movq (%rdi), temp0

      mov s_twiddle_shift, y0
      movabsq $0xEFFFFFFFFFFFFFDD, y3 // y3 = most significant 64 bits of -2*Modules.

      movq 8(%rdi), temp1
      movq 16(%rdi), temp2
      movq 24(%rdi), temp3
      add s_src_to_dst, %rdi // %rdi = &out1

      // Compute the index of next twiddle factor.
      shrx y0, %rsi, twiddle_ptr // twiddle_ptr = index (rsi) >> twiddle_shift (y0)
      add s_src_plus_distance, %rsi // %rsi = &in_2
      and s_twiddle_mask, twiddle_ptr

      // Compute the address of the next twiddle factor.
      add s_twiddle_array, twiddle_ptr

      // y = temp - 2*Modules
      movq $-2, y0
      addq temp0, y0
      movq $-1, y1
      movq y1, y2
      adcq temp1, y1
      adcq temp2, y2
      adcq temp3, y3

      // if (y < 0) y = temp.
      cmovsq temp0, y0
      cmovsq temp1, y1
      cmovsq temp2, y2
      cmovsq temp3, y3
      // At this point we have y = Value_type::ReduceIfNeeded(in_1, 2*Modules);

      // out1 = y + res
      // Note that we copy res to temp because later we will need both y and res to compute y - res.
      mov res0, temp0
      addq y0, temp0
      movq temp0, (%rdi)

      mov s_distance, temp0

      mov res1, temp1
      adcq y1, temp1
      movq temp1, 8(%rdi)

      movabs $0x1000000000000022, temp1  // temp1 = most significant 64 bits of 2*Modules.

      mov res2, temp2
      adcq y2, temp2
      movq temp2, 16(%rdi)
      mov res3, temp3
      adcq y3, temp3
      movq temp3, 24(%rdi)
      add temp0, %rdi // rdi = out2

      // y -= res
      sub res0, y0
      sbb res1, y1
      sbb res2, y2
      sbb res3, y3

      //y += kModulesTimesTwo
      addq $0x2, y0
      movq y0, (%rdi)
      adcq $0x0, y1
      movq y1, 8(%rdi)
      adcq $0x0, y2
      movq y2, 16(%rdi)
      adcq temp1, y3
      movq y3, 24(%rdi)

      cmpq s_src_end, %rsi
      mov %rsi, %rdi
      jb fft_loop

      popq %rbx
      .cfi_def_cfa_offset 48
      popq %r12
      .cfi_def_cfa_offset 40
      popq %r13
      .cfi_def_cfa_offset 32
      popq %r14
      .cfi_def_cfa_offset 24
      popq %r15
      .cfi_def_cfa_offset 16
      popq %rbp
      .cfi_def_cfa_offset 8
      retq

      .cfi_endproc
