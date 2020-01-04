#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAP_TYPE 0
#define REDUCE_TYPE 1

// msg lengths in bytes
#define MAP_MSG_LEN 48
#define REDUCE_MSG_LEN 32

#define MAX_BOARDS 4
#define HOST_ID 0
#define MAX_INT 0xFFFFFFFF

/**
 * Compute new boards based on the current board.
 */
int compute_boards(uint64_t board, uint64_t *new_boards, int *num_boards) {
  // TODO(sibanez): implement this ...
  new_boards[0] = board;
  new_boards[1] = board + 1;
  *num_boards = 2;
  return 0;
}

/**
 * Evaluate boards to compute minimax value.
 */
int evaluate_boards(uint64_t *new_boards, int num_boards, uint64_t *minimax_val) {
  // TODO(sibanez): implement this ...
  *minimax_val = 1;
  return 0;
}

struct state {
  uint64_t src_host_id;
  uint64_t src_msg_ptr;
  uint64_t map_cnt;
  uint64_t response_cnt;
  uint64_t minimax_val;
};

