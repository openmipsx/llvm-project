; Debug information must not change the code. CFI instructions are
; scheduling boundaries and delay-slot filler barriers, so the prologue CFI is
; added after the post-RA scheduler and the filler looks through it
; (MIPSXFrameCFI.cpp).
; RUN: llc -mtriple=mipsx -filetype=obj < %s -o %t.g.o
; RUN: opt -strip-debug < %s | llc -mtriple=mipsx -filetype=obj -o %t.o
; RUN: llvm-objcopy --dump-section=.text=%t.g.text %t.g.o /dev/null
; RUN: llvm-objcopy --dump-section=.text=%t.text %t.o /dev/null
; RUN: cmp %t.g.text %t.text
; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; From clang -g -O2 of:
;
;   int leaf(int a, int b);
;   int mid(int n) {
;     volatile int buf[4];
;     int s = 0;
;     for (int i = 0; i < n; i++) {
;       buf[i & 3] = i;
;       s += leaf(i, buf[i & 3]);
;     }
;     return s;
;   }
;
; The scheduler moves body instructions in between the callee-saved stores,
; and each rule follows its store.
; CHECK-LABEL: mid:
; CHECK:       addi r29,#-40,r29
; CHECK-NEXT:  .cfi_def_cfa_offset 40
; CHECK-NEXT:  st 32[r29],r16
; CHECK-NEXT:  .cfi_offset r16, -8
; CHECK-NOT:   .cfi_
; CHECK:       st 36[r29],r31
; CHECK-NEXT:  .cfi_escape 0x16, 0x1f, 0x05, 0x34, 0x1c, 0x06, 0x32, 0x24 ; r31 = [CFA-4] * 4

target datalayout = "E-m:e-p:32:32-i64:64-n32-S64"
target triple = "mipsx"

; Function Attrs: nounwind
define dso_local i32 @mid(i32 noundef %n) local_unnamed_addr #0 !dbg !10 {
entry:
  %buf = alloca [4 x i32], align 4, !DIAssignID !24
    #dbg_assign(i1 poison, !16, !DIExpression(), !24, ptr %buf, !DIExpression(), !25)
    #dbg_value(i32 %n, !15, !DIExpression(), !25)
  call void @llvm.lifetime.start.p0(ptr nonnull %buf) #3, !dbg !26
    #dbg_value(i32 0, !21, !DIExpression(), !25)
    #dbg_value(i32 0, !22, !DIExpression(), !27)
  %cmp9 = icmp sgt i32 %n, 0, !dbg !28
  br i1 %cmp9, label %for.body, label %for.cond.cleanup, !dbg !30

for.cond.cleanup:                                 ; preds = %for.body, %entry
  %s.0.lcssa = phi i32 [ 0, %entry ], [ %add, %for.body ], !dbg !25
  call void @llvm.lifetime.end.p0(ptr nonnull %buf) #3, !dbg !31
  ret i32 %s.0.lcssa, !dbg !32

for.body:                                         ; preds = %entry, %for.body
  %i.011 = phi i32 [ %inc, %for.body ], [ 0, %entry ]
  %s.010 = phi i32 [ %add, %for.body ], [ 0, %entry ]
    #dbg_value(i32 %i.011, !22, !DIExpression(), !27)
    #dbg_value(i32 %s.010, !21, !DIExpression(), !25)
  %and = and i32 %i.011, 3, !dbg !33
  %arrayidx = getelementptr inbounds nuw [4 x i8], ptr %buf, i32 %and, !dbg !35
  store volatile i32 %i.011, ptr %arrayidx, align 4, !dbg !36, !tbaa !37
  %0 = load volatile i32, ptr %arrayidx, align 4, !dbg !38, !tbaa !37
  %call = tail call i32 @leaf(i32 noundef %i.011, i32 noundef %0) #3, !dbg !39
  %add = add nsw i32 %call, %s.010, !dbg !40
    #dbg_value(i32 %add, !21, !DIExpression(), !25)
  %inc = add nuw nsw i32 %i.011, 1, !dbg !41
    #dbg_value(i32 %inc, !22, !DIExpression(), !27)
  %exitcond.not = icmp eq i32 %inc, %n, !dbg !42
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body, !dbg !43, !llvm.loop !44
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(ptr captures(none)) #1

