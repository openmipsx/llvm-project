// RUN: %clang_cc1 -triple mipsx -emit-llvm %s -o - | FileCheck %s

// __attribute__((aligned)) without a value gives the stack's alignment, 8
// bytes (abi.md section 8), which alloca and malloc guarantee.

struct S { int x __attribute__((aligned)); };
_Static_assert(_Alignof(struct S) == 8, "");
_Static_assert(__BIGGEST_ALIGNMENT__ == 8, "");

void g(void *);

// CHECK-LABEL: define{{.*}} void @f(
// CHECK: alloca %struct.S, align 8
void f(void) {
  struct S s;
  g(&s);
}
