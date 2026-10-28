; Fixups that resolve to values the field cannot hold.
; RUN: not llvm-mc -triple=mipsx -filetype=obj %s -o /dev/null 2>&1 \
; RUN:   | FileCheck %s --implicit-check-not=error:

.text
; Branch displacement = (target - branch) / 4 - 2, limits -32768 to 32767.
; Both branches go to fwd; the first is one word further away.
  beq r1,r2,fwd
; CHECK: :[[@LINE-1]]:13: error: branch target out of range (displacement 32768 words, limit -32768 to 32767)
  beq r1,r2,fwd
  .space 4 * 32768
fwd:
  nop

back:
  nop
  .space 4 * 32765
  b back
  b back
; CHECK: :[[@LINE-1]]:5: error: branch target out of range (displacement -32769 words, limit -32768 to 32767)

odd:
  .byte 0
  .p2align 2
  nop
  b odd+1
; CHECK: :[[@LINE-1]]:8: error: branch target is not a multiple of 4 bytes away

  addi r0,#big,r2
; CHECK: :[[@LINE-1]]:12: error: value 65536 out of range of a 17-bit signed immediate
  ld small[r1],r2
; CHECK: :[[@LINE-1]]:6: error: value -65537 out of range of a 17-bit signed immediate
  jspci r0,#%w(wbig),r31
; CHECK: :[[@LINE-1]]:13: error: %w value 65536 out of range of a 17-bit signed immediate
  jspci r0,#%w(wodd),r31
; CHECK: :[[@LINE-1]]:13: error: %w value is not a multiple of 4

.data
  .word %w(wodd)
; CHECK: :[[@LINE-1]]:9: error: %w value is not a multiple of 4

big = 65536
small = -65537
wbig = 0x40000
wodd = 0x1002
