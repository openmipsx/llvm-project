; RUN: llc -mtriple=mipsx < %s | FileCheck %s
; RUN: llc -mtriple=mipsx -filetype=obj < %s | llvm-readobj -r - \
; RUN:   | FileCheck --check-prefix=RELOC %s

; Jump tables hold word addresses (.word %w(label), docs/abi.md section
; 5.6), so dispatch is a load and jspci with no shift.

define i32 @sw(i32 %x) {
; CHECK-LABEL: sw:
; CHECK:       addi r0,#3,r2
; CHECK-NEXT:  blo r2,r4,[[D:.LBB[0-9_]+]]
; CHECK:       lsl r4,r2,#2
; CHECK-NEXT:  addi r0,#%hi([[JT:.LJTI[0-9_]+]]),r3
; CHECK-NEXT:  lsl r3,r3,#15
; CHECK-NEXT:  addi r3,#%lo([[JT]]),r3
; CHECK-NEXT:  add r3,r2,r2
; CHECK-NEXT:  ld 0[r2],r3
; CHECK-NEXT:  nop
; CHECK-NEXT:  jspci r3,#0,r0
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       .section .rodata
; CHECK:       [[JT]]:
; CHECK-NEXT:  .word %w([[A:.LBB[0-9_]+]])
; CHECK-NEXT:  .word %w([[B:.LBB[0-9_]+]])
; CHECK-NEXT:  .word %w([[C:.LBB[0-9_]+]])
; CHECK-NEXT:  .word %w([[E:.LBB[0-9_]+]])
  switch i32 %x, label %d [ i32 0, label %a
                            i32 1, label %b
                            i32 2, label %c
                            i32 3, label %e ]
a:
  ret i32 10
b:
  ret i32 20
c:
  ret i32 30
e:
  ret i32 40
d:
  ret i32 0
}

; RELOC:      .rela.rodata {
; RELOC-NEXT:   0x0 R_MIPSX_32W .text 0x
; RELOC-NEXT:   0x4 R_MIPSX_32W .text 0x
; RELOC-NEXT:   0x8 R_MIPSX_32W .text 0x
; RELOC-NEXT:   0xC R_MIPSX_32W .text 0x
