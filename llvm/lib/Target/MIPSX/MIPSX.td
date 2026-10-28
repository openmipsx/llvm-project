//===-- MIPSX.td - Describe the MIPS-X target machine ------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MIPS-X is the Stanford MIPS-X processor as used in the ESS ES3890. The
// instruction set is described in Paul Chow, "MIPS-X Instruction Set and
// Programmer's Manual" (CSL-86-289), with the ES3890 differences in the
// project's processor specification. The ABI is in the MIPS-X SDK,
// docs/abi.md.
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Subtarget features
//===----------------------------------------------------------------------===//

def FeatureES3890
    : SubtargetFeature<"es3890", "HasES3890", "true",
                       "ES3890 byte and halfword loads and stores">;
def HasES3890 : Predicate<"Subtarget->hasES3890()">,
                AssemblerPredicate<(all_of FeatureES3890),
                                   "ES3890 byte and halfword memory access">;

def FeatureLongCalls
    : SubtargetFeature<"long-calls", "UseLongCalls", "true",
                       "Make direct calls through a register (any reach)">;

//===----------------------------------------------------------------------===//
// Registers and instructions
//===----------------------------------------------------------------------===//

include "MIPSXRegisterInfo.td"
include "MIPSXInstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<GPR>;

def MIPSXInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Processors
//===----------------------------------------------------------------------===//

def : ProcessorModel<"mipsx", NoSchedModel, []>;
def : ProcessorModel<"es3890", NoSchedModel, [FeatureES3890]>;
def : ProcessorModel<"generic", NoSchedModel, [FeatureES3890]>;

//===----------------------------------------------------------------------===//
// Assembly syntax
//===----------------------------------------------------------------------===//

def MIPSXInstPrinter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
  bit isMCAsmWriter = 1;
}

def MIPSXAsmParser : AsmParser {
  let ShouldEmitMatchRegisterAltName = 0;
}

def MIPSXAsmParserVariant : AsmParserVariant {
  let RegisterPrefix = "";
  // '#' introduces an immediate; '[' and ']' delimit a memory base register.
  let TokenizingCharacters = "[]#";
}

def MIPSX : Target {
  let InstructionSet = MIPSXInstrInfo;
  let AssemblyParsers = [MIPSXAsmParser];
  let AssemblyParserVariants = [MIPSXAsmParserVariant];
  let AssemblyWriters = [MIPSXInstPrinter];
}
