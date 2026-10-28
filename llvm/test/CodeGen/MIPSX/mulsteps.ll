; RUN: llc -mtriple=mipsx < %s | FileCheck %s
; RUN: llc -mtriple=mipsx -mipsx-mul-inline-max-bits=0 < %s \
; RUN:   | FileCheck %s --check-prefix=CALL

; A multiplication where known bits show that one operand fits in n <= 17
; bits (signed) is done inline: the multiplier, shifted to the top n bits of
; MD, then mstart and n - 1 msteps (manual Appendix IV). A nop separates
; movtos from mstart (abi section 9, P4), and two adjacent sequences, so that
; no movtos follows an mstep. Wider operands call __mulsi3.

; 16 bits: MD = y << 16, 15 msteps.
define i32 @mul_i16(i32 %x, i16 %y) {
; CHECK-LABEL: mul_i16:
; CHECK:       lsl r5,r3,#16
; CHECK-NEXT:  movtos r3,md
; CHECK-NEXT:  nop
; CHECK-NEXT:  mstart r4,r2
; CHECK-COUNT-15: mstep r2,r4,r2
; CHECK-NEXT:  ret
; CALL-LABEL: mul_i16:
; CALL:        jspci r0,#%call(__mulsi3),r31
; CALL-NOT:    mstep
  %e = sext i16 %y to i32
  %r = mul i32 %x, %e
  ret i32 %r
}

; An unsigned 16-bit value needs 17 bits, and either operand may be the
; narrow one.
define i32 @mul_u16(i16 zeroext %y, i32 %x) {
; CHECK-LABEL: mul_u16:
; CHECK:       lsl r4,r3,#15
; CHECK-NEXT:  movtos r3,md
; CHECK-NEXT:  nop
; CHECK-NEXT:  mstart r5,r2
; CHECK-COUNT-16: mstep r2,r5,r2
; CHECK-NEXT:  ret
  %e = zext i16 %y to i32
  %r = mul i32 %e, %x
  ret i32 %r
}

; The narrower operand is the multiplier: 8 bits here, 7 msteps.
define i32 @mul_u15_s8(i32 %a, i32 %b) {
; CHECK-LABEL: mul_u15_s8:
; CHECK:       movtos {{r[0-9]+}},md
; CHECK-NEXT:  nop
; CHECK-NEXT:  mstart {{r[0-9]+}},r2
; CHECK-COUNT-7: mstep r2,{{r[0-9]+}},r2
; CHECK-NOT:   mstep
; CHECK:       ret
  %x = and i32 %a, 32767
  %s = shl i32 %b, 24
  %y = ashr i32 %s, 24
  %r = mul i32 %x, %y
  ret i32 %r
}

; Two in a row: a nop between the last mstep and the next movtos.
define i32 @mul_two(i32 %x, i32 %y, i32 %z) {
; CHECK-LABEL: mul_two:
; CHECK:       movtos [[MD:r[0-9]+]],md
; CHECK-NEXT:  nop
; CHECK-NEXT:  mstart
; CHECK-COUNT-7: mstep
; CHECK-NEXT:  nop
; CHECK-NEXT:  movtos [[MD]],md
; CHECK-NEXT:  nop
; CHECK-NEXT:  mstart
; CHECK-COUNT-7: mstep
  %m = and i32 %y, 127
  %a = mul i32 %m, %x
  %b = mul i32 %m, %z
  %r = add i32 %a, %b
  ret i32 %r
}

; A constant with too many terms for shifts and adds, but 15 bits wide.
define i32 @mul_c(i32 %x) {
; CHECK-LABEL: mul_c:
; CHECK:       movtos {{r[0-9]+}},md
; CHECK-NEXT:  nop
; CHECK-NEXT:  mstart r4,r2
; CHECK-COUNT-14: mstep r2,r4,r2
; CHECK-NEXT:  ret
  %r = mul i32 %x, 12345
  ret i32 %r
}

; 18 bits: __mulsi3.
define i32 @mul_18(i32 %x, i32 %y) {
; CHECK-LABEL: mul_18:
; CHECK:       jspci r0,#%call(__mulsi3),r31
; CHECK-NOT:   mstep
; CHECK:       .Lfunc_end
  %m = and i32 %y, 131071
  %r = mul i32 %x, %m
  ret i32 %r
}

define i32 @mul_wide(i32 %x, i32 %y) {
; CHECK-LABEL: mul_wide:
; CHECK:       jspci r0,#%call(__mulsi3),r31
; CHECK-NOT:   mstep
; CHECK:       .Lfunc_end
  %r = mul i32 %x, %y
  ret i32 %r
}

; The callee may end with an mstep in its return delay slots (__mulsi3
; does), so a nop separates the return point from a movtos.
declare i32 @f()
define i32 @after_call(i32 %x, i16 %y) {
; CHECK-LABEL: after_call:
; CHECK:       jspci r0,#%call(f),r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  movtos {{r[0-9]+}},md
  %e = sext i16 %y to i32
  %a = mul i32 %x, %e
  %c = call i32 @f()
  %b = mul i32 %c, %e
  %r = add i32 %a, %b
  ret i32 %r
}

; At -Oz (minsize) the call is shorter.
define i32 @mul_i16_minsize(i32 %x, i16 %y) minsize {
; CHECK-LABEL: mul_i16_minsize:
; CHECK:       jspci r0,#%call(__mulsi3),r31
; CHECK-NOT:   mstep
; CHECK:       .Lfunc_end
  %e = sext i16 %y to i32
  %r = mul i32 %x, %e
  ret i32 %r
}
