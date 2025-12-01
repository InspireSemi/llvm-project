#include <stdio.h>

int main() {
    int scalar = 42;
    int result = 0;
    
    #pragma omp target map(to: scalar) map(from: result)
    {
        result = scalar * 2;
    }
    
    printf("Result: %d\n", result);
    return 0;
}
