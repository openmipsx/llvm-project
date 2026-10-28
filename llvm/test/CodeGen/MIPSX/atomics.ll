; RUN: llc -mtriple=mipsx -verify-machineinstrs < %s | FileCheck %s

; MIPS-X has no atomic instructions (docs/abi.md section 13.6): every atomic
; operation is a call into the SDK's __atomic_* functions, and a fence is a
; compiler barrier that emits nothing.

@x = global i32 0
@b = global i8 0
@d = global i64 0

; CHECK-LABEL: rmw:
; CHECK: jspci r0,#%call(__atomic_fetch_add_4),r31
define i32 @rmw(i32 %v) {
  %old = atomicrmw add ptr @x, i32 %v seq_cst
  ret i32 %old
}

; CHECK-LABEL: xchg8:
; CHECK: jspci r0,#%call(__atomic_exchange_1),r31
define i8 @xchg8(i8 %v) {
  %old = atomicrmw xchg ptr @b, i8 %v monotonic
  ret i8 %old
}

; CHECK-LABEL: cas64:
; CHECK: jspci r0,#%call(__atomic_compare_exchange_8),r31
define i1 @cas64(i64 %e, i64 %n) {
  %r = cmpxchg ptr @d, i64 %e, i64 %n seq_cst seq_cst
  %ok = extractvalue { i64, i1 } %r, 1
  ret i1 %ok
}

; CHECK-LABEL: load_store:
; CHECK: jspci r0,#%call(__atomic_load_4),r31
; CHECK: jspci r0,#%call(__atomic_store_4),r31
define void @load_store() {
  %v = load atomic i32, ptr @x acquire, align 4
  store atomic i32 %v, ptr @x release, align 4
  ret void
}

; Nothing moves across a fence, not even into a delay slot: the store stays
; above the barrier rather than filling the return's slot.
; CHECK-LABEL: fence:
; CHECK: st %lo(x)[r2],r4
; CHECK-NEXT: ;MEMBARRIER
; CHECK-NEXT: ret
; CHECK-NEXT: nop
; CHECK-NEXT: nop
define void @fence(i32 %v) {
  store i32 %v, ptr @x
  fence seq_cst
  ret void
}
