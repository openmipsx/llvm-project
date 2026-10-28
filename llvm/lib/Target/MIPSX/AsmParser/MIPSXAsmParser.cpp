//===-- MIPSXAsmParser.cpp - Parse MIPS-X assembly to MCInst instructions -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Syntax as in MIPS-X SDK docs/abi.md section 10: sources first, destination
// last, '#' before immediates, "offset[base]" memory operands, registers rN
// or their ABI aliases.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "TargetInfo/MIPSXTargetInfo.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

namespace {

struct MIPSXOperand;

class MIPSXAsmParser : public MCTargetAsmParser {
  MCRegister matchRegister(StringRef Name);
  bool parseOperand(OperandVector &Operands);
  bool parseMemoryBase(OperandVector &Operands);

  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;
  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;
  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

#define GET_ASSEMBLER_HEADER
#include "MIPSXGenAsmMatcher.inc"

public:
  enum MIPSXMatchResultTy {
    Match_Dummy = FIRST_TARGET_MATCH_RESULT_TY,
#define GET_OPERAND_DIAGNOSTIC_TYPES
#include "MIPSXGenAsmMatcher.inc"
#undef GET_OPERAND_DIAGNOSTIC_TYPES
  };

  MIPSXAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                 const MCInstrInfo &MII)
      : MCTargetAsmParser(STI, MII) {
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};

struct MIPSXOperand : public MCParsedAsmOperand {
  enum KindTy { Token, Register, Immediate } Kind;
  SMLoc StartLoc, EndLoc;
  StringRef Tok;
  MCRegister Reg;
  const MCExpr *Imm = nullptr;

  MIPSXOperand(KindTy K, SMLoc S, SMLoc E) : Kind(K), StartLoc(S), EndLoc(E) {}

  bool isToken() const override { return Kind == Token; }
  bool isReg() const override { return Kind == Register; }
  bool isImm() const override { return Kind == Immediate; }
  bool isMem() const override { return false; }

  StringRef getToken() const {
    assert(isToken());
    return Tok;
  }
  MCRegister getReg() const override {
    assert(isReg());
    return Reg;
  }
  const MCExpr *getImm() const {
    assert(isImm());
    return Imm;
  }
  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  // A constant within [Lo, Hi], or (if AllowSymbolic) any other expression.
  bool isImmInRange(int64_t Lo, int64_t Hi, bool AllowSymbolic) const {
    if (!isImm())
      return false;
    int64_t Value;
    if (Imm->evaluateAsAbsolute(Value))
      return Value >= Lo && Value <= Hi;
    return AllowSymbolic;
  }

  bool isImm17() const { return isImmInRange(-65536, 65535, true); }
  bool isBrTarget() const { return isImmInRange(-32768, 32767, true); }
  bool isShAmt() const { return isImmInRange(1, 32, false); }
  bool isTrapVec() const { return isImmInRange(0, 255, false); }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::createReg(getReg()));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    int64_t Value;
    if (Imm->evaluateAsAbsolute(Value))
      Inst.addOperand(MCOperand::createImm(Value));
    else
      Inst.addOperand(MCOperand::createExpr(Imm));
  }

  void print(raw_ostream &OS, const MCAsmInfo &MAI) const override {
    switch (Kind) {
    case Token:
      OS << "Token: " << Tok;
      break;
    case Register:
      OS << "Reg: " << Reg.id();
      break;
    case Immediate:
      OS << "Imm: ";
      MAI.printExpr(OS, *Imm);
      break;
    }
  }

  static std::unique_ptr<MIPSXOperand> createToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<MIPSXOperand>(Token, S, S);
    Op->Tok = Str;
    return Op;
  }
  static std::unique_ptr<MIPSXOperand> createReg(MCRegister R, SMLoc S,
                                                 SMLoc E) {
    auto Op = std::make_unique<MIPSXOperand>(Register, S, E);
    Op->Reg = R;
    return Op;
  }
  static std::unique_ptr<MIPSXOperand> createImm(const MCExpr *Val, SMLoc S,
                                                 SMLoc E) {
    auto Op = std::make_unique<MIPSXOperand>(Immediate, S, E);
    Op->Imm = Val;
    return Op;
  }
};

} // end anonymous namespace

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "MIPSXGenAsmMatcher.inc"

// Register names are case-insensitive; the ABI aliases are accepted too.
MCRegister MIPSXAsmParser::matchRegister(StringRef Name) {
  std::string Lower = Name.lower();
  if (MCRegister Reg = MatchRegisterName(Lower))
    return Reg;
  return StringSwitch<MCRegister>(Lower)
      .Case("zero", MIPSX::R0)
      .Case("at", MIPSX::R1)
      .Case("cb0", MIPSX::R23)
      .Case("cb1", MIPSX::R24)
      .Case("gp", MIPSX::R28)
      .Case("sp", MIPSX::R29)
      .Case("fp", MIPSX::R30)
      .Case("ra", MIPSX::R31)
      .Default(MCRegister());
}

ParseStatus MIPSXAsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                             SMLoc &EndLoc) {
  const AsmToken &Tok = getParser().getTok();
  StartLoc = Tok.getLoc();
  EndLoc = Tok.getEndLoc();
  if (Tok.isNot(AsmToken::Identifier))
    return ParseStatus::NoMatch;
  Reg = matchRegister(Tok.getIdentifier());
  if (!Reg)
    return ParseStatus::NoMatch;
  getParser().Lex();
  return ParseStatus::Success;
}

