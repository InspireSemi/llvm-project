#!/bin/bash
# shellcheck enable=all
# shellcheck disable=2250,2312

set -euo pipefail
IFS=$'\n\t'

usage() {
	cat <<EOF
Usage: $0 --prefix <install-prefix> [options]

Options:
  --src <llvm-project>  Path to llvm-project (must have llvm/ openmp/ offload/)
  --jobs N              Parallel jobs (default: #cores)
  --plugin NAME         Next-gen plugin token (default: thunderbird)
  --with-host           Also build the 'host' plugin (default: off)
  --skip-host           Skip Phase 1 (LLVM/Clang/LLD)
  --skip-libomp         Skip Phase 2 (libomp)
  --skip-offload        Skip Phase 3 (offload)
  --lit PATH            Path to 'lit' if you want check-* targets enabled
  --ffi-so PATH         Override path to shared libffi.so
  --ffi-inc DIR         Override path to libffi headers
  --cmake-arg ARG       Extra CMake arg for Phase 3 (repeatable)
  -h|--help             This help
EOF
}

# ---- parse args
SRC_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)
PREFIX=""
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu || echo 8)"
PLUGIN="thunderbird"
WITH_HOST=0
SKIP_HOST=0
SKIP_LIBOMP=0
SKIP_OFFLOAD=0
LIT_PATH=""
FFI_SO=""
FFI_INC=""
EXTRA_CMAKE=()

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
		PLUGIN="$2"
		shift 2
		;;
	--with-host)
		WITH_HOST=1
		shift
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
	--lit)
		LIT_PATH="$2"
		shift 2
		;;
	--ffi-so)
		FFI_SO="$2"
		shift 2
		;;
	--ffi-inc)
		FFI_INC="$2"
		shift 2
		;;
	--cmake-arg)
		EXTRA_CMAKE+=("$2")
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
	rm -rf "$PHASE1_BUILD"
	declare -a CMAKE_PHASE1_ARGS=(
		-S "$SRC_ROOT/llvm" -B "$PHASE1_BUILD" -G Ninja
		-DCMAKE_BUILD_TYPE=RelWithDebInfo
		-DLLVM_ENABLE_ASSERTIONS=ON
		-DLLVM_TARGETS_TO_BUILD="Native;X86;RISCV"
		-DLLVM_ENABLE_PROJECTS="clang;lld"
		-DCMAKE_INSTALL_PREFIX="$PREFIX"
		-DLLVM_PARALLEL_LINK_JOBS=4
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

	)
	cmake "${CMAKE_PHASE1_ARGS[@]}"
	ninja -C "$PHASE1_BUILD" -j"$JOBS" install
else
	echo "==> Phase 1: skipped"
fi

# ============================================================
# Phase 2: host libomp (OpenMP runtime), no RPATH tweaks needed on Linux
# ============================================================
LIBOMP_SO="$PREFIX/lib/libomp.so"
if [[ "$(uname)" == "Darwin" ]]; then
	LIBOMP_SO="$PREFIX/lib/libomp.dylib"
fi

