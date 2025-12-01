#include <omp.h>
#include <stdio.h>
#include <stdint.h>

// SAXPY: Y = a*X + Y
// Single-precision A·X Plus Y operation

int main(void) {
  const int N = 256;
  float a = 2.5f;
  float X[N];
  float Y[N];
  float expected[N];

  // Initialize arrays
  for (int i = 0; i < N; i++) {
    X[i] = (float)i;
    Y[i] = (float)(i * 2);
    expected[i] = a * X[i] + Y[i];
  }

#pragma omp target map(to: X, a) map(tofrom: Y)
#pragma omp parallel for num_threads(4)
  for (int i = 0; i < N; i++) {
    Y[i] = a * X[i] + Y[i];
  }

  // Check results
  int error = 0;
  int error_count = 0;
  for (int i = 0; i < N; i++) {
    // Use small epsilon for floating-point comparison
    float diff = Y[i] - expected[i];
    if (diff < -0.0001f || diff > 0.0001f) {
      if (error_count < 10) {  // Only print first 10 errors
        printf("Error: Y[%d] is wrong! Expected %.2f, got %.2f\n", i, expected[i], Y[i]);
      }
      error = 1;
      error_count++;
    }
  }
  
  if (error) {
    printf("Computation failed! (%d errors found)\n", error_count);
    return 1;
  } else {
    printf("SAXPY computation passed! All %d elements correct.\n", N);
  }

  return 0;
}
