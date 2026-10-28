//===-- MIPSXRegisterInfo.td - MIPS-X register file --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// General registers r0-r31 and the special registers reached through movfrs
// and movtos. The ABI aliases (zero, at, cb0, cb1, gp, sp, fp, ra) are
// accepted by the assembler; the printer always writes rN.
//
//===----------------------------------------------------------------------===//

class MIPSXReg<bits<5> num, string n> : Register<n> {
  let HWEncoding{4-0} = num;
  let Namespace = "MIPSX";
}

foreach i = 0-31 in {
  def R#i : MIPSXReg<i, "r"#i>, DwarfRegNum<[i]>;
}

// Special registers. The encoding is the 3-bit Spec field of movfrs and
// movtos. Spec 4 is the PC chain: read as PC-4 (pcm4), written as PC-1 (pcm1).
def PSW  : MIPSXReg<1, "psw">, DwarfRegNum<[32]>;
def MD   : MIPSXReg<2, "md">, DwarfRegNum<[33]>;
def PCM4 : MIPSXReg<4, "pcm4">;
def PCM1 : MIPSXReg<4, "pcm1">;

def GPR : RegisterClass<"MIPSX", [i32], 32, (sequence "R%u", 0, 31)>;

// Special registers readable by movfrs and writable by movtos.
let isAllocatable = 0 in {
  def SPRRd : RegisterClass<"MIPSX", [i32], 32, (add PSW, MD, PCM4)>;
  def SPRWr : RegisterClass<"MIPSX", [i32], 32, (add PSW, MD, PCM1)>;
}
