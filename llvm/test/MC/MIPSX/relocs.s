; Every relocation type of docs/abi.md section 12, with its addend.
; RUN: llvm-mc -triple=mipsx -filetype=obj %s -o %t.o
; RUN: llvm-readobj -r %t.o | FileCheck %s
; RUN: llvm-objdump -dr %t.o | FileCheck --check-prefix=DIS %s

.text
  bne r1,r2,ext+8
  nop
  nop
  call func
  nop
  nop
  jmp ext
  nop
  nop
  la var+8,r4
  ld %gprel(var+4)[gp],r5
  addi r0,#abs-4,r6
  jspci r0,#%w(func),r31
  addi r0,#%w(here),r7
here:
  .reloc ., R_MIPSX_NONE, func
  nop

.data
var:
  .word ext+12, %w(func+4), here
  .half ext
  .byte ext
  .byte 0
  .word ext - .

; CHECK:      Section ({{.*}}) .rela.text {
; CHECK-NEXT:   0x0 R_MIPSX_PC16 ext 0x8
; CHECK-NEXT:   0xC R_MIPSX_CALL17 func 0x0
; CHECK-NEXT:   0x18 R_MIPSX_CALL17 ext 0x0
; CHECK-NEXT:   0x24 R_MIPSX_HI17 .data 0x8
; CHECK-NEXT:   0x2C R_MIPSX_LO15 .data 0x8
; CHECK-NEXT:   0x30 R_MIPSX_GPREL17 .data 0x4
; CHECK-NEXT:   0x34 R_MIPSX_ABS17 abs 0xFFFFFFFC
; CHECK-NEXT:   0x38 R_MIPSX_WORD17 func 0x0
; CHECK-NEXT:   0x3C R_MIPSX_WORD17 .text 0x40
; CHECK-NEXT:   0x40 R_MIPSX_NONE func 0x0
; CHECK-NEXT: }
; CHECK:      Section ({{.*}}) .rela.data {
; CHECK-NEXT:   0x0 R_MIPSX_32 ext 0xC
; CHECK-NEXT:   0x4 R_MIPSX_32W func 0x4
; CHECK-NEXT:   0x8 R_MIPSX_32 .text 0x40
; CHECK-NEXT:   0xC R_MIPSX_16 ext 0x0
; CHECK-NEXT:   0xE R_MIPSX_8 ext 0x0
; CHECK-NEXT:   0x10 R_MIPSX_32_PCREL ext 0x0
; CHECK-NEXT: }

; The fields of relocated instructions are left zero (RELA).
; DIS:      0: 28 44 00 00 bne r1,r2,0x8
; DIS-NEXT:    00000000: R_MIPSX_PC16 ext+0x8
; DIS:      c: c0 3e 00 00 jspci r0,#0,r31
; DIS-NEXT:    0000000c: R_MIPSX_CALL17 func
; DIS:      18: c0 00 00 00 jspci r0,#0,r0
; DIS-NEXT:    00000018: R_MIPSX_CALL17 ext
; DIS:      24: e0 08 00 00 addi r0,#0,r4
; DIS-NEXT:    00000024: R_MIPSX_HI17 .data+0x8
; DIS-NEXT: 28: 48 08 42 38 lsl r4,r4,#15
; DIS-NEXT: 2c: e1 08 00 00 addi r4,#0,r4
; DIS-NEXT:    0000002c: R_MIPSX_LO15 .data+0x8
; DIS-NEXT: 30: 87 0a 00 00 ld 0[r28],r5
; DIS-NEXT:    00000030: R_MIPSX_GPREL17 .data+0x4
; DIS-NEXT: 34: e0 0c 00 00 addi r0,#0,r6
; DIS-NEXT:    00000034: R_MIPSX_ABS17 abs-0x4
; DIS-NEXT: 38: c0 3e 00 00 jspci r0,#0,r31
; DIS-NEXT:    00000038: R_MIPSX_WORD17 func
; DIS-NEXT: 3c: e0 0e 00 00 addi r0,#0,r7
; DIS-NEXT:    0000003c: R_MIPSX_WORD17 .text+0x40
