#!/usr/bin/env python3
"""Link objects into a program for the MIPS-X simulator.

mipsx-link.py --sdk SDK --bin LLVM_BIN -o OUT OBJ...

Assembles SDK/rt/crt0.S, SDK/rt/exit-sim.S and SDK/rt/args-sim.S (crt0's
argv hooks, in SDKs that have them) with llvm-mc (next to OUT) and links
them before OBJ with ld.lld and SDK/rt/sim.ld. With --builtins,
the helper routines in SDK/rt/builtins (multiply, divide, variable shifts)
are linked too.
"""

import argparse
import glob
import os
import subprocess
import sys


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--sdk", required=True)
    p.add_argument("--bin", required=True)
    p.add_argument("-o", dest="out", required=True)
    p.add_argument("--builtins", action="store_true")
    p.add_argument("objs", nargs="+")
    a = p.parse_args()

    rt = os.path.join(a.sdk, "rt")
    sources = [os.path.join(rt, "crt0.S"), os.path.join(rt, "exit-sim.S")]
    if os.path.exists(os.path.join(rt, "args-sim.S")):
        sources.append(os.path.join(rt, "args-sim.S"))
    if a.builtins:
        sources += sorted(glob.glob(os.path.join(rt, "builtins", "*.S")))
    start = []
    for src in sources:
        obj = "%s.%s.o" % (a.out, os.path.basename(src))
        subprocess.check_call(
            [os.path.join(a.bin, "llvm-mc"), "-triple=mipsx", "-filetype=obj",
             src, "-o", obj])
        start.append(obj)
    subprocess.check_call(
        [os.path.join(a.bin, "ld.lld"), "-T", os.path.join(rt, "sim.ld")]
        + start + a.objs + ["-o", a.out])


if __name__ == "__main__":
    sys.exit(main())
