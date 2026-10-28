; Calls with stack arguments, 64-bit values split across r11 and the stack,
; values kept in callee-saved registers across calls, and sign and zero
; extension of narrow arguments and memory.
; The exit status is a sum of every result; it is 100 only if all are right.
; RUN: llc -mtriple=mipsx -filetype=obj %s -o %t.o
; RUN: %mipsx-link -o %t %t.o
; RUN: %mipsx-run --expect-exit=100 %t
; RUN: llc -mtriple=mipsx -O0 -filetype=obj %s -o %t0.o
; RUN: %mipsx-link -o %t0 %t0.o
; RUN: %mipsx-run --expect-exit=100 %t0

@bytes = global [4 x i8] c"\80\7F\FF\01"
@halves = global [2 x i16] [i16 -2, i16 65534]

; a1..a10: the ninth and tenth are on the stack.
define i32 @weighted(i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6,
                     i32 %a7, i32 %a8, i32 %a9, i32 %a10) noinline {
  %t9 = shl i32 %a9, 1
  %t10 = shl i32 %a10, 2
  %s1 = add i32 %a1, %a8
  %s2 = sub i32 %t9, %t10
  %s = add i32 %s1, %s2
  ret i32 %s
}

; The i64 straddles r11 (high word) and sp + 0 (low word).
define i32 @low_of_split(i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5,
                         i32 %a6, i32 %a7, i64 %b) noinline {
  %hi64 = lshr i64 %b, 32
  %hi = trunc i64 %hi64 to i32
  %lo = trunc i64 %b to i32
  %r = sub i32 %lo, %hi
  ret i32 %r
}

define i64 @make64(i32 %hi, i32 %lo) noinline {
  %h = zext i32 %hi to i64
  %l = zext i32 %lo to i64
  %hs = shl i64 %h, 32
  %r = or i64 %hs, %l
  ret i64 %r
}

define i32 @ext(i8 signext %s, i8 zeroext %z) noinline {
  %a = sext i8 %s to i32
  %b = zext i8 %z to i32
  %r = add i32 %a, %b
  ret i32 %r
}

define i32 @main() {
  ; 1 + 8 + 2*9 - 4*10 = -13
  %w = call i32 @weighted(i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7,
                          i32 8, i32 9, i32 10)
  ; 50 - 7 = 43
  %b = call i64 @make64(i32 7, i32 50)
  %l = call i32 @low_of_split(i32 0, i32 0, i32 0, i32 0, i32 0, i32 0,
                              i32 0, i64 %b)
  ; -1 + 255 = 254
  %e = call i32 @ext(i8 -1, i8 -1)
  ; Memory: -128 + 255 (ldb, ldbu) and -2 + 65534 (ldh, ldhu).
  %p0 = getelementptr [4 x i8], ptr @bytes, i32 0, i32 0
  %b0 = load i8, ptr %p0
  %b0s = sext i8 %b0 to i32
  %p2 = getelementptr [4 x i8], ptr @bytes, i32 0, i32 2
  %b2 = load i8, ptr %p2
  %b2z = zext i8 %b2 to i32
  %h0 = load i16, ptr @halves
  %h0s = sext i16 %h0 to i32
  %p1 = getelementptr [2 x i16], ptr @halves, i32 0, i32 1
  %h1 = load i16, ptr %p1
  %h1z = zext i16 %h1 to i32
  ; -13 + 43 + 254 + 127 + 65532 = 65943; minus 65843 = 100.
  %s1 = add i32 %w, %l
  %s2 = add i32 %s1, %e
  %s3 = add i32 %s2, %b0s
  %s4 = add i32 %s3, %b2z
  %s5 = add i32 %s4, %h0s
  %s6 = add i32 %s5, %h1z
  %r = sub i32 %s6, 65843
  ret i32 %r
}