if [[ "$SKIP_LIBOMP" -eq 0 ]]; then
	echo "==> Phase 2: libomp -> $PREFIX"
	rm -rf "$PHASE2_BUILD"
	mkdir -p "$PHASE2_BUILD"
	cmake -S "$SRC_ROOT/openmp" -B "$PHASE2_BUILD" -G Ninja \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DLIBOMP_OMPT_SUPPORT=ON \
		-DCMAKE_C_COMPILER="$PREFIX/bin/clang" \
		-DCMAKE_CXX_COMPILER="$PREFIX/bin/clang++" \
		-DCMAKE_INSTALL_PREFIX="$PREFIX" \
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
	echo "==> Phase 3: offload (+ $PLUGIN${WITH_HOST:+;host}) -> $PREFIX"
	rm -rf "$PHASE3_BUILD"
	mkdir -p "$PHASE3_BUILD"
	# Find libffi library and headers
	if [[ -z "$FFI_SO" ]] && command -v pkg-config >/dev/null && pkg-config --exists libffi; then
		echo "--> Found libffi via pkg-config"
		FFI_INC="$(pkg-config --cflags-only-I libffi | sed -e 's/^-I//' -e 's/ .*//')"
		# Use CMAKE_FIND_LIBRARY_SUFFIXES to let CMake find the right lib extension (.so, .dylib)
		FFI_LIB_DIR="$(pkg-config --libs-only-L libffi | sed 's/^-L//')"
		CMAKE_ARGS+=(-DFFI_LIBRARY_DIR="$FFI_LIB_DIR")
	fi

	# If user provided headers, use them. Otherwise, use what pkg-config found.
	if [[ -n "$FFI_INC" ]]; then
		CMAKE_ARGS+=(-DFFI_INCLUDE_DIR="$FFI_INC")
	fi
	# If user provided the full shared lib path, use it.
	if [[ -n "$FFI_SO" ]]; then
		CMAKE_ARGS+=(-DFFI_LIBRARY="$FFI_SO")
	fi

	# Some branches don’t propagate -lffi from libomptarget to tools; add -lffi to link flags.
	CMAKE_LINKER_FLAGS="-L$PREFIX/lib -lffi"

	PLUGINS="$PLUGIN"
	[[ $WITH_HOST -eq 1 ]] && PLUGINS="$PLUGINS;host"
	CMAKE_ARGS=(
		-G Ninja "$SRC_ROOT/offload"
		-DOPENMP_STANDALONE_BUILD=ON
		-DLIBOMPTARGET_PLUGINS_TO_BUILD="$PLUGINS"
		-DLIBOMPTARGET_FORCE_"$(echo "$PLUGIN" | tr '[:lower:]' '[:upper:]')"_TESTS=ON
		# In your CMake configuration for Phase 3, add:
		-DLIBOMPTARGET_BUILD_DEVICERTL_BCLIB=ON
		-DLIBOMPTARGET_DEVICE_ARCHITECTURES="riscv64"
		-DLLVM_ENABLE_ASSERTIONS=ON
		-DLLVM_DIR="$PREFIX/lib/cmake/llvm"
		-DOPENMP_LLVM_TOOLS_DIR="$PREFIX/bin"
		-DCMAKE_PREFIX_PATH="$PREFIX"
		-DCMAKE_C_COMPILER="$PREFIX/bin/clang"
		-DCMAKE_CXX_COMPILER="$PREFIX/bin/clang++"
		-DCMAKE_ASM_COMPILER="$PREFIX/bin/clang"
		-DCMAKE_INSTALL_PREFIX="$PREFIX"
		-DLIBOMP_STANDALONE="$LIBOMP_SO"
		-DCMAKE_EXE_LINKER_FLAGS="$CMAKE_LINKER_FLAGS"
		-DCMAKE_SHARED_LINKER_FLAGS="$CMAKE_LINKER_FLAGS"
		-DFFI_INCLUDE_DIR="$FFI_INC"
		-DCMAKE_ASM_COMPILER="$PREFIX/bin/clang"
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	)

	# Optional tests
	if [[ -n "$LIT_PATH" ]]; then
		CMAKE_ARGS+=(-DOPENMP_LLVM_LIT_EXECUTABLE="$LIT_PATH")
	fi

	# Extra args
	CMAKE_ARGS+=("${EXTRA_CMAKE[@]}")

	cmake -S "$SRC_ROOT/offload" -B "$PHASE3_BUILD" "${CMAKE_ARGS[@]}"

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
echo "Sanity:"
echo "  export LIBOMPTARGET_DEBUG=1 LIBOMPTARGET_NEXTGEN_PLUGINS=1"
echo "  $(find "$PHASE3_BUILD" -type f -name llvm-offload-device-info -perm -111 | head -n1 || echo "<tool in $PHASE3_BUILD/tools/deviceinfo/>") --format=json"
