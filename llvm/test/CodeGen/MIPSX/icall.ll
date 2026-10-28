; RUN: llc -mtriple=mipsx < %s | FileCheck %s
; RUN: llc -mtriple=mipsx -mattr=+long-calls < %s \
; RUN:   | FileCheck --check-prefix=LONG %s

; Function pointers are byte addresses (docs/abi.md section 4): an indirect
; call shifts right by 2 and jumps with jspci rs,#0,r31 (abi section 5.6).
; With long-calls, direct calls take the address with %hi/%lo the same way
; (abi section 5.4).

define i32 @icall(ptr %f, i32 %x) {
; CHECK-LABEL: icall:
; CHECK:       lsr r4,[[T:r[0-9]+]],#2
; CHECK-NEXT:  mov r5,r4
; CHECK-NEXT:  jspci [[T]],#0,r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
  %r = call i32 %f(i32 %x)
  ret i32 %r
}

; A pointer loaded from memory is used after its load delay.
define void @table(ptr %t) {
; CHECK-LABEL: table:
; CHECK:       ld 8[r4],[[P:r[0-9]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  lsr [[P]],[[T:r[0-9]+]],#2
; CHECK-NEXT:  jspci [[T]],#0,r31
  %p = getelementptr ptr, ptr %t, i32 2
  %f = load ptr, ptr %p
  call void %f()
  ret void
}

declare i32 @f(i32)
define i32 @direct(i32 %x) {
; CHECK-LABEL: direct:
; CHECK:       jspci r0,#%call(f),r31
; LONG-LABEL:  direct:
; LONG:        addi r0,#%hi(f),[[R:r[0-9]+]]
; LONG-NEXT:   lsl [[R]],[[R]],#15
; LONG-NEXT:   addi [[R]],#%lo(f),[[R]]
; LONG-NEXT:   lsr [[R]],[[T:r[0-9]+]],#2
; LONG-NEXT:   jspci [[T]],#0,r31
  %r = call i32 @f(i32 %x)
  ret i32 %r
}
