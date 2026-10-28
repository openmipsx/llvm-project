// RUN: %clang_cc1 -triple mipsx -fsyntax-only -verify %s

// MIPS-X inline assembly constraints and register names.
void f(int x) {
  asm volatile("" :: "I"(65535));
  asm volatile("" :: "I"(-65536));
  asm volatile("" :: "I"(65536)); // expected-error{{value '65536' out of range for constraint 'I'}}
  asm volatile("" :: "I"(-65537)); // expected-error{{value '-65537' out of range for constraint 'I'}}
  asm volatile("" :: "J"(0));
  asm volatile("" :: "J"(1)); // expected-error{{value '1' out of range for constraint 'J'}}
  asm volatile("" :: "K"(0));
  asm volatile("" :: "K"(31));
  asm volatile("" :: "K"(32)); // expected-error{{value '32' out of range for constraint 'K'}}
  asm volatile("" :: "r"(x), "m"(x));
  asm volatile("" :: "Q"(x)); // expected-error{{invalid input constraint 'Q' in asm}}
  asm volatile("" ::: "r0", "r1", "r16", "r31", "md", "psw", "memory");
  asm volatile("" ::: "zero", "at", "cb0", "cb1", "gp", "sp", "fp", "ra");
  asm volatile("" ::: "r32"); // expected-error{{unknown register name 'r32' in asm}}
  asm volatile("" ::: "$4"); // expected-error{{unknown register name '$4' in asm}}
}
