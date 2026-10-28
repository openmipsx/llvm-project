; main's result is the exit status (crt0 calls exit(main(0, 0))).
; RUN: llc -mtriple=mipsx -filetype=obj %s -o %t.o
; RUN: %mipsx-link -o %t %t.o
; RUN: %mipsx-run --expect-exit=42 %t

define i32 @main() {
  %a = add i32 40, 2
  ret i32 %a
}
