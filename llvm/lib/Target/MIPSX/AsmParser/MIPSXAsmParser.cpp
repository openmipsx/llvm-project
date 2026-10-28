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

#include "MCTargetDesc/MIPSXMCAsmInfo.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "TargetInfo/MIPSXTargetInfo.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
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
  bool parseImmExpr(const MCExpr *&Res, SMLoc &E);
  bool parseSpecifierExpr(const MCExpr *&Res, SMLoc &E);
  bool parseOperand(OperandVector &Operands);
  bool parseMemoryBase(OperandVector &Operands);
  bool parseDataExpr(const MCExpr *&Res) override;
  ParseStatus parseDirective(AsmToken DirectiveID) override;
  bool parseDirectiveData(unsigned Size, SMLoc L);

  bool expandPseudo(StringRef Mnemonic, OperandVector &Operands, SMLoc IDLoc,
                    MCStreamer &Out, bool &Handled);
  void emit(MCStreamer &Out, SMLoc Loc, MCInst Inst);
  void emitHiLo(MCStreamer &Out, SMLoc Loc, MCRegister Rd, const MCExpr *Hi,
                const MCExpr *Lo);

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

  // A constant within [Lo, Hi], or a symbolic expression whose outermost
  // relocation operator (if any) is one of Allowed.
  bool isImmInRange(int64_t Lo, int64_t Hi,
                    std::initializer_list<MIPSX::Specifier> Allowed) const {
    if (!isImm())
      return false;
    int64_t Value;
    if (Imm->evaluateAsAbsolute(Value))
      return Value >= Lo && Value <= Hi;
    MIPSX::Specifier S = MIPSX::S_None;
    if (const auto *SE = dyn_cast<MCSpecifierExpr>(Imm))
      S = SE->getSpecifier();
    else if (Allowed.size() == 0)
      return false;
    return S == MIPSX::S_None || llvm::is_contained(Allowed, S);
  }

  bool isImm17() const {
    return isImmInRange(-65536, 65535, {MIPSX::S_None, MIPSX::S_HI,
                                        MIPSX::S_LO, MIPSX::S_GPREL,
                                        MIPSX::S_W});
  }
  bool isMemOff17() const {
    return isImmInRange(-65536, 65535,
                        {MIPSX::S_None, MIPSX::S_HI, MIPSX::S_LO,
                         MIPSX::S_GPREL});
  }
  bool isJImm17() const {
    return isImmInRange(-65536, 65535,
                        {MIPSX::S_None, MIPSX::S_CALL, MIPSX::S_W});
  }
  bool isBrTarget() const {
    return isImmInRange(-32768, 32767, {MIPSX::S_None});
  }
  bool isShAmt() const { return isImmInRange(1, 32, {}); }
  bool isTrapVec() const { return isImmInRange(0, 255, {}); }

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

// Parse "%op(expr)". A constant operand is evaluated here (docs/abi.md
// section 10.6); %gprel always stays a relocation and %call needs a symbol.
bool MIPSXAsmParser::parseSpecifierExpr(const MCExpr *&Res, SMLoc &E) {
  MCAsmParser &Parser = getParser();
  SMLoc Loc = Parser.getTok().getLoc();
  Parser.Lex(); // Eat '%'.
  if (Parser.getTok().isNot(AsmToken::Identifier))
    return TokError("expected relocation operator after '%'");
  MIPSX::Specifier Spec =
      MIPSX::parseSpecifierName(Parser.getTok().getIdentifier());
  if (Spec == MIPSX::S_None)
    return TokError("unknown relocation operator");
  Parser.Lex(); // Eat the operator name.
  if (parseToken(AsmToken::LParen, "expected '('"))
    return true;
  const MCExpr *Sub;
  if (Parser.parseParenExpression(Sub, E))
    return true;

  int64_t V;
  if (Sub->evaluateAsAbsolute(V)) {
    int32_t V32 = static_cast<int32_t>(V);
    switch (Spec) {
    case MIPSX::S_HI:
      Res = MCConstantExpr::create(V32 >> 15, getContext());
      return false;
    case MIPSX::S_LO:
      Res = MCConstantExpr::create(V32 & 0x7fff, getContext());
      return false;
    case MIPSX::S_W:
      if (V32 & 3)
        return Error(Loc, "%w operand must be a multiple of 4");
      Res = MCConstantExpr::create(V32 >> 2, getContext());
      return false;
    case MIPSX::S_CALL:
      return Error(Loc, "%call needs a symbol");
    default:
      break;
    }
  }
  Res = MCSpecifierExpr::create(Sub, Spec, getContext(), Loc);
  return false;
}

