; Symbolic operands produce fixups, shown by -show-encoding (resolved or turned
; into relocations by the object writer: see relocs.s and fixup-resolve.s).
; RUN: llvm-mc -triple=mipsx -show-encoding %s | FileCheck %s

beq r1,r2,foo
; CHECK: beq r1,r2,foo ; encoding: [0x08,0x44,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: foo, kind: fixup_mipsx_pc16
b foo
; CHECK: b foo ; encoding: [0x08,0x00,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: foo, kind: fixup_mipsx_pc16
addi r0,#bar+4,r4
; CHECK: addi r0,#bar+4,r4 ; encoding: [0xe0,0b0000100A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: bar+4, kind: fixup_mipsx_abs17
ld bar[r0],r5
; CHECK: ld bar[r0],r5 ; encoding: [0x80,0b0000101A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: bar, kind: fixup_mipsx_abs17
