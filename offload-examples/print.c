#include <inttypes.h>
#include <omp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <zephyr/toolchain.h>

int main(void) {
  printf("Host: About to offload kernel.\n");

// This kernel attempts to print directly from the device.
#pragma omp target
  {
    printk("--- KERNEL: Hello from the RISC-V device! ---\n");
  }

  printk("Host: Kernel offload finished.\n");

  return 0;
}
