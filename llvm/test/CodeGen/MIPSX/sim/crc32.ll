; CRC-32 of 256 bytes, bit at a time (benchmark for the delay slot filler;
; checked against Python's binascii.crc32).
; RUN: llc -mtriple=mipsx -filetype=obj %s -o %t.o
; RUN: %mipsx-link --builtins -o %t %t.o
; RUN: %mipsx-run --expect-exit=0 %t
; RUN: llc -mtriple=mipsx -O0 -filetype=obj %s -o %t0.o
; RUN: %mipsx-link --builtins -o %t0 %t0.o
; RUN: %mipsx-run --expect-exit=0 %t0
@buf = global [256 x i8] c"\6B\1A\F0\C0\CB\D6\25\65\8A\AC\2C\9F\AA\07\D1\3C\44\7E\33\05\1E\EE\F9\5A\60\E5\61\43\D6\C4\3B\CA\D7\6C\00\8A\9B\0A\6B\5F\C9\33\15\4A\6D\E2\84\04\A8\97\C5\25\26\2E\6A\7C\07\BC\BE\E8\41\F7\45\C5\5D\4E\9F\74\7F\61\51\64\C6\F7\28\D7\18\35\37\13\82\7A\C8\83\D7\FB\96\59\23\40\74\F5\25\8F\6C\68\08\23\89\D2\E4\7F\1E\17\5A\90\BC\43\2F\B9\46\E6\A9\47\11\09\F3\B7\9F\11\0A\26\F6\22\9F\A3\45\25\26\E7\BC\16\42\AE\B4\2B\F2\27\D5\0F\FF\07\C3\C2\06\24\29\2E\3B\83\D5\A9\C6\EA\E1\EC\2A\0F\9E\2C\F6\0B\75\39\FE\F8\82\05\BC\9A\49\67\56\AF\E2\FF\7B\A7\CF\80\65\DC\66\6D\C4\70\A2\6B\45\44\FE\B3\14\20\8D\56\39\E6\F1\8C\6D\D3\C3\FC\A1\E7\A4\26\10\8E\15\8F\B5\9E\09\45\CF\E8\61\0C\88\79\48\18\3B\E4\37\BC\27\65\66\F3\83\5B\05\F1\12\5B\73\8B\B1\51\C9\72\2C\D2\C6\42\E6\E8\64\03\C0\AF\ED\A7"

define i32 @crc32(ptr %p, i32 %n) noinline {
entry:
  br label %byte
byte:
  %i = phi i32 [ 0, %entry ], [ %i1, %byte.next ]
  %c = phi i32 [ -1, %entry ], [ %c8, %byte.next ]
  %pb = getelementptr i8, ptr %p, i32 %i
  %b = load i8, ptr %pb
  %bz = zext i8 %b to i32
  %cx = xor i32 %c, %bz
  br label %bit
bit:
  %k = phi i32 [ 0, %byte ], [ %k1, %bit ]
  %cb = phi i32 [ %cx, %byte ], [ %cn, %bit ]
  %low = and i32 %cb, 1
  %sh = lshr i32 %cb, 1
  %neg = sub i32 0, %low
  %poly = and i32 %neg, -306674912
  %cn = xor i32 %sh, %poly
  %k1 = add i32 %k, 1
  %kmore = icmp ult i32 %k1, 8
  br i1 %kmore, label %bit, label %byte.next
byte.next:
  %c8 = phi i32 [ %cn, %bit ]
  %i1 = add i32 %i, 1
  %more = icmp ult i32 %i1, %n
  br i1 %more, label %byte, label %exit
exit:
  %r = xor i32 %c8, -1
  ret i32 %r
}

define i32 @main() {
  %r = call i32 @crc32(ptr @buf, i32 256)
  %bad = icmp ne i32 %r, -1298790649
  br i1 %bad, label %fail, label %ok
fail:
  ret i32 1
ok:
  ret i32 0
}
