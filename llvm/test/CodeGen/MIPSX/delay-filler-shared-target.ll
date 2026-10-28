; RUN: llc -mtriple=mipsx -filetype=obj < %s | llvm-objdump -d - | FileCheck %s

; A block that ends in a conditional and an unconditional branch to the
; same block: when the delay-slot filler retargets one of them past the
; instructions it copied, the other still needs the CFG edge, or the old
; target loses its label ("Undefined temporary symbol"). Reduced from
; clang/test/CodeGen/MIPSX/sim/callconv.c.

; CHECK-LABEL: <recv>:
; CHECK: ret
define i32 @recv(i32 %a.coerce, i64 noundef %b, [2 x i32] %c.coerce, i16 noundef signext %d, i32 noundef %e, i32 noundef %f, i32 noundef %g) {
entry:
  %c.coerce.fca.0.extract = extractvalue [2 x i32] %c.coerce, 0
  %c.coerce.fca.1.extract = extractvalue [2 x i32] %c.coerce, 1
  %0 = and i32 %a.coerce, -256
  %or.cond42 = icmp eq i32 %0, 16909056
  %cmp13 = icmp eq i64 %b, -2
  %or.cond43 = and i1 %or.cond42, %cmp13
  %1 = icmp eq i32 %c.coerce.fca.0.extract, 262149
  %or.cond45 = select i1 %or.cond43, i1 %1, i1 false
  %conv27.mask = and i32 %c.coerce.fca.1.extract, -65536
  %cmp28 = icmp eq i32 %conv27.mask, 393216
  %or.cond46 = select i1 %or.cond45, i1 %cmp28, i1 false
  %cmp32 = icmp eq i16 %d, -7
  %or.cond47 = and i1 %cmp32, %or.cond46
  %cmp35 = icmp eq i32 %e, 8
  %or.cond48 = and i1 %cmp35, %or.cond47
  %cmp38 = icmp eq i32 %f, 9
  %or.cond49 = and i1 %cmp38, %or.cond48
  %cmp40 = icmp eq i32 %g, 10
  %2 = and i1 %cmp40, %or.cond49
  %cond = select i1 %2, i32 90, i32 0
  ret i32 %cond
}
