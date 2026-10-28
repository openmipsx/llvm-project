; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; Variadic arguments are always on the stack, after the named stack words;
; va_list is a pointer to the next word, and 64-bit values take two words
; with no extra alignment (docs/abi.md section 7.2). In these functions the
; frame is 8 bytes, so the CFA is sp + 8.

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)

define i32 @first(i32 %n, ...) {
; CHECK-LABEL: first:
; CHECK:       ld 8[r29],r2
  %ap = alloca ptr
  call void @llvm.va_start(ptr %ap)
  %a = va_arg ptr %ap, i32
  call void @llvm.va_end(ptr %ap)
  ret i32 %a
}

; After an i32 the i64 is at CFA + 4 (high word) and CFA + 8 (low word).
define i64 @second64(i32 %n, ...) {
; CHECK-LABEL: second64:
; CHECK:       ld 12[r29],r2
; CHECK:       ld 16[r29],r3
  %ap = alloca ptr
  call void @llvm.va_start(ptr %ap)
  %a = va_arg ptr %ap, i32
  %b = va_arg ptr %ap, i64
  call void @llvm.va_end(ptr %ap)
  ret i64 %b
}

; Nine named words: the ninth is at CFA + 0, so va_start is CFA + 4.
define ptr @after_named(i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6,
                        i32 %a7, i32 %a8, i32 %a9, ...) {
; CHECK-LABEL: after_named:
; CHECK:       addi r29,#12,r2
  %ap = alloca ptr
  call void @llvm.va_start(ptr %ap)
  %p = load ptr, ptr %ap
  ret ptr %p
}

; The caller puts variadic arguments on the stack even though r5-r11 are
; free: 2 at sp + 0, then the i64 3 as 0 at sp + 4 and 3 at sp + 8.
declare i32 @vf(i32, ...)
define i32 @caller() {
; CHECK-LABEL: caller:
; CHECK:       addi r0,#3,r2
; CHECK-NEXT:  st 8[r29],r2
; CHECK-NEXT:  st 4[r29],r0
; CHECK-NEXT:  addi r0,#2,r2
; CHECK-NEXT:  st 0[r29],r2
; CHECK-NEXT:  addi r0,#1,r4
; CHECK-NEXT:  jspci r0,#%call(vf),r31
  %r = call i32 (i32, ...) @vf(i32 1, i32 2, i64 3)
  ret i32 %r
}
