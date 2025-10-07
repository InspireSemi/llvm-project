#include <omp.h>
#include <stdio.h>

int main(void) {
  int isHost = 0; // Starts at 0

#pragma omp target map(from : isHost)
  {
    // Change the value to 1 so the compiler sees a side effect
    isHost = 1;
  }

  // Add a print statement to "use" the variable
  printf("isHost value after offload: %d (0=host, 1=device)\n", isHost);

  return 0; // Return 0 for success
}
