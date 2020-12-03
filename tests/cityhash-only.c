#include <stdio.h>
#include <stdlib.h>

#define KEY_SIZE_WORDS   2


uint64_t HashLen16(uint64_t u, uint64_t v, uint64_t mul) {
  if (mul == 0) printf("HashLen16 mul is zero!!!\n");
  if (v == 0) printf("HashLen16 v is zero!!!\n");
  if (u == 0) printf("HashLen16 u is zero!!!\n");
  // Murmur-inspired hashing.
  volatile uint64_t a = (u ^ v) * mul;
  //printf("u=%ld v=%ld mul=%ld\n", u, v, mul);
  a ^= (a >> 47);
  volatile uint64_t b = (v ^ a) * mul;
  b ^= (b >> 47);
  b *= mul;
  //printf("preparing to return %llu\n", b);
  return b;
}

uint64_t rotate(uint64_t val, int shift) {
  // Avoid shifting by 64: doing so yields an undefined result.
  return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
}

uint64_t cityhash(uint64_t *s) {
  volatile uint64_t k2 = 0x9ae16a3b2f90404fULL;
  //printf("k2 is %llu\n", k2);
  // if (k2 == 0x9ae16a3b2f90404fULL) {
  //   printf("matched unprintable number\n");
  // }

  //printf("k2=%lx  mul=%ld   x=%ld\n", k2, 0UL, 0x9ae16a3b2f90404f);
  //if (k2 + s[0] == 0) printf("k2=0!!!!\n");
  volatile uint64_t mul = k2 + (KEY_SIZE_WORDS * 8) * 2;
  //printf("word is %ld\n", (KEY_SIZE_WORDS * 8) * 2);
  //printf("mul is %#lx\n", mul);
  volatile uint64_t a = s[0] + k2;
  volatile uint64_t b = s[1];
  volatile uint64_t c = rotate(b, 37) * mul + a;
  volatile uint64_t d = (rotate(a, 25) + b) * mul;
  // if (d == 0) {
  //   printf("d==0!!!!\n");
  // }
  // if (a == 0) {
  //   printf("a==0!!!!\n");
  // }
  // if (mul == 0) {
  //   printf("mul==0!!!!\n");
  // }

  //printf("Calling hashlen with c, d, mul of %ld, %ld, %ld\n", c, d, mul);
  return HashLen16(c, d, mul);

  // volatile uint64_t u = c;
  // volatile uint64_t v = d;

  // //if (mul == 0) printf("HashLen16 mul is zero!!!\n");
  // //if (v == 0) printf("HashLen16 v is zero!!!\n");
  // //if (u == 0) printf("HashLen16 u is zero!!!\n");

  // a = (u ^ v) * mul;
  // //printf("u=%ld v=%ld mul=%ld\n", u, v, mul);
  // a ^= (a >> 47);
  // b = (v ^ a) * mul;
  // b ^= (b >> 47);
  // b *= mul;
  // //printf("preparing to return %ld\n", b);
  // return b;
}
  uint64_t h[1];


int main() {
  uint64_t myval[2] = {0, 0};
  uint8_t *myvalp = (uint8_t *)myval;
  *myvalp = 3;

  h[0] = cityhash(myval);
  //uint8_t *p = (uint8_t *)&h[0];
  printf("is the hash==0? %d\n", h[0] == 0);
  //printf("hi %llx\n", h[0]);
  if (h[0] == 0xd9dee51d42a942e5LLU) {
    printf("correct hash value\n");
  } else {
    printf("value is %llx\n", h[0]);
  }    return 0;
}