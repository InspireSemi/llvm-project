#!/bin/bash

LLVM_INSTALL_DIR=/mnt/localstore/tcl_demo/llvm-project/myinstall

#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${LLVM_INSTALL_DIR}/lib

files=(
  "more_than_three_fails.c"
  "array_basic.c"
  "array_basic_parallel.c"
)

for file in "${files[@]}"; do
  ${LLVM_INSTALL_DIR}/bin/clang \
    -L${LLVM_INSTALL_DIR}/lib \
    -I${LLVM_INSTALL_DIR}/include \
    -fPIC \
    -fopenmp \
    --offload-arch=thunderbird \
    "$file" \
    -o "${file%.c}.out"
done

