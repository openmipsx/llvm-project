; ELF header: EM_MIPS_X, big-endian ELF32, RELA, and e_flags from the CPU
; (docs/abi.md section 11).
; RUN: llvm-mc -triple=mipsx -filetype=obj %s -o %t.o
; RUN: llvm-readobj -h -S %t.o | FileCheck --check-prefixes=CHECK,ES3890 %s
; RUN: llvm-readelf -h %t.o | FileCheck --check-prefix=GNU %s
; RUN: llvm-mc -triple=mipsx -mcpu=es3890 -filetype=obj %s -o %t.es.o
; RUN: llvm-readobj -h %t.es.o | FileCheck --check-prefix=ES3890 %s
; RUN: llvm-mc -triple=mipsx -mcpu=mipsx -filetype=obj %s -o %t.generic.o
; RUN: llvm-readobj -h %t.generic.o | FileCheck --check-prefix=GENERIC %s
; RUN: llvm-objdump -d %t.o | FileCheck --check-prefix=OBJDUMP %s

  call f
  nop
  nop

; CHECK:      Class: 32-bit
; CHECK:      DataEncoding: BigEndian
; CHECK:      Type: Relocatable
; CHECK-NEXT: Machine: EM_MIPS_X
; ES3890:     Flags [ (0x1)
; ES3890-NEXT:  EF_MIPSX_ES3890 (0x1)
; ES3890-NEXT: ]
; GENERIC:    Flags [ (0x0)
; GENERIC-NEXT: ]
; CHECK:      Name: .rela.text
; CHECK-NEXT: Type: SHT_RELA

; GNU: Machine: Stanford MIPS-X
; GNU: Flags: 0x1, es3890

; OBJDUMP: file format elf32-mipsx
; OBJDUMP: jspci r0,#0,r31
