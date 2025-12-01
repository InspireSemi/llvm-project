#include <omp.h>
#include <stdio.h>
#include <stdint.h>

// This test specifically verifies that scalar arguments are passed correctly
// to the offload kernel, not as pointers-to-scalars.
//
// If scalars are passed incorrectly (as addresses rather than values),
// this test will FAIL dramatically with wrong results.

int main(void) {
  const int N = 16;
  
  // Test scalars with specific, verifiable values
  int32_t multiplier = 7;      // Expected: 7, NOT an address like 0x7fff1234
  int32_t addend = 100;         // Expected: 100, NOT an address
  float scale_factor = 2.5f;    // Expected: 2.5, NOT an address
  
  uint32_t input[N];
  uint32_t output[N];
  uint32_t expected[N];
  
  // Initialize with known values
  for (int i = 0; i < N; i++) {
    input[i] = i + 1;  // 1, 2, 3, ..., 16
    // Formula: output = (input * multiplier + addend) * scale_factor
    expected[i] = (uint32_t)(((input[i] * multiplier) + addend) * scale_factor);
    output[i] = 0;  // Clear output
  }
  
  printf("Test parameters:\n");
  printf("  multiplier = %d\n", multiplier);
  printf("  addend = %d\n", addend);
  printf("  scale_factor = %.2f\n", scale_factor);
  printf("\nExpected results for first 3 elements:\n");
  for (int i = 0; i < 3; i++) {
    printf("  output[%d] = (%d * %d + %d) * %.2f = %u\n", 
           i, input[i], multiplier, addend, scale_factor, expected[i]);
  }
  printf("\n");

#pragma omp target map(to: input, multiplier, addend, scale_factor) map(from: output)
#pragma omp parallel for num_threads(4)
  for (int i = 0; i < N; i++) {
    // This computation relies on correct scalar values
    // If multiplier is an address (e.g., 0x7fff1234 instead of 7),
    // the result will be completely wrong
    uint32_t temp = (input[i] * multiplier) + addend;
    output[i] = (uint32_t)(temp * scale_factor);
  }

  // Verify results
  int errors = 0;
  printf("Verification:\n");
  for (int i = 0; i < N; i++) {
    if (output[i] != expected[i]) {
      printf("FAIL: output[%2d] = %u, expected %u (diff = %d)\n", 
             i, output[i], expected[i], (int32_t)(output[i] - expected[i]));
      errors++;
      
      // If we're getting huge numbers, scalars are likely being passed as addresses
      if (output[i] > 1000000) {
        printf("  ^^^ HUGE value suggests scalar passed as address!\n");
      }
    } else {
      printf("PASS: output[%2d] = %u ✓\n", i, output[i]);
    }
  }
  
  printf("\n");
  if (errors > 0) {
    printf("SCALAR VERIFICATION FAILED: %d/%d incorrect results\n", errors, N);
    printf("\nDiagnostic hints:\n");
    printf("- If output values are huge (>1M), scalars likely passed as addresses\n");
    printf("- If output values are zero, kernel may not have executed\n");
    printf("- If output values are wrong but reasonable, check computation logic\n");
    return 1;
  }
  
  printf("SCALAR VERIFICATION PASSED: All %d results correct!\n", N);
  printf("Scalars were passed by value correctly.\n");
  return 0;
}
