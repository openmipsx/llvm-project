add_llvm_component_group(MIPSX)

set(LLVM_TARGET_DEFINITIONS MIPSX.td)

tablegen(LLVM MIPSXGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM MIPSXGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM MIPSXGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM MIPSXGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM MIPSXGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM MIPSXGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(MIPSXCommonTableGen)

add_llvm_target(MIPSXCodeGen
  MIPSXTargetMachine.cpp

  LINK_COMPONENTS
  MC
  MIPSXDesc
  MIPSXInfo
  Support
  Target

  ADD_TO_COMPONENT
  MIPSX
  )

add_subdirectory(AsmParser)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
