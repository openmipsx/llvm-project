// RUN: %clang_cc1 -triple mipsx -emit-llvm -o - %s | FileCheck %s

// The MIPS-X calling convention, MIPS-X SDK docs/abi.md section 7: every
// argument becomes 32-bit words; aggregates up to 16 bytes are [N x i32]
// (i32 for one word), larger ones a pointer to a copy; aggregates up to 8
// bytes are returned in r2/r3, larger ones through sret.

// Type sizes and alignments, abi.md section 3.
_Static_assert(sizeof(long) == 4 && _Alignof(long) == 4, "");
_Static_assert(sizeof(long long) == 8 && _Alignof(long long) == 8, "");
_Static_assert(sizeof(double) == 8 && _Alignof(double) == 8, "");
_Static_assert(sizeof(long double) == 8 && _Alignof(long double) == 8, "");
_Static_assert(sizeof(void *) == 4 && _Alignof(void *) == 4, "");
_Static_assert(sizeof(__SIZE_TYPE__) == 4, "");
_Static_assert(sizeof(__WCHAR_TYPE__) == 4 && (__WCHAR_TYPE__)-1 < 0, "");
_Static_assert((char)-1 < 0, "char is signed");
_Static_assert(_Alignof(struct { char c; long long x; }) == 8, "");
_Static_assert(sizeof(enum { E0 }) == 4, "");

// CHECK-LABEL: define{{.*}} signext i8 @f_char(i8 noundef signext %c, i8 noundef zeroext %uc, i1 noundef zeroext %b)
char f_char(char c, unsigned char uc, _Bool b) { return c + uc + b; }

// CHECK-LABEL: define{{.*}} zeroext i16 @f_short(i16 noundef signext %s, i16 noundef zeroext %us)
unsigned short f_short(short s, unsigned short us) { return s + us; }

// CHECK-LABEL: define{{.*}} i32 @f_int(i32 noundef %i, i32 noundef %l, ptr noundef %p)
int f_int(int i, long l, void *p) { return i + l + (int)p; }

// CHECK-LABEL: define{{.*}} i64 @f_i64(i32 noundef %a, i64 noundef %b)
long long f_i64(int a, long long b) { return a + b; }

// CHECK-LABEL: define{{.*}} double @f_fp(float noundef %f, double noundef %d, double noundef %ld)
double f_fp(float f, double d, long double ld) { return f + d + ld; }

struct empty {};
struct c1 { char a; };
struct c3 { char a, b, c; };
struct s6 { short a, b, c; };
struct i2 { int a, b; };
struct i3 { int a, b, c; };
struct ll2 { long long a, b; };
struct big { int a[5]; };
union u { int i; char c[6]; };

// CHECK-LABEL: define{{.*}} void @f_empty()
void f_empty(struct empty e) {}

// CHECK-LABEL: define{{.*}} i32 @f_c1(i32 %x.coerce)
struct c1 f_c1(struct c1 x) { return x; }

// CHECK-LABEL: define{{.*}} i32 @f_c3(i32 %x.coerce)
struct c3 f_c3(struct c3 x) { return x; }

// CHECK-LABEL: define{{.*}} [2 x i32] @f_s6([2 x i32] %x.coerce)
struct s6 f_s6(struct s6 x) { return x; }

// CHECK-LABEL: define{{.*}} [2 x i32] @f_i2([2 x i32] %x.coerce)
struct i2 f_i2(struct i2 x) { return x; }

// A 12-byte struct is passed in words but returned through sret.
// CHECK-LABEL: define{{.*}} void @f_i3(ptr dead_on_unwind noalias writable sret(%struct.i3) align 4 %agg.result, [3 x i32] %x.coerce)
struct i3 f_i3(struct i3 x) { return x; }

// CHECK-LABEL: define{{.*}} void @f_ll2(ptr dead_on_unwind noalias writable sret(%struct.ll2) align 8 %agg.result, [4 x i32] %x.coerce)
struct ll2 f_ll2(struct ll2 x) { return x; }

// More than 16 bytes: a pointer to the caller's copy, not byval.
// CHECK-LABEL: define{{.*}} i32 @f_big(ptr noundef align 8 dead_on_return %x)
int f_big(struct big x) { return x.a[4]; }

// CHECK-LABEL: define{{.*}} [2 x i32] @f_union([2 x i32] %x.coerce)
union u f_union(union u x) { return x; }

// CHECK-LABEL: define{{.*}} [2 x i32] @f_cfloat([2 x i32] noundef %x.coerce)
_Complex float f_cfloat(_Complex float x) { return x; }

// CHECK-LABEL: define{{.*}} void @f_cdouble(ptr {{.*}}sret({ double, double }) align 8 %agg.result, [4 x i32] noundef %x.coerce)
_Complex double f_cdouble(_Complex double x) { return x; }

// _BitInt wider than 64 bits is an aggregate of its size (_BitInt(96) has
// 16 bytes).
// CHECK-LABEL: define{{.*}} i64 @f_bitint(i17 noundef signext %a, i64 noundef %b, [4 x i32] noundef %c.coerce)
_BitInt(64) f_bitint(_BitInt(17) a, _BitInt(64) b, _BitInt(96) c) {
  return a + b + (_BitInt(64))c;
}

// The caller makes the copy of a large aggregate and passes its address.
// CHECK-LABEL: define{{.*}} void @call_big()
// CHECK: %[[TMP:byval-temp]] = alloca %struct.big, align 8
// CHECK: call i32 @f_big(ptr noundef align 8 dead_on_return %[[TMP]])
void call_big(void) {
  struct big b = {{1, 2, 3, 4, 5}};
  f_big(b);
}
