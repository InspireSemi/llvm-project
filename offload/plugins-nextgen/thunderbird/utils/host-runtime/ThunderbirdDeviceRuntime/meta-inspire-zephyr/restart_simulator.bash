#!/usr/bin/env bash


# WARNING: This script will drop you into a QEMU shell.
# You will need to manually kill the QEMU process when you're done.
# Find the process ID and kill it directly like this:
# pkill -f -u $(whoami) qemu

CURRENT_DIR=$(pwd)
echo "Current working directory: $CURRENT_DIR"


pkill -f -u $(whoami) qemu
pkill -f -u $(whoami) ivshmem

# check if we are in an apptainer container, if not quit with an error telling the user to activate the environment of choice
# and if they need to they can comment out the check here.
if [ -z "$APPTAINER_ENVIRONMENT" ]; then
  echo "Error: Please activate your Apptainer environment. Comment out this check if you are not using Apptainer."
  exit 1
fi

#cd to riscv-inspire but first check if that path exists
RISCV_INSPIRE_PATH="/mnt/localstore/zephyr/riscv-inspire"
if [ ! -d "$RISCV_INSPIRE_PATH" ]; then
	echo "Error: Directory $RISCV_INSPIRE_PATH does not exist. Alter this code with your path"
	exit 1
fi
cd "$RISCV_INSPIRE_PATH"

# build based on current code edits.
# Check if REBUILD was passed as an argument
if [[ " $* " =~ " REBUILD " ]]; then
	echo "REBUILD flag detected - proceeding with build"
	# Perform the build
	kas build meta-inspire-zephyr/scripts/kas/thunderbird-thunderbolt-zephyr-shell-loader.yml:meta-inspire-zephyr/scripts/kas/qemuriscv64-machine.yml -- --postread scripts/bitbake/ivshared.conf
else
	echo "No REBUILD flag detected - skipping build step"
fi


# run in background but make sure it started successfully
build/tmp_zephyr/work/x86_64-linux/qemu-system-native/10.0.2/build/contrib/ivshmem-server/ivshmem-server -F -m /dev/shm -M ivshmem -v &
IVSHMEM_PID=$!
sleep 1
if ! kill -0 $IVSHMEM_PID 2>/dev/null; then
	echo "Error: ivshmem-server failed to start"
	exit 1
fi
echo "ivshmem-server started successfully (PID: $IVSHMEM_PID)"

# drop into another shell to execute the next commands
kas shell -E meta-inspire-zephyr/scripts/kas/thunderbird-thunderbolt-zephyr-shell-loader.yml:meta-inspire-zephyr/scripts/kas/qemuriscv64-machine.yml -c "cd .. && runqemu qemuriscv64 nographic slirp ramfs build/tmp_zephyr/deploy/images/qemuriscv64/zephyr-shell-loader-image-qemuriscv64.qemuboot.conf"

pkill -f -u $(whoami) qemu
pkill -f -u $(whoami) ivshmem
