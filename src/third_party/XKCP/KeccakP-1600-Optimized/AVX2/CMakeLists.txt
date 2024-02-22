# Surpress Clang "warning: argument unused during compilation".
add_library(Keccak1600F KeccakP-1600-AVX2.s)
set_target_properties(Keccak1600F PROPERTIES COMPILE_FLAGS "-Wno-unused-command-line-argument")
