; RUN: llc -mtriple=mipsx -mipsx-branch-disp-bits=5 < %s | FileCheck %s
; RUN: llc -mtriple=mipsx -mipsx-branch-disp-bits=5 -filetype=obj < %s \
; RUN:   | llvm-objdump -dr - | FileCheck --check-prefix=OBJ %s
; RUN: llc -mtriple=mipsx < %s | FileCheck --check-prefix=NEAR %s

; A conditional branch out of range (docs/abi.md section 5.5) becomes an
; inverted branch around jspci r0,#%call(label),r0. The hidden option
; shrinks the displacement to 5 bits so a short loop is enough.

define void @far(ptr %p, i32 %n) {
; CHECK-LABEL: far:
; CHECK:       [[LOOP:.LBB[0-9_]+]]:
; CHECK:       bge r{{[0-9]+}},r5,[[EXIT:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  ; %bb.3:
; CHECK-NEXT:  ;   in Loop
; CHECK-NEXT:  jspci r0,#%call([[LOOP]]),r0
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  [[EXIT]]:

; OBJ:      jspci r0,#0,r0
; OBJ-NEXT: R_MIPSX_CALL17 .text+0x

; NEAR-LABEL: far:
; NEAR:       blt r{{[0-9]+}},r5,
; NEAR-NOT:   jspci r0,#%call
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %i1, %loop ]
  store volatile i32 0, ptr %p
  store volatile i32 1, ptr %p
  store volatile i32 2, ptr %p
  store volatile i32 3, ptr %p
  store volatile i32 4, ptr %p
  store volatile i32 5, ptr %p
  store volatile i32 6, ptr %p
  store volatile i32 7, ptr %p
  store volatile i32 8, ptr %p
  store volatile i32 9, ptr %p
  store volatile i32 10, ptr %p
  store volatile i32 11, ptr %p
  store volatile i32 12, ptr %p
  store volatile i32 13, ptr %p
  store volatile i32 14, ptr %p
  store volatile i32 15, ptr %p
  store volatile i32 16, ptr %p
  store volatile i32 17, ptr %p
  store volatile i32 18, ptr %p
  store volatile i32 19, ptr %p
  %i1 = add i32 %i, 1
  %c = icmp slt i32 %i1, %n
  br i1 %c, label %loop, label %exit
exit:
  ret void
}
