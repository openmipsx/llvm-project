; DWARF in a relocatable object uses R_MIPSX_32, which RelocationResolver
; applies, so llvm-dwarfdump reads it without warnings.
; RUN: llvm-mc -triple=mipsx -filetype=obj -g -dwarf-version=4 %s -o %t.o
; RUN: llvm-readobj -r %t.o | FileCheck --check-prefix=RELOC %s
; RUN: llvm-dwarfdump --debug-info --debug-line %t.o 2>&1 | FileCheck %s \
; RUN:   --implicit-check-not=warning:

  nop
  .section .text.second,"ax"
  add r1,r2,r3
  nop

; RELOC: .rela.debug_info {
; RELOC: R_MIPSX_32 .debug_abbrev 0x0
; RELOC: .rela.debug_line {
; RELOC: R_MIPSX_32 .text.second 0x0

; CHECK: DW_TAG_compile_unit
; CHECK: DW_AT_name ("{{.*}}dwarf-relocs.s")
; CHECK: DW_AT_producer
; CHECK: Address Line
; CHECK: 0x0000000000000000 8 {{.*}} is_stmt
; CHECK: 0x0000000000000000 10 {{.*}} is_stmt
; CHECK: 0x0000000000000004 11 {{.*}} is_stmt
