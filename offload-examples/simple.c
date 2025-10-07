#include <omp.h>
#include <stdio.h>

#define ARRAY_SIZE 16

int main(void) {
  int data[ARRAY_SIZE];
  int errors = 0;

  // 1. Initialize data on the host
  for (int i = 0; i < ARRAY_SIZE; i++) {
    data[i] = i;
  }

// 2. Offload the computation. The 'data' array is mapped to the device,
//    modified there, and the results are mapped back to the host.
#pragma omp target map(tofrom : data)
  {
    // This loop has a clear side effect that the compiler cannot optimize away.
    for (int i = 0; i < ARRAY_SIZE; i++) {
      data[i] = data[i] + 100; // Add 100 to each element
    }
  } // End of target region

  // 3. Verify the results back on the host
  printf("Verifying results...\n");
  for (int i = 0; i < ARRAY_SIZE; i++) {
    int expected_value = i + 100;
    if (data[i] != expected_value) {
      printf("Error at index %d: Got %d, Expected %d\n", i, data[i],
             expected_value);
      errors++;
    }
  }

  if (errors == 0) {
    printf("Success! The offload region executed correctly.\n");
  } else {
    printf("FAILURE: Found %d errors.\n", errors);
  }

  return errors;
}
