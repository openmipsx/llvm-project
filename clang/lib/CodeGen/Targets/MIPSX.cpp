//===- MIPSX.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// MIPS-X ABI Implementation (MIPS-X SDK docs/abi.md section 7)
//
// Every argument becomes a sequence of 32-bit words, which the backend
// (MIPSXCallingConv.td) puts in r4-r11 and then on the stack; arguments that
// match "..." always go on the stack. So Clang only has to say how a type
// becomes words:
//
// - Integers up to 32 bits: one word, extended by the caller (and by the
//   callee for return values).
// - long long, double: i64 / double, which the type legaliser splits into
//   two words, most significant first.
// - Aggregates (and complex numbers and _BitInt wider than 64 bits) up to 16
//   bytes: [N x i32] (i32 if N = 1), loaded word by word from memory, so a
//   final partial word is left-justified. Larger ones: a pointer to a copy
//   made by the caller (not byval).
// - Return values: aggregates up to 8 bytes in r2/r3 like arguments; larger
//   ones through a hidden pointer in r4 (sret).
//===----------------------------------------------------------------------===//

namespace {
class MIPSXABIInfo : public DefaultABIInfo {
  static constexpr unsigned MaxArgBytes = 16;
  static constexpr unsigned MaxRetBytes = 8;

  bool isAggregateLike(QualType Ty) const;
  ABIArgInfo coerceToWords(QualType Ty) const;

public:
  MIPSXABIInfo(CodeGen::CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  ABIArgInfo classifyReturnType(QualType RetTy) const;
  ABIArgInfo classifyArgumentType(QualType Ty) const;

  void computeInfo(CGFunctionInfo &FI) const override;
  RValue EmitVAArg(CodeGenFunction &CGF, Address VAListAddr, QualType Ty,
                   AggValueSlot Slot) const override;
};
} // end anonymous namespace

bool MIPSXABIInfo::isAggregateLike(QualType Ty) const {
  if (isAggregateTypeForABI(Ty))
    return true;
  if (const auto *EIT = Ty->getAs<BitIntType>())
    return EIT->getNumBits() > 64;
  return false;
}

// Pass Ty as the words it occupies in memory.
ABIArgInfo MIPSXABIInfo::coerceToWords(QualType Ty) const {
  uint64_t Words = llvm::alignTo(getContext().getTypeSize(Ty), 32) / 32;
  llvm::Type *Int32 = llvm::Type::getInt32Ty(getVMContext());
  if (Words == 1)
    return ABIArgInfo::getDirect(Int32);
  return ABIArgInfo::getDirect(llvm::ArrayType::get(Int32, Words));
}

ABIArgInfo MIPSXABIInfo::classifyReturnType(QualType RetTy) const {
  if (RetTy->isVoidType())
    return ABIArgInfo::getIgnore();

  if (isAggregateLike(RetTy)) {
    uint64_t Size = getContext().getTypeSize(RetTy);
    if (Size == 0)
      return ABIArgInfo::getIgnore();
    if (Size <= MaxRetBytes * 8)
      return coerceToWords(RetTy);
    return getNaturalAlignIndirect(RetTy, getDataLayout().getAllocaAddrSpace(),
                                   /*ByVal=*/false);
  }

  if (const auto *ED = RetTy->getAsEnumDecl())
    RetTy = ED->getIntegerType();

  if (isPromotableIntegerTypeForABI(RetTy))
    return ABIArgInfo::getExtend(RetTy);
  return ABIArgInfo::getDirect();
}

ABIArgInfo MIPSXABIInfo::classifyArgumentType(QualType Ty) const {
  Ty = useFirstFieldIfTransparentUnion(Ty);

  // A C++ class that cannot be copied in registers is passed by reference.
  if (CGCXXABI::RecordArgABI RAA = getRecordArgABI(Ty, getCXXABI()))
    return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                   /*ByVal=*/RAA == CGCXXABI::RAA_DirectInMemory);

  if (isAggregateLike(Ty)) {
    uint64_t Size = getContext().getTypeSize(Ty);
    if (Size == 0)
      return ABIArgInfo::getIgnore();
    if (Size <= MaxArgBytes * 8)
      return coerceToWords(Ty);
    // The caller's copy is 8-byte aligned.
    CharUnits Align = std::max(getContext().getTypeAlignInChars(Ty),
                               CharUnits::fromQuantity(8));
    return ABIArgInfo::getIndirect(Align, getDataLayout().getAllocaAddrSpace(),
                                   /*ByVal=*/false);
  }

  if (const auto *ED = Ty->getAsEnumDecl())
    Ty = ED->getIntegerType();

  if (isPromotableIntegerTypeForABI(Ty))
    return ABIArgInfo::getExtend(Ty);
  return ABIArgInfo::getDirect();
}

void MIPSXABIInfo::computeInfo(CGFunctionInfo &FI) const {
  if (!getCXXABI().classifyReturnType(FI))
    FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
  for (auto &Arg : FI.arguments())
    Arg.info = classifyArgumentType(Arg.type);
}

// va_list is a char * to the next variadic word (abi.md section 7.2). Each
// argument takes ceil(size / 4) words with no extra alignment; aggregates
// larger than 16 bytes are a pointer.
RValue MIPSXABIInfo::EmitVAArg(CodeGenFunction &CGF, Address VAListAddr,
                               QualType Ty, AggValueSlot Slot) const {
  ABIArgInfo AI = classifyArgumentType(Ty);
  if (AI.isIgnore())
    return Slot.asRValue();

  TypeInfoChars TInfo = getContext().getTypeInfoInChars(Ty);
  return emitVoidPtrVAArg(CGF, VAListAddr, Ty, /*IsIndirect=*/AI.isIndirect(),
                          TInfo, CharUnits::fromQuantity(4),
                          /*AllowHigherAlign=*/false, Slot);
}

namespace {
class MIPSXTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  MIPSXTargetCodeGenInfo(CodeGen::CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<MIPSXABIInfo>(CGT)) {}
};
} // end anonymous namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createMIPSXTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<MIPSXTargetCodeGenInfo>(CGM.getTypes());
}
