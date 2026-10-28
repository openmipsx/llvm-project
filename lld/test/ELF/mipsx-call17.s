# REQUIRES: mipsx
## R_MIPSX_CALL17 (docs/abi.md section 5.2): the linker tries r0 (base 0),
## r23 (__mipsx_cb0 / 4) and r24 (__mipsx_cb1 / 4) in that order, writes
## the first one that reaches the target into Src1 (bits 26-22), and keeps
## Rdest (call or jump).
# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -filetype=obj -triple=mipsx a.s -o a.o

## Defaults when the script does not define the bases: cb0 = .text +
## 0x40000 = 0x42000, cb1 = cb0 + 0x80000 = 0xc2000.
# RUN: ld.lld -T lds a.o -o out --defsym=far=0x2000
# RUN: llvm-objdump -d --no-show-raw-insn out | FileCheck %s
# RUN: llvm-readelf -s out | FileCheck --check-prefix=SYMS %s

# CHECK:      2000: jspci r0,#2072,r31
# CHECK:      200c: jspci r0,#65535,r31
# CHECK:      2018: jspci r23,#-2048,r31
# CHECK:      2024: jspci r23,#63488,r31
# CHECK:      2030: jspci r23,#65535,r0
# CHECK:      203c: jspci r24,#63488,r31
# CHECK:      2048: jspci r24,#65535,r0
# CHECK:      2054: jspci r0,#2048,r31
# SYMS-DAG: 00042000 0 NOTYPE LOCAL HIDDEN 1 __mipsx_cb0
# SYMS-DAG: 000c2000 0 NOTYPE LOCAL HIDDEN 1 __mipsx_cb1

## Beyond cb1's reach.
# RUN: not ld.lld -T lds a.o -o /dev/null --defsym=far=0x102000 2>&1 \
# RUN:   | FileCheck --check-prefix=ERR %s
# ERR: error: a.o:(.text+0x54): relocation R_MIPSX_CALL17 target 0x102000 (far) is out of reach of r0, __mipsx_cb0 and __mipsx_cb1; place the code nearer a code base or use -mlong-calls

## Bases defined by the script (or --defsym) are used as they are.
# RUN: ld.lld -T lds a.o -o out2 --defsym=__mipsx_cb0=0x100000 \
# RUN:   --defsym=__mipsx_cb1=0x80000 --defsym=far=0x102000
# RUN: llvm-objdump -d --no-show-raw-insn out2 | FileCheck --check-prefix=SCRIPT %s
# SCRIPT:      2000: jspci r0,#2072,r31
# SCRIPT:      200c: jspci r0,#65535,r31
# SCRIPT:      2018: jspci r24,#-65536,r31
# SCRIPT:      2024: jspci r24,#0,r31
# SCRIPT:      2030: jspci r24,#2047,r0
# SCRIPT:      203c: jspci r23,#0,r31
# SCRIPT:      2048: jspci r23,#2047,r0
# SCRIPT:      2054: jspci r23,#2048,r31

## A target that is not a multiple of 4.
# RUN: not ld.lld -T lds a.o -o /dev/null --defsym=far=0x2002 2>&1 \
# RUN:   | FileCheck --check-prefix=ALIGN %s
# ALIGN: error: a.o:(.text+0x54): improper alignment for relocation R_MIPSX_CALL17: 0x2002 is not aligned to 4 bytes

#--- lds
SECTIONS {
  .text 0x2000 : { *(.text) }
  .lo 0x3fffc : { *(.lo) }
  .cb0lo 0x40000 : { *(.cb0lo) }
  .cb0 0x80000 : { *(.cb0) }
  .cb0hi 0x81ffc : { *(.cb0hi) }
  .cb1 0x100000 : { *(.cb1) }
  .cb1hi 0x101ffc : { *(.cb1hi) }
}

#--- a.s
.globl _start
_start:
## Local code: through r0.
  call f_text
  nop
  nop
## The last word r0 reaches (0x3fffc / 4 = 65535).
  call f_lo
  nop
  nop
## Just beyond r0: through cb0, 0x40000 / 4 - 0x42000 / 4 = -2048.
  call f_cb0lo
  nop
  nop
  call f_cb0
  nop
  nop
  jmp f_cb0hi
  nop
  nop
  call f_cb1
  nop
  nop
  jmp f_cb1hi
  nop
  nop
  call far
  nop
  nop
f_text:
  ret
  nop
  nop

.section .lo,"ax"
f_lo: nop
.section .cb0lo,"ax"
f_cb0lo: nop
.section .cb0,"ax"
f_cb0: nop
.section .cb0hi,"ax"
f_cb0hi: nop
.section .cb1,"ax"
f_cb1: nop
.section .cb1hi,"ax"
f_cb1hi: nop
