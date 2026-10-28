//===-- MIPSXMCTargetDesc.h - MIPS-X target descriptions --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXMCTARGETDESC_H
#define LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXMCTARGETDESC_H

#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include <cstdint>
#include <memory>
#include <optional>

namespace llvm {
class MCAsmBackend;
class MCCFIInstruction;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCSubtargetInfo;
class MCSymbol;
class Target;

MCCodeEmitter *createMIPSXMCCodeEmitter(const MCInstrInfo &MCII,
                                        MCContext &Ctx);

MCAsmBackend *createMIPSXAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                    const MCRegisterInfo &MRI,
                                    const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createMIPSXELFObjectWriter(uint8_t OSABI);

namespace MIPSX {
/// Encode the right-shift amount n (1 to 32) of sh and asr as the 7-bit
/// Comp Func field: e = 32 - n, bits 6-4 = e >> 2, bits 3-0 = 1 << (e & 3).
inline unsigned encodeShiftAmount(unsigned N) {
  unsigned E = (32 - N) & 31;
  return ((E >> 2) << 4) | (1u << (E & 3));
}

/// The return address column (DWARF 31, r31) holds a byte address, but r31
/// and its stack slot hold a word address (docs/abi.md section 2.4), so its
/// rule is a DW_CFA_val_expression that multiplies by 4. With no CFAOffset,
/// the return address is in r31 (the CIE's initial rule); otherwise its word
/// is saved at CFA + CFAOffset, which is negative.
MCCFIInstruction createReturnAddressCFI(MCSymbol *L,
                                        std::optional<int64_t> CFAOffset);
} // namespace MIPSX

} // namespace llvm

#define GET_REGINFO_ENUM
#include "MIPSXGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "MIPSXGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "MIPSXGenSubtargetInfo.inc"

#endif // LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXMCTARGETDESC_H
