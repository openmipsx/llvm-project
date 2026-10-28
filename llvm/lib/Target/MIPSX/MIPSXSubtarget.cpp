//===-- MIPSXSubtarget.cpp - MIPS-X subtarget information -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MIPSXSubtarget.h"
#include "MIPSXSelectionDAGInfo.h"

using namespace llvm;

#define DEBUG_TYPE "mipsx-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "MIPSXGenSubtargetInfo.inc"

MIPSXSubtarget &MIPSXSubtarget::initializeSubtargetDependencies(StringRef CPU,
                                                                StringRef FS) {
  if (CPU.empty())
    CPU = "es3890";
  ParseSubtargetFeatures(CPU, /*TuneCPU=*/CPU, FS);
  return *this;
}

MIPSXSubtarget::MIPSXSubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                               const TargetMachine &TM)
    : MIPSXGenSubtargetInfo(TT, CPU, /*TuneCPU=*/CPU, FS),
      InstrInfo(initializeSubtargetDependencies(CPU, FS)),
      FrameLowering(*this), TLInfo(TM, *this),
      TSInfo(std::make_unique<MIPSXSelectionDAGInfo>()) {}

MIPSXSubtarget::~MIPSXSubtarget() = default;
