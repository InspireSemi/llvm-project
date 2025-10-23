# Thunderbird Host Runtime

This repository contains the host runtime component for the Thunderbird system, which communicates with device runtime instances.

## Prerequisites

Before building the host runtime, you must first build the **ThunderbirdDeviceRuntime** using the Yocto workflow. Please refer to the ThunderbirdDeviceRuntime directory's instructions for complete setup details.

## Building the Host Runtime

1. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```

2. Configure the project with CMake:
   ```bash
   cmake /path/to/source/directory
   ```

3. Build the project:
   ```bash
   make
   ```

## Running the Demo

After building, you can find the test binary at:
```
build/ThunderbirdHostRuntime/tests/test_thunderbird_runtime
```

### Testing with Device Runtime

To demonstrate the complete system:

1. Start the ThunderbirdDeviceRuntime application (refer to its documentation for startup instructions)
2. Run the host runtime test binary:
   ```bash
   ./build/ThunderbirdHostRuntime/tests/test_thunderbird_runtime
   ```

The test binary will communicate with the device runtime to demonstrate host-to-device command execution.

## Project Structure

- `ThunderbirdHostRuntime/` - Main host runtime implementation
- `DataTransferEngine/` - Data transfer components
- `ThunderbirdDeviceRuntime/` - Device runtime application that must be built in Yocto and invoked with aa provided script
- `build/` - Build output directory (created during build process)