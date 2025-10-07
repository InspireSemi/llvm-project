 int main(void) {
  int isHost = 11;
  int isvar = 346;
  char* address = 0;

#pragma omp target map(tofrom : isHost, isvar, address)
  { isHost = 34; }
 }
