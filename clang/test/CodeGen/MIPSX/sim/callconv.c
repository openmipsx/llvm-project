// Cross-check of Clang's MIPS-X calling convention (MIPS-X SDK docs/abi.md
// section 7) against hand-written assembly that follows the document: spy
// records the argument words a call puts in r4-r11 and on the stack,
// spy_sret/spy return values, and call_recv calls a C function with
// arguments it laid out itself. main exits with the number of the first
// wrong result.
//
// RUN: %clang --target=mipsx --sysroot=%mipsx-sysroot -O2 %s %S/Inputs/spy.s -o %t
// RUN: %mipsx-run --expect-exit=0 %t
// RUN: %clang --target=mipsx --sysroot=%mipsx-sysroot -O0 %s %S/Inputs/spy.s -o %t0
// RUN: %mipsx-run --expect-exit=0 %t0

#include <stdarg.h>

typedef unsigned int u32;
extern u32 spy_regs[8], spy_stack[8];
void spy(void);
void spyv(int, ...);
int call_recv(void);
struct i3 spy_sret(void);

struct c3 { char a, b, c; };
struct s6 { short a, b, c; };
struct i2 { int a, b; };
struct i3 { int a, b, c; };
struct ll2 { long long a, b; };
struct big { int a[5]; };

static int failed;
static int check;

// Word N of the arguments: r4-r11, then the stack.
static u32 word(int n) { return n < 8 ? spy_regs[n] : spy_stack[n - 8]; }

static void expect(int n, u32 mask, u32 value) {
  ++check;
  if (!failed && (word(n) & mask) != value)
    failed = check;
}

int recv(struct c3 a, long long b, struct s6 c, short d, int e, int f,
         int g) {
  return a.a == 1 && a.b == 2 && a.c == 3 && b == -2 && c.a == 4 &&
                 c.b == 5 && c.c == 6 && d == -7 && e == 8 && f == 9 &&
                 g == 10
             ? 0x5a
             : 0;
}

int sumv(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int ok = n == 5;
  ok &= va_arg(ap, int) == -1;
  ok &= va_arg(ap, long long) == 0x100000000LL;
  struct c3 c = va_arg(ap, struct c3);
  ok &= c.a == 1 && c.b == 2 && c.c == 3;
  // Compare the bits: there are no floating-point helpers yet.
  double d = va_arg(ap, double);
  unsigned long long bits;
  __builtin_memcpy(&bits, &d, sizeof(bits));
  ok &= bits == 0x3fe0000000000000ULL;
  struct big b = va_arg(ap, struct big);
  ok &= b.a[0] == 1 && b.a[4] == 5;
  va_end(ap);
  return ok ? 0x5a : 0;
}

#define ALL 0xffffffffu
#define SPY(proto) ((proto)(void (*)(void))spy)

int main(void) {
  // Narrow integers are extended by the caller according to their type.
  SPY(void (*)(char, unsigned char, short, unsigned short, _Bool, int))(
      -1, 0xff, -2, 0xfffe, 1, 0x12345678);
  expect(0, ALL, 0xffffffff);
  expect(1, ALL, 0xff);
  expect(2, ALL, 0xfffffffe);
  expect(3, ALL, 0xfffe);
  expect(4, ALL, 1);
  expect(5, ALL, 0x12345678);

  // 64-bit values: two words, most significant first, no alignment; a
  // value can be split between r11 and the stack.
  SPY(void (*)(int, long long, double, float, int, int, long long, int))(
      1, 0x1122334455667788LL, 2.0, 1.5f, 6, 7, 0x0a0b0c0d0e0f1011LL, 9);
  expect(0, ALL, 1);
  expect(1, ALL, 0x11223344);
  expect(2, ALL, 0x55667788);
  expect(3, ALL, 0x40000000);
  expect(4, ALL, 0);
  expect(5, ALL, 0x3fc00000);
  expect(6, ALL, 6);
  expect(7, ALL, 7);
  expect(8, ALL, 0x0a0b0c0d);
  expect(9, ALL, 0x0e0f1011);
  expect(10, ALL, 9);

  // Small aggregates: their memory image, word by word, the last partial
  // word left-justified (padding undefined).
  struct c3 c3 = {1, 2, 3};
  struct s6 s6 = {4, 5, 6};
  struct ll2 ll2 = {0x0102030405060708LL, 0x1112131415161718LL};
  SPY(void (*)(struct c3, struct s6, int, int, int, struct ll2))(
      c3, s6, 20, 21, 22, ll2);
  expect(0, 0xffffff00, 0x01020300);
  expect(1, ALL, 0x00040005);
  expect(2, 0xffff0000, 0x00060000);
  expect(3, ALL, 20);
  expect(4, ALL, 21);
  expect(5, ALL, 22);
  expect(6, ALL, 0x01020304);
  expect(7, ALL, 0x05060708);
  expect(8, ALL, 0x11121314);
  expect(9, ALL, 0x15161718);

  // A large aggregate: a pointer to a copy, 8-byte aligned.
  struct big big = {{1, 2, 3, 4, 5}};
  SPY(void (*)(int, struct big, int))(30, big, 31);
  expect(0, ALL, 30);
  {
    const struct big *p = (const struct big *)spy_regs[1];
    ++check;
    if (!failed && (p == &big || ((u32)p & 7) || p->a[0] != 1 ||
                    p->a[4] != 5))
      failed = check;
  }
  expect(2, ALL, 31);

  // Variadic arguments always go on the stack, even with r5-r11 free.
  spyv(40, 41, 0x2122232425262728LL, c3, 3.0);
  expect(0, ALL, 40);
  expect(8, ALL, 41);
  expect(9, ALL, 0x21222324);
  expect(10, ALL, 0x25262728);
  expect(11, 0xffffff00, 0x01020300);
  expect(12, ALL, 0x40080000);
  expect(13, ALL, 0);

  // Return values: r2, r3 as a memory image; larger ones through r4.
  ++check;
  {
    struct i2 r = ((struct i2 (*)(void))spy)();
    if (!failed && (r.a != 0x01020304 || r.b != 0x05060708))
      failed = check;
  }
  ++check;
  {
    struct c3 r = ((struct c3 (*)(void))spy)();
    if (!failed && (r.a != 1 || r.b != 2 || r.c != 3))
      failed = check;
  }
  ++check;
  {
    long long r = ((long long (*)(void))spy)();
    if (!failed && r != 0x0102030405060708LL)
      failed = check;
  }
  ++check;
  {
    struct i3 r = spy_sret();
    if (!failed && (r.a != 0x11 || r.b != 0x22 || r.c != 0x33))
      failed = check;
  }

  // The callee side: assembly lays out the arguments of recv.
  ++check;
  if (!failed && call_recv() != 0x5a)
    failed = check;

  // va_arg reads what a variadic call wrote.
  ++check;
  if (!failed && sumv(5, (char)-1, 0x100000000LL, c3, 0.5, big) != 0x5a)
    failed = check;

  return failed;
}
