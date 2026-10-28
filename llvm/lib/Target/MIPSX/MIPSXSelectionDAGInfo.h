//===-- MIPSXSelectionDAGInfo.h - MIPS-X SelectionDAG info ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MIPSXSELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_MIPSX_MIPSXSELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

#define GET_SDNODE_ENUM
#include "MIPSXGenSDNodeInfo.inc"

namespace llvm {

class MIPSXSelectionDAGInfo : public SelectionDAGGenTargetInfo {
public:
  MIPSXSelectionDAGInfo();
  ~MIPSXSelectionDAGInfo() override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MIPSXSELECTIONDAGINFO_H
