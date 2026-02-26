#!/bin/bash
# shellcheck enable=all
# shellcheck disable=2250,2312
#
# DEPRECATED: This script is no longer used by the Yocto build.
# The three-phase build is now driven directly by Yocto recipes in meta-inspire:
#   Phase 1 (LLVM/Clang/LLD): llvm-thunderbird_git.bb
#                              (inherit cmake cmake-native, BBCLASSEXTEND=native nativesdk)
#   Phase 2 (libomp):          nativesdk-libomp-thunderbird_git.bb
#   Phase 3 (libomptarget/DeviceRTL): nativesdk-llvm-thunderbird_git.bb
# This script is retained for reference and manual/standalone builds outside Yocto.
#

set -euo pipefail
IFS=$'\n\t'

usage() {
  cat <<EOF
Usage: $0 --prefix <install-prefix> [options]

Options:
  --src <llvm-project>  Path to llvm-project (must have llvm/ openmp/ offload/)
  --jobs N              Parallel jobs (default: #cores)
  --plugin NAME         Next-gen plugin token (default: thunderbird)
  --sysroot PATH        Path to RISC-V Linux sysroot for DeviceRTL (required for thunderbird)
  --offload-platform-path PATH  Path to offload-platform repository (for API integration)
  --skip-host           Skip Phase 1 (LLVM/Clang/LLD)
  --skip-libomp         Skip Phase 2 (libomp)
  --skip-offload        Skip Phase 3 (offload)
  --clean               Clean all phase build dirs before building (forces full rebuild)
  --clean-host          Clean Phase 1 build dir only
  --clean-omp           Clean Phase 2 build dir only
  --clean-offload       Clean Phase 3 build dir only
  --lit PATH            Path to 'lit' if you want check-* targets enabled
  --cmake-arg ARG       Extra CMake arg for Phase 3 (repeatable)
  --host-cmake-arg ARG  Extra CMake arg for Phase 1 / host build (repeatable)
  -h|--help             This help
EOF
}

# ---- parse args
SRC_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)
PREFIX=""
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu || echo 8)"
PLUGINS="thunderbird"
DEVICE_SYSROOT=""
OFFLOAD_PLATFORM_PATH=""
SKIP_HOST=0
SKIP_LIBOMP=0
SKIP_OFFLOAD=0
CLEAN_HOST=0
CLEAN_OMP=0
CLEAN_OFFLOAD=0
LIT_PATH=""
EXTRA_CMAKE=()
EXTRA_HOST_CMAKE=()

while [[ $# -gt 0 ]]; do
  case "$1" in
  --src)
    SRC_ROOT="$2"
    shift 2
    ;;
  --prefix)
    PREFIX="$2"
    shift 2
    ;;
  --jobs)
    JOBS="$2"
    shift 2
    ;;
  --plugin)
    PLUGINS="$2"
    shift 2
    ;;
  --sysroot)
    DEVICE_SYSROOT="$2"
    shift 2
    ;;
  --offload-platform-path)
    OFFLOAD_PLATFORM_PATH="$2"
    shift 2
    ;;
  --skip-host)
    SKIP_HOST=1
    shift
    ;;
  --skip-libomp)
    SKIP_LIBOMP=1
    shift
    ;;
  --skip-offload)
    SKIP_OFFLOAD=1
    shift
    ;;
  --clean)
    CLEAN_HOST=1
    CLEAN_OMP=1
    CLEAN_OFFLOAD=1
    shift
    ;;
  --clean-host)
    CLEAN_HOST=1
    shift
    ;;
  --clean-omp)
    CLEAN_OMP=1
    shift
    ;;
  --clean-offload)
    CLEAN_OFFLOAD=1
    shift
    ;;
  --lit)
    LIT_PATH="$2"
    shift 2
    ;;
  --cmake-arg)
    EXTRA_CMAKE+=("$2")
    shift 2
    ;;
  --host-cmake-arg)
    EXTRA_HOST_CMAKE+=("$2")
    shift 2
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  *)
    echo "Unknown arg: $1" >&2
    usage >&2
    exit 1
    ;;
  esac
done

[[ -n "$SRC_ROOT" && -n "$PREFIX" ]] || {
  usage >&2
  exit 1
} >&2

