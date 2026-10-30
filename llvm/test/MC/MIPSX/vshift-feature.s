; The shifts by a register amount need the vshift feature, which the
; default CPU has and -mcpu=mipsx does not (docs/abi.md section 10.3).
; RUN: llvm-mc -triple=mipsx -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=mipsx -mcpu=es3890 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=mipsx -mcpu=mipsx -mattr=+vshift -show-encoding %s \
; RUN:   | FileCheck %s
; RUN: not llvm-mc -triple=mipsx -mcpu=mipsx %s 2>&1 \
; RUN:   | FileCheck --check-prefix=ERR %s
; RUN: not llvm-mc -triple=mipsx -mcpu=mipsx -mattr=+es3890 %s 2>&1 \
; RUN:   | FileCheck --check-prefix=ERR %s

rotlb r1,r2,r3
; CHECK: rotlb r1,r2,r3 ; encoding: [0x48,0x44,0x30,0xc0]
; ERR-NOT: :[[@LINE-2]]:{{.*}}error
shlv r1,r2,r3
; CHECK: shlv r1,r2,r3 ; encoding: [0x48,0x44,0x30,0x80]
; ERR: :[[@LINE-2]]:1: error: instruction requires the ES3890 variable shift (-mcpu=es3890 or -mattr=+vshift)
shav r1,r2,r3
; CHECK: shav r1,r2,r3 ; encoding: [0x48,0x44,0x31,0x80]
; ERR: :[[@LINE-2]]:1: error: instruction requires the ES3890 variable shift (-mcpu=es3890 or -mattr=+vshift)
