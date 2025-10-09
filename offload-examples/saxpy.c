#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  int n = 1 << 20; // ~1 million elements
  float a = 2.0f;

  float *x = malloc(n * sizeof(float));
  float *y = malloc(n * sizeof(float));

  for (int i = 0; i < n; i++) {
    x[i] = 1.0f;
    y[i] = 2.0f;
  }

#pragma omp parallel for
  for (int i = 0; i < n; i++) {
    y[i] = a * x[i] + y[i];
  }

  printf("y[0] = %f\n", y[0]); // sanity check

  free(x);
  free(y);
  return 0;
}
