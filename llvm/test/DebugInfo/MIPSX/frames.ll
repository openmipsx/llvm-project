; RUN: llc -mtriple=mipsx -filetype=obj < %s -o %t.o
; RUN: llvm-dwarfdump --debug-info --debug-frame %t.o | FileCheck %s
; RUN: llvm-readobj -r %t.o | FileCheck --check-prefix=RELOC %s
; RUN: llvm-dwarfdump --verify %t.o | FileCheck --check-prefix=VERIFY %s

; Frame bases and variable locations (MIPS-X SDK docs/abi.md section 8):
; without a frame pointer the frame base is sp and objects are at sp +
; offset after the prologue; with one it is fp = CFA and objects are below
; it. And the unwind rows for both shapes, from clang -g -O0 of:
;
;   int g;
;   int callee(int x);
;   int f(int a, int b) {            // "frame-pointer"="none"
;     int arr[4];
;     arr[a & 3] = b;
;     return callee(arr[b & 3]) + g;
;   }
;   int dyn(int n) {                 // a VLA: frame pointer
;     char buf[n];
;     buf[0] = 1;
;     return callee(buf[n - 1]);
;   }

; f: addi r29,#-32,r29; st 28[r29],r31; st 24[r29],r4; st 20[r29],r5.
; CHECK:      DW_TAG_subprogram
; CHECK-NEXT:   DW_AT_low_pc (0x00000000)
; CHECK:        DW_AT_frame_base (DW_OP_reg29 R29)
; CHECK:        DW_AT_name ("f")
; CHECK:        DW_TAG_formal_parameter
; CHECK-NEXT:     DW_AT_location (DW_OP_fbreg +24)
; CHECK-NEXT:     DW_AT_name ("a")
; CHECK:        DW_TAG_formal_parameter
; CHECK-NEXT:     DW_AT_location (DW_OP_fbreg +20)
; CHECK-NEXT:     DW_AT_name ("b")
; CHECK:        DW_TAG_variable
; CHECK-NEXT:     DW_AT_location (DW_OP_fbreg +4)
; CHECK-NEXT:     DW_AT_name ("arr")

; dyn: fp = CFA, st -12[r30],r4.
; CHECK:      DW_TAG_subprogram
; CHECK-NEXT:   DW_AT_low_pc (0x[[DYN:[0-9a-f]+]])
; CHECK:        DW_AT_frame_base (DW_OP_reg30 R30)
; CHECK:        DW_AT_name ("dyn")
; CHECK:        DW_TAG_formal_parameter
; CHECK-NEXT:     DW_AT_location (DW_OP_fbreg -12)
; CHECK-NEXT:     DW_AT_name ("n")

; CHECK:      .debug_frame contents:
; CHECK:      CIE
; CHECK:        Code alignment factor: 4
; CHECK:        Return address column: 31
; CHECK:        DW_CFA_def_cfa: R29 +0
; CHECK-NEXT:   DW_CFA_val_expression: R31 DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl
; CHECK:      FDE cie=00000000 pc=00000000...
; CHECK:        0x0: CFA=R29: R31=DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl
; CHECK-NEXT:   0x4: CFA=R29+32: R31=DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl
; CHECK-NEXT:   0x8: CFA=R29+32: R31=DW_OP_lit4, DW_OP_minus, DW_OP_deref, DW_OP_lit2, DW_OP_shl
; CHECK-NOT:    0x
; CHECK:      FDE cie=00000000 pc=[[DYN]]...
; CHECK:        CFA=R29+{{[0-9]+}}: R30=[CFA-8], R31=DW_OP_lit4, DW_OP_minus, DW_OP_deref, DW_OP_lit2, DW_OP_shl
; CHECK-NEXT:   CFA=R30: R30=[CFA-8], R31=DW_OP_lit4, DW_OP_minus, DW_OP_deref, DW_OP_lit2, DW_OP_shl

