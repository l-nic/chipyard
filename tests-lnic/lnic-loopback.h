#ifndef RISCV_LNIC_LOOPBACK_H
#define RISCV_LNIC_LOOPBACK_H

#include "lnic.h"

#define REPEAT_2(X) X X
#define REPEAT_3(X) X X X
#define REPEAT_4(X) X X X X
#define REPEAT_8(X) REPEAT_4(X) REPEAT_4(X)
#define REPEAT_15(X) REPEAT_8(X) REPEAT_4(X) X X X
#define REPEAT_16(X) REPEAT_8(X) REPEAT_8(X)
#define REPEAT_32(X) REPEAT_16(X) REPEAT_16(X)
#define REPEAT_48(X) REPEAT_32(X) REPEAT_16(X)
#define REPEAT_62(X) REPEAT_32(X) REPEAT_16(X) REPEAT_8(X) REPEAT_4(X) X X
#define REPEAT_63(X) REPEAT_62(X) X
#define REPEAT_64(X) REPEAT_32(X) REPEAT_32(X)
#define REPEAT_125(X) REPEAT_64(X) REPEAT_32(X) REPEAT_16(X) REPEAT_8(X) REPEAT_4(X) X
#define REPEAT_127(X) REPEAT_125(X) X X
#define REPEAT_128(X) REPEAT_64(X) REPEAT_64(X)
#define REPEAT_174(X) REPEAT_128(X) REPEAT_32(X) REPEAT_8(X) X X X X X X

/* Copy the specified number of bytes from netRX to netTX
 */
void copy_payload(uint16_t num_bytes) {

  if (num_bytes == 0) {
    return;
  } else if (num_bytes == 16) {
    goto copy_2;
  } else if (num_bytes == 48) {
    goto copy_6;
  } else if (num_bytes == 112) {
    goto copy_14;
  } else if (num_bytes == 240) {
    goto copy_30;
  } else if (num_bytes == 496) {
    goto copy_62;
  } else if (num_bytes == 1008) {
    goto copy_126;
  } else {
    printf("Application is not optimized to increment %d bytes:\n", num_bytes);
    int num_words = num_bytes/LNIC_WORD_SIZE;
    if (num_bytes % LNIC_WORD_SIZE != 0) { num_words++; }
    int i;
    for (i = 0; i < num_words; i++) {
      lnic_copy();
    }
    return;
  }

copy_126:
  REPEAT_64(lnic_copy();)
copy_62:
  REPEAT_32(lnic_copy();)
copy_30:
  REPEAT_16(lnic_copy();)
copy_14:
  REPEAT_8(lnic_copy();)
copy_6:
  REPEAT_4(lnic_copy();)
copy_2:
  REPEAT_2(lnic_copy();)

}

/* Copy and increment all except the last word back into the network.
 */
#define lnic_inc() asm volatile ("addi "LWRITE", "LREAD", 1\n\t")
void inc_payload(uint16_t num_bytes) {

  if (num_bytes == 0) {
    return;
  } else if (num_bytes == 16) {
    goto copy_2;
  } else if (num_bytes == 48) {
    goto copy_6;
  } else if (num_bytes == 112) {
    goto copy_14;
  } else if (num_bytes == 240) {
    goto copy_30;
  } else if (num_bytes == 496) {
    goto copy_62;
  } else if (num_bytes == 1008) {
    goto copy_126;
  } else {
    printf("Application is not optimized to increment %d bytes:\n", num_bytes);
    int num_words = num_bytes/LNIC_WORD_SIZE;
    if (num_bytes % LNIC_WORD_SIZE != 0) { num_words++; }
    int i;
    for (i = 0; i < num_words; i++) {
      lnic_inc();
    }
    return;
  }

copy_126:
  REPEAT_64(lnic_inc();)
copy_62:
  REPEAT_32(lnic_inc();)
copy_30:
  REPEAT_16(lnic_inc();)
copy_14:
  REPEAT_8(lnic_inc();)
copy_6:
  REPEAT_4(lnic_inc();)
copy_2:
  REPEAT_2(lnic_inc();)
}

#endif // RISCV_LNIC_LOOPBACK_H
