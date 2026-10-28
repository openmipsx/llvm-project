add_llvm_component_library(LLVMMIPSXDisassembler
  MIPSXDisassembler.cpp

  LINK_COMPONENTS
  MC
  MCDisassembler
  MIPSXDesc
  MIPSXInfo
  Support

  ADD_TO_COMPONENT
  MIPSX
  )
