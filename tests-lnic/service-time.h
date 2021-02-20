#ifndef RISCV_SERVICE_TIME_H
#define RISCV_SERVICE_TIME_H

#include "encoding.h"

#define CONFIG_TYPE 0
#define DATA_REQ_TYPE 1

void process_msg(uint64_t service_time) {
  uint64_t start_stall_time = rdcycle();
  while (rdcycle() < start_stall_time + service_time);
}

#endif // RISCV_SERVICE_TIME_H
