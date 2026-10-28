add_llvm_component_library(LLVMMIPSXDesc
  MIPSXAsmBackend.cpp
  MIPSXELFObjectWriter.cpp
  MIPSXInstPrinter.cpp
  MIPSXMCAsmInfo.cpp
  MIPSXMCCodeEmitter.cpp
  MIPSXMCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  MIPSXInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  MIPSX
  )
