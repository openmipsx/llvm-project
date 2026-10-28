# REQUIRES: mipsx
## Debug information and unwind tables in a linked program (MIPS-X SDK
## docs/abi.md sections 4 and 8.6): .debug_* refer to code through
## R_MIPSX_32 and .eh_frame through R_MIPSX_32_PCREL, and both resolve to
## byte addresses, like ELF symbols. The CIE's initial rules (CFA = r29,
## return address = r31 * 4) come from the assembler.
# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -filetype=obj -triple=mipsx -g -dwarf-version=5 a.s -o a.o
# RUN: llvm-mc -filetype=obj -triple=mipsx -g -dwarf-version=5 b.s -o b.o
# RUN: ld.lld -T lds --eh-frame-hdr a.o b.o -o out
# RUN: llvm-nm out | FileCheck --check-prefix=NM %s
# RUN: llvm-dwarfdump --debug-info --debug-line out | FileCheck %s
# RUN: llvm-dwarfdump --debug-frame out | FileCheck --check-prefix=FRAME %s
# RUN: llvm-dwarfdump --eh-frame out | FileCheck --check-prefix=EH %s
# RUN: llvm-readelf -x .eh_frame_hdr out | FileCheck --check-prefix=HDR %s
# RUN: llvm-dwarfdump --verify out | FileCheck --check-prefix=VERIFY %s
# RUN: llvm-symbolizer --obj=out 0x2000 0x2008 0x2010 | FileCheck --check-prefix=SYM %s
# RUN: llvm-objdump -d --line-numbers out | FileCheck --check-prefix=OBJDUMP %s

# NM: 00002000 T _start
# NM: 00002010 T func

## The line tables of both units, at byte addresses.
# CHECK:      DW_TAG_compile_unit
# CHECK:        DW_AT_low_pc (0x00002000)
# CHECK-NEXT:   DW_AT_high_pc (0x00002010)
# CHECK-NEXT:   DW_AT_name ("a.s")
# CHECK:      DW_TAG_compile_unit
# CHECK:        DW_AT_low_pc (0x00002010)
# CHECK-NEXT:   DW_AT_high_pc (0x00002020)
# CHECK-NEXT:   DW_AT_name ("b.s")
# CHECK:      Address Line
# CHECK:      0x0000000000002000 [[#]] {{.*}} is_stmt
# CHECK-NEXT: 0x0000000000002004 [[#]] {{.*}} is_stmt
# CHECK-NEXT: 0x0000000000002008 [[#]] {{.*}} is_stmt
# CHECK-NEXT: 0x000000000000200c [[#]] {{.*}} is_stmt
# CHECK-NEXT: 0x0000000000002010 [[#]] {{.*}} is_stmt end_sequence
# CHECK:      Address Line
# CHECK:      0x0000000000002010 [[#]] {{.*}} is_stmt
# CHECK:      0x0000000000002020 [[#]] {{.*}} is_stmt end_sequence

## func (b.s) goes to .debug_frame, _start (a.s) to .eh_frame.
# FRAME:      CIE
# FRAME:        Code alignment factor: 4
# FRAME:        Return address column: 31
# FRAME:        DW_CFA_def_cfa: R29 +0
# FRAME-NEXT:   DW_CFA_val_expression: R31 DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl
# FRAME:      FDE cie=00000000 pc=00002010...00002020
# FRAME:        0x2010: CFA=R29: R31=DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl
# FRAME-NEXT:   0x2014: CFA=R29+8: R31=DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl
# FRAME-NEXT:   0x2018: CFA=R29+8: R16=[CFA-8], R31=DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl
# FRAME-NEXT:   0x201c: CFA=R29+8: R16=[CFA-8], R31=DW_OP_lit4, DW_OP_minus, DW_OP_deref, DW_OP_lit2, DW_OP_shl

# EH:         CIE
# EH:           Augmentation: "zR"
# EH:           DW_CFA_def_cfa: R29 +0
# EH:         FDE cie=00000000 pc=00002000...00002010
# EH:           0x2000: CFA=R29: R31=DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl
# EH-NEXT:      0x2004: CFA=R29+8: R31=DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl

## version 1, sdata4 pcrel eh_frame_ptr, udata4 count, datarel sdata4
## table; one FDE.
# HDR:        011b033b {{[0-9a-f]+}} 00000001

# VERIFY: No errors.

# SYM:        _start
# SYM-NEXT:   a.s:[[#]]:0
# SYM:        _start
# SYM-NEXT:   a.s:[[#]]:0
# SYM:        func
# SYM-NEXT:   b.s:[[#]]:0

# OBJDUMP:      <_start>:
# OBJDUMP-NEXT: ; _start():
# OBJDUMP-NEXT: ; {{.*}}a.s:[[#]]
# OBJDUMP-NEXT: 2000:
# OBJDUMP:      <func>:
# OBJDUMP-NEXT: ; func():
# OBJDUMP-NEXT: ; {{.*}}b.s:[[#]]
# OBJDUMP-NEXT: 2010:

#--- lds
SECTIONS {
  .text 0x2000 : { *(.text) }
}

#--- a.s
  .globl _start
  .type _start,@function
_start:
  .cfi_startproc
  addi r29,#-8,r29
  .cfi_def_cfa_offset 8
  nop
  nop
  nop
  .cfi_endproc
  .size _start, .-_start

#--- b.s
  .cfi_sections .debug_frame
  .globl func
  .type func,@function
func:
  .cfi_startproc
  addi r29,#-8,r29
  .cfi_def_cfa_offset 8
  st 0[r29],r16
  .cfi_offset r16, -8
  st 4[r29],r31
  .cfi_escape 0x16, 0x1f, 0x05, 0x34, 0x1c, 0x06, 0x32, 0x24
  nop
  .cfi_endproc
  .size func, .-func
