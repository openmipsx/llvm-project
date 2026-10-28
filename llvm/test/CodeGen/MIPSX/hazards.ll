; RUN: llc -mtriple=mipsx < %s | FileCheck %s
; RUN: llc -mtriple=mipsx -O0 < %s | FileCheck %s

; Until the delay slot filler exists (work package S11), every delay slot
; holds a nop and every load is followed by one (docs/abi.md section 9).

declare void @f()

define i32 @load(ptr %p) {
; CHECK-LABEL: load:
; CHECK:       ld 0[r4],r{{[0-9]+}}
; CHECK-NEXT:  nop
  %v = load i32, ptr %p
  ret i32 %v
}

define void @call() {
; CHECK-LABEL: call:
; CHECK:       jspci r0,#%call(f),r31
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK:       ret
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
  call void @f()
  ret void
}

; A load never writes its own address register (P2).
define i32 @chase(ptr %p) {
; CHECK-LABEL: chase:
; CHECK:       ld 0[r4],[[P:r[0-9]+]]
; CHECK-NEXT:  nop
; CHECK-NOT:   ld 0[[[P]]],[[P]]
; CHECK:       ld 0[[[P]]],r{{[0-9]+}}
; CHECK-NEXT:  nop
  %q = load ptr, ptr %p
  %v = load i32, ptr %q
  ret i32 %v
}

; An unconditional branch is beq r0,r0 (printed b) with two nops.
define void @spin(ptr %p) {
; CHECK-LABEL: spin:
; CHECK:       [[L:.LBB[0-9_]+]]:
; CHECK:       b [[L]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
entry:
  br label %loop
loop:
  store volatile i32 1, ptr %p
  br label %loop
}
