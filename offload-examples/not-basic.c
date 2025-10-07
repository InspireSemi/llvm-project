// not-basic.c
#include <omp.h>
#include <stdio.h>

int main(void) {
  const int N = 1<<20;
  double sum = 0.0;

#pragma omp target teams distribute parallel for map(tofrom:sum) reduction(+:sum)
  for (int i = 0; i < N; ++i) sum += 1.0/(i+1.0);

  int host_threads = 0;
#pragma omp parallel
  #pragma omp single
    host_threads = omp_get_num_threads();
 

  printf("sum=%f (N=%d) host_threads=%d\n", sum, N, host_threads);
  return 0;
}
