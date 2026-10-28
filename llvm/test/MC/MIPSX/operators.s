; Relocation operators (docs/abi.md section 10.6) and data directives. With
; a constant operand the operator is evaluated by the assembler.
; RUN: llvm-mc -triple=mipsx -show-encoding %s | FileCheck %s

addi r0,#%hi(x),r2
; CHECK: addi r0,#%hi(x),r2 ; encoding: [0xe0,0b0000010A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %hi(x), kind: fixup_mipsx_hi17
addi r2,#%lo(x+4),r2
; CHECK: addi r2,#%lo(x+4),r2 ; encoding: [0xe0,0b1000010A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %lo(x+4), kind: fixup_mipsx_lo15
ld %lo(x)[r2],r3
; CHECK: ld %lo(x)[r2],r3 ; encoding: [0x80,0b1000011A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %lo(x), kind: fixup_mipsx_lo15
st %hi(x)[r2],r3
; CHECK: st %hi(x)[r2],r3 ; encoding: [0x90,0b1000011A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %hi(x), kind: fixup_mipsx_hi17
ld %gprel(y)[gp],r4
; CHECK: ld %gprel(y)[r28],r4 ; encoding: [0x87,0b0000100A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %gprel(y), kind: fixup_mipsx_gprel17
addi gp,#%gprel(y),r4
; CHECK: addi r28,#%gprel(y),r4 ; encoding: [0xe7,0b0000100A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %gprel(y), kind: fixup_mipsx_gprel17
jspci r0,#%w(f),r31
; CHECK: jspci r0,#%w(f),r31 ; encoding: [0xc0,0b0011111A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %w(f), kind: fixup_mipsx_word17
addi r0,#%w(f),r1
; CHECK: addi r0,#%w(f),r1 ; encoding: [0xe0,0b0000001A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %w(f), kind: fixup_mipsx_word17
jspci r23,#%call(f),r31
; CHECK: jspci r23,#%call(f),r31 ; encoding: [0xc5,0b1111111A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %call(f), kind: fixup_mipsx_call17

; Constant operands.
addi r0,#%hi(0x12345678),r1
; CHECK: addi r0,#9320,r1 ; encoding: [0xe0,0x02,0x24,0x68]
addi r1,#%lo(0x12345678),r1
; CHECK: addi r1,#22136,r1 ; encoding: [0xe0,0x42,0x56,0x78]
addi r0,#%hi(-1),r1
; CHECK: addi r0,#-1,r1 ; encoding: [0xe0,0x03,0xff,0xff]
jspci r0,#%w(0x1000),r31
; CHECK: jspci r0,#1024,r31 ; encoding: [0xc0,0x3e,0x04,0x00]
ld %lo(0xffff8004)[r2],r3
; CHECK: ld 4[r2],r3 ; encoding: [0x80,0x86,0x00,0x04]

; Data directives.
.word 1, 0x12345678, %w(0x40)
; CHECK: .word 1
; CHECK-NEXT: .word 305419896
; CHECK-NEXT: .word 16
.word foo, %w(lab)
; CHECK-NEXT: .word foo
; CHECK-NEXT: .word %w(lab)
.long %w(lab+4)
; CHECK-NEXT: .word %w(lab+4)
.half 2, -1, 0xffff
; CHECK-NEXT: .half 2
; CHECK-NEXT: .half -1
; CHECK-NEXT: .half 65535
