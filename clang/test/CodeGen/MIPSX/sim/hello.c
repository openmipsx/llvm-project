// RUN: %clang --target=mipsx --sysroot=%mipsx-sysroot -O2 %s -o %t
// RUN: %mipsx-run --expect-exit=0 %t | FileCheck %s
// RUN: %clang --target=mipsx --sysroot=%mipsx-sysroot -O0 %s -o %t0
// RUN: %mipsx-run --expect-exit=0 %t0 | FileCheck %s

// CHECK: Hello, world!

// There is no C library yet; write to the simulator's stdout register
// (MIPS-X SDK docs/abi.md section 13.5).
static void put(char c) { *(volatile char *)0xFFFF0000 = c; }

int main(void) {
  for (const char *s = "Hello, world!\n"; *s; s++)
    put(*s);
  return 0;
}
