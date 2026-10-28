; RUN: llc -mtriple=mipsx -O2 -verify-machineinstrs -verify-regalloc < %s | FileCheck %s

; Spill reloads must not be the early-clobber LD (rule P2 of docs/abi.md
; section 9): the spiller gives a reload's def a normal slot, so an
; early-clobber reload broke the live intervals, crashing the greedy
; allocator or miscompiling. Reloads are LDfi, an ld without the constraint;
; their base is sp or fp, never the destination. Twelve values live across
; a call in a loop force some reloads. (Found in S14 and S15.)

; CHECK-LABEL: f:
; CHECK: ld {{[0-9]+}}[r29],r{{[0-9]+}} ; 4-byte Folded Reload

declare void @g()

define dso_local i32 @f(ptr nofree noundef readonly captures(none) %p, i32 noundef %n) local_unnamed_addr {
entry:
  %0 = load i32, ptr %p, align 4
  %arrayidx1 = getelementptr inbounds nuw i8, ptr %p, i32 4
  %1 = load i32, ptr %arrayidx1, align 4
  %arrayidx2 = getelementptr inbounds nuw i8, ptr %p, i32 8
  %2 = load i32, ptr %arrayidx2, align 4
  %arrayidx3 = getelementptr inbounds nuw i8, ptr %p, i32 12
  %3 = load i32, ptr %arrayidx3, align 4
  %arrayidx4 = getelementptr inbounds nuw i8, ptr %p, i32 16
  %4 = load i32, ptr %arrayidx4, align 4
  %arrayidx5 = getelementptr inbounds nuw i8, ptr %p, i32 20
  %5 = load i32, ptr %arrayidx5, align 4
  %arrayidx6 = getelementptr inbounds nuw i8, ptr %p, i32 24
  %6 = load i32, ptr %arrayidx6, align 4
  %arrayidx7 = getelementptr inbounds nuw i8, ptr %p, i32 28
  %7 = load i32, ptr %arrayidx7, align 4
  %arrayidx8 = getelementptr inbounds nuw i8, ptr %p, i32 32
  %8 = load i32, ptr %arrayidx8, align 4
  %arrayidx9 = getelementptr inbounds nuw i8, ptr %p, i32 36
  %9 = load i32, ptr %arrayidx9, align 4
  %arrayidx10 = getelementptr inbounds nuw i8, ptr %p, i32 40
  %10 = load i32, ptr %arrayidx10, align 4
  %arrayidx11 = getelementptr inbounds nuw i8, ptr %p, i32 44
  %11 = load i32, ptr %arrayidx11, align 4
  %cmp69 = icmp sgt i32 %n, 0
  br i1 %cmp69, label %for.body, label %for.cond.cleanup

for.cond.cleanup:                                 ; preds = %for.body, %entry
  %a0.0.lcssa = phi i32 [ %0, %entry ], [ %add, %for.body ]
  %a1.0.lcssa = phi i32 [ %1, %entry ], [ %xor, %for.body ]
  %a2.0.lcssa = phi i32 [ %2, %entry ], [ %add12, %for.body ]
  %a3.0.lcssa = phi i32 [ %3, %entry ], [ %xor13, %for.body ]
  %a4.0.lcssa = phi i32 [ %4, %entry ], [ %add14, %for.body ]
  %a5.0.lcssa = phi i32 [ %5, %entry ], [ %xor15, %for.body ]
  %a6.0.lcssa = phi i32 [ %6, %entry ], [ %add16, %for.body ]
  %a7.0.lcssa = phi i32 [ %7, %entry ], [ %xor17, %for.body ]
  %a8.0.lcssa = phi i32 [ %8, %entry ], [ %add18, %for.body ]
  %a9.0.lcssa = phi i32 [ %9, %entry ], [ %xor19, %for.body ]
  %a10.0.lcssa = phi i32 [ %10, %entry ], [ %add20, %for.body ]
  %a11.0.lcssa = phi i32 [ %11, %entry ], [ %xor21, %for.body ]
  %add22 = add nsw i32 %a1.0.lcssa, %a0.0.lcssa
  %add23 = add nsw i32 %add22, %a2.0.lcssa
  %add24 = add nsw i32 %add23, %a3.0.lcssa
  %add25 = add nsw i32 %add24, %a4.0.lcssa
  %add26 = add nsw i32 %add25, %a5.0.lcssa
  %add27 = add nsw i32 %add26, %a6.0.lcssa
  %add28 = add nsw i32 %add27, %a7.0.lcssa
  %add29 = add nsw i32 %add28, %a8.0.lcssa
  %add30 = add nsw i32 %add29, %a9.0.lcssa
  %add31 = add nsw i32 %add30, %a10.0.lcssa
  %add32 = add nsw i32 %add31, %a11.0.lcssa
  ret i32 %add32

for.body:                                         ; preds = %entry, %for.body
  %i.082 = phi i32 [ %inc, %for.body ], [ 0, %entry ]
  %a11.081 = phi i32 [ %xor21, %for.body ], [ %11, %entry ]
  %a10.080 = phi i32 [ %add20, %for.body ], [ %10, %entry ]
  %a9.079 = phi i32 [ %xor19, %for.body ], [ %9, %entry ]
  %a8.078 = phi i32 [ %add18, %for.body ], [ %8, %entry ]
  %a7.077 = phi i32 [ %xor17, %for.body ], [ %7, %entry ]
  %a6.076 = phi i32 [ %add16, %for.body ], [ %6, %entry ]
  %a5.075 = phi i32 [ %xor15, %for.body ], [ %5, %entry ]
  %a4.074 = phi i32 [ %add14, %for.body ], [ %4, %entry ]
  %a3.073 = phi i32 [ %xor13, %for.body ], [ %3, %entry ]
  %a2.072 = phi i32 [ %add12, %for.body ], [ %2, %entry ]
  %a1.071 = phi i32 [ %xor, %for.body ], [ %1, %entry ]
  %a0.070 = phi i32 [ %add, %for.body ], [ %0, %entry ]
  tail call void @g()
  %add = add nsw i32 %a1.071, %a0.070
  %xor = xor i32 %a2.072, %a1.071
  %add12 = add nsw i32 %a3.073, %a2.072
  %xor13 = xor i32 %a4.074, %a3.073
  %add14 = add nsw i32 %a5.075, %a4.074
  %xor15 = xor i32 %a6.076, %a5.075
  %add16 = add nsw i32 %a7.077, %a6.076
  %xor17 = xor i32 %a8.078, %a7.077
  %add18 = add nsw i32 %a9.079, %a8.078
  %xor19 = xor i32 %a10.080, %a9.079
  %add20 = add nsw i32 %a11.081, %a10.080
  %xor21 = xor i32 %a11.081, %add
  %inc = add nuw nsw i32 %i.082, 1
  %exitcond.not = icmp eq i32 %inc, %n
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body
}
