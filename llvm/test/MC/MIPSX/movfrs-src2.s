; movfrs keeps Dest in the Src2 field, bits 21-17, like addi; bits 16-12
; are 0 (processor spec §2, §6, "movfrs destination"; devices b6cdbcb).
; 0xD8040001 is OP 011 compute-immediate, Spec 1 (psw), r2 in bits 21-17.
; RUN: llvm-mc -triple=mipsx -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=mipsx -filetype=obj %s -o %t.o
; RUN: llvm-objdump --triple=mipsx -d %t.o | FileCheck --check-prefix=OBJ %s

movfrs psw,r2
; CHECK: movfrs psw,r2 ; encoding: [0xd8,0x04,0x00,0x01]
movfrs pcm4,r31
; CHECK: movfrs pcm4,r31 ; encoding: [0xd8,0x3e,0x00,0x04]

; OBJ: d8 04 00 01 movfrs psw,r2
; OBJ: d8 3e 00 04 movfrs pcm4,r31
