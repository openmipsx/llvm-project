; RUN: not llc -mtriple=mipsx -filetype=null < %s 2>&1 | FileCheck %s

; Out-of-range immediates for the MIPS-X inline assembly constraints.

; CHECK: error: value out of range for constraint 'I'
define i32 @i_range(i32 %a) {
  %r = call i32 asm "addi $1,#$2,$0", "=r,r,I"(i32 %a, i32 65536)
  ret i32 %r
}

; CHECK: error: value out of range for constraint 'K'
define i32 @k_range(i32 %a) {
  %r = call i32 asm "asr $1,$0,#$2", "=r,r,K"(i32 %a, i32 32)
  ret i32 %r
}
