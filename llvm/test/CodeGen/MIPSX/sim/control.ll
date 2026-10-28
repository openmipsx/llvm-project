; Loops, a switch (jump table), globals, selects, variable shifts, multiply
; and divide, run on the simulator with the SDK's helper routines. main
; prints a string with a loop of byte loads, then checks each result and
; exits with a different code for each wrong one (expected values computed
; independently of LLVM).
; RUN: llc -mtriple=mipsx -filetype=obj %s -o %t.o
; RUN: %mipsx-link --builtins -o %t %t.o
; RUN: %mipsx-run --expect-exit=0 %t | FileCheck %s
; RUN: llc -mtriple=mipsx -O0 -filetype=obj %s -o %t0.o
; RUN: %mipsx-link --builtins -o %t0 %t0.o
; RUN: %mipsx-run --expect-exit=0 %t0 | FileCheck %s
; CHECK: MIPS-X ok!

@data = global [8 x i32] [i32 3, i32 -7, i32 12, i32 0, i32 100, i32 -50, i32 9, i32 1]
@msg = constant [12 x i8] c"MIPS-X ok!\0A\00"

define void @putc(i8 %c) noinline {
  store volatile i8 %c, ptr inttoptr (i32 -65536 to ptr)
  ret void
}

; Six cases and a default: a jump table.
define i32 @classify(i32 %v) noinline {
entry:
  %k = and i32 %v, 7
  switch i32 %k, label %d [ i32 0, label %c0
                            i32 1, label %c1
                            i32 2, label %c2
                            i32 3, label %c3
                            i32 4, label %c4
                            i32 5, label %c5 ]
c0:
  ret i32 1
c1:
  ret i32 2
c2:
  ret i32 3
c3:
  ret i32 4
c4:
  ret i32 5
c5:
  ret i32 6
d:
  ret i32 0
}

define i32 @main() {
entry:
  br label %ploop

; Print msg up to its terminating zero.
ploop:
  %pi = phi i32 [ 0, %entry ], [ %pi1, %pbody ]
  %pp = getelementptr [12 x i8], ptr @msg, i32 0, i32 %pi
  %c = load i8, ptr %pp
  %z = icmp eq i8 %c, 0
  br i1 %z, label %loop, label %pbody
pbody:
  call void @putc(i8 %c)
  %pi1 = add i32 %pi, 1
  br label %ploop

loop:
  %i = phi i32 [ 0, %ploop ], [ %i1, %loop ]
  %sum = phi i32 [ 0, %ploop ], [ %sum1, %loop ]
  %mx = phi i32 [ -2147483648, %ploop ], [ %mx1, %loop ]
  %mn = phi i32 [ 2147483647, %ploop ], [ %mn1, %loop ]
  %cls = phi i32 [ 0, %ploop ], [ %cls1, %loop ]
  %al = phi i32 [ 0, %ploop ], [ %al1, %loop ]
  %aa = phi i32 [ 0, %ploop ], [ %aa1, %loop ]
  %p = getelementptr [8 x i32], ptr @data, i32 0, i32 %i
  %v = load i32, ptr %p
  %sum1 = add i32 %sum, %v
  %gt = icmp sgt i32 %v, %mx
  %mx1 = select i1 %gt, i32 %v, i32 %mx
  %lt = icmp slt i32 %v, %mn
  %mn1 = select i1 %lt, i32 %v, i32 %mn
  %k = call i32 @classify(i32 %v)
  %cls1 = add i32 %cls, %k
  %sh = shl i32 %v, %i
  %l = lshr i32 %sh, %i
  %a = ashr i32 %sh, %i
  %al1 = add i32 %al, %l
  %aa1 = add i32 %aa, %a
  %i1 = add i32 %i, 1
  %more = icmp ult i32 %i1, 8
  br i1 %more, label %loop, label %done

done:
  %prod = mul i32 %sum1, %mx1
  %q = sdiv i32 %mn1, 7
  %r = srem i32 %mn1, 7
  %uq = udiv i32 -16, 3
  %ur = urem i32 -16, 3
  br label %check1

check1:
  %e1 = icmp ne i32 %sum1, 68
  br i1 %e1, label %fail1, label %check2
fail1:
  ret i32 1

check2:
  %e2 = icmp ne i32 %mx1, 100
  br i1 %e2, label %fail2, label %check3
fail2:
  ret i32 2

check3:
  %e3 = icmp ne i32 %mn1, -50
  br i1 %e3, label %fail3, label %check4
fail3:
  ret i32 3

check4:
  %e4 = icmp ne i32 %cls1, 21
  br i1 %e4, label %fail4, label %check5
fail4:
  ret i32 4

check5:
  %e5 = icmp ne i32 %al1, -2013265852
  br i1 %e5, label %fail5, label %check6
fail5:
  ret i32 5

check6:
  %e6 = icmp ne i32 %aa1, 68
  br i1 %e6, label %fail6, label %check7
fail6:
  ret i32 6

check7:
  %e7 = icmp ne i32 %prod, 6800
  br i1 %e7, label %fail7, label %check8
fail7:
  ret i32 7

check8:
  %e8 = icmp ne i32 %q, -7
  br i1 %e8, label %fail8, label %check9
fail8:
  ret i32 8

check9:
  %e9 = icmp ne i32 %r, -1
  br i1 %e9, label %fail9, label %check10
fail9:
  ret i32 9

check10:
  %e10 = icmp ne i32 %uq, 1431655760
  br i1 %e10, label %fail10, label %check11
fail10:
  ret i32 10

check11:
  %e11 = icmp ne i32 %ur, 0
  br i1 %e11, label %fail11, label %check12
fail11:
  ret i32 11

check12:
  %e12 = icmp ne i32 %pi, 11
  br i1 %e12, label %fail12, label %pass
fail12:
  ret i32 12

pass:
  ret i32 0
}
