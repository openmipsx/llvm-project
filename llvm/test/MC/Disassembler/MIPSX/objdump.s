; Assemble to an object and disassemble it with llvm-objdump. A data word
; that is not an instruction shows as <unknown>.
; RUN: llvm-mc -triple=mipsx -filetype=obj %s -o %t.o
; RUN: llvm-objdump --triple=mipsx -d %t.o | FileCheck %s

add r1,r2,r3
.word 0
lsl r3,r4,#2
li #0x12345678,r5
b -2
nop
nop
; CHECK:      0: 60 44 30 19 add r1,r2,r3
; CHECK-NEXT: 4: 00 00 00 00 <unknown>
; CHECK-NEXT: 8: 48 06 42 04 lsl r3,r4,#2
; CHECK-NEXT: c: e0 0a 24 68 addi r0,#9320,r5
; CHECK-NEXT: 10: 48 0a 52 38 lsl r5,r5,#15
; CHECK-NEXT: 14: e1 4a 56 78 addi r5,#22136,r5
; CHECK-NEXT: 18: 08 00 ff fe b 0x18
; CHECK-NEXT: 1c: 60 00 00 19 nop
; CHECK-NEXT: 20: 60 00 00 19 nop

; llvm-objdump shows branch targets as addresses: branch + 4 * (disp + 2).
beq r1,r2,5
bltsq r3,r4,-3
nop
nop
; CHECK-NEXT: 24: 08 44 00 05 beq r1,r2,0x40
; CHECK-NEXT: 28: 18 c9 ff fd bltsq r3,r4,0x24
