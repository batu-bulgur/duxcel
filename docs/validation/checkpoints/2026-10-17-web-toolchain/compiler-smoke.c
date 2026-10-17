#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
int main(void) { uint64_t n=UINT64_C(9007199254740993); printf("C99-Wasm %" PRIu64 "\n",n); return n==UINT64_C(9007199254740993) ? 0 : 1; }
