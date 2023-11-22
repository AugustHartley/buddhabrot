#define BUDDHABROT buddhabrot_avx2

#pragma GCC target "avx2"

#define TEST(a) a[0] & a[1] & a[2] & a[3] & a[4] & a[5] & a[6] & a[7]
#define VEC_SIZE 8

#include "buddhabrot.c"
