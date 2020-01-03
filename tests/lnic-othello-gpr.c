#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encoding.h"

/**
 * Othello nanoservice implementation on RISC-V using LNIC GPR implementation.
 */

/**
 * OthelloHdr format:
 *   msg_type - type of msg (either MapMsg or ReduceMsg)
 * 
 * MapMsg Format:
 *   board - othello game board
 *   max_depth - max depth into the game tree this map message should propagate
 *   cur_depth - how deep into the game tree this msg currently is
 *   src_host_id - ID of the host that generated this msg
 *   src_msg_ptr - ptr to the MsgState on the src_host
 * 
 * ReduceMsg Format:
 *   target_host_id - ID of the host that this msg should be sent to
 *   target_msg_ptr - ptr to the MsgState on the target_host
 *   minimax_val - the min (or max) value sent up the tree
 * 
 * Othello MsgState:
 *   src_host_id - who to send the result to once all responses arrive
 *   src_msg_ptr - ptr to the MsgState on the src_host
 *   map_cnt - number of messages (i.e. boards) that were generated in response to processing msg_id
 *   response_cnt - number of the responses that have arrived so far (incremented during the reduce phase)
 *   minimax_val - the running min/max value of the responses
 * 
 * ###############################
 * # Othello Program Pseudo Code #
 * ###############################
 * Receive and parse msg from the network
 * If MapMsg:
 *   Compute new boards
 *   If the desired depth has been reached:
 *     Evalute the boards and compute running min (or max)
 *     Send ReduceMsg with min (or max) value to parent
 *   Else: # depth has not been reached yet
 *     Record MsgState so that we know when we've received all the responses
 *     Send out new MapMsgs into the network
 * Else: # it is a ReduceMsg
 *   Lookup MsgState associated with the msg
 *   If all responses have been received:
 *     Send ReduceMsg with min (or max) value up to parent
 */

// LNIC CSRs
#define CSR_LMSGSRDY 0x052

#define LREAD "x30"
#define LWRITE "x31"

#define MAP_TYPE 0
#define REDUCE_TYPE 1
#define REDUCE_TYPE_STR "2"

// msg lengths in bytes
#define MAP_MSG_LEN 48
#define REDUCE_MSG_LEN 32

#define MAX_BOARDS 4
#define HOST_ID 0
#define MAX_INT 0xFFFFFFFF

#define lnic_wait() while (read_csr(0x052) == 0)
#define lnic_read() ({ uint64_t __tmp; \
  asm volatile ("mv %0, "LREAD  : "=r"(__tmp)); \
  __tmp; })
#define lnic_copy() asm volatile ("mv "LWRITE", "LREAD)
#define lnic_write_r(val) asm volatile ("mv "LWRITE", %0" : /*no outputs*/ : "r"(val))
#define lnic_write_i(val) asm volatile ("li "LWRITE", %0" : /*no outputs*/ : "i"(val))

/**
 * Compute new boards based on the current board.
 */
int compute_boards(uint64_t board, uint64_t *new_boards, int *num_boards) {
  // TODO(sibanez): implement this ...
  new_boards[0] = board;
  *num_boards = 1;
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

struct MsgState {
  uint64_t src_host_id;
  uint64_t src_msg_ptr;
  uint64_t map_cnt;
  uint64_t response_cnt;
  uint64_t minimax_val;
};

int main(void) {
  // local variables	
  uint64_t board;
  // TODO(sibanez): make this an array eventually, one MsgState for each incomming map msg
  struct MsgState msg_state;
othello_start:
  // poll the lmsgsrdy CSR to wait for a msg to arrive
  lnic_wait();
  // read app header and branch based on msg type
  asm goto ("mv t0, "LREAD"\n\t" // read app header (src info & msg len) 
            "li t0, "REDUCE_TYPE_STR"\n\t"
            "beq t0, "LREAD", %l[process_reduce]\n\t"
            : // No outputs
            : // No inputs
            : "t0" // clobbers t0
            : process_reduce);
process_map:
  board = lnic_read();
  uint64_t new_boards[MAX_BOARDS];
  int num_boards;
  compute_boards(board, new_boards, &num_boards);
  uint64_t max_depth, cur_depth;
  // TODO(sibanez): these reads are not gauranteed to occur in the correct order ...
  max_depth = lnic_read();
  cur_depth = lnic_read();
  if (cur_depth < max_depth) {
    // send out new boards in map msgs
    // record msg state (on stack because don't want to implement malloc)
    msg_state.src_host_id = lnic_read();
    msg_state.src_msg_ptr = lnic_read();
    msg_state.map_cnt = num_boards;
    msg_state.response_cnt = 0;
    msg_state.minimax_val = MAX_INT;
    // send map msgs
    for (int i = 0; i < num_boards; i++) {
      // write msg len
      lnic_write_i(MAP_MSG_LEN);
      // write msg type
      lnic_write_i(MAP_TYPE);
      // write new_board, max_depth, cur_depth, src_host_id, src_msg_ptr
      lnic_write_r(new_boards[i]); // TODO(sibanez): need a lnic_write_m() for memory writes?
      lnic_write_r(max_depth);
      lnic_write_r(cur_depth + 1);
      lnic_write_i(HOST_ID);
      lnic_write_r(&msg_state);
    }
  } else {
    // evaluate_boards to compute the min (or max)
    uint64_t minimax_val;
    evaluate_boards(new_boards, num_boards, &minimax_val);
    // construct reduce msg and send into network
    // write msg length
    lnic_write_i(REDUCE_MSG_LEN);
    // write msg type
    lnic_write_i(REDUCE_TYPE);
    // write target_host_id, target_msg_ptr, minimax_val
    lnic_copy();
    lnic_copy();
    lnic_write_r(minimax_val);
  }
  goto othello_start;
process_reduce:
  // discard target_host_id
  lnic_read();
  // get msg state ptr
  struct MsgState *state_ptr;
  state_ptr = (struct MsgState*)lnic_read();
  // lookup map_cnt, response_cnt, minimax_val
  uint64_t map_cnt, response_cnt, minimax_val, msg_minimax_val;
  map_cnt = (*state_ptr).map_cnt;
  (*state_ptr).response_cnt += 1; // increment response_cnt
  response_cnt = (*state_ptr).response_cnt;
  minimax_val = (*state_ptr).minimax_val;
  msg_minimax_val = lnic_read();
  // compute running min val
  if (msg_minimax_val < minimax_val) {
    (*state_ptr).minimax_val = msg_minimax_val;
  }
  // check if all responses have been received
  if (response_cnt == map_cnt) {
    // send reduce_msg to parent
    // write msg length
    lnic_write_i(REDUCE_MSG_LEN);
    // write msg type
    lnic_write_i(REDUCE_TYPE);
    // write target_host_id, target_msg_ptr, minimax_val
    lnic_write_r((*state_ptr).src_host_id);
    lnic_write_r((*state_ptr).src_msg_ptr);
    lnic_write_r((*state_ptr).minimax_val);
    goto exit;
  } else {
    // still need to process more reduce msgs
    goto othello_start;
  }
exit:
  return 0;
}

