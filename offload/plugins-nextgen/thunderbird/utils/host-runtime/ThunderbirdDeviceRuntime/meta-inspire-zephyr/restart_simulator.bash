#!/usr/bin/env bash

# WARNING: This script will drop you into a QEMU shell.
# You will need to manually kill the QEMU process when you're done.
# Find the process ID and kill it directly like this:
# pkill -f -u $(whoami) qemu

# Configuration: Application script (built on top of the model workflow)
APPLICATION_SCRIPT="meta-inspire-zephyr/scripts/kas/zephyr-offload-device.yml"

# Configuration: Base model script (can be QEMU or other model types)
MODEL_SCRIPT="meta-inspire-zephyr/scripts/kas/qemuriscv64-a1.yml"

# Configuration: Path to riscv-inspire directory
RISCV_INSPIRE_PATH="/mnt/localstore/tcl_demo/riscv-inspire"

# Configuration: Path to ivshmem-server application
IVSHMEM_SERVER_PATH="build/tmp_zephyr/work/x86_64-linux/qemu-system-native/10.0.2/build/contrib/ivshmem-server/ivshmem-server"

# Configuration: qemuriscv64 deploy directory
QEMU_DEPLOY_DIR="build/tmp_zephyr/deploy/images/qemuriscv64"

# Configuration: ivshared config file path
IVSHARED_CONF="scripts/bitbake/ivshared.conf"

# Configuration: Shared memory device filepath
SHARED_MEMORY_PATH="/dev/shm"

# Derive the qemuboot config filename from the application script
# Extract the base name from APPLICATION_SCRIPT (e.g., "zephyr-offload-device" from "meta-inspire-zephyr/scripts/kas/zephyr-offload-device.yml")
APP_BASE_NAME=$(basename "$APPLICATION_SCRIPT" .yml)
# Construct the qemuboot config filename
QEMUBOOT_CONF="${APP_BASE_NAME}-image-qemuriscv64.qemuboot.conf"

# Cache the current user
CURRENT_USER=$(whoami)

# Build and run QEMU model commands
BUILD_QEMU_MODEL_CMD="kas build ${APPLICATION_SCRIPT}:${MODEL_SCRIPT} -- --postread ${IVSHARED_CONF}"
RUN_QEMU_MODEL_CMD="kas shell -E ${APPLICATION_SCRIPT}:${MODEL_SCRIPT} -c \"cd .. && runqemu qemuriscv64 nographic slirp ramfs ${QEMU_DEPLOY_DIR}/${QEMUBOOT_CONF}\""

CURRENT_DIR=$(pwd)
echo "Current working directory: $CURRENT_DIR"

pkill -f -u ${CURRENT_USER} qemu
pkill -f -u ${CURRENT_USER} ivshmem

# check if we are in an apptainer container, if not quit with an error telling the user to activate the environment of choice
# and if they need to they can comment out the check here.
if [ -z "$APPTAINER_ENVIRONMENT" ]; then
  echo "Error: Please activate your Apptainer environment. Comment out this check if you are not using Apptainer."
  exit 1
fi

#cd to riscv-inspire but first check if that path exists
if [ ! -d "$RISCV_INSPIRE_PATH" ]; then
	echo "Error: Directory $RISCV_INSPIRE_PATH does not exist. Alter this code with your path"
	exit 1
fi
cd "$RISCV_INSPIRE_PATH"

# build based on current code edits.
# Check if REBUILD was passed as an argument
if [[ " $* " =~ " REBUILD " ]]; then
	echo "REBUILD flag detected - proceeding with build"
	${BUILD_QEMU_MODEL_CMD}
else
	echo "No REBUILD flag detected - skipping build step"
fi

# run in background but make sure it started successfully
${IVSHMEM_SERVER_PATH} -F -m ${SHARED_MEMORY_PATH} -M ivshmem -v &
IVSHMEM_PID=$!
sleep 1
if ! kill -0 $IVSHMEM_PID 2>/dev/null; then
	echo "Error: ivshmem-server failed to start"
	exit 1
fi
echo "ivshmem-server started successfully (PID: $IVSHMEM_PID)"

eval ${RUN_QEMU_MODEL_CMD}

pkill -f -u ${CURRENT_USER} qemu
pkill -f -u ${CURRENT_USER} ivshmem
