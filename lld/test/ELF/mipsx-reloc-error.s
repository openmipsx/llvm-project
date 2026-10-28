# REQUIRES: mipsx
## Values a MIPS-X relocation field cannot hold.
# RUN: llvm-mc -filetype=obj -triple=mipsx %s -o %t.o
# RUN: not ld.lld %t.o -o /dev/null -Ttext=0x2000 --defsym=far=0x22008 \
# RUN:   --defsym=big=0x10000 --defsym=wbig=0x40000 --defsym=wodd=0x1002 \
# RUN:   --defsym=_gp=0x100000 --defsym=h=0x10000 --defsym=b=0x100 2>&1 \
# RUN:   | FileCheck %s --implicit-check-not=error:

.globl _start
_start:
## (0x22008 - 0x2000) / 4 - 2 = 32768.
  beq r1,r2,far
# CHECK-DAG: error: {{.*}}:(.text+0x0): relocation R_MIPSX_PC16 out of range: 32768 is not in [-32768, 32767]
  beq r1,r2,wodd
# CHECK-DAG: error: {{.*}}:(.text+0x4): improper alignment for relocation R_MIPSX_PC16: 0xFFFFFFFFFFFFEFFE is not aligned to 4 bytes
  addi r0,#big,r2
# CHECK-DAG: error: {{.*}}:(.text+0x8): relocation R_MIPSX_ABS17 out of range: 65536 is not in [-65536, 65535]
  jspci r0,#%w(wbig),r31
# CHECK-DAG: error: {{.*}}:(.text+0xc): relocation R_MIPSX_WORD17 out of range: 65536 is not in [-65536, 65535]
  jspci r0,#%w(wodd),r31
# CHECK-DAG: error: {{.*}}:(.text+0x10): improper alignment for relocation R_MIPSX_WORD17: 0x1002 is not aligned to 4 bytes
  ld %gprel(_start)[gp],r2
# CHECK-DAG: error: {{.*}}:(.text+0x14): relocation R_MIPSX_GPREL17 out of range: -1040384 is not in [-65536, 65535]

.data
  .word %w(wodd)
# CHECK-DAG: error: {{.*}}:(.data+0x0): improper alignment for relocation R_MIPSX_32W: 0x1002 is not aligned to 4 bytes
  .half h
# CHECK-DAG: error: {{.*}}:(.data+0x4): relocation R_MIPSX_16 out of range: 65536 is not in [-32768, 65535]
  .byte b
# CHECK-DAG: error: {{.*}}:(.data+0x6): relocation R_MIPSX_8 out of range: 256 is not in [-128, 255]
