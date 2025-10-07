#include <omp.h>
#include <stdio.h>

int main(void) {
  uint32_t isHost = 11;
  uint8_t isvar = 49;
  uint64_t address = UINT64_MAX;

#pragma omp target map(tofrom : isHost, isvar, address)
  { isHost = 34;
    isvar = 39;
    address = 235;}

  if (isHost < 0) {
  }
  printf("Ishost value is %d\n", isHost);
  // CHECK: Target region executed on the device
  printf("Target region executed on the %s\n", isHost ? "host" : "device");

  return isHost;
}
