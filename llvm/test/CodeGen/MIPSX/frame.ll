; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; Stack frames (docs/abi.md section 8): sp 8-byte aligned, outgoing area at
; sp + 0, r31 saved by functions that call, callee-saved registers saved
; and restored, and frames beyond 17-bit offsets built in r1.

declare void @use(ptr)
declare i32 @get()

define void @leaf(i32 %x) {
; CHECK-LABEL: leaf:
; CHECK-NOT:   r29
; CHECK:       ret
  ret void
}

define void @local_array() {
; CHECK-LABEL: local_array:
; CHECK:       addi r29,#-24,r29
; CHECK-NEXT:  st 20[r29],r31
; CHECK-NEXT:  addi r0,#7,r2
; CHECK-NEXT:  st 12[r29],r2
; CHECK-NEXT:  addi r29,#4,r4
; CHECK-NEXT:  jspci r0,#%call(use),r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  ld 20[r29],r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  addi r29,#24,r29
; CHECK-NEXT:  ret
  %a = alloca [4 x i32]
  %p = getelementptr [4 x i32], ptr %a, i32 0, i32 2
  store i32 7, ptr %p
  call void @use(ptr %a)
  ret void
}

; A value live across a call stays in a callee-saved register.
define i32 @keep(i32 %x) {
; CHECK-LABEL: keep:
; CHECK:       addi r29,#-8,r29
; CHECK-NEXT:  st 4[r29],r31
; CHECK-NEXT:  st 0[r29],r16
; CHECK-NEXT:  mov r4,r16
; CHECK-NEXT:  jspci r0,#%call(get),r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  add r2,r16,r2
; CHECK-NEXT:  ld 0[r29],r16
; CHECK-NEXT:  nop
; CHECK-NEXT:  ld 4[r29],r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  addi r29,#8,r29
; CHECK-NEXT:  ret
  %a = call i32 @get()
  %b = add i32 %a, %x
  ret i32 %b
}

; 70008 bytes: the adjustment and the r31 slot (sp + 70004) are built in r1.
define void @big_frame() {
; CHECK-LABEL: big_frame:
; CHECK:       addi r0,#-3,r1
; CHECK-NEXT:  lsl r1,r1,#15
; CHECK-NEXT:  addi r1,#28296,r1
; CHECK-NEXT:  add r29,r1,r29
; CHECK-NEXT:  addi r0,#2,r1
; CHECK-NEXT:  lsl r1,r1,#15
; CHECK-NEXT:  addi r1,#4468,r1
; CHECK-NEXT:  add r29,r1,r1
; CHECK-NEXT:  st 0[r1],r31
; CHECK-NEXT:  addi r29,#4,r4
; CHECK:       ld 0[r1],r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  addi r0,#2,r1
; CHECK-NEXT:  lsl r1,r1,#15
; CHECK-NEXT:  addi r1,#4472,r1
; CHECK-NEXT:  add r29,r1,r29
; CHECK-NEXT:  ret
  %a = alloca [70000 x i8]
  call void @use(ptr %a)
  ret void
}
