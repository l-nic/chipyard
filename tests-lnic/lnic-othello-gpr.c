#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "othello.h"

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
 *   timestamp
 * 
 * ReduceMsg Format:
 *   target_host_id - ID of the host that this msg should be sent to
 *   target_msg_ptr - ptr to the MsgState on the target_host
 *   minimax_val - the min (or max) value sent up the tree
 *   timestamp
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

int main(void) {
  // register context ID with L-NIC
  lnic_add_context(0, 0);

  // local variables
  uint64_t app_hdr;
  uint64_t board;
  // TODO(sibanez): make this an array eventually, one msg_state per incomming map msg
  struct state msg_state;
  uint64_t map_start_time = 0;
  uint64_t reduce_start_time = 0;
  uint64_t map_type = MAP_TYPE;
  uint64_t new_boards[MAX_BOARDS];
  int num_boards;
  uint64_t max_depth, cur_depth;
  int i;
  uint64_t minimax_val;
  struct state *state_ptr;
  uint64_t map_cnt, response_cnt, msg_minimax_val;
  printf("Ready!\n");
  // process pkts 
othello_start:
  lnic_wait();
  // read app hdr
  app_hdr = lnic_read();
  // read Othello hdr and branch based on msg_type
  lnic_branch("bne", map_type, process_reduce);
  // process map msg
  board = lnic_read();
  compute_boards(board, new_boards, &num_boards);
  max_depth = lnic_read();
  cur_depth = lnic_read();
  //printf("Processing Map message.\n\tboard = %lu\n\tmax_depth = %lu\n\tcur_depth = %lu\n", board, max_depth, cur_depth);
  if (cur_depth < max_depth) {
    // send out new boards in map msgs
    // record msg state (on stack because don't want to implement malloc)
    msg_state.src_host_id = lnic_read();
    msg_state.src_msg_ptr = lnic_read();
    map_start_time = lnic_read();
    msg_state.map_cnt = num_boards;
    msg_state.response_cnt = 0;
    msg_state.minimax_val = MAX_INT;
    // send map msgs
    for (i = 0; i < num_boards; i++) {
      // write msg len
      lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | MAP_MSG_LEN);
      // write msg type
      lnic_write_i(MAP_TYPE);
      // write new_board, max_depth, cur_depth, src_host_id, src_msg_ptr, timestamp
      lnic_write_r(new_boards[i]); // TODO(sibanez): need a lnic_write_m() for memory writes?
      lnic_write_r(max_depth);
      lnic_write_r(cur_depth + 1);
      lnic_write_i(HOST_ID);
      lnic_write_r(&msg_state);
      lnic_write_r(map_start_time);
    }
  } else {
    // evaluate_boards to compute the min (or max)
    evaluate_boards(new_boards, num_boards, &minimax_val);
    // construct reduce msg and send into network
    // write msg length
    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | REDUCE_MSG_LEN);
    // write msg type
    lnic_write_i(REDUCE_TYPE);
    // write target_host_id, target_msg_ptr, minimax_val, timestamp
    lnic_copy();
    lnic_copy();
    lnic_write_r(minimax_val);
    lnic_copy();
  }
  lnic_msg_done();
  goto othello_start;
process_reduce:
  // discard target_host_id
  lnic_read();
  // get msg state ptr
  state_ptr = (struct state *)lnic_read();
  // lookup map_cnt, response_cnt, minimax_val
  map_cnt = (*state_ptr).map_cnt;
  (*state_ptr).response_cnt += 1; // increment response_cnt
  response_cnt = (*state_ptr).response_cnt;
  minimax_val = (*state_ptr).minimax_val;
  msg_minimax_val = lnic_read();
  // record timestamp of first reduce msg
  if (response_cnt == 1) {
    reduce_start_time = lnic_read();
  } else {
    lnic_read();
  }
  // compute running min val
  if (msg_minimax_val < minimax_val) {
    (*state_ptr).minimax_val = msg_minimax_val;
  }
  // check if all responses have been received
  if (response_cnt == map_cnt) {
    // send reduce_msg to parent
    // write msg length
    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | REDUCE_MSG_LEN);
    // write msg type
    lnic_write_i(REDUCE_TYPE);
    // write target_host_id, target_msg_ptr, minimax_val
    lnic_write_m((*state_ptr).src_host_id);
    lnic_write_m((*state_ptr).src_msg_ptr);
    lnic_write_m((*state_ptr).minimax_val);
    lnic_write_r(reduce_start_time);
  }
  lnic_msg_done();
  goto othello_start;
  return 0;
}

