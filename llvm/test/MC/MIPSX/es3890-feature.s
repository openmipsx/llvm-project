; The byte and halfword memory instructions need the es3890 feature, which
; the default CPU has and -mcpu=mipsx does not.
; RUN: llvm-mc -triple=mipsx -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=mipsx -mcpu=es3890 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=mipsx -mcpu=mipsx -mattr=+es3890 -show-encoding %s \
; RUN:   | FileCheck %s
; RUN: not llvm-mc -triple=mipsx -mcpu=mipsx %s 2>&1 \
; RUN:   | FileCheck --check-prefix=ERR %s

ld 0[r1],r2
; CHECK: ld 0[r1],r2 ; encoding: [0x80,0x44,0x00,0x00]
; ERR-NOT: :[[@LINE-2]]:{{.*}}error
ldbu 1[r1],r2
; CHECK: ldbu 1[r1],r2 ; encoding: [0xa0,0x44,0x00,0x01]
; ERR: :[[@LINE-2]]:1: error: instruction requires the ES3890 (-mcpu=es3890)
ldb 1[r1],r2
; CHECK: ldb 1[r1],r2 ; encoding: [0xa8,0x44,0x00,0x01]
; ERR: :[[@LINE-2]]:1: error: instruction requires the ES3890 (-mcpu=es3890)
ldhu 2[r1],r2
; CHECK: ldhu 2[r1],r2 ; encoding: [0x88,0x44,0x00,0x02]
; ERR: :[[@LINE-2]]:1: error: instruction requires the ES3890 (-mcpu=es3890)
ldh 2[r1],r2
; CHECK: ldh 2[r1],r2 ; encoding: [0xb8,0x44,0x00,0x02]
; ERR: :[[@LINE-2]]:1: error: instruction requires the ES3890 (-mcpu=es3890)
stb 1[r1],r2
; CHECK: stb 1[r1],r2 ; encoding: [0x98,0x44,0x00,0x01]
; ERR: :[[@LINE-2]]:1: error: instruction requires the ES3890 (-mcpu=es3890)
sth 2[r1],r2
; CHECK: sth 2[r1],r2 ; encoding: [0xb0,0x44,0x00,0x02]
; ERR: :[[@LINE-2]]:1: error: instruction requires the ES3890 (-mcpu=es3890)