# Validate sysroot for thunderbird plugin
if [[ "$PLUGINS" == *"thunderbird"* && -n "$DEVICE_SYSROOT" ]]; then
  [[ -d "$DEVICE_SYSROOT" ]] || {
    echo "ERROR: --sysroot '$DEVICE_SYSROOT' does not exist" >&2
    exit 1
  }
  # Verify it looks like a sysroot (has usr/lib or lib)
  [[ -d "$DEVICE_SYSROOT/usr/lib" || -d "$DEVICE_SYSROOT/lib" ]] || {
    echo "WARNING: --sysroot '$DEVICE_SYSROOT' doesn't look like a sysroot (missing usr/lib or lib)" >&2
  }
  echo "==> Using device sysroot: $DEVICE_SYSROOT"
elif [[ "$PLUGINS" == *"thunderbird"* ]]; then
  echo "WARNING: Building thunderbird plugin without --sysroot. DeviceRTL may fail to find pthread headers." >&2
fi

# Validate offload-platform path if provided
if [[ -n "$OFFLOAD_PLATFORM_PATH" ]]; then
  [[ -d "$OFFLOAD_PLATFORM_PATH" ]] || {
    echo "ERROR: --offload-platform-path '$OFFLOAD_PLATFORM_PATH' does not exist" >&2
    exit 1
  }
  [[ -d "$OFFLOAD_PLATFORM_PATH/simplified-api/include" ]] || {
    echo "ERROR: --offload-platform-path '$OFFLOAD_PLATFORM_PATH' missing simplified-api/include" >&2
    exit 1
  }
  echo "==> Using offload-platform from: $OFFLOAD_PLATFORM_PATH"
  # Check if libtbird_host.so exists
  if [[ -f "$OFFLOAD_PLATFORM_PATH/simplified-api/host/libtbird_host.so" ]]; then
    echo "    Found libtbird_host.so"
  else
    echo "    WARNING: libtbird_host.so not found - you may need to build it first"
    echo "             cd $OFFLOAD_PLATFORM_PATH/simplified-api/host && make"
  fi
fi

need() { command -v "$1" >/dev/null || {
  echo "Missing '$1'" >&2
  exit 1
}; }
need cmake
need ninja

# ---- validate tree
[[ -d "$SRC_ROOT/llvm" ]] || {
  echo "Missing $SRC_ROOT/llvm" >&2
  exit 1
}
[[ -d "$SRC_ROOT/openmp" ]] || {
  echo "Missing $SRC_ROOT/openmp" >&2
  exit 1
}
[[ -d "$SRC_ROOT/offload" ]] || {
  echo "Missing $SRC_ROOT/offload" >&2
  exit 1
}

# ---- build dirs
BUILD_ROOT="$SRC_ROOT/build-thunderbird"
PHASE1_BUILD="$BUILD_ROOT/host"
PHASE2_BUILD="$BUILD_ROOT/libomp"
PHASE3_BUILD="$BUILD_ROOT/offload"
mkdir -p "$BUILD_ROOT"

# ============================================================
# Phase 1: host toolchain (LLVM + Clang + LLD)
# ============================================================
if [[ "$SKIP_HOST" -eq 0 ]]; then
  echo "==> Phase 1: LLVM/Clang/LLD -> $PREFIX"
  if [[ "$CLEAN_HOST" -eq 1 ]]; then
    echo "  (--clean-host: removing $PHASE1_BUILD)"
    rm -rf "$PHASE1_BUILD"
  fi
  declare -a CMAKE_PHASE1_ARGS=(
    -S "$SRC_ROOT/llvm" -B "$PHASE1_BUILD" -G Ninja
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DLLVM_ENABLE_ASSERTIONS=ON
    -DLLVM_TARGETS_TO_BUILD="Native;X86;RISCV"
    -DLLVM_ENABLE_PROJECTS="clang;lld"
    -DCMAKE_INSTALL_PREFIX="$PREFIX"
    -DLLVM_PARALLEL_LINK_JOBS=4
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    "${EXTRA_HOST_CMAKE[@]}"

  )
  cmake "${CMAKE_PHASE1_ARGS[@]}"
  ninja -C "$PHASE1_BUILD" -j"$JOBS" install
else
  echo "==> Phase 1: skipped"
fi

# ============================================================
# Phase 2: host libomp (OpenMP runtime), no RPATH tweaks needed on Linux
# ============================================================
#AG addition: newly built libs need to be in LD_LIBRARY_PATH (maybe unnecessary with below fixes to use the llvm linker instead of system linker)
#export LD_LIBRARY_PATH=/mnt/raid0/ahgray/omp-review/llvm-install/lib:${LD_LIBRARY_PATH}


