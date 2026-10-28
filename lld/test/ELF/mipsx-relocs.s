# REQUIRES: mipsx
## Every MIPS-X relocation (MIPS-X SDK docs/abi.md section 12): symbol
## values and addends are bytes, branch displacements and %w are words.
# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -filetype=obj -triple=mipsx a.s -o a.o
# RUN: llvm-mc -filetype=obj -triple=mipsx b.s -o b.o
# RUN: ld.lld -T lds a.o b.o -o out --defsym=ext32=0x12345678 \
# RUN:   --defsym=ext16=0xfffe --defsym=ext8=0x80 --defsym=abs=0xffff0000
# RUN: llvm-objdump -d --no-show-raw-insn out | FileCheck %s
# RUN: llvm-objdump -s -j .data out | FileCheck --check-prefix=DATA %s

#--- lds
SECTIONS {
  .text 0x2000 : { *(.text) }
  .data 0x3000 : { *(.data) }
  .sdata 0x3100 : { *(.sdata) }
}

#--- a.s
.globl _start
_start:
## R_MIPSX_PC16: disp = (0x2034 - 0x2000) / 4 - 2 = 11.
  beq r1,r2,func
# CHECK:      2000: beq r1,r2,0x2034
  nop
  nop
## R_MIPSX_CALL17: func is in reach of r0; word 0x2034 / 4 = 2061.
  call func
# CHECK:      200c: jspci r0,#2061,r31
  nop
  nop
## R_MIPSX_HI17, R_MIPSX_LO15: var+4 = 0x3104.
  la var+4,r4
# CHECK:      2018: addi r0,#0,r4
# CHECK-NEXT: 201c: lsl r4,r4,#15
# CHECK-NEXT: 2020: addi r4,#12548,r4
## R_MIPSX_GPREL17: _gp defaults to .sdata + 0x10000 = 0x13100.
  ld %gprel(var)[gp],r5
# CHECK-NEXT: 2024: ld -65536[r28],r5
## R_MIPSX_ABS17: 0xffff0000 is -65536 as a signed 32-bit value.
  addi r0,#abs,r6
# CHECK-NEXT: 2028: addi r0,#-65536,r6
## R_MIPSX_WORD17.
  jspci r0,#%w(func),r31
# CHECK-NEXT: 202c: jspci r0,#2061,r31
## R_MIPSX_NONE changes nothing.
  .reloc ., R_MIPSX_NONE, func
  nop
# CHECK-NEXT: 2030: nop

.data
## R_MIPSX_32, R_MIPSX_32W, R_MIPSX_32_PCREL (0x2034 - 0x3008), R_MIPSX_16,
## R_MIPSX_8.
  .word ext32, %w(func), func - .
  .half ext16
  .byte ext8
# DATA: 3000 12345678 0000080d fffff02c fffe80

#--- b.s
.globl func, var
.text
func:
  ret
  nop
  nop
.section .sdata,"aw"
var:
  .word 0
