; 4x4 integer matrix multiply, repeated 10 times (benchmark for the delay
; slot filler; uses __mulsi3; the checksum is checked).
; RUN: llc -mtriple=mipsx -filetype=obj %s -o %t.o
; RUN: %mipsx-link --builtins -o %t %t.o
; RUN: %mipsx-run --expect-exit=0 %t
; RUN: llc -mtriple=mipsx -O0 -filetype=obj %s -o %t0.o
; RUN: %mipsx-link --builtins -o %t0 %t0.o
; RUN: %mipsx-run --expect-exit=0 %t0
@A = global [16 x i32] [i32 33, i32 -24, i32 -38, i32 42, i32 32, i32 41, i32 -35, i32 -23, i32 -19, i32 -1, i32 -39, i32 -11, i32 18, i32 -9, i32 -17, i32 41]
@B = global [16 x i32] [i32 -48, i32 -6, i32 14, i32 -40, i32 -46, i32 6, i32 -7, i32 20, i32 3, i32 48, i32 -15, i32 12, i32 -47, i32 -23, i32 -42, i32 4]
@C = global [16 x i32] zeroinitializer

define void @matmul(ptr %a, ptr %b, ptr %c) noinline {
entry:
  br label %li
li:
  %i = phi i32 [ 0, %entry ], [ %i1, %li.next ]
  br label %lj
lj:
  %j = phi i32 [ 0, %li ], [ %j1, %lj.next ]
  br label %lk
lk:
  %k = phi i32 [ 0, %lj ], [ %k1, %lk ]
  %s = phi i32 [ 0, %lj ], [ %s1, %lk ]
  %i4 = shl i32 %i, 2
  %aik = add i32 %i4, %k
  %k4 = shl i32 %k, 2
  %bkj = add i32 %k4, %j
  %pa = getelementptr i32, ptr %a, i32 %aik
  %pb = getelementptr i32, ptr %b, i32 %bkj
  %va = load i32, ptr %pa
  %vb = load i32, ptr %pb
  %m = mul i32 %va, %vb
  %s1 = add i32 %s, %m
  %k1 = add i32 %k, 1
  %kmore = icmp slt i32 %k1, 4
  br i1 %kmore, label %lk, label %lj.next
lj.next:
  %cij = add i32 %i4, %j
  %pc = getelementptr i32, ptr %c, i32 %cij
  %old = load i32, ptr %pc
  %new = add i32 %old, %s1
  store i32 %new, ptr %pc
  %j1 = add i32 %j, 1
  %jmore = icmp slt i32 %j1, 4
  br i1 %jmore, label %lj, label %li.next
li.next:
  %i1 = add i32 %i, 1
  %imore = icmp slt i32 %i1, 4
  br i1 %imore, label %li, label %exit
exit:
  ret void
}

define i32 @main() {
entry:
  br label %rep
rep:
  %r = phi i32 [ 0, %entry ], [ %r1, %rep ]
  call void @matmul(ptr @A, ptr @B, ptr @C)
  %r1 = add i32 %r, 1
  %rmore = icmp slt i32 %r1, 10
  br i1 %rmore, label %rep, label %sum
sum:
  %x = phi i32 [ 0, %rep ], [ %x1, %sum ]
  %acc = phi i32 [ 0, %rep ], [ %acc1, %sum ]
  %px = getelementptr [16 x i32], ptr @C, i32 0, i32 %x
  %v = load i32, ptr %px
  %w = add i32 %x, 1
  %m = mul i32 %v, %w
  %acc1 = add i32 %acc, %m
  %x1 = add i32 %x, 1
  %more = icmp slt i32 %x1, 16
  br i1 %more, label %sum, label %done
done:
  %bad = icmp ne i32 %acc1, -1161620
  br i1 %bad, label %fail, label %ok
fail:
  ret i32 1
ok:
  ret i32 0
}
