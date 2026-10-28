; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; Arithmetic and logic, immediates and constants (docs/abi.md sections 6
; and 7: arguments in r4, r5, ..., result in r2).

define i32 @add(i32 %a, i32 %b) {
; CHECK-LABEL: add:
; CHECK:       add r4,r5,r2
; CHECK-NEXT:  ret
  %r = add i32 %a, %b
  ret i32 %r
}

define i32 @sub(i32 %a, i32 %b) {
; CHECK-LABEL: sub:
; CHECK:       sub r4,r5,r2
  %r = sub i32 %a, %b
  ret i32 %r
}

define i32 @and(i32 %a, i32 %b) {
; CHECK-LABEL: and:
; CHECK:       and r4,r5,r2
  %r = and i32 %a, %b
  ret i32 %r
}

define i32 @or(i32 %a, i32 %b) {
; CHECK-LABEL: or:
; CHECK:       or r4,r5,r2
  %r = or i32 %a, %b
  ret i32 %r
}

define i32 @xor(i32 %a, i32 %b) {
; CHECK-LABEL: xor:
; CHECK:       xor r4,r5,r2
  %r = xor i32 %a, %b
  ret i32 %r
}

define i32 @not(i32 %a) {
; CHECK-LABEL: not:
; CHECK:       not r4,r2
  %r = xor i32 %a, -1
  ret i32 %r
}

; bic rs1,rs2,rd is rs2 & ~rs1.
define i32 @andnot(i32 %a, i32 %b) {
; CHECK-LABEL: andnot:
; CHECK:       bic r5,r4,r2
  %n = xor i32 %b, -1
  %r = and i32 %a, %n
  ret i32 %r
}

define i32 @addi(i32 %a) {
; CHECK-LABEL: addi:
; CHECK:       addi r4,#65535,r2
  %r = add i32 %a, 65535
  ret i32 %r
}

define i32 @subi(i32 %a) {
; CHECK-LABEL: subi:
; CHECK:       addi r4,#-65536,r2
  %r = sub i32 %a, 65536
  ret i32 %r
}

; Constants: one addi within 17 bits, else addi / lsl 15 / addi.
define i32 @small_const() {
; CHECK-LABEL: small_const:
; CHECK:       addi r0,#-65536,r2
  ret i32 -65536
}

define i32 @big_const() {
; CHECK-LABEL: big_const:
; CHECK:       addi r0,#9320,r2
; CHECK-NEXT:  lsl r2,r2,#15
; CHECK-NEXT:  addi r2,#22136,r2
  ret i32 305419896
}

define i32 @zero() {
; CHECK-LABEL: zero:
; CHECK:       mov r0,r2
  ret i32 0
}

; Large operand of a logic operation: built in a register.
define i32 @and_const(i32 %a) {
; CHECK-LABEL: and_const:
; CHECK:       addi r0,#255,[[R:r[0-9]+]]
; CHECK-NEXT:  and r4,[[R]],r2
  %r = and i32 %a, 255
  ret i32 %r
}

; Shifts by a constant: lsl, lsr (sh) and asr.
define i32 @shl(i32 %a) {
; CHECK-LABEL: shl:
; CHECK:       lsl r4,r2,#3
  %r = shl i32 %a, 3
  ret i32 %r
}

define i32 @lshr(i32 %a) {
; CHECK-LABEL: lshr:
; CHECK:       lsr r4,r2,#31
  %r = lshr i32 %a, 31
  ret i32 %r
}

define i32 @ashr(i32 %a) {
; CHECK-LABEL: ashr:
; CHECK:       asr r4,r2,#7
  %r = ashr i32 %a, 7
  ret i32 %r
}

; sign_extend_inreg is a shift pair.
define i32 @sext_inreg(i32 %a) {
; CHECK-LABEL: sext_inreg:
; CHECK:       lsl r4,[[R:r[0-9]+]],#24
; CHECK-NEXT:  asr [[R]],r2,#24
  %t = trunc i32 %a to i8
  %r = sext i8 %t to i32
  ret i32 %r
}
