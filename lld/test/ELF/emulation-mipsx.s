# REQUIRES: mipsx
## The -m elf32mipsx emulation and OUTPUT_FORMAT(elf32-mipsx), and e_flags:
## the output ORs the inputs' flags (docs/abi.md section 11.2) and rejects
## unknown bits.
# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -filetype=obj -triple=mipsx a.s -o es3890.o
# RUN: llvm-mc -filetype=obj -triple=mipsx -mcpu=mipsx a.s -o generic.o
# RUN: llvm-mc -filetype=obj -triple=mipsx -mcpu=mipsx b.s -o generic2.o
# RUN: ld.lld es3890.o -o out1
# RUN: llvm-readobj --file-headers out1 | FileCheck --check-prefixes=CHECK,ES3890 %s
# RUN: ld.lld -m elf32mipsx es3890.o -o out2
# RUN: cmp out1 out2
# RUN: echo 'OUTPUT_FORMAT(elf32-mipsx)' > fmt.lds
# RUN: ld.lld -T fmt.lds es3890.o -o out3
# RUN: llvm-readobj --file-headers out3 | FileCheck --check-prefixes=CHECK,ES3890 %s

# RUN: ld.lld generic.o -o out4
# RUN: llvm-readobj --file-headers out4 | FileCheck --check-prefix=GENERIC %s
# RUN: ld.lld generic2.o es3890.o -o out5
# RUN: llvm-readobj --file-headers out5 | FileCheck --check-prefix=ES3890 %s

## e_flags is the big-endian word at offset 36.
# RUN: llvm-mc -filetype=obj -triple=mipsx b.s -o bad.o
# RUN: %python -c "import sys; d = bytearray(open('bad.o','rb').read()); d[36:40] = (4).to_bytes(4,'big'); open('bad.o','wb').write(d)"
# RUN: not ld.lld es3890.o bad.o -o /dev/null 2>&1 | FileCheck --check-prefix=BAD %s

# CHECK:      Class: 32-bit (0x1)
# CHECK-NEXT: DataEncoding: BigEndian (0x2)
# CHECK:      Type: Executable (0x2)
# CHECK-NEXT: Machine: EM_MIPS_X (0x33)
# ES3890:     Flags [ (0x1)
# ES3890-NEXT:  EF_MIPSX_ES3890 (0x1)
# GENERIC:    Flags [ (0x0)
# BAD: error: bad.o: unknown e_flags 0x4

#--- a.s
.globl _start
_start:
  nop

#--- b.s
  nop

