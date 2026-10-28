// RUN: %clang_cc1 -triple mipsx -emit-llvm -o - %s | FileCheck %s

// MIPS-X SDK docs/abi.md section 7.2: va_list is a char * to the next
// variadic word; every argument takes ceil(size / 4) words with no extra
// alignment, and aggregates larger than 16 bytes are read as a pointer.

#include <stdarg.h>

_Static_assert(sizeof(va_list) == 4, "");

struct c3 { char a, b, c; };
struct big { int a[5]; };

// CHECK-LABEL: define{{.*}} i32 @va_int(i32 noundef %n, ...)
// CHECK: %[[AP:.*]] = load ptr, ptr %ap, align 4
// CHECK: %[[NEXT:.*]] = getelementptr inbounds i8, ptr %[[AP]], i32 4
// CHECK: store ptr %[[NEXT]], ptr %ap, align 4
// CHECK: load i32, ptr %[[AP]], align 4
int va_int(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int r = va_arg(ap, int);
  va_end(ap);
  return r;
}

// No 8-byte alignment of ap for double and long long.
// CHECK-LABEL: define{{.*}} double @va_double(i32 noundef %n, ...)
// CHECK: %[[AP:.*]] = load ptr, ptr %ap, align 4
// CHECK-NOT: llvm.ptrmask
// CHECK: %[[NEXT:.*]] = getelementptr inbounds i8, ptr %[[AP]], i32 8
// CHECK: load double, ptr %[[AP]], align 4
double va_double(int n, ...) {
  va_list ap;
  va_start(ap, n);
  double r = va_arg(ap, double);
  va_end(ap);
  return r;
}

// A 3-byte struct takes one word and is left-justified in it.
// CHECK-LABEL: define{{.*}} i32 @va_c3(i32 noundef %n, ...)
// CHECK: %[[AP:.*]] = load ptr, ptr %ap, align 4
// CHECK: getelementptr inbounds i8, ptr %[[AP]], i32 4
// CHECK: call void @llvm.memcpy.p0.p0.i32(ptr align 1 %{{.*}}, ptr align 4 %[[AP]], i32 3, i1 false)
int va_c3(int n, ...) {
  va_list ap;
  va_start(ap, n);
  struct c3 s = va_arg(ap, struct c3);
  va_end(ap);
  return s.c;
}

// CHECK-LABEL: define{{.*}} i32 @va_big(i32 noundef %n, ...)
// CHECK: %[[AP:.*]] = load ptr, ptr %ap, align 4
// CHECK: getelementptr inbounds i8, ptr %[[AP]], i32 4
// CHECK: %[[PTR:.*]] = load ptr, ptr %[[AP]], align 4
// CHECK: call void @llvm.memcpy.p0.p0.i32(ptr align 4 %{{.*}}, ptr align 4 %[[PTR]], i32 20, i1 false)
int va_big(int n, ...) {
  va_list ap;
  va_start(ap, n);
  struct big b = va_arg(ap, struct big);
  va_end(ap);
  return b.a[4];
}

// Variadic arguments are words like named ones; the backend puts them on
// the stack.
// CHECK-LABEL: define{{.*}} void @call_va()
// CHECK: call i32 (i32, ...) @va_int(i32 noundef 1, i32 noundef 2, i64 noundef 3, double noundef 4.000000e+00, i32 %{{.*}}, ptr noundef align 8 dead_on_return %{{.*}})
void call_va(void) {
  struct c3 s = {1, 2, 3};
  struct big b = {{0}};
  va_int(1, (short)2, 3LL, 4.0f, s, b);
}
