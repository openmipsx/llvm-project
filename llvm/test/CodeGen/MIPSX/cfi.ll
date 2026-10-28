; RUN: llc -mtriple=mipsx < %s | FileCheck %s
; RUN: llc -mtriple=mipsx -filetype=obj < %s -o %t.o
; RUN: llvm-dwarfdump --eh-frame %t.o | FileCheck --check-prefix=DUMP %s

; Call frame information for each frame shape (docs/abi.md section 8.6):
; the CFA is sp + size after the sp adjustment, callee-saved registers are
; at their slots, and with a frame pointer the CFA is fp + 0. The return
; address column 31 is r31 * 4: the CIE says so for r31 in its register,
; and a saved r31 gets "[CFA-n] * 4". Epilogues have no CFI.

declare void @use(ptr)
declare i32 @get()

; No frame: only the CIE's rules.
define i32 @leaf(i32 %x) uwtable {
; CHECK-LABEL: leaf:
; CHECK:       .cfi_startproc
; CHECK-NOT:   .cfi_
; CHECK:       .cfi_endproc
  ret i32 %x
}

; A frame without calls saves nothing.
define void @leaf_frame() uwtable {
; CHECK-LABEL: leaf_frame:
; CHECK:       addi r29,#-16,r29
; CHECK-NEXT:  .cfi_def_cfa_offset 16
; CHECK-NOT:   .cfi_
; CHECK:       .cfi_endproc
  %a = alloca [4 x i32]
  store volatile i32 1, ptr %a
  ret void
}

; A call saves r31 and a callee-saved register.
define i32 @keep(i32 %x) uwtable {
; CHECK-LABEL: keep:
; CHECK:       addi r29,#-8,r29
; CHECK-NEXT:  .cfi_def_cfa_offset 8
; CHECK-DAG:   st 4[r29],r31
; CHECK-DAG:   .cfi_escape 0x16, 0x1f, 0x05, 0x34, 0x1c, 0x06, 0x32, 0x24 ; r31 = [CFA-4] * 4
; CHECK-DAG:   st 0[r29],r16
; CHECK-DAG:   .cfi_offset r16, -8
; CHECK:       jspci r0,#%call(get),r31
; CHECK-NOT:   .cfi_
; CHECK:       .cfi_endproc
  %a = call i32 @get()
  %b = add i32 %a, %x
  ret i32 %b
}

; Frame pointer: after fp = sp + size the CFA is fp + 0. Here the filler
; moves fp = sp + 24 into the call's delay slots and its rule stays before
; the call: exact at the call, two instructions early.
define void @with_fp() uwtable "frame-pointer"="all" {
; CHECK-LABEL: with_fp:
; CHECK:       addi r29,#-24,r29
; CHECK-NEXT:  .cfi_def_cfa_offset 24
; CHECK-NEXT:  st 16[r29],r30
; CHECK-NEXT:  .cfi_offset r30, -8
; CHECK-NEXT:  .cfi_def_cfa r30, 0
; CHECK-NEXT:  st 20[r29],r31
; CHECK-NEXT:  .cfi_escape 0x16, 0x1f, 0x05, 0x34, 0x1c, 0x06, 0x32, 0x24 ; r31 = [CFA-4] * 4
; CHECK-NEXT:  jspci r0,#%call(use),r31
; CHECK-NEXT:  addi r29,#24,r30
; CHECK-NOT:   .cfi_
; CHECK:       .cfi_endproc
  %a = alloca [4 x i32]
  call void @use(ptr %a)
  ret void
}

; Dynamic alloca: sp moves in the body, so the CFA is fp-based.
define void @dynamic(i32 %n) uwtable {
; CHECK-LABEL: dynamic:
; CHECK:       .cfi_def_cfa_offset 8
; CHECK:       .cfi_def_cfa r30, 0
; CHECK:       sub r29,r2,r4
; CHECK-NOT:   .cfi_
; CHECK:       .cfi_endproc
  %a = alloca i8, i32 %n
  call void @use(ptr %a)
  ret void
}

; Realigned frame: sp is rounded down after fp is set, so the CFA stays
; fp-based.
define void @realigned() uwtable {
; CHECK-LABEL: realigned:
; CHECK:       .cfi_def_cfa r30, 0
; CHECK:       and r29,r1,r29
; CHECK-NOT:   .cfi_
; CHECK:       .cfi_endproc
  %a = alloca i32, align 32
  call void @use(ptr %a)
  ret void
}

; Large frame: the adjustment is built in r1 and the rule follows the
; instruction that changes sp; the saved r31 is still at CFA - 4.
define void @large() uwtable {
; CHECK-LABEL: large:
; CHECK:       add r29,r1,r29
; CHECK-NEXT:  .cfi_def_cfa_offset 100008
; CHECK:       .cfi_escape 0x16, 0x1f, 0x05, 0x34, 0x1c, 0x06, 0x32, 0x24 ; r31 = [CFA-4] * 4
; CHECK-NOT:   .cfi_
; CHECK:       .cfi_endproc
  %a = alloca [100000 x i8]
  call void @use(ptr %a)
  ret void
}

; A nounwind function without uwtable or debug information has no CFI.
define void @nouwtable() nounwind {
; CHECK-LABEL: nouwtable:
; CHECK-NOT:   .cfi_
; CHECK:       .size nouwtable
; CHECK-NOT:   .cfi_
  %a = alloca [4 x i32]
  call void @use(ptr %a)
  ret void
}

; The CIE: CFA = r29 + 0 and the return address is r31 * 4.
; DUMP:      CIE
; DUMP:      Code alignment factor: 4
; DUMP:      Data alignment factor: -4
; DUMP:      Return address column: 31
; DUMP:      DW_CFA_def_cfa: R29 +0
; DUMP-NEXT: DW_CFA_val_expression: R31 DW_OP_drop, DW_OP_breg31 R31+0, DW_OP_lit2, DW_OP_shl
; DUMP:      FDE
; DUMP:      FDE
; DUMP:      DW_CFA_def_cfa_offset: +24
