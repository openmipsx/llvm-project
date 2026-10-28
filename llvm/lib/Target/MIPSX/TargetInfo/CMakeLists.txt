add_llvm_component_library(LLVMMIPSXInfo
  MIPSXTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  MIPSX
  )
