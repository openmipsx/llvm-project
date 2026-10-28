add_llvm_component_library(LLVMMIPSXAsmParser
  MIPSXAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  MIPSXDesc
  MIPSXInfo
  Support

  ADD_TO_COMPONENT
  MIPSX
  )
