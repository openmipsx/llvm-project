; Operand range and syntax errors.
; RUN: not llvm-mc -triple=mipsx %s 2>&1 | FileCheck %s

addi r1,#65536,r2
; CHECK: :[[@LINE-1]]:10: error: immediate must be in the range [-65536, 65535]
addi r1,#-65537,r2
; CHECK: :[[@LINE-1]]:10: error: immediate must be in the range [-65536, 65535]
ld 65536[r1],r2
; CHECK: :[[@LINE-1]]:4: error: immediate must be in the range [-65536, 65535]
jspci r1,#-65537,r31
; CHECK: :[[@LINE-1]]:11: error: immediate must be in the range [-65536, 65535]
beq r1,r2,32768
; CHECK: :[[@LINE-1]]:11: error: branch displacement must be in the range [-32768, 32767]
bnesq r1,r2,-32769
; CHECK: :[[@LINE-1]]:13: error: branch displacement must be in the range [-32768, 32767]
sh r1,r2,r3,#0
; CHECK: :[[@LINE-1]]:14: error: shift amount must be in the range [1, 32]
sh r1,r2,r3,#33
; CHECK: :[[@LINE-1]]:14: error: shift amount must be in the range [1, 32]
asr r1,r2,#0
; CHECK: :[[@LINE-1]]:12: error: shift amount must be in the range [1, 32]
trap #256
; CHECK: :[[@LINE-1]]:7: error: trap vector must be in the range [0, 255]
trap #-1
; CHECK: :[[@LINE-1]]:7: error: trap vector must be in the range [0, 255]
add r1,r2
; CHECK: :[[@LINE-1]]:1: error: too few operands for instruction
frob r1,r2,r3
; CHECK: :[[@LINE-1]]:1: error: unrecognized instruction mnemonic
movfrs pcm1,r1
; CHECK: :[[@LINE-1]]:8: error: invalid operand for instruction
movtos r1,pcm4
; CHECK: :[[@LINE-1]]:11: error: invalid operand for instruction
add r1,r2,r32
; CHECK: :[[@LINE-1]]:11: error: invalid operand for instruction
ld 0[r1,r2
; CHECK: :[[@LINE-1]]:8: error: expected ']'
ld 0[4],r2
; CHECK: :[[@LINE-1]]:6: error: expected base register
