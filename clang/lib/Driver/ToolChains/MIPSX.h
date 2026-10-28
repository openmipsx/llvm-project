//===--- MIPSX.h - MIPS-X ToolChain Implementations -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_MIPSX_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_MIPSX_H

#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace tools {
namespace mipsx {

void getMIPSXTargetFeatures(const Driver &D, const llvm::opt::ArgList &Args,
                            std::vector<llvm::StringRef> &Features);

class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("mipsx::Linker", "ld.lld", TC) {}
  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // end namespace mipsx
} // end namespace tools

namespace toolchains {

/// Bare-metal toolchain for MIPS-X (the ES3890). Programs link with ld.lld
/// against the MIPS-X SDK's run-time, found in the sysroot:
///
///   <sysroot>/include          C library headers (none yet)
///   <sysroot>/lib/crt0.o       start-up code
///   <sysroot>/lib/sim.ld       linker script for the simulator (default)
///   <sysroot>/lib/device.ld    linker script for the device
///   <sysroot>/lib/libsim.a     _exit for the simulator
///   <sysroot>/lib/libdevice.a  _exit for the device
///   <sysroot>/lib/libbuiltins.a  multiply, divide and shift helpers
///   <sysroot>/lib/libc.a       C library, linked if present
///
/// The sysroot is --sysroot, else <bin>/../lib/clang-runtimes/mipsx.
/// -mboard=sim|device picks the linker script and _exit library.
class LLVM_LIBRARY_VISIBILITY MIPSXToolChain : public ToolChain {
public:
  MIPSXToolChain(const Driver &D, const llvm::Triple &Triple,
                 const llvm::opt::ArgList &Args);

  std::string computeSysRoot() const override;

  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  void
  addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                        llvm::opt::ArgStringList &CC1Args, BoundArch BA,
                        Action::OffloadKind DeviceOffloadKind) const override;

  bool isPICDefault() const override { return false; }
  bool isPIEDefault(const llvm::opt::ArgList &Args) const override {
    return false;
  }
  bool isPICDefaultForced() const override { return false; }
  bool SupportsProfiling() const override { return false; }
  bool HasNativeLLVMSupport() const override { return true; }
  const char *getDefaultLinker() const override { return "ld.lld"; }
  UnwindTableLevel
  getDefaultUnwindTableLevel(const llvm::opt::ArgList &Args) const override {
    return UnwindTableLevel::None;
  }

  /// The board selected by -mboard (sim or device).
  StringRef getBoard(const llvm::opt::ArgList &Args) const;

protected:
  Tool *buildLinker() const override;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_MIPSX_H
