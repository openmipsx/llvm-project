; Pseudo-instructions and swapped branch conditions (docs/abi.md sections 6
; and 10.5).
; RUN: llvm-mc -triple=mipsx -show-encoding %s | FileCheck %s

; li: one instruction if the value fits in 17 bits, two if it is a 17-bit
; value shifted left, otherwise the hi/lo form.
li #5,r2
; CHECK: addi r0,#5,r2 ; encoding: [0xe0,0x04,0x00,0x05]
li #-65536,r2
; CHECK: addi r0,#-65536,r2 ; encoding: [0xe0,0x05,0x00,0x00]
li #0x10000,r3
; CHECK: addi r0,#1,r3 ; encoding: [0xe0,0x06,0x00,0x01]
; CHECK-NEXT: lsl r3,r3,#16 ; encoding: [0x48,0x06,0x32,0x41]
li #0x80000000,r6
; CHECK: addi r0,#-1,r6 ; encoding: [0xe0,0x0d,0xff,0xff]
; CHECK-NEXT: lsl r6,r6,#31 ; encoding: [0x48,0x0c,0x62,0x78]
li #0x12345678,r4
; CHECK: addi r0,#9320,r4 ; encoding: [0xe0,0x08,0x24,0x68]
; CHECK-NEXT: lsl r4,r4,#15 ; encoding: [0x48,0x08,0x42,0x38]
; CHECK-NEXT: addi r4,#22136,r4 ; encoding: [0xe1,0x08,0x56,0x78]
li #0xfffe1234,r4
; CHECK: addi r0,#-31603,r4 ; encoding: [0xe0,0x09,0x84,0x8d]
; CHECK-NEXT: lsl r4,r4,#2 ; encoding: [0x48,0x08,0x42,0x04]
li #0xfffe1235,r4
; CHECK: addi r0,#-4,r4 ; encoding: [0xe0,0x09,0xff,0xfc]
; CHECK-NEXT: lsl r4,r4,#15 ; encoding: [0x48,0x08,0x42,0x38]
; CHECK-NEXT: addi r4,#4661,r4 ; encoding: [0xe1,0x08,0x12,0x35]

; la: always three instructions.
la foo+8,r7
; CHECK: addi r0,#%hi(foo+8),r7 ; encoding: [0xe0,0b0000111A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %hi(foo+8), kind: fixup_mipsx_hi17
; CHECK-NEXT: lsl r7,r7,#15 ; encoding: [0x48,0x0e,0x72,0x38]
; CHECK-NEXT: addi r7,#%lo(foo+8),r7 ; encoding: [0xe1,0b1100111A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %lo(foo+8), kind: fixup_mipsx_lo15
la 0x8000,r8
; CHECK: addi r0,#1,r8 ; encoding: [0xe0,0x10,0x00,0x01]
; CHECK-NEXT: lsl r8,r8,#15 ; encoding: [0x48,0x10,0x82,0x38]
; CHECK-NEXT: addi r8,#0,r8 ; encoding: [0xe2,0x10,0x00,0x00]

; call and jmp: jspci through a linker-selected base.
call bar
; CHECK: jspci r0,#%call(bar),r31 ; encoding: [0xc0,0b0011111A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %call(bar), kind: fixup_mipsx_call17
jmp baz
; CHECK: jspci r0,#%call(baz),r0 ; encoding: [0xc0,0b0000000A,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: %call(baz), kind: fixup_mipsx_call17

; Swapped branch conditions.
bgt r1,r2,10
; CHECK: blt r2,r1,10 ; encoding: [0x18,0x82,0x00,0x0a]
ble r1,r2,10
; CHECK: bge r2,r1,10 ; encoding: [0x38,0x82,0x00,0x0a]
bhi r1,r2,10
; CHECK: blo r2,r1,10 ; encoding: [0x30,0x82,0x00,0x0a]
bls r1,r2,10
; CHECK: bhs r2,r1,10 ; encoding: [0x10,0x82,0x00,0x0a]
bgtsq r1,r2,10
; CHECK: bltsq r2,r1,10 ; encoding: [0x18,0x83,0x00,0x0a]
blesq r1,r2,10
; CHECK: bgesq r2,r1,10 ; encoding: [0x38,0x83,0x00,0x0a]
bhisq r1,r2,lab
; CHECK: blosq r2,r1,lab ; encoding: [0x30,0x83,A,A]
; CHECK-NEXT: ; fixup A - offset: 0, value: lab, kind: fixup_mipsx_pc16
blssq r1,r2,10
; CHECK: bhssq r2,r1,10 ; encoding: [0x10,0x83,0x00,0x0a]
