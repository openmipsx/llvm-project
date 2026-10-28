; Bubble sort of 64 words in a global array (benchmark for the delay slot
; filler; the result is checked against Python's sort).
; RUN: llc -mtriple=mipsx -filetype=obj %s -o %t.o
; RUN: %mipsx-link --builtins -o %t %t.o
; RUN: %mipsx-run --expect-exit=0 %t
; RUN: llc -mtriple=mipsx -O0 -filetype=obj %s -o %t0.o
; RUN: %mipsx-link --builtins -o %t0 %t0.o
; RUN: %mipsx-run --expect-exit=0 %t0
@a = global [64 x i32] [i32 4823, i32 8342, i32 5258, i32 4805, i32 6640, i32 9247, i32 -3778, i32 -3950, i32 6774, i32 5589, i32 -3900, i32 -6916, i32 4633, i32 -59, i32 -5354, i32 -7029, i32 7651, i32 -8628, i32 9511, i32 2981, i32 4843, i32 -4840, i32 -9509, i32 7313, i32 -7931, i32 -8049, i32 -8832, i32 -3768, i32 -2073, i32 9648, i32 -9015, i32 5202, i32 691, i32 4435, i32 9364, i32 -3600, i32 7010, i32 -2344, i32 -362, i32 6376, i32 -9850, i32 -7216, i32 4985, i32 -886, i32 3329, i32 8063, i32 -7274, i32 -1678, i32 331, i32 -2475, i32 6806, i32 -529, i32 -9026, i32 -7699, i32 8453, i32 -6464, i32 3120, i32 -6468, i32 -468, i32 2665, i32 -7811, i32 -9447, i32 -9983, i32 -3004]

define void @sort(ptr %p, i32 %n) noinline {
entry:
  br label %outer
outer:
  %i = phi i32 [ 0, %entry ], [ %i1, %outer.next ]
  %lim = sub i32 %n, %i
  %lim1 = sub i32 %lim, 1
  br label %inner
inner:
  %j = phi i32 [ 0, %outer ], [ %j1, %inner.next ]
  %pj = getelementptr i32, ptr %p, i32 %j
  %pk = getelementptr i32, ptr %pj, i32 1
  %x = load i32, ptr %pj
  %y = load i32, ptr %pk
  %gt = icmp sgt i32 %x, %y
  br i1 %gt, label %swap, label %inner.next
swap:
  store i32 %y, ptr %pj
  store i32 %x, ptr %pk
  br label %inner.next
inner.next:
  %j1 = add i32 %j, 1
  %more = icmp slt i32 %j1, %lim1
  br i1 %more, label %inner, label %outer.next
outer.next:
  %i1 = add i32 %i, 1
  %omore = icmp slt i32 %i1, %n
  br i1 %omore, label %outer, label %exit
exit:
  ret void
}

define i32 @main() {
entry:
  call void @sort(ptr @a, i32 64)
  br label %sum
sum:
  %i = phi i32 [ 0, %entry ], [ %i1, %sum ]
  %acc = phi i32 [ 0, %entry ], [ %acc1, %sum ]
  %pi = getelementptr [64 x i32], ptr @a, i32 0, i32 %i
  %v = load i32, ptr %pi
  %m = mul i32 %v, %i
  %acc1 = add i32 %acc, %m
  %i1 = add i32 %i, 1
  %more = icmp slt i32 %i1, 64
  br i1 %more, label %sum, label %done
done:
  %bad = icmp ne i32 %acc1, 6727990
  br i1 %bad, label %fail, label %ok
fail:
  ret i32 1
ok:
  ret i32 0
}