bool MIPSXAsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                   SMLoc &EndLoc) {
  if (!tryParseRegister(Reg, StartLoc, EndLoc).isSuccess())
    return Error(StartLoc, "invalid register name");
  return false;
}

// Parse "[reg]" after a memory offset.
bool MIPSXAsmParser::parseMemoryBase(OperandVector &Operands) {
  MCAsmParser &Parser = getParser();
  Operands.push_back(MIPSXOperand::createToken("[", Parser.getTok().getLoc()));
  Parser.Lex(); // Eat '['.

  MCRegister Reg;
  SMLoc S, E;
  if (!tryParseRegister(Reg, S, E).isSuccess())
    return Error(Parser.getTok().getLoc(), "expected base register");
  Operands.push_back(MIPSXOperand::createReg(Reg, S, E));

  if (Parser.getTok().isNot(AsmToken::RBrac))
    return Error(Parser.getTok().getLoc(), "expected ']'");
  Operands.push_back(MIPSXOperand::createToken("]", Parser.getTok().getLoc()));
  Parser.Lex(); // Eat ']'.
  return false;
}

bool MIPSXAsmParser::parseOperand(OperandVector &Operands) {
  MCAsmParser &Parser = getParser();
  SMLoc S = Parser.getTok().getLoc();

  // Immediate: '#' expr.
  if (Parser.getTok().is(AsmToken::Hash)) {
    Operands.push_back(MIPSXOperand::createToken("#", S));
    Parser.Lex();
    S = Parser.getTok().getLoc();
    const MCExpr *Val;
    SMLoc E;
    if (Parser.parseExpression(Val, E))
      return true;
    Operands.push_back(MIPSXOperand::createImm(Val, S, E));
    return false;
  }

  // Register.
  MCRegister Reg;
  SMLoc RS, RE;
  if (tryParseRegister(Reg, RS, RE).isSuccess()) {
    Operands.push_back(MIPSXOperand::createReg(Reg, RS, RE));
    return false;
  }

  // Memory operand with no offset: "[reg]" means "0[reg]".
  if (Parser.getTok().is(AsmToken::LBrac)) {
    Operands.push_back(
        MIPSXOperand::createImm(MCConstantExpr::create(0, getContext()), S, S));
    return parseMemoryBase(Operands);
  }

  // Expression: a branch target, or a memory offset followed by "[reg]".
  const MCExpr *Val;
  SMLoc E;
  if (Parser.parseExpression(Val, E))
    return true;
  Operands.push_back(MIPSXOperand::createImm(Val, S, E));
  if (Parser.getTok().is(AsmToken::LBrac))
    return parseMemoryBase(Operands);
  return false;
}

bool MIPSXAsmParser::parseInstruction(ParseInstructionInfo &Info,
                                      StringRef Name, SMLoc NameLoc,
                                      OperandVector &Operands) {
  // Mnemonics are case-insensitive. The token must outlive this call, so
  // keep the lower-case copy in the context.
  Operands.push_back(MIPSXOperand::createToken(
      getContext().allocateString(Name.lower()), NameLoc));

  if (getLexer().is(AsmToken::EndOfStatement)) {
    getParser().Lex();
    return false;
  }

  if (parseOperand(Operands))
    return true;
  while (parseOptionalToken(AsmToken::Comma))
    if (parseOperand(Operands))
      return true;

  return parseEOL();
}

bool MIPSXAsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                             OperandVector &Operands,
                                             MCStreamer &Out,
                                             uint64_t &ErrorInfo,
                                             bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned Result =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);

  SMLoc ErrorLoc = IDLoc;
  if (ErrorInfo != ~0ULL && ErrorInfo < Operands.size()) {
    ErrorLoc = Operands[ErrorInfo]->getStartLoc();
    if (ErrorLoc == SMLoc())
      ErrorLoc = IDLoc;
  }

  switch (Result) {
  case Match_Success:
    Inst.setLoc(IDLoc);
    Out.emitInstruction(Inst, getSTI());
    Opcode = Inst.getOpcode();
    return false;
  case Match_MissingFeature:
    return Error(IDLoc, "instruction requires the ES3890 (-mcpu=es3890)");
  case Match_MnemonicFail:
    return Error(IDLoc, "unrecognized instruction mnemonic");
  case Match_InvalidOperand:
    if (ErrorInfo != ~0ULL && ErrorInfo >= Operands.size())
      return Error(IDLoc, "too few operands for instruction");
    return Error(ErrorLoc, "invalid operand for instruction");
  case Match_InvalidImm17:
    return Error(ErrorLoc, "immediate must be in the range [-65536, 65535]");
  case Match_InvalidBrTarget:
    return Error(ErrorLoc,
                 "branch displacement must be in the range [-32768, 32767]");
  case Match_InvalidShAmt:
    return Error(ErrorLoc, "shift amount must be in the range [1, 32]");
  case Match_InvalidTrapVec:
    return Error(ErrorLoc, "trap vector must be in the range [0, 255]");
  }
  llvm_unreachable("Unknown match type detected!");
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMIPSXAsmParser() {
  RegisterMCAsmParser<MIPSXAsmParser> X(getTheMIPSXTarget());
}