LIBOMP_SO="$PREFIX/lib/libomp.so"
if [[ "$(uname)" == "Darwin" ]]; then
  LIBOMP_SO="$PREFIX/lib/libomp.dylib"
fi

if [[ "$SKIP_LIBOMP" -eq 0 ]]; then
  echo "==> Phase 2: libomp -> $PREFIX"
  if [[ "$CLEAN_OMP" -eq 1 ]]; then
    echo "  (--clean-omp: removing $PHASE2_BUILD)"
    rm -rf "$PHASE2_BUILD"
  fi
  # Invalidate cmake cache if the compiler was updated since last configure.
  # CMake caches only the compiler path, not a hash — it won't detect in-place updates.
  if [[ -f "$PHASE2_BUILD/CMakeCache.txt" && \
        "$PREFIX/bin/clang" -nt "$PHASE2_BUILD/CMakeCache.txt" ]]; then
    echo "  (clang updated since last configure — invalidating Phase 2 cmake cache)"
    rm -f "$PHASE2_BUILD/CMakeCache.txt"
    rm -rf "$PHASE2_BUILD/CMakeFiles/3."*
  fi
  mkdir -p "$PHASE2_BUILD"
  cmake -S "$SRC_ROOT/openmp" -B "$PHASE2_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLIBOMP_OMPT_SUPPORT=ON \
    -DCMAKE_C_COMPILER="$PREFIX/bin/clang" \
    -DCMAKE_CXX_COMPILER="$PREFIX/bin/clang++" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld -L$PREFIX/lib" \
    -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld -L$PREFIX/lib" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  ninja -C "$PHASE2_BUILD" -j"$JOBS" install
else
  echo "==> Phase 2: skipped"
fi

[[ -f "$LIBOMP_SO" ]] || {
  echo "FATAL: libomp not found at $LIBOMP_SO" >&2
  exit 1
}

