#!/bin/bash

LLVM_INSTALL_DIR=/mnt/localstore/offload/llvm-project/myinstall
DEVICE_SYSROOT=/mnt/localstore/oecore-x86_64/sysroots/riscv64-inspire-linux

echo "Building OpenMP offload tests for Thunderbird"

# Check if we are in the correct directory
if [[ ! "$PWD" == *"llvm-project/scripts"* ]]; then
  echo "Error: Must be run from llvm-project/scripts directory"
  exit 1
fi

# Check if llvm-project is in LD_LIBRARY_PATH
if [[ "$LD_LIBRARY_PATH" != *"llvm-project"* ]]; then
  echo "Warning: llvm-project not found in LD_LIBRARY_PATH"
  echo "Adding ${LLVM_INSTALL_DIR}/lib to LD_LIBRARY_PATH"
  export LD_LIBRARY_PATH="${LLVM_INSTALL_DIR}/lib:${LD_LIBRARY_PATH}"
fi

cd tests_staging

files=( saxpy_parallel.c )

for file in "${files[@]}"; do
  base_name="${file%.c}"
  echo "========================================="
  echo "Processing $file"
  echo "========================================="
  
  # Stage 1: Generate LLVM IR
  echo "Stage 1: Generating LLVM IR..."
  ${LLVM_INSTALL_DIR}/bin/clang \
    -I${LLVM_INSTALL_DIR}/include \
    -fPIC \
    -fopenmp \
    --offload-arch=thunderbird \
    -S -emit-llvm \
    "$file" \
    -o "${base_name}.ll"
  
  if [ $? -eq 0 ] && [ -f "${base_name}.ll" ]; then
    echo "✓ IR generated: ${base_name}.ll"
    # Validate IR contains device triple
    if grep -q "riscv64.*inspire" "${base_name}.ll"; then
      echo "  ✓ Contains RISC-V device code"
    else
      echo "  ✗ WARNING: No RISC-V device triple found in IR"
    fi
  else
    echo "✗ FAILED to generate IR"
  fi
  
  # Stage 2: Compile to object file
  echo "Stage 2: Compiling to object file..."
  ${LLVM_INSTALL_DIR}/bin/clang \
    -I${LLVM_INSTALL_DIR}/include \
    -fPIC \
    -fopenmp \
    --offload-arch=thunderbird \
    -c "$file" \
    -o "${base_name}.o"
  
  if [ $? -eq 0 ] && [ -f "${base_name}.o" ]; then
    echo "✓ Object compiled: ${base_name}.o"
    # Validate object has offload section
    if ${LLVM_INSTALL_DIR}/bin/llvm-readelf -S "${base_name}.o" 2>/dev/null | grep -q ".llvm.offloading"; then
      echo "  ✓ Contains .llvm.offloading section"
    else
      echo "  ✗ WARNING: No .llvm.offloading section found"
    fi
  else
    echo "✗ FAILED to compile object file"
  fi
  
  # Stage 3: Link executable
  echo "Stage 3: Linking executable..."
  ${LLVM_INSTALL_DIR}/bin/clang \
    -v \
    -L${LLVM_INSTALL_DIR}/lib \
    -I${LLVM_INSTALL_DIR}/include \
    -fPIC \
    -fopenmp \
    --offload-arch=thunderbird \
    -Xoffload-linker --device-sysroot=${DEVICE_SYSROOT} \
    "$file" \
    -o "${base_name}.elf"
  
  if [ $? -eq 0 ] && [ -f "${base_name}.elf" ]; then
    echo "✓ Executable linked: ${base_name}.elf"
    # Verify host architecture
    if ${LLVM_INSTALL_DIR}/bin/llvm-readelf -h "${base_name}.elf" 2>/dev/null | grep -q "Machine:.*X86-64"; then
      echo "  ✓ Host executable is x86_64"
    else
      echo "  ⚠ WARNING: Unexpected host architecture:"
      ${LLVM_INSTALL_DIR}/bin/llvm-readelf -h "${base_name}.elf" 2>/dev/null | grep Machine
    fi
    # Validate executable has offload section
    if ${LLVM_INSTALL_DIR}/bin/llvm-readelf -S "${base_name}.elf" 2>/dev/null | grep -q ".llvm.offloading"; then
      echo "  ✓ Contains embedded device code"
    else
      echo "  ✗ WARNING: No embedded device code found"
    fi
  else
    echo "✗ FAILED to link executable (exit code: $?)"
    echo ""
    continue
  fi
  
  # Stage 4: Extract device image from linked executable (contains ET_DYN shared library)
  echo "Stage 4: Extracting device image from linked executable..."
  bundle_file="${base_name}.bundle"
  device_img="${base_name}.riscv64.img"
  
  # Extract from .elf (linked by clang-linker-wrapper with -shared flag)
  # not from .o (which contains pre-linked ET_REL code)
  ${LLVM_INSTALL_DIR}/bin/llvm-objcopy \
    --dump-section=.llvm.offloading="${bundle_file}" \
    "${base_name}.elf"
  
  if [ $? -eq 0 ] && [ -f "${bundle_file}" ]; then
    echo "✓ Bundle extracted from linked executable"
    
    # Unpack device image using clang-offload-packager
    ${LLVM_INSTALL_DIR}/bin/clang-offload-packager \
      --image=triple=riscv64-inspire-linux-gnu,arch=thunderbird,kind=openmp,file="${device_img}" \
      "${bundle_file}"
    
    if [ $? -eq 0 ] && [ -f "${device_img}" ]; then
      echo "✓ Device image extracted: ${device_img}"
      ls -lh "${device_img}"
      
      # Verify it's a valid RISC-V ELF and check if it's already ET_DYN
      if ${LLVM_INSTALL_DIR}/bin/llvm-readelf -h "${device_img}" 2>/dev/null | grep -q "RISC-V"; then
        # Check ELF type
        if ${LLVM_INSTALL_DIR}/bin/llvm-readelf -h "${device_img}" 2>/dev/null | grep -q "Type:.*DYN"; then
          echo "  ✓ Already ET_DYN shared object (clang-linker-wrapper worked!)"
          echo "  ✓ Ready for dlopen"
        else
          echo "  ⚠ ELF Type is not DYN (clang-linker-wrapper may not have been invoked)"
          ${LLVM_INSTALL_DIR}/bin/llvm-readelf -h "${device_img}" 2>/dev/null | grep Type
          echo "  → This may cause dlopen to fail with 'only ET_DYN and ET_EXEC can be loaded'"
        fi
        
        rm -f "${bundle_file}"  # Clean up intermediate bundle
      else
        echo "  ✗ WARNING: Extracted image is not a valid RISC-V ELF file"
        file "${device_img}" 2>/dev/null || od -A x -t x1z -v "${device_img}" | head -n 2
      fi
    else
      echo "✗ FAILED to extract device image with clang-offload-packager"
    fi
  else
    echo "✗ FAILED to extract offload bundle section from linked executable"
    echo "   Note: Extracting from .elf (not .o) to get clang-linker-wrapper output"
  fi
  
  # Summary
  echo ""
  echo "Summary for $file:"
  [ -f "${base_name}.ll" ] && echo "  ✓ ${base_name}.ll (LLVM IR)" || echo "  ✗ ${base_name}.ll"
  [ -f "${base_name}.o" ] && echo "  ✓ ${base_name}.o (Object)" || echo "  ✗ ${base_name}.o"
  [ -f "${base_name}.elf" ] && echo "  ✓ ${base_name}.elf (Executable)" || echo "  ✗ ${base_name}.elf"
  [ -f "${base_name}.riscv64.img" ] && echo "  ✓ ${base_name}.riscv64.img (Device image)" || echo "  ✗ ${base_name}.riscv64.img"
  echo ""
done

cd ..
