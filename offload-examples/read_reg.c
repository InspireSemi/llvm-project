#include <omp.h>
#include <stdio.h>

// This prototype is required for the compiler intrinsic.
unsigned long long __builtin_riscv_rdcycle(void);

int main(void) {
  unsigned long long deviceCycles = 0;

  printf("Host: deviceCycles before offload = %llu\n", deviceCycles);

#pragma omp target map(from : deviceCycles)
  {
    // Use the compiler intrinsic to guarantee the correct instruction is
    // generated.
    deviceCycles = __builtin_riscv_rdcycle();
  }

  printf("Host: deviceCycles after offload = %llu (0=failed)\n", deviceCycles);

  if (deviceCycles > 0) {
    printf("SUCCESS: Kernel executed on the rv64gc target.\n");
  } else {
    printf("FAILURE: Kernel did not execute correctly.\n");
  }

  return 0;
}
