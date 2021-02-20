
#define CONFIG_TYPE 0
#define TRAVERSAL_REQ_TYPE 1
#define TRAVERSAL_RESP_TYPE 2

#define G 667e2
#define RESP_MSG_LEN 24

double sqrt (double x) {
  asm volatile ("fsqrt.d %0, %1" : "=f" (x) : "f" (x));
  return x;
}

uint64_t compute_force(uint64_t xcom, uint64_t ycom, uint64_t xpos, uint64_t ypos) {
  // compute force on the particle
  // assume unit masses
  // If the particle is sufficiently far away then set valid and compute force.
  // Otherwise, unset valid.
  double xdiff = xcom - xpos;
  double ydiff = ycom - ypos;
  double distance = sqrt(xdiff*xdiff + ydiff*ydiff);
  double force_d = G/(distance*distance);
  uint64_t force = (uint64_t)force_d;
//  printf("xcom=%ld, ycom=%ld, xpos=%ld, ypos=%ld, force=%ld\n", xcom, ycom, xpos, ypos, force);
  return force;
}
