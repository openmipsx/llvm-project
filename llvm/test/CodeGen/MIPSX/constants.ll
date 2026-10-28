; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; 32-bit constants (docs/abi.md section 6.1): one addi within 17 bits, two
; instructions for a 17-bit value shifted left, otherwise addi / lsl 15 /
; addi.

define i32 @fits17() {
; CHECK-LABEL: fits17:
; CHECK:       addi r0,#65535,r2
; CHECK-NEXT:  ret
  ret i32 65535
}

define i32 @shifted() {
; CHECK-LABEL: shifted:
; CHECK:       addi r0,#1,r2
; CHECK-NEXT:  lsl r2,r2,#16
; CHECK-NEXT:  ret
  ret i32 65536
}

define i32 @int_min() {
; CHECK-LABEL: int_min:
; CHECK:       addi r0,#-1,r2
; CHECK-NEXT:  lsl r2,r2,#31
; CHECK-NEXT:  ret
  ret i32 -2147483648
}

define i32 @shifted_neg() {
; CHECK-LABEL: shifted_neg:
; CHECK:       addi r0,#-31603,r2
; CHECK-NEXT:  lsl r2,r2,#2
; CHECK-NEXT:  ret
  ret i32 -126412
}

define i32 @hilo() {
; CHECK-LABEL: hilo:
; CHECK:       addi r0,#-4,r2
; CHECK-NEXT:  lsl r2,r2,#15
; CHECK-NEXT:  addi r2,#4661,r2
; CHECK-NEXT:  ret
  ret i32 -126411
}

define i32 @all_ones() {
; CHECK-LABEL: all_ones:
; CHECK:       addi r0,#-1,r2
; CHECK-NEXT:  ret
  ret i32 -1
}
