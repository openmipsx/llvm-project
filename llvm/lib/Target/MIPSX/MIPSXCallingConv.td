//===-- MIPSXCallingConv.td - MIPS-X calling conventions ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MIPS-X SDK docs/abi.md section 7. Arguments are a stream of 32-bit words
// in r4-r11 and then 4-byte stack slots at sp + 0, + 4, ...; i64 and f64 are
// split by the type legaliser, high word first. Variadic arguments always go
// on the stack. Return values are in r2 and r3.
//
//===----------------------------------------------------------------------===//

def CC_MIPSX : CallingConv<[
  // Narrow integers are extended to 32 bits by the caller.
  CCIfType<[i1, i8, i16], CCPromoteToType<i32>>,

  // Arguments matching "..." are passed on the stack.
  CCIfArgVarArg<CCAssignToStack<4, 4>>,

  CCIfType<[i32], CCAssignToReg<[R4, R5, R6, R7, R8, R9, R10, R11]>>,
  CCIfType<[i32], CCAssignToStack<4, 4>>
]>;

def RetCC_MIPSX : CallingConv<[
  // Narrow integers are extended to 32 bits by the callee.
  CCIfType<[i1, i8, i16], CCPromoteToType<i32>>,
  CCIfType<[i32], CCAssignToReg<[R2, R3]>>
]>;

// Callee-saved registers (abi section 2.1), and r31 so that the prologue
// saves the return address of a function that makes calls; calls define
// r31 explicitly.
def CSR_MIPSX : CalleeSavedRegs<(add R31, R30, (sequence "R%u", 16, 22),
                                     R25, R26, R27)>;

// Registers preserved by the SDK's 32-bit helper routines, from the clobber
// lists in mipsx-sdk rt/builtins/README.md (abi section 7.4). A call to one
// of them uses these instead of CSR_MIPSX, so the caller keeps more values
// in registers. r31 is defined by the call itself.
// __mulsi3, __udivsi3, __lshrsi3, __ashrsi3: change r1-r3 (and MD).
def CSR_MIPSX_R1R3 : CalleeSavedRegs<(add (sequence "R%u", 4, 30))>;
// __ashlsi3: changes r1-r3 and r5.
def CSR_MIPSX_Ashl : CalleeSavedRegs<(add R4, (sequence "R%u", 6, 30))>;
// __umodsi3: changes r1-r3, r15 (and MD).
def CSR_MIPSX_Umod : CalleeSavedRegs<(add (sequence "R%u", 4, 14),
                                          (sequence "R%u", 16, 30))>;
// __divsi3, __modsi3: change r1-r5, r12, r13, r15 (and MD).
def CSR_MIPSX_Div : CalleeSavedRegs<(add (sequence "R%u", 6, 11), R14,
                                         (sequence "R%u", 16, 30))>;
