# Thunderbird OpenMP Offloading Tutorial

This tutorial demonstrates how to build and run an OpenMP offloading demo targeting the Thunderbird architecture with Zephyr 

## Overview

This demo showcases OpenMP target offloading capabilities using:
- LLVM/Clang with OpenMP support
- Zephyr RTOS with custom UART driver patches
- Thunderbird architecture target

## Prerequisites

- Git
- Build tools (clang++, make, etc.)
- Access to the InspireSemi LLVM project repository
- Zephyr RTOS build environment

## Part 1: Build LLVM with OpenMP Support

### 1. Clone the LLVM Project

```bash
git clone git@github.com:InspireSemi/llvm-project.git
cd llvm-project
```

### 2. Switch to the Appropriate Branch

```bash
git switch thunderbird/devel
```

### 3. Build LLVM

Run the build script with the installation prefix:

```bash
./scripts/build_thunderbird.sh --prefix path/to/llvm-install --src ./ --jobs 12
```

**Parameters:**
- `--prefix`: Installation directory for LLVM
- `--src`: Source directory (current directory)
- `--jobs`: Number of parallel build jobs

## Part 2: Set Up Zephyr RTOS

### Apply the Inspire Zephyr Patch

**Patch location:** 
```
meta-inspire-zephyr/recipes-kernel/zephyr-kernel/files/inspire_zephyr.patch
```

### Zephyr Setup Steps

1. Follow Michael White's workflow for setting up Zephyr
2. Apply the provided patch to `meta_inspire_zephyr`
3. Use the `runtime_fixes` branch on host-runtime

## Part 3: Build the Demo Application

### 1. Navigate to Demo Directory

```bash
cd demo_dir
```

### 2. Compile the Demo

```bash
path/to/llvm-install/bin/clang++ \
  -L/path/to/llvm-install/lib \
  -I/path/to/llvm-install/include \
  -fPIC \
  -fopenmp \
  --offload-arch=thunderbird \
  offloading_success.cpp \
  -o demo.out
```

**Compiler flags explained:**
- `-L`: Library search path
- `-I`: Include search path
- `-fPIC`: Generate position-independent code
- `-fopenmp`: Enable OpenMP support
- `--offload-arch=thunderbird`: Target Thunderbird architecture for offloading

### 3. Set Library Path

```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/llvm-install/lib
```

## Part 4: Run the Demo

```bash
./demo.out
```

### Expected Output

The demo will display the values of variables after OpenMP target offloading:

```
x value is 34, y value is 39, z value is 235
```

## Demo Code Explanation

The demo code demonstrates OpenMP target mapping:

```c
#include <comp.h>
#include <stdio.h>

int main(void) {
    uint32_t x = 11;
    uint8_t y = 49;
    uint64_t z = UINT64_MAX;

    #pragma omp target map(tofrom : x, y, z)
    { 
        x = 34;
        y = 39;
        z = 235;
    }

    printf("x value is %u, y value is %d, z value is %lu\n", x, y, z);
    
    return x;
}
```

**What it does:**
- Declares three variables of different types
- Uses OpenMP `target` directive to offload computation
- Maps variables bidirectionally (`tofrom`) between host and device
- Modifies values on the target device
- Prints the updated values back on the host

## Troubleshooting

- Ensure all paths are correctly specified for your installation
- Verify the Zephyr patch has been applied successfully
- Check that the `runtime_fixes` branch is being used for host-runtime
- Confirm that the LD_LIBRARY_PATH includes the LLVM installation directory

## File Locations Summary

- **LLVM Source:** `llvm-project/`
- **Zephyr Patch:** `meta-inspire-zephyr/recipes-kernel/zephyr-kernel/files/inspire_zephyr.patch`
- **Demo Source:** `demo_dir/offloading_success.cpp`
