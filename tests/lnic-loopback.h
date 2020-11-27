#ifndef RISCV_LNIC_LOOPBACK_H
#define RISCV_LNIC_LOOPBACK_H

#include "lnic.h"

#define REPEAT_3(X) X X X
#define REPEAT_4(X) X X X X
#define REPEAT_8(X) REPEAT_4(X) REPEAT_4(X)
#define REPEAT_15(X) REPEAT_8(X) REPEAT_4(X) X X X
#define REPEAT_16(X) REPEAT_8(X) REPEAT_8(X)
#define REPEAT_32(X) REPEAT_16(X) REPEAT_16(X)
#define REPEAT_63(X) REPEAT_32(X) REPEAT_16(X) REPEAT_8(X) REPEAT_4(X) X X X
#define REPEAT_64(X) REPEAT_32(X) REPEAT_32(X)
#define REPEAT_127(X) REPEAT_64(X) REPEAT_32(X) REPEAT_16(X) REPEAT_8(X) REPEAT_4(X) X X X
#define REPEAT_128(X) REPEAT_64(X) REPEAT_64(X)
#define REPEAT_174(X) REPEAT_128(X) REPEAT_32(X) REPEAT_8(X) X X X X X X

/* Copy all except the last word back into the network.
 */
void copy_payload(uint16_t msg_len) {

  if (msg_len == 8) {
    return;
  } else if (msg_len == 32) {
    REPEAT_3(lnic_copy();)
  } else if (msg_len == 128) {
    REPEAT_15(lnic_copy();)
  } else if (msg_len == 512) {
    REPEAT_63(lnic_copy();)
  } else if (msg_len == 1024) {
    REPEAT_127(lnic_copy();)
  } else if (msg_len == 1400) {
    REPEAT_174(lnic_copy();)
  } else {
    printf("Application is not optimized for msg len: %d\n", msg_len);
    int num_words = msg_len/LNIC_WORD_SIZE;
    if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
    int i;
    for (i = 0; i < num_words-1; i++) {
      lnic_copy();
    }
  }
}

/* Copy and increment all except the last word back into the network.
 */
void inc_payload(uint16_t msg_len) {

  if (msg_len == 8) {
    return;
  } else if (msg_len == 32) {
    REPEAT_3(asm volatile ("addi "LWRITE", "LREAD", 1\n\t");)
  } else if (msg_len == 128) {
    REPEAT_15(asm volatile ("addi "LWRITE", "LREAD", 1\n\t");)
  } else if (msg_len == 512) {
    REPEAT_63(asm volatile ("addi "LWRITE", "LREAD", 1\n\t");)
  } else if (msg_len == 1024) {
    REPEAT_127(asm volatile ("addi "LWRITE", "LREAD", 1\n\t");)
  } else if (msg_len == 1400) {
    REPEAT_174(asm volatile ("addi "LWRITE", "LREAD", 1\n\t");)
  } else {
    printf("Application is not optimized for msg len: %d\n", msg_len);
    int num_words = msg_len/LNIC_WORD_SIZE;
    if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
    int i;
    for (i = 0; i < num_words-1; i++) {
      asm volatile ("addi "LWRITE", "LREAD", 1\n\t");
    }
  }
}

#endif // RISCV_LNIC_LOOPBACK_H
