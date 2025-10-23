#!/bin/bash

LLVM_INSTALL_DIR=/mnt/localstore/tcl_demo/llvm-project/myinstall
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${LLVM_INSTALL_DIR}/lib

# Check if we are in the correct directory
if [[ ! "$PWD" == *"llvm-project/scripts"* ]]; then
  echo "Error: Must be run from llvm-project/scripts directory"
  exit 1
fi

# Check if llvm-project is in LD_LIBRARY_PATH
if [[ "$LD_LIBRARY_PATH" != *"llvm-project"* ]]; then
  echo "Error: llvm-project not found in LD_LIBRARY_PATH, this error might be a mistake if you installed outside of the source tree."
  echo "Otherwise, please set LD_LIBRARY_PATH to include <right-place>/llvm-project/lib"
  exit 1
fi

cd tests_staging

files=( *.c )

for file in "${files[@]}"; do
  echo "Compiling $file"
  ${LLVM_INSTALL_DIR}/bin/clang \
    -L${LLVM_INSTALL_DIR}/lib \
    -I${LLVM_INSTALL_DIR}/include \
    -fPIC \
    -fopenmp \
    --offload-arch=thunderbird \
    "$file" \
    -o "${file%.c}.out"
done

cd ..
