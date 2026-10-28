//===--- MIPSX.cpp - MIPS-X ToolChain Implementations -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MIPSX.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Options/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

void mipsx::getMIPSXTargetFeatures(const Driver &D, const ArgList &Args,
                                   std::vector<StringRef> &Features) {
  // abi.md section 5.4.
  if (Arg *A = Args.getLastArg(options::OPT_mlong_calls,
                               options::OPT_mno_long_calls))
    Features.push_back(A->getOption().matches(options::OPT_mlong_calls)
                           ? "+long-calls"
                           : "-long-calls");
}

MIPSXToolChain::MIPSXToolChain(const Driver &D, const llvm::Triple &Triple,
                               const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  getProgramPaths().push_back(D.Dir);
  SmallString<128> LibDir(computeSysRoot());
  llvm::sys::path::append(LibDir, "lib");
  getFilePaths().push_back(std::string(LibDir));
}

std::string MIPSXToolChain::computeSysRoot() const {
  const Driver &D = getDriver();
  if (!D.SysRoot.empty())
    return D.SysRoot;
  SmallString<128> Dir(D.Dir);
  llvm::sys::path::append(Dir, "..", "lib", "clang-runtimes", "mipsx");
  return std::string(Dir);
}

void MIPSXToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                               ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc))
    return;

  if (!DriverArgs.hasArg(options::OPT_nobuiltininc)) {
    SmallString<128> Dir(getDriver().ResourceDir);
    llvm::sys::path::append(Dir, "include");
    addSystemInclude(DriverArgs, CC1Args, Dir);
  }

  if (DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;

  SmallString<128> Dir(computeSysRoot());
  llvm::sys::path::append(Dir, "include");
  addSystemInclude(DriverArgs, CC1Args, Dir);
}

void MIPSXToolChain::addClangTargetOptions(
    const ArgList &DriverArgs, ArgStringList &CC1Args, BoundArch BA,
    Action::OffloadKind DeviceOffloadKind) const {
  // -mboard only matters when linking; accept it when compiling too.
  getBoard(DriverArgs);
}

StringRef MIPSXToolChain::getBoard(const ArgList &Args) const {
  const Arg *A = Args.getLastArg(options::OPT_mboard_EQ);
  if (!A)
    return "sim";
  StringRef Board = A->getValue();
  if (Board != "sim" && Board != "device") {
    getDriver().Diag(diag::err_drv_unsupported_option_argument)
        << A->getSpelling() << Board;
    return "sim";
  }
  return Board;
}

Tool *MIPSXToolChain::buildLinker() const {
  return new tools::mipsx::Linker(*this);
}

void mipsx::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                 const InputInfo &Output,
                                 const InputInfoList &Inputs,
                                 const ArgList &Args,
                                 const char *LinkingOutput) const {
  const auto &TC = static_cast<const MIPSXToolChain &>(getToolChain());
  ArgStringList CmdArgs;
  StringRef Board = TC.getBoard(Args);
  bool Relocatable = Args.hasArg(options::OPT_r);

  CmdArgs.push_back("-Bstatic");

  if (!Relocatable &&
      !Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles))
    CmdArgs.push_back(Args.MakeArgString(TC.GetFilePath("crt0.o")));

  Args.addAllArgs(CmdArgs, {options::OPT_L});
  TC.AddFilePathLibArgs(Args, CmdArgs);
  Args.addAllArgs(CmdArgs, {options::OPT_u, options::OPT_T_Group,
                            options::OPT_s, options::OPT_t, options::OPT_r});

  // The board's linker script (abi.md section 13), unless the command line
  // gives one.
  if (!Relocatable && !Args.hasArg(options::OPT_T))
    CmdArgs.push_back(
        Args.MakeArgString("-T" + TC.GetFilePath(Args.MakeArgString(Board + ".ld"))));

  AddLinkerInputs(TC, Inputs, Args, CmdArgs, JA);

  if (!Relocatable &&
      !Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    CmdArgs.push_back("--start-group");
    // There is no C library yet (plan S15); link one if the sysroot has it.
    if (!Args.hasArg(options::OPT_nolibc) &&
        llvm::sys::path::is_absolute(TC.GetFilePath("libc.a")))
      CmdArgs.push_back("-lc");
    // The multiply, divide and shift helpers the code generator calls.
    CmdArgs.push_back("-lbuiltins");
    CmdArgs.push_back(Args.MakeArgString("-l" + Board));
    CmdArgs.push_back("--end-group");
  }

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::AtFileCurCP(),
      Args.MakeArgString(TC.GetLinkerPath()), CmdArgs, Inputs, Output));
}
