#!/usr/bin/env python3
"""Measure the simulator programs in this directory: cycles, instructions
and the share of delay slots that hold nop, as reported by mipsx-sim
--stats. Not run by lit; used to record the effect of the delay slot
filler (work package S11).

measure.py --sdk SDK --bin LLVM_BIN [llc options...]
"""

import argparse
import glob
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TESTS = os.path.dirname(HERE)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--sdk", required=True)
    p.add_argument("--bin", required=True)
    a, llc_opts = p.parse_known_args()
    sim = os.path.join(a.sdk, "sim", "build", "mipsx-sim")
    tot = [0, 0, 0, 0]
    print("%-14s %9s %9s %7s %7s %6s" % ("program", "cycles", "insns", "slots",
                                         "nops", "nop%"))
    with tempfile.TemporaryDirectory() as tmp:
        for src in sorted(glob.glob(os.path.join(TESTS, "*.ll"))):
            name = os.path.splitext(os.path.basename(src))[0]
            obj = os.path.join(tmp, name + ".o")
            elf = os.path.join(tmp, name)
            subprocess.check_call([os.path.join(a.bin, "llc"), "-mtriple=mipsx",
                                   "-filetype=obj", src, "-o", obj] + llc_opts)
            subprocess.check_call([sys.executable,
                                   os.path.join(HERE, "mipsx-link.py"),
                                   "--sdk", a.sdk, "--bin", a.bin, "--builtins",
                                   "-o", elf, obj])
            r = subprocess.run([sim, "--stats", elf], capture_output=True,
                               text=True)
            m = re.search(r"cycles (\d+), instructions (\d+).*delay slots "
                          r"(\d+) \(nop (\d+)\)", r.stderr)
            if not m:
                sys.exit("%s: no statistics (status %d)\n%s"
                         % (name, r.returncode, r.stderr))
            c, i, s, n = map(int, m.groups())
            for k, v in enumerate((c, i, s, n)):
                tot[k] += v
            print("%-14s %9d %9d %7d %7d %5.1f%%" % (name, c, i, s, n,
                                                    100.0 * n / s))
    print("%-14s %9d %9d %7d %7d %5.1f%%" % ("total", tot[0], tot[1], tot[2],
                                             tot[3], 100.0 * tot[3] / tot[2]))


if __name__ == "__main__":
    main()
