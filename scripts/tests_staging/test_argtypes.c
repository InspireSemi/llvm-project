#include <stdio.h>

int main() {
    int N = 100;
    int scalar = 42;
    float array[100];
    float *ptr = array;

    // Test different argument types
    #pragma omp target map(to: array[0:N]) map(tofrom: scalar)
    {
        for (int i = 0; i < N; i++) {
            array[i] = scalar + i;
        }
    }

    printf("Done\n");
    return 0;
}
