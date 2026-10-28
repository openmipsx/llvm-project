; RUN: llc -mtriple=mipsx < %s | FileCheck %s

; Block addresses are byte addresses like any pointer (docs/abi.md section
; 4), so an indirect branch shifts right by 2 before jspci.

define i32 @computed(i32 %x) {
; CHECK-LABEL: computed:
; CHECK:       addi r0,#%hi([[L:.Ltmp[0-9]+]]),r2
; CHECK-NEXT:  lsl r2,r2,#15
; CHECK-NEXT:  addi r2,#%lo([[L]]),r2
; CHECK-NEXT:  lsr r2,r2,#2
; CHECK-NEXT:  jspci r2,#0,r0
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
entry:
  %u = icmp eq i32 %x, 0
  %d = select i1 %u, ptr blockaddress(@computed, %a), ptr blockaddress(@computed, %b)
  indirectbr ptr %d, [label %a, label %b]
a:
  ret i32 1
b:
  ret i32 2
}
