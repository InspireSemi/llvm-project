#include <omp.h>
#include <stdio.h>
#include <stdint.h>



int main(void) {
  uint32_t x = 0xf;
  uint32_t y = 0xf0;
  uint32_t z = 0xf00;
  uint32_t i = 0xf000;
  
  // Compute expected results on the host
  uint32_t expected_x = x * 4;
  uint32_t expected_y = y * 4;
  uint32_t expected_z = z * 4;
  uint32_t expected_i = i * 4;
  
#pragma omp target map(tofrom : x, y, z, i)
  {
    x = x*4;
    y = y*4;
    z = z*4;
    i = i*4;
  }

  printf("x=%x, y=%x, z=%x, i=%x\n", x, y, z, i);
  
  int error = 0;
  if (x != expected_x) {
    printf("Error: x is wrong! Expected %x, got %x\n", expected_x, x);
    error = 1;
  } else {
    printf("x is correct\n");
  }
  if (y != expected_y) {
    printf("Error: y is wrong! Expected %x, got %x\n", expected_y, y);
    error = 1;
  } else {
    printf("y is correct\n");
  }
  if (z != expected_z) {
    printf("Error: z is wrong! Expected %x, got %x\n", expected_z, z);
    error = 1;
  } else {
    printf("z is correct\n");
  }
  if (i != expected_i) {
    printf("Error: i is wrong! Expected %x, got %x\n", expected_i, i);
    error = 1;
  } else {
    printf("i is correct\n");
  }

  if (error) {
    printf("Computation failed!\n");
    return 1;
  }

  return 0;
}