// An immediate or offset: a relocation operator or a plain expression.
bool MIPSXAsmParser::parseImmExpr(const MCExpr *&Res, SMLoc &E) {
  if (getParser().getTok().is(AsmToken::Percent))
    return parseSpecifierExpr(Res, E);
  return getParser().parseExpression(Res, E);
}

// Data directives (.word, .long, .4byte, ...) accept %w(expr).
bool MIPSXAsmParser::parseDataExpr(const MCExpr *&Res) {
  SMLoc E;
  SMLoc Loc = getParser().getTok().getLoc();
  if (parseImmExpr(Res, E))
    return true;
  if (const auto *SE = dyn_cast<MCSpecifierExpr>(Res))
    if (SE->getSpecifier() != MIPSX::S_W)
      return Error(Loc, "only %w is allowed in data");
  return false;
}

// .word and .half, as in the MIPS-X manual: 32-bit and 16-bit values.
bool MIPSXAsmParser::parseDirectiveData(unsigned Size, SMLoc L) {
  auto ParseOne = [&]() -> bool {
    SMLoc Loc = getParser().getTok().getLoc();
    const MCExpr *Value;
    if (parseDataExpr(Value))
      return true;
    int64_t V;
    if (Value->evaluateAsAbsolute(V)) {
      if (!isIntN(Size * 8, V) && !isUIntN(Size * 8, V))
        return Error(Loc, "out of range literal value");
      getStreamer().emitIntValue(V, Size);
      return false;
    }
    if (Size != 4 && isa<MCSpecifierExpr>(Value))
      return Error(Loc, "%w is only allowed in 32-bit data");
    getStreamer().emitValue(Value, Size, Loc);
    return false;
  };
  return parseMany(ParseOne);
}

ParseStatus MIPSXAsmParser::parseDirective(AsmToken DirectiveID) {
  StringRef IDVal = DirectiveID.getIdentifier();
  if (IDVal == ".word")
    return parseDirectiveData(4, DirectiveID.getLoc());
  if (IDVal == ".half")
    return parseDirectiveData(2, DirectiveID.getLoc());
  return ParseStatus::NoMatch;
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
    if (parseImmExpr(Val, E))
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
  if (parseImmExpr(Val, E))
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
  if (parseEOL())
    return true;

  // bgt, ble, bhi and bls (and their squashing forms) are blt, bge, blo and
  // bhs with the register operands swapped.
  auto &Mnemonic = static_cast<MIPSXOperand &>(*Operands[0]);
  StringRef Base = Mnemonic.getToken();
  bool Squash = Base.consume_back("sq");
  StringRef Swapped = StringSwitch<StringRef>(Base)
                          .Case("bgt", "blt")
                          .Case("ble", "bge")
                          .Case("bhi", "blo")
                          .Case("bls", "bhs")
                          .Default("");
  if (!Swapped.empty() && Operands.size() == 4 && Operands[1]->isReg() &&
      Operands[2]->isReg()) {
    Mnemonic.Tok = getContext().allocateString(Swapped.str() +
                                               (Squash ? "sq" : ""));
    std::swap(Operands[1], Operands[2]);
  }
  return false;
}

void MIPSXAsmParser::emit(MCStreamer &Out, SMLoc Loc, MCInst Inst) {
  Inst.setLoc(Loc);
  Out.emitInstruction(Inst, getSTI());
}

