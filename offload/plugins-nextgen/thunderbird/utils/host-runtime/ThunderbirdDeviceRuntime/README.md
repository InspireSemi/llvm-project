## Build Instructions

This project is designed to be built using the Yocto Project workflow specifically for the Zephyr QEMU-based RISC-V model.

### Prerequisites
- Yocto Project environment configured for Zephyr
- QEMU RISC-V target support
- Included replacement dtsi and bb files are copied 
- Included build and run script is customized for your environment

### Important Notes
- **Host Compatibility**: This project is **not** compatible with host systems for direct execution
- **Source Code Reuse**: Individual source code components may be extracted and used separately in other projects
- **Target Platform**: Exclusively targets Zephyr RTOS running on QEMU RISC-V emulation

### Build Process and Run Notes

Taking the first meta-inspire-zephyr tarball that Michael White had sent, 
identify the files that need to be overwritten with those found in the ThunderbirdDeviceRuntime/meta-inspire-zephyr directory. 
	
The file zephyr-shell-loader.bb will have paths not meaningful to your system, particularly:
ZEPHYR_SRC_DIR = "/mnt/localstore/zephyr/drta"

You need to copy the whole ThunderbirdDeviceRuntime subdirectory separately into non-networked storage compatible with a Yocto workflow and reflect that
path in ZEPHYR_SRC_DIR.

The dtsi file adds a reserved region to the device tree and should be copied over the file with the same name.

After correcting filepaths (probably just RISCV_INSPIRE_PATH) to how they are for your particular setup, use the included bash script which drives Yocto Project workflow for Zephyr RISC-V targets to build this project.

Use the REBUILD option for the script initially as the script isn't smart enough to check build status:
./restart_simulator.bash REBUILD

Running the script will invoke a whole Yocto build initially and then launch the ivshmem server as well as the QEMU simulation. Unit tests will automatically run and device runtime will accept incoming communications until qemu is forcefully stopped.

You can kill the simulation at any time by running:
pkill -f -u $(whoami) qemu
pkill -f -u $(whoami) ivshmem

If you modify the ThunderbirdDeviceRuntime code in the directory where the Yocto build process will pick it up, and you do a REBUILD, only your modified and affected code
will need to be rebuilt, sparing a decent amount of time.

Current runtime will process two command batches from the current host runtime demo and process them correctly.