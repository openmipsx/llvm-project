# DWARF register numbers (MIPS-X SDK docs/abi.md section 2.4): r0-r31 are
# 0-31, psw 32, md 33; the ABI aliases name the same registers. The CIE's
# initial rules are CFA = r29 + 0 and return address (column 31) = r31 * 4
# (section 8.6), with code alignment 4 and data alignment -4.
# RUN: llvm-mc -triple=mipsx -filetype=obj %s -o %t.o
# RUN: llvm-dwarfdump --eh-frame %t.o | FileCheck %s
# RUN: llvm-mc -triple=mipsx %s | FileCheck --check-prefix=ASM %s

# CHECK:      CIE
# CHECK:        Code alignment factor: 4
# CHECK-NEXT:   Data alignment factor: -4
# CHECK-NEXT:   Return address column: 31
# CHECK:        DW_CFA_def_cfa: R29 +0
# CHECK-NEXT:   DW_CFA_val_expression: R31 DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl
# CHECK:      FDE

# CHECK:      DW_CFA_offset: R0 -4
# CHECK-NEXT: DW_CFA_offset: R1 -8
# CHECK-NEXT: DW_CFA_offset: R2 -12
# CHECK-NEXT: DW_CFA_offset: R3 -16
# CHECK-NEXT: DW_CFA_offset: R4 -20
# CHECK-NEXT: DW_CFA_offset: R5 -24
# CHECK-NEXT: DW_CFA_offset: R6 -28
# CHECK-NEXT: DW_CFA_offset: R7 -32
# CHECK-NEXT: DW_CFA_offset: R8 -36
# CHECK-NEXT: DW_CFA_offset: R9 -40
# CHECK-NEXT: DW_CFA_offset: R10 -44
# CHECK-NEXT: DW_CFA_offset: R11 -48
# CHECK-NEXT: DW_CFA_offset: R12 -52
# CHECK-NEXT: DW_CFA_offset: R13 -56
# CHECK-NEXT: DW_CFA_offset: R14 -60
# CHECK-NEXT: DW_CFA_offset: R15 -64
# CHECK-NEXT: DW_CFA_offset: R16 -68
# CHECK-NEXT: DW_CFA_offset: R17 -72
# CHECK-NEXT: DW_CFA_offset: R18 -76
# CHECK-NEXT: DW_CFA_offset: R19 -80
# CHECK-NEXT: DW_CFA_offset: R20 -84
# CHECK-NEXT: DW_CFA_offset: R21 -88
# CHECK-NEXT: DW_CFA_offset: R22 -92
# CHECK-NEXT: DW_CFA_offset: R23 -96
# CHECK-NEXT: DW_CFA_offset: R24 -100
# CHECK-NEXT: DW_CFA_offset: R25 -104
# CHECK-NEXT: DW_CFA_offset: R26 -108
# CHECK-NEXT: DW_CFA_offset: R27 -112
# CHECK-NEXT: DW_CFA_offset: R28 -116
# CHECK-NEXT: DW_CFA_offset: R29 -120
# CHECK-NEXT: DW_CFA_offset: R30 -124
# CHECK-NEXT: DW_CFA_offset: R31 -128
# CHECK-NEXT: DW_CFA_offset: PSW -200
# CHECK-NEXT: DW_CFA_offset: MD -204
# CHECK-NEXT: DW_CFA_def_cfa: R29 +16
# CHECK-NEXT: DW_CFA_def_cfa: R30 +0
# CHECK-NEXT: DW_CFA_offset: R31 -4
# CHECK-NEXT: DW_CFA_offset: R0 -8
# CHECK-NEXT: DW_CFA_offset: R1 -12
# CHECK-NEXT: DW_CFA_offset: R23 -16
# CHECK-NEXT: DW_CFA_offset: R24 -20
# CHECK-NEXT: DW_CFA_offset: R28 -24

# The printer uses rN names.
# ASM:      .cfi_offset r0, -4
# ASM:      .cfi_offset psw, -200
# ASM-NEXT: .cfi_offset md, -204
# ASM-NEXT: .cfi_def_cfa r29, 16
# ASM-NEXT: .cfi_def_cfa r30, 0
# ASM-NEXT: .cfi_offset r31, -4

f:
  .cfi_startproc
  .cfi_offset r0, -4
  .cfi_offset r1, -8
  .cfi_offset r2, -12
  .cfi_offset r3, -16
  .cfi_offset r4, -20
  .cfi_offset r5, -24
  .cfi_offset r6, -28
  .cfi_offset r7, -32
  .cfi_offset r8, -36
  .cfi_offset r9, -40
  .cfi_offset r10, -44
  .cfi_offset r11, -48
  .cfi_offset r12, -52
  .cfi_offset r13, -56
  .cfi_offset r14, -60
  .cfi_offset r15, -64
  .cfi_offset r16, -68
  .cfi_offset r17, -72
  .cfi_offset r18, -76
  .cfi_offset r19, -80
  .cfi_offset r20, -84
  .cfi_offset r21, -88
  .cfi_offset r22, -92
  .cfi_offset r23, -96
  .cfi_offset r24, -100
  .cfi_offset r25, -104
  .cfi_offset r26, -108
  .cfi_offset r27, -112
  .cfi_offset r28, -116
  .cfi_offset r29, -120
  .cfi_offset r30, -124
  .cfi_offset r31, -128
  .cfi_offset psw, -200
  .cfi_offset md, -204
  .cfi_def_cfa sp, 16
  .cfi_def_cfa fp, 0
  .cfi_offset ra, -4
  .cfi_offset zero, -8
  .cfi_offset at, -12
  .cfi_offset cb0, -16
  .cfi_offset cb1, -20
  .cfi_offset gp, -24
  nop
  .cfi_endproc