static MCOperand immOperand(const MCExpr *E) {
  int64_t V;
  if (E->evaluateAsAbsolute(V))
    return MCOperand::createImm(V);
  return MCOperand::createExpr(E);
}

// addi r0,#Hi,rd; lsl rd,rd,#15; addi rd,#Lo,rd (docs/abi.md section 6).
void MIPSXAsmParser::emitHiLo(MCStreamer &Out, SMLoc Loc, MCRegister Rd,
                              const MCExpr *Hi, const MCExpr *Lo) {
  emit(Out, Loc,
       MCInstBuilder(MIPSX::ADDI).addReg(Rd).addReg(MIPSX::R0).addOperand(
           immOperand(Hi)));
  emit(Out, Loc,
       MCInstBuilder(MIPSX::SH).addReg(Rd).addReg(MIPSX::R0).addReg(Rd).addImm(
           32 - 15));
  emit(Out, Loc,
       MCInstBuilder(MIPSX::ADDI).addReg(Rd).addReg(Rd).addOperand(
           immOperand(Lo)));
}

// Pseudo-instructions of docs/abi.md section 10.5 that are not TableGen
// aliases. Sets Handled when Mnemonic is one of them; returns true on error.
bool MIPSXAsmParser::expandPseudo(StringRef Mnemonic, OperandVector &Operands,
                                  SMLoc IDLoc, MCStreamer &Out,
                                  bool &Handled) {
  auto Op = [&](unsigned I) -> MIPSXOperand & {
    return static_cast<MIPSXOperand &>(*Operands[I]);
  };
  auto IsTok = [&](unsigned I, StringRef T) {
    return I < Operands.size() && Op(I).isToken() && Op(I).getToken() == T;
  };
  auto IsReg = [&](unsigned I) {
    return I < Operands.size() && Op(I).isReg();
  };
  auto IsImm = [&](unsigned I) {
    return I < Operands.size() && Op(I).isImm();
  };
  auto Const = [&](unsigned I, int64_t &V) {
    return Op(I).getImm()->evaluateAsAbsolute(V);
  };
  MCContext &Ctx = getContext();

  if (Mnemonic == "li") {
    // li #v,rd
    Handled = true;
    if (Operands.size() != 4 || !IsTok(1, "#") || !IsImm(2) || !IsReg(3))
      return Error(IDLoc, "expected 'li #value,rd'");
    int64_t V;
    if (!Const(2, V))
      return Error(Op(2).getStartLoc(),
                   "li needs a constant; use la for a symbol");
    if (!isInt<32>(V) && !isUInt<32>(V))
      return Error(Op(2).getStartLoc(), "value does not fit in 32 bits");
    int32_t V32 = static_cast<int32_t>(V);
    MCRegister Rd = Op(3).getReg();
    if (isInt<17>(V32)) {
      emit(Out, IDLoc,
           MCInstBuilder(MIPSX::ADDI).addReg(Rd).addReg(MIPSX::R0).addImm(V32));
      return false;
    }
    unsigned T = llvm::countr_zero(static_cast<uint32_t>(V32));
    if (isInt<17>(V32 >> T)) {
      emit(Out, IDLoc,
           MCInstBuilder(MIPSX::ADDI).addReg(Rd).addReg(MIPSX::R0).addImm(
               V32 >> T));
      emit(Out, IDLoc,
           MCInstBuilder(MIPSX::SH).addReg(Rd).addReg(MIPSX::R0).addReg(Rd)
               .addImm(32 - T));
      return false;
    }
    emitHiLo(Out, IDLoc, Rd, MCConstantExpr::create(V32 >> 15, Ctx),
             MCConstantExpr::create(V32 & 0x7fff, Ctx));
    return false;
  }

  if (Mnemonic == "la") {
    // la expr,rd
    Handled = true;
    unsigned I = IsTok(1, "#") ? 2 : 1;
    if (Operands.size() != I + 2 || !IsImm(I) || !IsReg(I + 1))
      return Error(IDLoc, "expected 'la symbol,rd'");
    const MCExpr *E = Op(I).getImm();
    if (isa<MCSpecifierExpr>(E))
      return Error(Op(I).getStartLoc(),
                   "la takes an address, not a relocation operator");
    MCRegister Rd = Op(I + 1).getReg();
    int64_t V;
    if (E->evaluateAsAbsolute(V)) {
      int32_t V32 = static_cast<int32_t>(V);
      emitHiLo(Out, IDLoc, Rd, MCConstantExpr::create(V32 >> 15, Ctx),
               MCConstantExpr::create(V32 & 0x7fff, Ctx));
      return false;
    }
    emitHiLo(Out, IDLoc, Rd, MCSpecifierExpr::create(E, MIPSX::S_HI, Ctx),
             MCSpecifierExpr::create(E, MIPSX::S_LO, Ctx));
    return false;
  }

  if (Mnemonic == "call" || Mnemonic == "jmp") {
    // jspci r0,#%call(sym),r31 (call) or ...,r0 (jmp)
    Handled = true;
    if (Operands.size() != 2 || !IsImm(1))
      return Error(IDLoc, "expected '" + Mnemonic + " symbol'");
    const MCExpr *E = Op(1).getImm();
    int64_t V;
    if (E->evaluateAsAbsolute(V) || isa<MCSpecifierExpr>(E))
      return Error(Op(1).getStartLoc(), Mnemonic + " needs a symbol");
    MCRegister Rd = Mnemonic == "call" ? MIPSX::R31 : MIPSX::R0;
    emit(Out, IDLoc,
         MCInstBuilder(MIPSX::JSPCI)
             .addReg(Rd)
             .addReg(MIPSX::R0)
             .addExpr(MCSpecifierExpr::create(E, MIPSX::S_CALL, Ctx)));
    return false;
  }

  if (Mnemonic == "lsl" || Mnemonic == "lsr") {
    // lsl rs,rd,#k = sh r0,rs,rd,#(32-k); lsr rs,rd,#k = sh rs,r0,rd,#k
    Handled = true;
    if (Operands.size() != 5 || !IsReg(1) || !IsReg(2) || !IsTok(3, "#") ||
        !IsImm(4))
      return Error(IDLoc, "expected '" + Mnemonic + " rs,rd,#amount'");
    int64_t K;
    bool Left = Mnemonic == "lsl";
    if (!Const(4, K) || (Left ? (K < 0 || K > 31) : (K < 1 || K > 32)))
      return Error(Op(4).getStartLoc(),
                   Left ? "shift amount must be in the range [0, 31]"
                        : "shift amount must be in the range [1, 32]");
    MCRegister Rs = Op(1).getReg(), Rd = Op(2).getReg();
    MCInstBuilder B(MIPSX::SH);
    B.addReg(Rd);
    if (Left)
      B.addReg(MIPSX::R0).addReg(Rs).addImm(32 - K);
    else
      B.addReg(Rs).addReg(MIPSX::R0).addImm(K);
    emit(Out, IDLoc, B);
    return false;
  }

  return false;
}

bool MIPSXAsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                             OperandVector &Operands,
                                             MCStreamer &Out,
                                             uint64_t &ErrorInfo,
                                             bool MatchingInlineAsm) {
  bool Handled = false;
  StringRef Mnemonic = static_cast<MIPSXOperand &>(*Operands[0]).getToken();
  if (expandPseudo(Mnemonic, Operands, IDLoc, Out, Handled))
    return true;
  if (Handled)
    return false;

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
    return Error(ErrorLoc, "immediate must be in the range [-65536, 65535], "
                           "a symbol, or %hi, %lo, %gprel or %w");
  case Match_InvalidMemOff17:
    return Error(ErrorLoc, "offset must be in the range [-65536, 65535], "
                           "a symbol, or %hi, %lo or %gprel");
  case Match_InvalidJImm17:
    return Error(ErrorLoc, "immediate must be in the range [-65536, 65535], "
                           "a symbol, or %call or %w");
  case Match_InvalidBrTarget:
    return Error(ErrorLoc, "branch target must be a label or a displacement "
                           "in the range [-32768, 32767]");
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
