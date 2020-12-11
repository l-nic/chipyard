#ifndef VECTOR_SIZE
#define VECTOR_SIZE 512
#endif

#if (VECTOR_SIZE % 4) != 0
#error "VECTOR_SIZE must be a multiple of 4 for loop unrolling"
#endif

uint64_t compute_dist_squared(const uint16_t *v1, const uint16_t *v2) {
  uint64_t dist_sqrd = 0;
  for (unsigned i = 0; i < VECTOR_SIZE; i += 4) {
    int d1 = v1[i+0] - v2[i+0];
    int d2 = v1[i+1] - v2[i+1];
    int d3 = v1[i+2] - v2[i+2];
    int d4 = v1[i+3] - v2[i+3];
    dist_sqrd += d1*d1 + d2*d2 + d3*d3 + d4*d4;
  }
  return dist_sqrd;
}
