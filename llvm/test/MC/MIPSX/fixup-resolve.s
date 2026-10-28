; Fixups the assembler resolves itself: branches to labels in the same
; section, and operands whose value becomes a constant only at the end of
; assembly. No relocations are left.
; RUN: llvm-mc -triple=mipsx -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d %t.o | FileCheck %s
; RUN: llvm-readobj -r %t.o | FileCheck --check-prefix=RELOC %s
; RUN: llvm-objdump -s -j .data %t.o | FileCheck --check-prefix=DATA %s

; RELOC:      Relocations [
; RELOC-NEXT: ]

.text
back:
  nop
; disp = (target - branch) / 4 - 2
  beq r1,r2,fwd
; CHECK: 4: 08 44 00 03 beq r1,r2,0x18
  nop
  nop
  bltsq r3,r4,back
; CHECK: 10: 18 c9 ff fa bltsq r3,r4,0x0
  nop
fwd:
  nop
; A branch to itself and to the instruction after its delay slots.
self:
  b self
; CHECK: 1c: 08 00 ff fe b 0x1c
next0:
  b next
; CHECK: 20: 08 00 00 00 b 0x28
  nop
next:
  nop

; Forward-defined constants are resolved when assembly ends.
  addi r0,#c1,r2
; CHECK: e0 05 ff ff addi r0,#-1,r2
  addi r0,#c2,r2
; CHECK: e0 04 ff ff addi r0,#65535,r2
  addi r0,#c3,r2
; CHECK: e0 05 00 00 addi r0,#-65536,r2
  ld c4[r0],r2
; CHECK: 80 05 00 00 ld -65536[r0],r2
  jspci r0,#%w(c5),r31
; CHECK: c0 3e 04 00 jspci r0,#1024,r31
  addi r0,#%hi(c6),r2
; CHECK: e0 04 24 68 addi r0,#9320,r2
  addi r2,#%lo(c6),r2
; CHECK: e0 84 56 78 addi r2,#22136,r2
  addi r0,#%hi(c7),r2
; CHECK: e0 05 ff ff addi r0,#-1,r2
  addi r2,#%lo(c7),r2
; CHECK: e0 84 7f fc addi r2,#32764,r2
  la c6,r9
; CHECK: e0 12 24 68 addi r0,#9320,r9
; CHECK-NEXT: lsl r9,r9,#15
; CHECK-NEXT: e2 52 56 78 addi r9,#22136,r9

.data
  .word c6, %w(c5), c1
  .half c2, c1
  .byte c1
; DATA: 0000 12345678 00000400 ffffffff ffffffff
; DATA: 0010 ff

c1 = -1
c2 = 65535
c3 = -65536
c4 = 0xffff0000
c5 = 0x1000
c6 = 0x12345678
c7 = -4
