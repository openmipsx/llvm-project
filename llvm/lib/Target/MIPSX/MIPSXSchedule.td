//===-- MIPSXSchedule.td - MIPS-X scheduling model ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// In order, one instruction per cycle, no interlocks. A loaded value can be
// used two instructions after the load (MIPS-X manual section 3.2, Table
// 3-2; MIPS-X SDK docs/abi.md section 9, P1); everything else is available
// to the next instruction through bypassing. The schedulers use the load
// latency to put independent instructions between a load and its use, so
// that MIPSXHazardPadding needs fewer nops.
//
//===----------------------------------------------------------------------===//

def MIPSXSchedModel : SchedMachineModel {
  let IssueWidth = 1;
  let MicroOpBufferSize = 0; // In order.
  let LoadLatency = 2;
  let CompleteModel = 0;
  let PostRAScheduler = 1;
}
