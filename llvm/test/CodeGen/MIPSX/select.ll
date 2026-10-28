; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; select and setcc have no instructions: they become a branch around a
; move (a select_cc diamond).

define i32 @max(i32 %a, i32 %b) {
; CHECK-LABEL: max:
; CHECK:       mov r4,r2
; CHECK-NEXT:  blt r5,r2,[[J:.LBB[0-9_]+]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  ; %bb.1:
; CHECK-NEXT:  mov r5,r2
; CHECK-NEXT:  [[J]]:
; CHECK-NEXT:  ret
  %c = icmp sgt i32 %a, %b
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}

define i32 @umin(i32 %a, i32 %b) {
; CHECK-LABEL: umin:
; CHECK:       mov r4,r2
; CHECK-NEXT:  blo r2,r5,[[J:.LBB[0-9_]+]]
; CHECK:       mov r5,r2
; CHECK-NEXT:  [[J]]:
  %c = icmp ult i32 %a, %b
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}

define i32 @setlt(i32 %a, i32 %b) {
; CHECK-LABEL: setlt:
; CHECK:       blt r4,r5,[[T:.LBB[0-9_]+]]
; CHECK:       mov r0,r2
; CHECK-NEXT:  ret
; CHECK:       [[T]]:
; CHECK-NEXT:  addi r0,#1,r2
; CHECK-NEXT:  ret
  %c = icmp slt i32 %a, %b
  %r = zext i1 %c to i32
  ret i32 %r
}

define i32 @seteq(i32 %a, i32 %b) {
; CHECK-LABEL: seteq:
; CHECK:       beq r4,r5,[[T:.LBB[0-9_]+]]
; CHECK:       mov r0,r2
; CHECK:       [[T]]:
; CHECK-NEXT:  addi r0,#1,r2
  %c = icmp eq i32 %a, %b
  %r = zext i1 %c to i32
  ret i32 %r
}

; An i1 argument is tested against zero after masking.
define i32 @select_bool(i1 %c, i32 %a, i32 %b) {
; CHECK-LABEL: select_bool:
; CHECK:       mov r5,r2
; CHECK-NEXT:  addi r0,#1,r3
; CHECK-NEXT:  and r4,r3,r3
; CHECK-NEXT:  bne r3,r0,[[J:.LBB[0-9_]+]]
; CHECK:       mov r6,r2
; CHECK-NEXT:  [[J]]:
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}
