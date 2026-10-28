; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; Loads and stores of every width with sign and zero extension, and the
; reg+imm17 addressing mode.

define i32 @ld(ptr %p) {
; CHECK-LABEL: ld:
; CHECK:      ld 12[r4],r2
  %q = getelementptr i32, ptr %p, i32 3
  %v = load i32, ptr %q
  ret i32 %v
}

define i32 @ldb(ptr %p) {
; CHECK-LABEL: ldb:
; CHECK:      ldb -1[r4],r2
  %q = getelementptr i8, ptr %p, i32 -1
  %v = load i8, ptr %q
  %r = sext i8 %v to i32
  ret i32 %r
}

define i32 @ldbu(ptr %p) {
; CHECK-LABEL: ldbu:
; CHECK:      ldbu 0[r4],r2
  %v = load i8, ptr %p
  %r = zext i8 %v to i32
  ret i32 %r
}

define i32 @ldh(ptr %p) {
; CHECK-LABEL: ldh:
; CHECK:      ldh 2[r4],r2
  %q = getelementptr i16, ptr %p, i32 1
  %v = load i16, ptr %q
  %r = sext i16 %v to i32
  ret i32 %r
}

define i32 @ldhu(ptr %p) {
; CHECK-LABEL: ldhu:
; CHECK:      ldhu 0[r4],r2
  %v = load i16, ptr %p
  %r = zext i16 %v to i32
  ret i32 %r
}

define void @st(ptr %p, i32 %v) {
; CHECK-LABEL: st:
; CHECK:      st 65532[r4],r5
  %q = getelementptr i32, ptr %p, i32 16383
  store i32 %v, ptr %q
  ret void
}

define void @stb(ptr %p, i8 %v) {
; CHECK-LABEL: stb:
; CHECK:      stb 0[r4],r5
  store i8 %v, ptr %p
  ret void
}

define void @sth(ptr %p, i16 %v) {
; CHECK-LABEL: sth:
; CHECK:      sth 0[r4],r5
  store i16 %v, ptr %p
  ret void
}

; An offset beyond 17 bits is added to the base first.
define void @far_offset(ptr %p, i32 %v) {
; CHECK-LABEL: far_offset:
; CHECK:      addi r0,#2,r2
; CHECK-NEXT: lsl r2,r2,#15
; CHECK-NEXT: addi r2,#0,r2
; CHECK-NEXT: add r4,r2,r2
; CHECK-NEXT: st 0[r2],r5
  %q = getelementptr i8, ptr %p, i32 65536
  store i32 %v, ptr %q
  ret void
}

; A constant address in the top 64 KB uses r0 (the simulator's I/O page).
define void @io_page(i8 %c) {
; CHECK-LABEL: io_page:
; CHECK:      stb -65536[r0],r4
  store volatile i8 %c, ptr inttoptr (i32 -65536 to ptr)
  ret void
}

; Zero is r0.
define void @st_zero(ptr %p) {
; CHECK-LABEL: st_zero:
; CHECK:      st 0[r4],r0
  store i32 0, ptr %p
  ret void
}

define i1 @ld_i1(ptr %p) {
; CHECK-LABEL: ld_i1:
; CHECK:      ldbu 0[r4],r2
  %v = load i1, ptr %p
  ret i1 %v
}

; The loaded pointer is used after a nop, and a load never overwrites its
; address register (docs/abi.md section 9, P1 and P2).
define i32 @chase(ptr %p) {
; CHECK-LABEL: chase:
; CHECK:      ld 0[r4],[[P:r[0-9]+]]
; CHECK-NEXT: nop
; CHECK-NEXT: ld 0[[[P]]],r2
  %q = load ptr, ptr %p
  %v = load i32, ptr %q
  ret i32 %v
}