declare !dbg !48 dso_local i32 @leaf(i32 noundef, i32 noundef) local_unnamed_addr #2

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(ptr captures(none)) #1

attributes #0 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="es3890" "target-features"="+es3890" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="es3890" "target-features"="+es3890" }
attributes #3 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4}
!llvm.errno.tbaa = !{!5}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "same.c", directory: "/tmp", checksumkind: CSK_MD5, checksum: "e478b776a72fffe75c2138150a5e0228")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 7, !"debug-info-assignment-tracking", i1 true}
!5 = !{!6, !7, i64 0}
!6 = !{!"__libc_errno", !7, i64 0}
!7 = !{!"int", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C/C++ TBAA"}
!10 = distinct !DISubprogram(name: "mid", scope: !1, file: !1, line: 2, type: !11, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !14, keyInstructions: true)
!11 = !DISubroutineType(types: !12)
!12 = !{!13, !13}
!13 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!14 = !{!15, !16, !21, !22}
!15 = !DILocalVariable(name: "n", arg: 1, scope: !10, file: !1, line: 2, type: !13)
!16 = !DILocalVariable(name: "buf", scope: !10, file: !1, line: 3, type: !17)
!17 = !DICompositeType(tag: DW_TAG_array_type, baseType: !18, size: 128, elements: !19)
!18 = !DIDerivedType(tag: DW_TAG_volatile_type, baseType: !13)
!19 = !{!20}
!20 = !DISubrange(count: 4)
!21 = !DILocalVariable(name: "s", scope: !10, file: !1, line: 4, type: !13)
!22 = !DILocalVariable(name: "i", scope: !23, file: !1, line: 5, type: !13)
!23 = distinct !DILexicalBlock(scope: !10, file: !1, line: 5, column: 3)
!24 = distinct !DIAssignID()
!25 = !DILocation(line: 0, scope: !10)
!26 = !DILocation(line: 3, column: 3, scope: !10)
!27 = !DILocation(line: 0, scope: !23)
!28 = !DILocation(line: 5, column: 21, scope: !29, atomGroup: 11, atomRank: 1)
!29 = distinct !DILexicalBlock(scope: !23, file: !1, line: 5, column: 3)
!30 = !DILocation(line: 5, column: 3, scope: !23, atomGroup: 12, atomRank: 1)
!31 = !DILocation(line: 10, column: 1, scope: !10)
!32 = !DILocation(line: 9, column: 3, scope: !10, atomGroup: 10, atomRank: 1)
!33 = !DILocation(line: 6, column: 11, scope: !34)
!34 = distinct !DILexicalBlock(scope: !29, file: !1, line: 5, column: 31)
!35 = !DILocation(line: 6, column: 5, scope: !34)
!36 = !DILocation(line: 6, column: 16, scope: !34, atomGroup: 5, atomRank: 1)
!37 = !{!7, !7, i64 0}
!38 = !DILocation(line: 7, column: 18, scope: !34)
!39 = !DILocation(line: 7, column: 10, scope: !34)
!40 = !DILocation(line: 7, column: 7, scope: !34, atomGroup: 6, atomRank: 2)
!41 = !DILocation(line: 5, column: 27, scope: !29, atomGroup: 7, atomRank: 2)
!42 = !DILocation(line: 5, column: 21, scope: !29, atomGroup: 3, atomRank: 1)
!43 = !DILocation(line: 5, column: 3, scope: !23, atomGroup: 4, atomRank: 1)
!44 = distinct !{!44, !45, !46, !47}
!45 = !DILocation(line: 5, column: 3, scope: !23)
!46 = !DILocation(line: 8, column: 3, scope: !23)
!47 = !{!"llvm.loop.mustprogress"}
!48 = !DISubprogram(name: "leaf", scope: !1, file: !1, line: 1, type: !49, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!49 = !DISubroutineType(types: !50)
!50 = !{!13, !13, !13}
