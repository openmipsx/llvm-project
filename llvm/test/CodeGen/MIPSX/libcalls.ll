; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; Variable shifts, multiply and divide call the MIPS-X SDK helpers. A call
; to a helper preserves every register its clobber list (mipsx-sdk
; rt/builtins/README.md) leaves alone, so values stay in caller-saved
; registers across it.

define i32 @shl(i32 %a, i32 %b) {
; CHECK-LABEL: shl:
; CHECK:       jspci r0,#%call(__ashlsi3),r31
  %r = shl i32 %a, %b
  ret i32 %r
}

define i32 @ashr(i32 %a, i32 %b) {
; CHECK-LABEL: ashr:
; CHECK:       jspci r0,#%call(__ashrsi3),r31
  %r = ashr i32 %a, %b
  ret i32 %r
}

; __lshrsi3 changes only r1-r3: the shift amount stays in r5.
define i32 @lshr_keep_r5(i32 %a, i32 %b) {
; CHECK-LABEL: lshr_keep_r5:
; CHECK-NOT:   mov
; CHECK:       jspci r0,#%call(__lshrsi3),r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  add r2,r5,r2
  %r = lshr i32 %a, %b
  %s = add i32 %r, %b
  ret i32 %s
}

; __ashlsi3 also changes r5, so the amount is copied first; r6 survives.
define i32 @shl_keep(i32 %a, i32 %b) {
; CHECK-LABEL: shl_keep:
; CHECK:       mov r5,r6
; CHECK-NEXT:  jspci r0,#%call(__ashlsi3),r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  add r2,r6,r2
  %r = shl i32 %a, %b
  %s = add i32 %r, %b
  ret i32 %s
}

define i32 @mul_keep(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: mul_keep:
; CHECK:       jspci r0,#%call(__mulsi3),r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  add r2,r6,r2
  %r = mul i32 %a, %b
  %s = add i32 %r, %c
  ret i32 %s
}

define i32 @sdiv_keep(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: sdiv_keep:
; CHECK:       jspci r0,#%call(__divsi3),r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  add r2,r6,r2
  %r = sdiv i32 %a, %b
  %s = add i32 %r, %c
  ret i32 %s
}

; A call to an ordinary function clobbers r2-r15: c is saved in r16.
declare i32 @f(i32, i32)
define i32 @ordinary(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: ordinary:
; CHECK:       mov r6,r16
; CHECK:       jspci r0,#%call(f),r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  add r2,r16,r2
  %r = call i32 @f(i32 %a, i32 %b)
  %s = add i32 %r, %c
  ret i32 %s
}
