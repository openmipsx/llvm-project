; RUN: llc -mtriple=mipsx < %s | FileCheck %s
; RUN: llc -mtriple=mipsx -data-sections < %s | FileCheck --check-prefix=DS %s
; RUN: llc -mtriple=mipsx -mipsx-extern-sdata=false < %s \
; RUN:   | FileCheck --check-prefix=NOEXT %s
; RUN: llc -mtriple=mipsx -mipsx-ssection-threshold=0 < %s \
; RUN:   | FileCheck --check-prefix=G0 %s
; RUN: llc -mtriple=mipsx -filetype=obj < %s | llvm-readelf -r -S - \
; RUN:   | FileCheck --check-prefix=OBJ %s

; Small data (docs/abi.md section 6.3): variables of at most 8 bytes
; (-mipsx-ssection-threshold) go in .sdata or .sbss and are reached
; through gp in one instruction, with no address to build, as long as
; the address is inside the variable. Small constants go in .sdata too. Larger, unsized, weak-undefined, common and
; explicitly placed variables keep the %hi/%lo form.

@word = global i32 5
@pair = global [2 x i32] zeroinitializer
@three = global [3 x i32] zeroinitializer
@cst = constant i16 7
@local = internal global i8 1
@ext = external global i32
@ext_unsized = external global [0 x i32]
@weak_undef = extern_weak global i32
@common = common global i32 0
@placed = global i32 1, section ".mydata"
@in_sbss = global [16 x i32] zeroinitializer, section ".sbss.big"

; CHECK-LABEL: load_word:
; CHECK:      ld %gprel(word)[r28],r2
; G0-LABEL:   load_word:
; G0:         addi r0,#%hi(word),r2
; G0:         ld %lo(word)[r3],r2
define i32 @load_word() {
  %v = load i32, ptr @word
  ret i32 %v
}

; CHECK-LABEL: store_pair:
; CHECK:      st %gprel(pair+4)[r28],r4
define void @store_pair(i32 %v) {
  store i32 %v, ptr getelementptr (i32, ptr @pair, i32 1)
  ret void
}

; An address outside the variable may be outside gp's reach (torture
; tests loop-2d and 20030928-1 had a[i - 1] and far offsets).
; CHECK-LABEL: outside:
; CHECK-DAG:  addi r0,#%hi(pair-4),
; CHECK-DAG:  addi r0,#%hi(word+4),
; CHECK-DAG:  addi r0,#%hi(word+196608),
define i32 @outside() {
  %a = load i32, ptr getelementptr (i8, ptr @pair, i32 -4)
  %b = load i32, ptr getelementptr (i8, ptr @word, i32 4)
  %c = load i32, ptr getelementptr (i8, ptr @word, i32 196608)
  %s = add i32 %a, %b
  %t = add i32 %s, %c
  ret i32 %t
}

; Twelve bytes is above the threshold.
; CHECK-LABEL: load_three:
; CHECK:      addi r0,#%hi(three+8),r2
define i32 @load_three() {
  %v = load i32, ptr getelementptr (i32, ptr @three, i32 2)
  ret i32 %v
}

; CHECK-LABEL: address:
; CHECK:      addi r28,#%gprel(word),r2
define ptr @address() {
  ret ptr @word
}

; CHECK-LABEL: others:
; CHECK-DAG:  ldhu %gprel(cst)[r28],
; CHECK-DAG:  ldbu %gprel(local)[r28],
; CHECK-DAG:  ld %gprel(ext)[r28],
; CHECK-DAG:  ld %lo(ext_unsized+4)[
; CHECK-DAG:  addi r0,#%hi(weak_undef),
; CHECK-DAG:  ld %lo(common)[
; CHECK-DAG:  ld %lo(placed)[
; CHECK-DAG:  ld %gprel(in_sbss+60)[r28],
; NOEXT-LABEL: others:
; NOEXT:      addi r0,#%hi(ext),
define i32 @others() {
  %a = load i16, ptr @cst
  %a32 = zext i16 %a to i32
  %b = load i8, ptr @local
  %b32 = zext i8 %b to i32
  %c = load i32, ptr @ext
  %d = load i32, ptr getelementptr (i32, ptr @ext_unsized, i32 1)
  %e = load i32, ptr @weak_undef
  %f = load i32, ptr @common
  %g = load i32, ptr @placed
  %h = load i32, ptr getelementptr (i32, ptr @in_sbss, i32 15)
  %s1 = add i32 %a32, %b32
  %s2 = add i32 %s1, %c
  %s3 = add i32 %s2, %d
  %s4 = add i32 %s3, %e
  %s5 = add i32 %s4, %f
  %s6 = add i32 %s5, %g
  %s7 = add i32 %s6, %h
  ret i32 %s7
}

; CHECK:      .section .sdata,"aw",@progbits
; CHECK-NEXT: .globl word
; CHECK:      .section .sbss,"aw",@nobits
; CHECK-NEXT: .globl pair
; CHECK:      .bss
; CHECK-NEXT: .globl three
; CHECK:      .section .sdata,"aw",@progbits
; CHECK-NEXT: .globl cst
; CHECK-NOT:  .section
; CHECK:      local:
; CHECK:      .comm common,4,4

; DS: .section .sdata.word,"aw",@progbits
; DS: .section .sbss.pair,"aw",@nobits
; DS: .section .bss.three,"aw",@nobits
; DS: .section .sdata.cst,"aw",@progbits
; DS: .section .sdata.local,"aw",@progbits

; G0:     .data
; G0-NEXT: .globl word

; OBJ:     .sdata
; OBJ:     .sbss
; OBJ:     R_MIPSX_GPREL17 {{.*}} word + 0
; OBJ:     R_MIPSX_GPREL17 {{.*}} pair + 4
