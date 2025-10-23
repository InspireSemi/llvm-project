#include <omp.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
  uint32_t arr[16];
  uint32_t expected[16];

  for (int idx = 0; idx < 16; idx++) {
    arr[idx] = idx;
    expected[idx] = idx * 4;
  }

#pragma omp target map(tofrom: arr)
#pragma omp parallel for num_threads(2)
  for (int idx = 0; idx < 16; idx++) {
    arr[idx] = arr[idx] * 4;
  }

  // Check results
  int error = 0;
  for (int idx = 0; idx < 16; idx++) {
    if (arr[idx] != expected[idx]) {
      printf("Error: arr[%d] is wrong! Expected %u, got %u\n", idx, expected[idx], arr[idx]);
      error = 1;
    } else {
      printf("arr[%d] is correct\n", idx);
    }
  }
  if (error) {
    printf("Computation failed!\n");
    return 1;
  }

  return 0;
}
