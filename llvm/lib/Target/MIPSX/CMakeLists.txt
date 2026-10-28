add_llvm_component_group(MIPSX)

set(LLVM_TARGET_DEFINITIONS MIPSX.td)

tablegen(LLVM MIPSXGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM MIPSXGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM MIPSXGenCallingConv.inc -gen-callingconv)
tablegen(LLVM MIPSXGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM MIPSXGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM MIPSXGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM MIPSXGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM MIPSXGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM MIPSXGenSDNodeInfo.inc -gen-sd-node-info)
tablegen(LLVM MIPSXGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(MIPSXCommonTableGen)

add_llvm_target(MIPSXCodeGen
  MIPSXAsmPrinter.cpp
  MIPSXDelaySlotFiller.cpp
  MIPSXFrameCFI.cpp
  MIPSXFrameLowering.cpp
  MIPSXHazardPadding.cpp
  MIPSXInstrInfo.cpp
  MIPSXISelDAGToDAG.cpp
  MIPSXISelLowering.cpp
  MIPSXRegisterInfo.cpp
  MIPSXSelectionDAGInfo.cpp
  MIPSXSubtarget.cpp
  MIPSXTargetMachine.cpp
  MIPSXTargetObjectFile.cpp

  LINK_COMPONENTS
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  MC
  MIPSXDesc
  MIPSXInfo
  SelectionDAG
  Support
  Target
  TargetParser

  ADD_TO_COMPONENT
  MIPSX
  )

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
