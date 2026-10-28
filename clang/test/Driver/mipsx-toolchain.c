// A bare-metal MIPS-X toolchain: the run-time comes from the sysroot (see
// clang/lib/Driver/ToolChains/MIPSX.h), and programs link with ld.lld.

// RUN: %clang -### --target=mipsx --sysroot=%S/Inputs/mipsx_tree %s 2>&1 \
// RUN:   | FileCheck --check-prefix=DEFAULT %s
// DEFAULT: "-cc1" "-triple" "mipsx"
// DEFAULT-SAME: "-target-cpu" "es3890"
// DEFAULT-SAME: "-internal-isystem" "{{.*}}Inputs/mipsx_tree{{/|\\\\}}include"
// DEFAULT: "{{.*}}ld.lld" "-Bstatic"
// DEFAULT-SAME: "{{.*}}Inputs/mipsx_tree{{/|\\\\}}lib{{/|\\\\}}crt0.o"
// DEFAULT-SAME: "-L{{.*}}Inputs/mipsx_tree{{/|\\\\}}lib"
// DEFAULT-SAME: "-T{{.*}}Inputs/mipsx_tree{{/|\\\\}}lib{{/|\\\\}}sim.ld"
// DEFAULT-SAME: "{{.*}}.o" "--start-group" "-lbuiltins" "-lsim" "--end-group"
// DEFAULT-SAME: "-o" "a.out"

// RUN: %clang -### --target=mipsx --sysroot=%S/Inputs/mipsx_tree \
// RUN:   -mboard=device %s 2>&1 | FileCheck --check-prefix=DEVICE %s
// DEVICE: "-T{{.*}}Inputs/mipsx_tree{{/|\\\\}}lib{{/|\\\\}}device.ld"
// DEVICE-SAME: "--start-group" "-lbuiltins" "-ldevice" "--end-group"

// A linker script on the command line replaces the board's.
// RUN: %clang -### --target=mipsx --sysroot=%S/Inputs/mipsx_tree \
// RUN:   -T my.ld %s 2>&1 | FileCheck --check-prefix=SCRIPT %s
// SCRIPT: "{{.*}}ld.lld"
// SCRIPT-SAME: "-T" "my.ld"
// SCRIPT-NOT: sim.ld

// libc.a is linked when the sysroot has one.
// RUN: %clang -### --target=mipsx --sysroot=%S/Inputs/mipsx_libc_tree %s 2>&1 \
// RUN:   | FileCheck --check-prefix=LIBC %s
// LIBC: "--start-group" "-lc" "-lbuiltins" "-lsim" "--end-group"
// RUN: %clang -### --target=mipsx --sysroot=%S/Inputs/mipsx_libc_tree \
// RUN:   -nolibc %s 2>&1 | FileCheck --check-prefix=NOLIBC %s
// NOLIBC: "--start-group" "-lbuiltins" "-lsim" "--end-group"

// RUN: %clang -### --target=mipsx --sysroot=%S/Inputs/mipsx_tree \
// RUN:   -nostdlib %s 2>&1 | FileCheck --check-prefix=NOSTDLIB %s
// NOSTDLIB: "{{.*}}ld.lld"
// NOSTDLIB-NOT: crt0.o
// NOSTDLIB-NOT: "-lbuiltins"
// RUN: %clang -### --target=mipsx --sysroot=%S/Inputs/mipsx_tree \
// RUN:   -nostartfiles %s 2>&1 | FileCheck --check-prefix=NOSTART %s
// NOSTART: "{{.*}}ld.lld"
// NOSTART-NOT: crt0.o
// NOSTART: "-lbuiltins"

// Without --sysroot: <bin>/../lib/clang-runtimes/mipsx.
// RUN: %clang -### --target=mipsx -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOSYSROOT %s
// NOSYSROOT: "-internal-isystem" "{{.*}}lib{{/|\\\\}}clang-runtimes{{/|\\\\}}mipsx{{/|\\\\}}include"

// -mcpu, -mlong-calls; no frame pointer when optimising.
// RUN: %clang -### --target=mipsx -mcpu=mipsx -mlong-calls -O2 -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=FEATURES %s
// FEATURES: "-target-cpu" "mipsx"
// FEATURES-SAME: "-target-feature" "+long-calls"
// FEATURES-NOT: "-mframe-pointer=all"
// RUN: %clang -### --target=mipsx -O0 -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=O0 %s
// O0: "-mframe-pointer=all"

// RUN: not %clang -### --target=mipsx -mboard=foo %s 2>&1 \
// RUN:   | FileCheck --check-prefix=BADBOARD %s
// BADBOARD: error: unsupported argument 'foo' to option '-mboard='
// RUN: not %clang -### --target=x86_64-linux-gnu -mboard=sim -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=OTHER %s
// OTHER: error: unsupported option '-mboard=' for target 'x86_64-linux-gnu'

// Plain char is signed.
// RUN: %clang -### --target=mipsx -c %s 2>&1 | FileCheck --check-prefix=CHAR %s
// CHAR-NOT: "-fno-signed-char"
