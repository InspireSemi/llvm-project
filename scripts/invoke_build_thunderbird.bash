#!/bin/bash
#
# Example invocation of build_thunderbird.sh
# 
# For Thunderbird (RISC-V Linux target), you MUST specify a Linux sysroot
# that contains glibc/musl, pthread headers, etc.
#
# Example sysroot locations:
#   - Yocto (Linux): build/tmp/work/riscv64-poky-linux/<image>/recipe-sysroot
#   - Buildroot: output/staging
#   - SDK: $(riscv64-unknown-linux-gnu-gcc -print-sysroot)

# Use Yocto SDK sysroot (built with bitbake meta-toolchain or similar)
SYSROOT="/mnt/localstore/oecore-x86_64/sysroots/riscv64-inspire-linux/"

# Offload platform path for API integration
OFFLOAD_PLATFORM="/mnt/localstore/offload/offload-platform"

# Validate it's actually a Linux sysroot
if [[ ! -f "$SYSROOT/usr/include/pthread.h" ]]; then
  echo "ERROR: $SYSROOT doesn't have pthread.h - not a Linux sysroot!"
  echo "Thunderbird requires glibc/musl with pthread support."
  exit 1
fi

echo "Using Linux sysroot: $SYSROOT"
echo "Verifying: $(file "$SYSROOT/usr/include/pthread.h" 2>/dev/null || echo 'found')"

./scripts/build_thunderbird.sh \
  --prefix /mnt/localstore/offload/llvm-project/myinstall \
  --src ./ \
  --jobs 24 \
  --sysroot "$SYSROOT" \
  --offload-platform-path "$OFFLOAD_PLATFORM" \
  --skip-host
