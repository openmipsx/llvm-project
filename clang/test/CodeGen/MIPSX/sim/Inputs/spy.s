; Hand-written halves of callconv.c, following MIPS-X SDK docs/abi.md
; section 7 independently of the compiler.

    .text

; spy, spyv: record the argument registers r4-r11 in spy_regs and the
; first 8 stack words at the call in spy_stack; return r2 = 0x01020304,
; r3 = 0x05060708.
    .globl  spy
    .globl  spyv
spy:
spyv:
    la      spy_regs,r1
    st      0[r1],r4
    st      4[r1],r5
    st      8[r1],r6
    st      12[r1],r7
    st      16[r1],r8
    st      20[r1],r9
    st      24[r1],r10
    st      28[r1],r11
    la      spy_stack,r1
    ld      0[r29],r2
    ld      4[r29],r3
    st      0[r1],r2
    ld      8[r29],r2
    st      4[r1],r3
    ld      12[r29],r3
    st      8[r1],r2
    ld      16[r29],r2
    st      12[r1],r3
    ld      20[r29],r3
    st      16[r1],r2
    ld      24[r29],r2
    st      20[r1],r3
    ld      28[r29],r3
    st      24[r1],r2
    st      28[r1],r3
    li      #0x01020304,r2
    li      #0x05060708,r3
    ret
    nop
    nop

; spy_sret: return a 12-byte struct {0x11, 0x22, 0x33} through the buffer
; whose address is in r4.
    .globl  spy_sret
spy_sret:
    addi    r0,#0x11,r2
    st      0[r4],r2
    addi    r0,#0x22,r2
    st      4[r4],r2
    addi    r0,#0x33,r2
    st      8[r4],r2
    li      #0x05060708,r3
    ret
    nop
    nop

; call_recv: call recv (callconv.c) as recv(struct c3 {1, 2, 3}, -2LL,
; struct s6 {4, 5, 6}, (short)-7, 8, 9, 10) and return its result. The
; last argument is the ninth word, in the first stack slot.
    .globl  call_recv
call_recv:
    addi    r29,#-16,r29
    st      12[r29],r31
    li      #0x010203aa,r4          ; c3, padding byte 0xaa
    li      #-1,r5                  ; -2LL: high word
    li      #-2,r6                  ;       low word
    li      #0x00040005,r7          ; s6
    li      #0x0006bbbb,r8          ;    padding 0xbbbb
    li      #-7,r9                  ; short, sign-extended by the caller
    addi    r0,#8,r10
    addi    r0,#9,r11
    addi    r0,#10,r2
    st      0[r29],r2               ; ninth word: first stack slot
    call    recv
    nop
    nop
    ld      12[r29],r31
    addi    r29,#16,r29
    ret
    nop
    nop

    .data
    .globl  spy_regs
    .globl  spy_stack
spy_regs:
    .space  32
spy_stack:
    .space  32
