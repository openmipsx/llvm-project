; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; The calling convention of docs/abi.md section 7.

; The ninth and later argument words are on the stack at the CFA (sp at entry).
define i32 @tenth(i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6, i32 %a7, i32 %a8, i32 %a9, i32 %a10) {
; CHECK-LABEL: tenth:
; CHECK:      ld 4[r29],r2
  ret i32 %a10
}

; Outgoing stack arguments at sp + 0, + 4 in the reserved call frame; r31 is
; saved because the function calls.
define i32 @call_ten() {
; CHECK-LABEL: call_ten:
; CHECK:      addi r29,#-16,r29
; CHECK-NEXT: st 12[r29],r31
; CHECK-NEXT: addi r0,#10,r2
; CHECK-NEXT: st 4[r29],r2
; CHECK-NEXT: addi r0,#9,r2
; CHECK-NEXT: st 0[r29],r2
; CHECK-NEXT: addi r0,#1,r4
; CHECK-NEXT: addi r0,#2,r5
; CHECK-NEXT: addi r0,#3,r6
; CHECK-NEXT: addi r0,#4,r7
; CHECK-NEXT: addi r0,#5,r8
; CHECK-NEXT: addi r0,#6,r9
; CHECK-NEXT: addi r0,#7,r10
; CHECK-NEXT: addi r0,#8,r11
; CHECK-NEXT: jspci r0,#%call(tenth),r31
; CHECK-NEXT: nop
; CHECK-NEXT: nop
; CHECK-NEXT: ld 12[r29],r31
; CHECK-NEXT: nop
; CHECK-NEXT: addi r29,#16,r29
; CHECK-NEXT: ret
  %r = call i32 @tenth(i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10)
  ret i32 %r
}

; 64-bit values: most significant word first, in r4:r5 and r2:r3.
define i64 @ret64(i64 %a) {
; CHECK-LABEL: ret64:
; CHECK:      mov r5,r3
; CHECK-NEXT: mov r4,r2
  %r = add i64 %a, 0
  ret i64 %r
}

; No even-register alignment for 64-bit arguments.
define i64 @swap64(i32 %x, i64 %a) {
; CHECK-LABEL: swap64:
; CHECK:      mov r6,r3
; CHECK-NEXT: mov r5,r2
  ret i64 %a
}

; A 64-bit argument can straddle r11 and the stack: low word at sp + 0.
define i32 @split64(i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6, i32 %a7, i64 %b) {
; CHECK-LABEL: split64:
; CHECK:      ld 0[r29],r2
  %l = trunc i64 %b to i32
  ret i32 %l
}

declare void @takes_char(i8 signext)
; The caller extends narrow arguments.
define void @pass_char(i32 %x) {
; CHECK-LABEL: pass_char:
; CHECK:      addi r29,#-8,r29
; CHECK-NEXT: st 4[r29],r31
; CHECK-NEXT: lsl r4,r2,#24
; CHECK-NEXT: asr r2,r4,#24
; CHECK-NEXT: jspci r0,#%call(takes_char),r31
  %c = trunc i32 %x to i8
  call void @takes_char(i8 signext %c)
  ret void
}

; The callee extends narrow return values.
define signext i16 @ret_short(i32 %x) {
; CHECK-LABEL: ret_short:
; CHECK:      lsl r4,r2,#16
; CHECK-NEXT: asr r2,r2,#16
  %c = trunc i32 %x to i16
  ret i16 %c
}

%big = type { i32, i32, i32 }
; Larger return values go through a hidden pointer in r4; arguments start at r5.
define %big @ret_big(i32 %x) {
; CHECK-LABEL: ret_big:
; CHECK:      addi r0,#2,r2
; CHECK-NEXT: st 8[r4],r2
; CHECK-NEXT: addi r0,#1,r2
; CHECK-NEXT: st 4[r4],r2
; CHECK-NEXT: st 0[r4],r5
  %a = insertvalue %big undef, i32 %x, 0
  %b = insertvalue %big %a, i32 1, 1
  %c = insertvalue %big %b, i32 2, 2
  ret %big %c
}

; No multiply or divide instructions: the default library calls.
define i32 @mul(i32 %a, i32 %b) {
; CHECK-LABEL: mul:
; CHECK:      jspci r0,#%call(__mulsi3),r31
  %r = mul i32 %a, %b
  ret i32 %r
}

define i32 @udiv(i32 %a, i32 %b) {
; CHECK-LABEL: udiv:
; CHECK:      jspci r0,#%call(__udivsi3),r31
  %r = udiv i32 %a, %b
  ret i32 %r
}
