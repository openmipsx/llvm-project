; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; 64-bit arithmetic is split into 32-bit words by the type legaliser, high
; word first (a in r4:r5, b in r6:r7, result in r2:r3). The carry comes from
; an unsigned compare; multiply, divide and variable shifts call the SDK's
; 64-bit helpers, whose clobber lists let values stay in r12-r15 across them.

define i64 @add64(i64 %a, i64 %b) {
; CHECK-LABEL: add64:
; CHECK:       add r5,r7,r3
; CHECK-NEXT:  blo r3,r5,[[C:.LBB[0-9_]+]]
; CHECK:       mov r0,r2
; CHECK:       [[C]]:
; CHECK-NEXT:  addi r0,#1,r2
; CHECK:       add r4,r6,r4
; CHECK-NEXT:  add r4,r2,r2
  %r = add i64 %a, %b
  ret i64 %r
}

define i64 @sub64(i64 %a, i64 %b) {
; CHECK-LABEL: sub64:
; CHECK:       blo r5,r7,
; CHECK:       sub r4,r6,r3
; CHECK-NEXT:  sub r3,r2,r2
; CHECK-NEXT:  sub r5,r7,r3
  %r = sub i64 %a, %b
  ret i64 %r
}

define i64 @mul64(i64 %a, i64 %b) {
; CHECK-LABEL: mul64:
; CHECK:       jspci r0,#%call(__muldi3),r31
  %r = mul i64 %a, %b
  ret i64 %r
}

define i64 @sdiv64(i64 %a, i64 %b) {
; CHECK-LABEL: sdiv64:
; CHECK:       jspci r0,#%call(__divdi3),r31
  %r = sdiv i64 %a, %b
  ret i64 %r
}

; The shift amount goes in r6 (mipsx-sdk rt/builtins/README.md).
define i64 @shl64(i64 %a, i64 %b) {
; CHECK-LABEL: shl64:
; CHECK:       mov r7,r6
; CHECK-NEXT:  jspci r0,#%call(__ashldi3),r31
  %r = shl i64 %a, %b
  ret i64 %r
}

define i64 @lshr64_const(i64 %a) {
; CHECK-LABEL: lshr64_const:
; CHECK:       lsr r4,r3,#8
; CHECK-NEXT:  mov r0,r2
  %r = lshr i64 %a, 40
  ret i64 %r
}

; __udivdi3 changes r1-r11: c moves to r12 rather than a callee-saved
; register.
define i64 @keep(i64 %a, i64 %b, i32 %c) {
; CHECK-LABEL: keep:
; CHECK:       mov r8,r12
; CHECK-NEXT:  jspci r0,#%call(__udivdi3),r31
; CHECK-NOT:   r16
; CHECK:       ret
  %r = udiv i64 %a, %b
  %e = zext i32 %c to i64
  %s = add i64 %r, %e
  ret i64 %s
}
