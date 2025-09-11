# Copyright 2025 Inspire Semiconductor, Incorporated (www.inspiresmi.com)
# 
# SPDX-License-Identifier: MIT

SUMMARY = "Zephyr sample shell dynamic loader"
DESCRIPTION = "This example provides shell access to the `llext` system and \
provides the ability to manage loadable code extensions in the shell."

require zephyr-inspire.inc
require recipes-kernel/zephyr-kernel/zephyr-image.inc

inherit populate_sdk
TOOLCHAIN_TARGET_TASK:append = " zephyr-hello-world-mod-dev "

# ZEPHYR_SRC_DIR = "${S}/zephyr/samples/subsys/llext/shell_loader"
ZEPHYR_SRC_DIR = "/mnt/localstore/zephyr/drta"

FILESEXTRAPATHS:append := "${ZEPHYR_SRC_DIR}/../"

SRC_URI:append = "\
    file://drta/ \
"

FILESEXTRAPATHS:prepend := "${THISDIR}/files/shell_loader:"




SRC_URI_PATCHES:append = "\
	file://inspire_shell_loader.patch;patchdir=zephyr \
"

SDK_POSTPROCESS_COMMAND:prepend = "insp_update_env "

fakeroot insp_update_env() {
	echo "export LLEXTFLAGS=\"-DLL_EXTENSION_BUILD -imacros generated/zephyr/autoconf.h -mcmodel=medany -mabi=lp64d -imacros zephyr/toolchain/zephyr_stdint.h -mno-relax -c\"" >> ${SDK_DIR}/image/usr/local/oe-sdk-hardcoded-buildpath/environment-setup-riscv64-inspire-elf
}
