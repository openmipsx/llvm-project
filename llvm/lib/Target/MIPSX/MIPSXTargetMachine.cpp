//===-- MIPSXTargetMachine.cpp - Define TargetMachine for MIPS-X ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The MIPS-X target has no code generator yet: only the MC layer (assembler
// and instruction printer) exists. This file provides the target's
// initialization entry point so that tools that initialize every target
// link; code generation is registered here once it exists.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/Compiler.h"

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMIPSXTarget() {}
