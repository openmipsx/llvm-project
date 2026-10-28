; RUN: llc -mtriple=mipsx < %s | FileCheck %s
; RUN: llc -mtriple=mipsx -filetype=obj < %s | llvm-objdump -dr - \
; RUN:   | FileCheck --check-prefix=OBJ %s

; Global addresses use %hi / lsl 15 / %lo (docs/abi.md section 6.2), with
; constant offsets folded into the relocations.

@g = global i32 5
@arr = global [4 x i16] zeroinitializer

define i32 @load_global() {
; CHECK-LABEL: load_global:
; CHECK:       addi r0,#%hi(g),[[R:r[0-9]+]]
; CHECK-NEXT:  lsl [[R]],[[R]],#15
; CHECK-NEXT:  addi [[R]],#%lo(g),[[A:r[0-9]+]]
; CHECK-NEXT:  ld 0[[[A]]],r2
  %v = load i32, ptr @g
  ret i32 %v
}

define void @store_element(i16 %v) {
; CHECK-LABEL: store_element:
; CHECK:       addi r0,#%hi(arr+6),r2
; CHECK-NEXT:  lsl r2,r2,#15
; CHECK-NEXT:  addi r2,#%lo(arr+6),r2
; CHECK-NEXT:  sth 0[r2],r4
  %p = getelementptr [4 x i16], ptr @arr, i32 0, i32 3
  store i16 %v, ptr %p
  ret void
}

define ptr @address() {
; CHECK-LABEL: address:
; CHECK:       addi r0,#%hi(g+4),r2
; CHECK-NEXT:  lsl r2,r2,#15
; CHECK-NEXT:  addi r2,#%lo(g+4),r2
  ret ptr getelementptr (i8, ptr @g, i32 4)
}

; OBJ:      R_MIPSX_HI17 g
; OBJ:      R_MIPSX_LO15 g
; OBJ:      R_MIPSX_HI17 arr+0x6
; OBJ:      R_MIPSX_LO15 arr+0x6
; OBJ:      R_MIPSX_HI17 g+0x4
; OBJ:      R_MIPSX_LO15 g+0x4
