; Operand range and syntax errors.
; RUN: not llvm-mc -triple=mipsx %s 2>&1 | FileCheck %s

addi r1,#65536,r2
; CHECK: :[[@LINE-1]]:10: error: immediate must be in the range [-65536, 65535], a symbol, or %hi, %lo, %gprel or %w
addi r1,#-65537,r2
; CHECK: :[[@LINE-1]]:10: error: immediate must be in the range [-65536, 65535], a symbol, or %hi, %lo, %gprel or %w
ld 65536[r1],r2
; CHECK: :[[@LINE-1]]:4: error: offset must be in the range [-65536, 65535], a symbol, or %hi, %lo or %gprel
jspci r1,#-65537,r31
; CHECK: :[[@LINE-1]]:11: error: immediate must be in the range [-65536, 65535], a symbol, or %call or %w
beq r1,r2,32768
; CHECK: :[[@LINE-1]]:11: error: branch target must be a label or a displacement in the range [-32768, 32767]
bnesq r1,r2,-32769
; CHECK: :[[@LINE-1]]:13: error: branch target must be a label or a displacement in the range [-32768, 32767]
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

; Relocation operators in the wrong place (docs/abi.md section 10.6).
jspci r0,#%hi(f),r31
; CHECK: :[[@LINE-1]]:11: error: immediate must be in the range [-65536, 65535], a symbol, or %call or %w
addi r0,#%call(f),r2
; CHECK: :[[@LINE-1]]:10: error: immediate must be in the range [-65536, 65535], a symbol, or %hi, %lo, %gprel or %w
ld %w(f)[r0],r2
; CHECK: :[[@LINE-1]]:4: error: offset must be in the range [-65536, 65535], a symbol, or %hi, %lo or %gprel
beq r1,r2,%lo(f)
; CHECK: :[[@LINE-1]]:11: error: branch target must be a label or a displacement in the range [-32768, 32767]
addi r0,#%foo(f),r2
; CHECK: :[[@LINE-1]]:11: error: unknown relocation operator
addi r0,#%hi f,r2
; CHECK: :[[@LINE-1]]:14: error: expected '('
jspci r0,#%call(16),r31
; CHECK: :[[@LINE-1]]:11: error: %call needs a symbol
addi r0,#%w(6),r2
; CHECK: :[[@LINE-1]]:10: error: %w operand must be a multiple of 4
.word %hi(f)
; CHECK: :[[@LINE-1]]:7: error: only %w is allowed in data
.half %w(f)
; CHECK: :[[@LINE-1]]:7: error: %w is only allowed in 32-bit data
.half 65536
; CHECK: :[[@LINE-1]]:7: error: out of range literal value

; Pseudo-instructions.
li #f,r2
; CHECK: :[[@LINE-1]]:5: error: li needs a constant; use la for a symbol
li #0x100000000,r2
; CHECK: :[[@LINE-1]]:5: error: value does not fit in 32 bits
li r2,#5
; CHECK: :[[@LINE-1]]:1: error: expected 'li #value,rd'
la r2,f
; CHECK: :[[@LINE-1]]:1: error: expected 'la symbol,rd'
la %lo(f),r2
; CHECK: :[[@LINE-1]]:4: error: la takes an address, not a relocation operator
call 64
; CHECK: :[[@LINE-1]]:6: error: call needs a symbol
jmp r5
; CHECK: :[[@LINE-1]]:1: error: expected 'jmp symbol'
lsl r1,r2,#32
; CHECK: :[[@LINE-1]]:12: error: shift amount must be in the range [0, 31]
lsr r1,r2,#0
; CHECK: :[[@LINE-1]]:12: error: shift amount must be in the range [1, 32]
lsl r1,r2,#n
; CHECK: :[[@LINE-1]]:12: error: shift amount must be in the range [0, 31]
