; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; Frame pointer and dynamic allocas (docs/abi.md section 8.2): fp = CFA,
; r31 at fp - 4, the caller's fp at fp - 8. The body addresses the frame
; from fp; the epilogue resets sp from fp before restoring. With dynamic
; allocas each call moves sp around its outgoing arguments.

declare void @use(ptr)
declare void @nine(i32, i32, i32, i32, i32, i32, i32, i32, i32)

define void @dyn_call(i32 %n) {
; CHECK-LABEL: dyn_call:
; CHECK:       addi r29,#-8,r29
; CHECK-NEXT:  st 4[r29],r31
; CHECK-NEXT:  st 0[r29],r30
; CHECK-NEXT:  addi r29,#8,r30
; CHECK:       addi r0,#-8,r3
; CHECK-NEXT:  and r2,r3,r2
; CHECK-NEXT:  sub r29,r2,r4
; CHECK-NEXT:  mov r4,r29
; CHECK-NEXT:  jspci r0,#%call(use),r31
; CHECK:       addi r29,#-8,r29
; CHECK-NEXT:  addi r0,#9,r2
; CHECK-NEXT:  st 0[r29],r2
; CHECK:       jspci r0,#%call(nine),r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  addi r29,#8,r29
; CHECK-NEXT:  addi r30,#-8,r29
; CHECK-NEXT:  ld 0[r29],r30
; CHECK-NEXT:  nop
; CHECK-NEXT:  ld 4[r29],r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  addi r29,#8,r29
; CHECK-NEXT:  ret
  %a = alloca i32, i32 %n
  call void @use(ptr %a)
  call void @nine(i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9)
  ret void
}

; "frame-pointer"="all": incoming stack arguments are at fp + 0, + 4, ...
define i32 @arg_fp(i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6,
                   i32 %a7, i32 %a8, i32 %a9) "frame-pointer"="all" {
; CHECK-LABEL: arg_fp:
; CHECK:       addi r29,#-8,r29
; CHECK-NEXT:  st 4[r29],r31
; CHECK-NEXT:  st 0[r29],r30
; CHECK-NEXT:  addi r29,#8,r30
; CHECK-NEXT:  ld 0[r30],r2
  ret i32 %a9
}

; A large frame with a frame pointer: offsets from fp beyond 17 bits go
; through r1.
define void @big_fp(i32 %n) {
; CHECK-LABEL: big_fp:
; CHECK:       addi r0,#-3,r1
; CHECK-NEXT:  lsl r1,r1,#15
; CHECK-NEXT:  addi r1,#28296,r1
; CHECK-NEXT:  add r29,r1,r29
; CHECK:       addi r1,#4472,r1
; CHECK-NEXT:  add r29,r1,r30
; CHECK:       addi r1,#28296,r1
; CHECK-NEXT:  add r30,r1,r1
; CHECK-NEXT:  addi r1,#0,r4
; CHECK-NEXT:  jspci r0,#%call(use),r31
; CHECK:       addi r1,#28296,r1
; CHECK-NEXT:  add r30,r1,r29
  %b = alloca [70000 x i8]
  %a = alloca i32, i32 %n
  call void @use(ptr %b)
  ret void
}
