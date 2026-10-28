#!/usr/bin/env python3
"""Link objects into a program for the MIPS-X simulator.

mipsx-link.py --sdk SDK --bin LLVM_BIN -o OUT OBJ...

Assembles SDK/rt/crt0.S and SDK/rt/exit-sim.S with llvm-mc (next to OUT)
and links them before OBJ with ld.lld and SDK/rt/sim.ld.
"""

import argparse
import os
import subprocess
import sys


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--sdk", required=True)
    p.add_argument("--bin", required=True)
    p.add_argument("-o", dest="out", required=True)
    p.add_argument("objs", nargs="+")
    a = p.parse_args()

    rt = os.path.join(a.sdk, "rt")
    start = []
    for name in ("crt0.S", "exit-sim.S"):
        obj = "%s.%s.o" % (a.out, name)
        subprocess.check_call(
            [os.path.join(a.bin, "llvm-mc"), "-triple=mipsx", "-filetype=obj",
             os.path.join(rt, name), "-o", obj])
        start.append(obj)
    subprocess.check_call(
        [os.path.join(a.bin, "ld.lld"), "-T", os.path.join(rt, "sim.ld")]
        + start + a.objs + ["-o", a.out])


if __name__ == "__main__":
    sys.exit(main())
