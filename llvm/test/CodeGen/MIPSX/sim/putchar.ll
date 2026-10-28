; Output through the simulator's I/O page (docs/abi.md section 13.5): a byte
; stored to 0xFFFF0000 goes to stdout. The string is a global, read with
; byte loads.
; RUN: llc -mtriple=mipsx -filetype=obj %s -o %t.o
; RUN: %mipsx-link -o %t %t.o
; RUN: %mipsx-run --print-exit %t | FileCheck %s
; CHECK: Hi!
; CHECK-NEXT: exit: 0

@msg = constant [4 x i8] c"Hi!\0A"

define void @putc(i8 %c) {
  store volatile i8 %c, ptr inttoptr (i32 -65536 to ptr)
  ret void
}

define i32 @main() {
  %p0 = getelementptr [4 x i8], ptr @msg, i32 0, i32 0
  %c0 = load i8, ptr %p0
  call void @putc(i8 %c0)
  %p1 = getelementptr [4 x i8], ptr @msg, i32 0, i32 1
  %c1 = load i8, ptr %p1
  call void @putc(i8 %c1)
  %p2 = getelementptr [4 x i8], ptr @msg, i32 0, i32 2
  %c2 = load i8, ptr %p2
  call void @putc(i8 %c2)
  %p3 = getelementptr [4 x i8], ptr @msg, i32 0, i32 3
  %c3 = load i8, ptr %p3
  call void @putc(i8 %c3)
  ret i32 0
}
