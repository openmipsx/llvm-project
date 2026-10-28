; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; Conditional branches (docs/abi.md section 5.3). Each function branches to its
; false block, so the condition is inverted; gt, le, ugt and ule swap the
; operands. Every branch has two delay-slot nops.

define void @br_eq(i32 %a, i32 %b, ptr %p) {
; CHECK-LABEL: br_eq:
; CHECK:       bne r4,r5,[[F:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       [[F]]:
entry:
  %c = icmp eq i32 %a, %b
  br i1 %c, label %t, label %f
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

define void @br_ne(i32 %a, i32 %b, ptr %p) {
; CHECK-LABEL: br_ne:
; CHECK:       beq r4,r5,[[F:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       [[F]]:
entry:
  %c = icmp ne i32 %a, %b
  br i1 %c, label %t, label %f
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

define void @br_slt(i32 %a, i32 %b, ptr %p) {
; CHECK-LABEL: br_slt:
; CHECK:       bge r4,r5,[[F:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       [[F]]:
entry:
  %c = icmp slt i32 %a, %b
  br i1 %c, label %t, label %f
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

define void @br_sge(i32 %a, i32 %b, ptr %p) {
; CHECK-LABEL: br_sge:
; CHECK:       blt r4,r5,[[F:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       [[F]]:
entry:
  %c = icmp sge i32 %a, %b
  br i1 %c, label %t, label %f
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

define void @br_sgt(i32 %a, i32 %b, ptr %p) {
; CHECK-LABEL: br_sgt:
; CHECK:       bge r5,r4,[[F:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       [[F]]:
entry:
  %c = icmp sgt i32 %a, %b
  br i1 %c, label %t, label %f
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

define void @br_sle(i32 %a, i32 %b, ptr %p) {
; CHECK-LABEL: br_sle:
; CHECK:       blt r5,r4,[[F:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       [[F]]:
entry:
  %c = icmp sle i32 %a, %b
  br i1 %c, label %t, label %f
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

define void @br_ult(i32 %a, i32 %b, ptr %p) {
; CHECK-LABEL: br_ult:
; CHECK:       bhs r4,r5,[[F:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       [[F]]:
entry:
  %c = icmp ult i32 %a, %b
  br i1 %c, label %t, label %f
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

define void @br_uge(i32 %a, i32 %b, ptr %p) {
; CHECK-LABEL: br_uge:
; CHECK:       blo r4,r5,[[F:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       [[F]]:
entry:
  %c = icmp uge i32 %a, %b
  br i1 %c, label %t, label %f
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

define void @br_ugt(i32 %a, i32 %b, ptr %p) {
; CHECK-LABEL: br_ugt:
; CHECK:       bhs r5,r4,[[F:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       [[F]]:
entry:
  %c = icmp ugt i32 %a, %b
  br i1 %c, label %t, label %f
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

define void @br_ule(i32 %a, i32 %b, ptr %p) {
; CHECK-LABEL: br_ule:
; CHECK:       blo r5,r4,[[F:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       [[F]]:
entry:
  %c = icmp ule i32 %a, %b
  br i1 %c, label %t, label %f
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

; A loop: the back edge branches on the condition itself.
define i32 @sum(i32 %n) {
; CHECK-LABEL: sum:
; CHECK:       mov r0,r3
; CHECK-NEXT:  mov r0,r2
; CHECK-NEXT:  [[L:.LBB[0-9_]+]]:
; CHECK:       add r2,r3,r2
; CHECK-NEXT:  addi r3,#1,r3
; CHECK-NEXT:  blt r3,r4,[[L]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  ; %bb.2:
; CHECK-NEXT:  ret
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %i1, %loop ]
  %s = phi i32 [ 0, %entry ], [ %s1, %loop ]
  %s1 = add i32 %s, %i
  %i1 = add i32 %i, 1
  %c = icmp slt i32 %i1, %n
  br i1 %c, label %loop, label %exit
exit:
  ret i32 %s1
}

; Comparing with a constant needs the constant in a register (r0 for 0).
define void @cmp_const(i32 %a, ptr %p) {
; CHECK-LABEL: cmp_const:
; CHECK:       addi r0,#100,[[C:r[0-9]+]]
; CHECK-NEXT:  blt [[C]],r4,
entry:
  %c = icmp sgt i32 %a, 100
  br i1 %c, label %f, label %t
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}

define void @cmp_zero(i32 %a, ptr %p) {
; CHECK-LABEL: cmp_zero:
; CHECK:       beq r4,r0,
entry:
  %c = icmp eq i32 %a, 0
  br i1 %c, label %f, label %t
t:
  store volatile i32 1, ptr %p
  ret void
f:
  ret void
}
