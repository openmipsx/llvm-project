//===--- MIPSX.cpp - Implement MIPS-X target feature support --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements MIPS-X TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "MIPSX.h"
#include "Targets.h"
#include "clang/Basic/MacroBuilder.h"
#include "llvm/ADT/StringSwitch.h"

using namespace clang;
using namespace clang::targets;

MIPSXTargetInfo::MIPSXTargetInfo(const llvm::Triple &Triple,
                                 const TargetOptions &)
    : TargetInfo(Triple) {
  // abi.md section 3. char is signed and wchar_t is int, which are the
  // TargetInfo defaults; long long and double are 8-byte aligned.
  BigEndian = true;
  SizeType = UnsignedInt;
  PtrDiffType = SignedInt;
  IntPtrType = SignedInt;
  LongDoubleWidth = 64;
  LongDoubleAlign = 64;
  LongDoubleFormat = &llvm::APFloat::IEEEdouble();
  SuitableAlign = 64;
  // abi.md section 13.6: no atomic instructions, so every atomic operation
  // is a call into the SDK.
  MaxAtomicPromoteWidth = 64;
  MaxAtomicInlineWidth = 0;
  resetDataLayout();
}

const char *const MIPSXTargetInfo::GCCRegNames[] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",  "r8",
    "r9",  "r10", "r11", "r12", "r13", "r14", "r15", "r16", "r17",
    "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "r26",
    "r27", "r28", "r29", "r30", "r31", "psw", "md"};

ArrayRef<const char *> MIPSXTargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

// abi.md section 10.1.
const TargetInfo::GCCRegAlias MIPSXTargetInfo::GCCRegAliases[] = {
    {{"zero"}, "r0"}, {{"at"}, "r1"},  {{"cb0"}, "r23"}, {{"cb1"}, "r24"},
    {{"gp"}, "r28"},  {{"sp"}, "r29"}, {{"fp"}, "r30"},  {{"ra"}, "r31"},
};

ArrayRef<TargetInfo::GCCRegAlias> MIPSXTargetInfo::getGCCRegAliases() const {
  return llvm::ArrayRef(GCCRegAliases);
}

bool MIPSXTargetInfo::isValidCPUName(StringRef Name) const {
  return Name == "es3890" || Name == "mipsx";
}

void MIPSXTargetInfo::fillValidCPUList(
    SmallVectorImpl<StringRef> &Values) const {
  Values.emplace_back("es3890");
  Values.emplace_back("mipsx");
}

bool MIPSXTargetInfo::setCPU(StringRef Name) {
  if (!isValidCPUName(Name))
    return false;
  CPU = Name.str();
  return true;
}

bool MIPSXTargetInfo::initFeatureMap(
    llvm::StringMap<bool> &Features, DiagnosticsEngine &Diags, StringRef CPU,
    const std::vector<std::string> &FeaturesVec) const {
  // The default CPU is the ES3890 (abi.md section 10.3).
  if (CPU != "mipsx")
    Features["es3890"] = true;
  return TargetInfo::initFeatureMap(Features, Diags, CPU, FeaturesVec);
}

bool MIPSXTargetInfo::handleTargetFeatures(std::vector<std::string> &Features,
                                           DiagnosticsEngine &Diags) {
  for (const std::string &Feature : Features) {
    if (Feature == "+es3890")
      HasES3890 = true;
    else if (Feature == "-es3890")
      HasES3890 = false;
    else if (Feature == "+long-calls")
      UseLongCalls = true;
    else if (Feature == "-long-calls")
      UseLongCalls = false;
  }
  return true;
}

bool MIPSXTargetInfo::hasFeature(StringRef Feature) const {
  return llvm::StringSwitch<bool>(Feature)
      .Case("mipsx", true)
      .Case("es3890", HasES3890)
      .Case("long-calls", UseLongCalls)
      .Default(false);
}

bool MIPSXTargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  switch (*Name) {
  default:
    return false;
  case 'I': // Signed 17-bit immediate (addi, memory offsets).
    Info.setRequiresImmediate(-65536, 65535);
    return true;
  case 'J': // Zero.
    Info.setRequiresImmediate(0);
    return true;
  case 'K': // Shift amount, 0 to 31.
    Info.setRequiresImmediate(0, 31);
    return true;
  }
}

void MIPSXTargetInfo::getTargetDefines(const LangOptions &Opts,
                                       MacroBuilder &Builder) const {
  // __mipsx__, __mipsx and (in GNU modes) mipsx. Never __mips__: the
  // instruction set has nothing in common with MIPS.
  DefineStd(Builder, "mipsx", Opts);
  Builder.defineMacro("__MIPSX__");
  if (CPU == "es3890")
    Builder.defineMacro("__mipsx_es3890__");
  // The byte and halfword loads and stores of the ES3890.
  if (HasES3890)
    Builder.defineMacro("__mipsx_byte_access__");
  if (UseLongCalls)
    Builder.defineMacro("__mipsx_long_calls__");
}