# ============================================================
# Phase 3: stand-alone offload + next-gen plugin(s)
# ============================================================
if [[ "$SKIP_OFFLOAD" -eq 0 ]]; then
  echo "==> Phase 3: offload (+ $PLUGINS) -> $PREFIX"
  if [[ "$CLEAN_OFFLOAD" -eq 1 ]]; then
    echo "  (--clean-offload: removing $PHASE3_BUILD)"
    rm -rf "$PHASE3_BUILD"
  fi
  # Invalidate cmake cache if the compiler was updated since last configure.
  if [[ -f "$PHASE3_BUILD/CMakeCache.txt" && \
        "$PREFIX/bin/clang" -nt "$PHASE3_BUILD/CMakeCache.txt" ]]; then
    echo "  (clang updated since last configure — invalidating Phase 3 cmake cache)"
    rm -f "$PHASE3_BUILD/CMakeCache.txt"
    rm -rf "$PHASE3_BUILD/CMakeFiles/3."*
  fi
  mkdir -p "$PHASE3_BUILD"

  CMAKE_ARGS=(
    -G Ninja "$SRC_ROOT/offload"
    -DOPENMP_STANDALONE_BUILD=ON
    -DLIBOMPTARGET_PLUGINS_TO_BUILD="$PLUGINS"
    -DLIBOMPTARGET_FORCE_"$(echo "$PLUGINS" | tr '[:lower:]' '[:upper:]')"_TESTS=ON
    -DLIBOMPTARGET_BUILD_DEVICERTL_BCLIB=ON
    -DLLVM_ENABLE_ASSERTIONS=ON
    -DLLVM_DIR="$PREFIX/lib/cmake/llvm"
    -DOPENMP_LLVM_TOOLS_DIR="$PREFIX/bin"
    -DCMAKE_PREFIX_PATH="$PREFIX"
    -DCMAKE_C_COMPILER="$PREFIX/bin/clang"
    -DCMAKE_CXX_COMPILER="$PREFIX/bin/clang++"
    -DCMAKE_ASM_COMPILER="$PREFIX/bin/clang"
    -DCMAKE_INSTALL_PREFIX="$PREFIX"
    -DLIBOMP_STANDALONE="$LIBOMP_SO"
    -DCMAKE_ASM_COMPILER="$PREFIX/bin/clang"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_BUILD_TYPE="Debug"
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld -L$PREFIX/lib"
    -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld -L$PREFIX/lib"
  )

  # Pass sysroot to DeviceRTL compilation if specified
  if [[ -n "$DEVICE_SYSROOT" ]]; then
    CMAKE_ARGS+=(
      -DLIBOMPTARGET_DEVICE_SYSROOT="$DEVICE_SYSROOT"
    )
  fi

  # Pass offload-platform path if specified
  if [[ -n "$OFFLOAD_PLATFORM_PATH" ]]; then
    CMAKE_ARGS+=(
      -DOFFLOAD_PLATFORM_PATH="$OFFLOAD_PLATFORM_PATH"
    )
  fi

  # Optional tests
  if [[ -n "$LIT_PATH" ]]; then
    CMAKE_ARGS+=(-DOPENMP_LLVM_LIT_EXECUTABLE="$LIT_PATH")
  fi

  # Extra args
  CMAKE_ARGS+=("${EXTRA_CMAKE[@]}")

  cmake -S "$SRC_ROOT/offload" -B "$PHASE3_BUILD" "${CMAKE_ARGS[@]}"

  # libomp.so is an order-only dependency for the helper tools (llvm-offload-device-info,
  # llvm-omp-kernel-replay) in Phase 3's build.ninja — Ninja only checks that it exists,
  # not that it is fresh, for those targets. libomptarget.so.21.0git itself uses a normal
  # dependency and Ninja does detect libomp.so changes for it. The cmake reconfigure also
  # picks up libomp.so changes and regenerates build.ninja, triggering a full rebuild.
  # This explicit clean is belt-and-suspenders: ensures relink even if cmake detection
  # is somehow bypassed (e.g. if cmake is skipped in future refactors).
  LIBOMPTARGET_INSTALLED="$PREFIX/lib/libomptarget.so.21.0git"
  if [[ -f "$LIBOMPTARGET_INSTALLED" && -f "$LIBOMP_SO" ]] && \
     [[ "$LIBOMP_SO" -nt "$LIBOMPTARGET_INSTALLED" ]]; then
    echo "  (libomp.so updated — cleaning Phase 3 to force relink)"
    ninja -C "$PHASE3_BUILD" clean
  fi

  # Build the shared runtime + helper tools
  ninja -C "$PHASE3_BUILD" -j"$JOBS" omptarget llvm-offload-device-info llvm-omp-kernel-replay
  ninja -C "$PHASE3_BUILD" install

  # Show helper paths
  DEVINFO_BIN="$(find "$PHASE3_BUILD" -type f -name llvm-offload-device-info -perm -111 | head -n1 || true)"
  KREPLAY_BIN="$(find "$PHASE3_BUILD" -type f -name llvm-omp-kernel-replay -perm -111 | head -n1 || true)"

  echo
  echo "==> Helpers:"
  echo "  device-info : ${DEVINFO_BIN:-<not found>}"
  echo "  kernel-replay: ${KREPLAY_BIN:-<not found>}"
else
  echo "==> Phase 3: skipped"
fi

echo
echo "==> DONE"
echo "Prefix: $PREFIX"
if [[ -n "$DEVICE_SYSROOT" ]]; then
  echo "Device sysroot: $DEVICE_SYSROOT"
fi
echo "Sanity:"
echo "  export LIBOMPTARGET_DEBUG=1 LIBOMPTARGET_NEXTGEN_PLUGINS=1"
echo "  $(find "$PHASE3_BUILD" -type f -name llvm-offload-device-info -perm -111 | head -n1 || echo "<tool in $PHASE3_BUILD/tools/deviceinfo/>") --format=json"
echo
if [[ -n "$DEVICE_SYSROOT" ]]; then
  echo "To compile user code with Thunderbird offload:"
  echo "  export PATH=$PREFIX/bin:\$PATH"
  echo "  export LD_LIBRARY_PATH=$PREFIX/lib:\$LD_LIBRARY_PATH"
  echo "  clang -fopenmp --offload-arch=thunderbird --sysroot='$DEVICE_SYSROOT' your_code.c"
  echo
  echo "The --sysroot is REQUIRED for device code compilation (pthread.h, etc.)"
else
  echo "WARNING: No sysroot specified. User compilations will need:"
  echo "  clang -fopenmp --offload-arch=thunderbird --sysroot=/path/to/riscv64-linux-sysroot your_code.c"
fi
