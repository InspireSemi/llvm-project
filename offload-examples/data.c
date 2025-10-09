#include <omp.h>
#include <stdio.h>

int main(void) {
  // Use an array instead of a single int. Size = 100 bytes.
  int data[25] = {0};

  printf("Host: data[0]=%d, data[24]=%d before offload.\n", data[0], data[24]);

#pragma omp target map(from : data)
  {
    // Modify the data to prove the kernel ran.
    data[0] = 123;
    data[24] = 456;
  }

  printf("Host: data[0]=%d, data[24]=%d after offload.\n", data[0], data[24]);

  // Check for the expected result
  if (data[0] == 123 && data[24] == 456) {
    printf("SUCCESS: Data correctly modified on the device.\n");
  } else {
    printf("FAILURE: Data not modified as expected.\n");
  }

  return 0;
}
