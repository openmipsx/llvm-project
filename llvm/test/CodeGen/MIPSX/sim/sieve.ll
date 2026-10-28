; Sieve of Eratosthenes below 2000 with byte flags (benchmark for the delay
; slot filler; the prime count is checked).
; RUN: llc -mtriple=mipsx -filetype=obj %s -o %t.o
; RUN: %mipsx-link --builtins -o %t %t.o
; RUN: %mipsx-run --expect-exit=0 %t
; RUN: llc -mtriple=mipsx -O0 -filetype=obj %s -o %t0.o
; RUN: %mipsx-link --builtins -o %t0 %t0.o
; RUN: %mipsx-run --expect-exit=0 %t0
@flags = global [2000 x i8] zeroinitializer

define i32 @sieve(ptr %f, i32 %n) noinline {
entry:
  br label %outer
outer:
  %i = phi i32 [ 2, %entry ], [ %i1, %outer.next ]
  %count = phi i32 [ 0, %entry ], [ %count1, %outer.next ]
  %pi = getelementptr i8, ptr %f, i32 %i
  %fl = load i8, ptr %pi
  %comp = icmp ne i8 %fl, 0
  br i1 %comp, label %outer.next, label %prime
prime:
  %c1 = add i32 %count, 1
  %start = add i32 %i, %i
  br label %mark
mark:
  %j = phi i32 [ %start, %prime ], [ %j1, %mark.body ]
  %jin = icmp ult i32 %j, %n
  br i1 %jin, label %mark.body, label %outer.next
mark.body:
  %pj = getelementptr i8, ptr %f, i32 %j
  store i8 1, ptr %pj
  %j1 = add i32 %j, %i
  br label %mark
outer.next:
  %count1 = phi i32 [ %count, %outer ], [ %c1, %mark ]
  %i1 = add i32 %i, 1
  %more = icmp ult i32 %i1, %n
  br i1 %more, label %outer, label %exit
exit:
  ret i32 %count1
}

define i32 @main() {
  %r = call i32 @sieve(ptr @flags, i32 2000)
  %bad = icmp ne i32 %r, 303
  br i1 %bad, label %fail, label %ok
fail:
  ret i32 1
ok:
  ret i32 0
}