; Debug sections refer to code by byte address, through R_MIPSX_32.
; RELOC:      .rela.debug_info {
; RELOC:        R_MIPSX_32 .debug_abbrev 0x0
; RELOC:      .rela.debug_addr {
; RELOC:        R_MIPSX_32 .text 0x0
; RELOC-NEXT:   R_MIPSX_32 .text 0x{{[1-9a-f][0-9a-f]*}}
; RELOC:      .rela.debug_frame {
; RELOC-NEXT:   R_MIPSX_32 .debug_frame 0x0
; RELOC-NEXT:   R_MIPSX_32 .text 0x0
; RELOC-NEXT:   R_MIPSX_32 .debug_frame 0x0
; RELOC-NEXT:   R_MIPSX_32 .text 0x{{[1-9a-f][0-9a-f]*}}
; RELOC:      .rela.debug_line {
; RELOC:        R_MIPSX_32 .text 0x0

; VERIFY: No errors.

target datalayout = "E-m:e-p:32:32-i64:64-n32-S64"
target triple = "mipsx"

@g = dso_local global i32 0, align 4, !dbg !0

; Function Attrs: noinline nounwind optnone
define dso_local i32 @f(i32 noundef %a, i32 noundef %b) #3 !dbg !9 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  %arr = alloca [4 x i32], align 4
  store i32 %a, ptr %a.addr, align 4
    #dbg_declare(ptr %a.addr, !13, !DIExpression(), !14)
  store i32 %b, ptr %b.addr, align 4
    #dbg_declare(ptr %b.addr, !15, !DIExpression(), !16)
    #dbg_declare(ptr %arr, !17, !DIExpression(), !21)
  %0 = load i32, ptr %b.addr, align 4, !dbg !22
  %1 = load i32, ptr %a.addr, align 4, !dbg !23
  %and = and i32 %1, 3, !dbg !24
  %arrayidx = getelementptr inbounds [4 x i32], ptr %arr, i32 0, i32 %and, !dbg !25
  store i32 %0, ptr %arrayidx, align 4, !dbg !26
  %2 = load i32, ptr %b.addr, align 4, !dbg !27
  %and1 = and i32 %2, 3, !dbg !28
  %arrayidx2 = getelementptr inbounds [4 x i32], ptr %arr, i32 0, i32 %and1, !dbg !29
  %3 = load i32, ptr %arrayidx2, align 4, !dbg !29
  %call = call i32 @callee(i32 noundef %3), !dbg !30
  %4 = load i32, ptr @g, align 4, !dbg !31
  %add = add nsw i32 %call, %4, !dbg !32
  ret i32 %add, !dbg !33
}

declare dso_local i32 @callee(i32 noundef) #1

; Function Attrs: noinline nounwind optnone
define dso_local i32 @dyn(i32 noundef %n) #0 !dbg !34 {
entry:
  %n.addr = alloca i32, align 4
  %saved_stack = alloca ptr, align 4
  %__vla_expr0 = alloca i32, align 4
  store i32 %n, ptr %n.addr, align 4
    #dbg_declare(ptr %n.addr, !37, !DIExpression(), !38)
  %0 = load i32, ptr %n.addr, align 4, !dbg !39
  %1 = call ptr @llvm.stacksave.p0(), !dbg !40
  store ptr %1, ptr %saved_stack, align 4, !dbg !40
  %vla = alloca i8, i32 %0, align 1, !dbg !40
  store i32 %0, ptr %__vla_expr0, align 4, !dbg !40
    #dbg_declare(ptr %__vla_expr0, !41, !DIExpression(), !43)
    #dbg_declare(ptr %vla, !44, !DIExpression(), !49)
  %arrayidx = getelementptr inbounds i8, ptr %vla, i32 0, !dbg !50
  store i8 1, ptr %arrayidx, align 1, !dbg !51
  %2 = load i32, ptr %n.addr, align 4, !dbg !52
  %sub = sub nsw i32 %2, 1, !dbg !53
  %arrayidx1 = getelementptr inbounds i8, ptr %vla, i32 %sub, !dbg !54
  %3 = load i8, ptr %arrayidx1, align 1, !dbg !54
  %conv = sext i8 %3 to i32, !dbg !54
  %call = call i32 @callee(i32 noundef %conv), !dbg !55
  %4 = load ptr, ptr %saved_stack, align 4, !dbg !56
  call void @llvm.stackrestore.p0(ptr %4), !dbg !56
  ret i32 %call, !dbg !56
}

; Function Attrs: nocallback nofree nosync nounwind willreturn
declare ptr @llvm.stacksave.p0() #2

; Function Attrs: nocallback nofree nosync nounwind willreturn
declare void @llvm.stackrestore.p0(ptr) #2

attributes #0 = { noinline nounwind optnone "frame-pointer"="all" "target-cpu"="es3890" "target-features"="+es3890" }
attributes #1 = { "target-cpu"="es3890" "target-features"="+es3890" }
attributes #3 = { noinline nounwind optnone "frame-pointer"="none" "target-cpu"="es3890" "target-features"="+es3890" }
attributes #2 = { nocallback nofree nosync nounwind willreturn }

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!6, !7, !8}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "g", scope: !2, file: !3, line: 1, type: !5, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C11, file: !3, isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, globals: !4, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "frames.c", directory: "/tmp", checksumkind: CSK_MD5, checksum: "f21a88296f2ba1687739a4237634113b")
!4 = !{!0}
!5 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!6 = !{i32 7, !"Dwarf Version", i32 5}
!7 = !{i32 2, !"Debug Info Version", i32 3}
!8 = !{i32 7, !"frame-pointer", i32 2}
!9 = distinct !DISubprogram(name: "f", scope: !3, file: !3, line: 4, type: !10, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !2, retainedNodes: !12)
!10 = !DISubroutineType(types: !11)
!11 = !{!5, !5, !5}
!12 = !{}
!13 = !DILocalVariable(name: "a", arg: 1, scope: !9, file: !3, line: 4, type: !5)
!14 = !DILocation(line: 4, column: 11, scope: !9)
!15 = !DILocalVariable(name: "b", arg: 2, scope: !9, file: !3, line: 4, type: !5)
!16 = !DILocation(line: 4, column: 18, scope: !9)
!17 = !DILocalVariable(name: "arr", scope: !9, file: !3, line: 5, type: !18)
!18 = !DICompositeType(tag: DW_TAG_array_type, baseType: !5, size: 128, elements: !19)
!19 = !{!20}
!20 = !DISubrange(count: 4)
!21 = !DILocation(line: 5, column: 7, scope: !9)
!22 = !DILocation(line: 6, column: 16, scope: !9)
!23 = !DILocation(line: 6, column: 7, scope: !9)
!24 = !DILocation(line: 6, column: 9, scope: !9)
!25 = !DILocation(line: 6, column: 3, scope: !9)
!26 = !DILocation(line: 6, column: 14, scope: !9)
!27 = !DILocation(line: 7, column: 21, scope: !9)
!28 = !DILocation(line: 7, column: 23, scope: !9)
!29 = !DILocation(line: 7, column: 17, scope: !9)
!30 = !DILocation(line: 7, column: 10, scope: !9)
!31 = !DILocation(line: 7, column: 31, scope: !9)
!32 = !DILocation(line: 7, column: 29, scope: !9)
!33 = !DILocation(line: 7, column: 3, scope: !9)
!34 = distinct !DISubprogram(name: "dyn", scope: !3, file: !3, line: 10, type: !35, scopeLine: 10, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !2, retainedNodes: !12)
!35 = !DISubroutineType(types: !36)
!36 = !{!5, !5}
!37 = !DILocalVariable(name: "n", arg: 1, scope: !34, file: !3, line: 10, type: !5)
!38 = !DILocation(line: 10, column: 13, scope: !34)
!39 = !DILocation(line: 11, column: 12, scope: !34)
!40 = !DILocation(line: 11, column: 3, scope: !34)
!41 = !DILocalVariable(name: "__vla_expr0", scope: !34, type: !42, flags: DIFlagArtificial)
!42 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!43 = !DILocation(line: 0, scope: !34)
!44 = !DILocalVariable(name: "buf", scope: !34, file: !3, line: 11, type: !45)
!45 = !DICompositeType(tag: DW_TAG_array_type, baseType: !46, elements: !47)
!46 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!47 = !{!48}
!48 = !DISubrange(count: !41)
!49 = !DILocation(line: 11, column: 8, scope: !34)
!50 = !DILocation(line: 12, column: 3, scope: !34)
!51 = !DILocation(line: 12, column: 10, scope: !34)
!52 = !DILocation(line: 13, column: 21, scope: !34)
!53 = !DILocation(line: 13, column: 23, scope: !34)
!54 = !DILocation(line: 13, column: 17, scope: !34)
!55 = !DILocation(line: 13, column: 10, scope: !34)
!56 = !DILocation(line: 14, column: 1, scope: !34)
