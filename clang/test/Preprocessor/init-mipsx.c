// Predefined macros for MIPS-X (MIPS-X SDK docs/abi.md section 3).

// RUN: %clang_cc1 -E -dM -ffreestanding -triple=mipsx < /dev/null \
// RUN:   | FileCheck -match-full-lines -check-prefixes=MIPSX,GNU,ES3890 %s
// RUN: %clang_cc1 -E -dM -ffreestanding -triple=mipsx -std=c11 < /dev/null \
// RUN:   | FileCheck -match-full-lines -check-prefixes=MIPSX,NOGNU,ES3890 %s
// RUN: %clang_cc1 -E -dM -ffreestanding -triple=mipsx -target-cpu mipsx < /dev/null \
// RUN:   | FileCheck -match-full-lines -check-prefixes=MIPSX,GNU,GENERIC %s
// RUN: %clang -E -dM -ffreestanding --target=mipsx -mlong-calls -x c /dev/null \
// RUN:   | FileCheck -match-full-lines -check-prefix=LONGCALLS %s

// MIPSX: #define __BIGGEST_ALIGNMENT__ 8
// MIPSX: #define __BIG_ENDIAN__ 1
// MIPSX: #define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__
// MIPSX-NOT: #define __CHAR_UNSIGNED__
// MIPSX: #define __CLANG_ATOMIC_INT_LOCK_FREE 1
// MIPSX: #define __CLANG_ATOMIC_LLONG_LOCK_FREE 1
// MIPSX-NOT: #define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
// MIPSX: #define __INT64_TYPE__ long long int
// MIPSX: #define __INTMAX_TYPE__ long long int
// MIPSX: #define __INTPTR_TYPE__ int
// MIPSX: #define __LDBL_MANT_DIG__ 53
// MIPSX: #define __MIPSX__ 1
// MIPSX: #define __POINTER_WIDTH__ 32
// MIPSX: #define __PTRDIFF_TYPE__ int
// MIPSX: #define __SIZEOF_DOUBLE__ 8
// MIPSX: #define __SIZEOF_INT__ 4
// MIPSX: #define __SIZEOF_LONG_DOUBLE__ 8
// MIPSX: #define __SIZEOF_LONG_LONG__ 8
// MIPSX: #define __SIZEOF_LONG__ 4
// MIPSX: #define __SIZEOF_POINTER__ 4
// MIPSX: #define __SIZEOF_SIZE_T__ 4
// MIPSX: #define __SIZEOF_WCHAR_T__ 4
// MIPSX: #define __SIZE_TYPE__ unsigned int
// MIPSX: #define __WCHAR_TYPE__ int
// MIPSX: #define __mipsx 1
// MIPSX-NOT: #define __mips
// MIPSX: #define __mipsx__ 1
// ES3890: #define __mipsx_byte_access__ 1
// ES3890: #define __mipsx_es3890__ 1
// GENERIC-NOT: #define __mipsx_byte_access__
// GENERIC-NOT: #define __mipsx_es3890__
// MIPSX-NOT: #define __mipsx_long_calls__
// GNU: #define mipsx 1
// NOGNU-NOT: #define mipsx 1

// LONGCALLS: #define __mipsx_long_calls__ 1
