#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"
#include "othello.h"

/**
 * Othello nanoservice implementation on RISC-V using IceNIC.
 */

#define OTHELLO_HEADER_SIZE 8
struct othello_header {
  uint64_t type;
};

struct map_header {
  uint64_t board;
  uint64_t max_depth;
  uint64_t cur_depth;
  uint64_t src_host_id;
  uint64_t src_msg_ptr;
  uint64_t timestamp;
};

struct reduce_header {
  uint64_t target_host_id;
  uint64_t target_msg_ptr;
  uint64_t minimax_val;
  uint64_t timestamp;
};

/**
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

uint64_t buffer[ETH_MAX_WORDS];

int main(void) {
  // headers
  struct lnic_header *lnic;
  struct othello_header *othello;
  struct map_header *map;
  struct reduce_header *reduce;

  // local variables
  uint64_t board;
  ssize_t size;
  // TODO(sibanez): make this an array eventually, one msg_state per incomming map msg
  struct state msg_state;
  uint64_t map_start_time;
  uint64_t reduce_start_time;

  printf("Ready!\n");
  // process pkts 
  while (1) {
    nic_recv_lnic(buffer, &lnic);
    // read Othello hdr and branch based on msg_type
    othello = (void *)lnic + LNIC_HEADER_SIZE;
    if (ntohl(othello->type) == MAP_TYPE) {
      // process map msg
      map = (void *)othello + OTHELLO_HEADER_SIZE;
      board = ntohl(map->board);
      uint64_t new_boards[MAX_BOARDS];
      int num_boards;
      compute_boards(board, new_boards, &num_boards);
      uint64_t max_depth, cur_depth;
      max_depth = ntohl(map->max_depth);
      cur_depth = ntohl(map->cur_depth);
      map_start_time = ntohl(map->timestamp);
      if (cur_depth < max_depth) {
        // send out new boards in map msgs
        // record msg state (on stack because don't want to implement malloc)
        msg_state.src_host_id = ntohl(map->src_host_id);
        msg_state.src_msg_ptr = ntohl(map->src_msg_ptr);
        msg_state.map_cnt = num_boards;
        msg_state.response_cnt = 0;
        msg_state.minimax_val = MAX_INT;
        // send out map msgs
        swap_eth(buffer);
        for (int i = 0; i < num_boards; i++) {
          // write new_board, max_depth, cur_depth, src_host_id, src_msg_ptr
	  map->board = htonl(new_boards[i]);
	  map->max_depth = htonl(max_depth);
	  map->cur_depth = htonl(cur_depth + 1);
	  map->src_host_id = htonl(HOST_ID);
	  map->src_msg_ptr = htonl(&msg_state);
	  // NOTE: no need to update map_start_time because it is already there
	  size = ceil_div(ETH_HEADER_SIZE + 20 + LNIC_HEADER_SIZE +  MAP_MSG_LEN, 8) * 8;
	  nic_send(buffer, size);
        }
      } else {
        // evaluate_boards to compute the min (or max)
        uint64_t minimax_val;
        evaluate_boards(new_boards, num_boards, &minimax_val);
        // construct reduce msg and send into network
	reduce = (void *)othello + OTHELLO_HEADER_SIZE;
        // write msg type
	othello->type = htonl(REDUCE_TYPE);
        // write target_host_id, target_msg_ptr, minimax_val
	reduce->target_host_id = map->src_host_id;
	reduce->target_msg_ptr = map->src_msg_ptr;
	reduce->minimax_val = htonl(minimax_val);
	reduce->timestamp = htonl(map_start_time);
	size = ceil_div(ETH_HEADER_SIZE + 20 + LNIC_HEADER_SIZE +  REDUCE_MSG_LEN, 8) * 8;
	nic_send(buffer, size);
      }
    } else { // REDUCE_MSG
      reduce = (void *)othello + OTHELLO_HEADER_SIZE;
      // get msg state ptr
      struct state *state_ptr;
      state_ptr = (struct state *)ntohl(reduce->target_msg_ptr);
      // lookup map_cnt, response_cnt, minimax_val
      uint64_t map_cnt, response_cnt, minimax_val, msg_minimax_val;
      map_cnt = (*state_ptr).map_cnt;
      (*state_ptr).response_cnt += 1; // increment response_cnt
      response_cnt = (*state_ptr).response_cnt;
      minimax_val = (*state_ptr).minimax_val;
      msg_minimax_val = ntohl(reduce->minimax_val);
      // compute running min val
      if (msg_minimax_val < minimax_val) {
        (*state_ptr).minimax_val = msg_minimax_val;
      }
      // record start time of first reduce msg
      if (response_cnt == 1) {
        reduce_start_time = ntohl(reduce->timestamp);
      }
      // check if all responses have been received
      if (response_cnt == map_cnt) {
        // send reduce_msg to parent
	swap_eth(buffer);
        // write target_host_id, target_msg_ptr, minimax_val
	reduce->target_host_id = htonl((*state_ptr).src_host_id);
	reduce->target_msg_ptr = htonl((*state_ptr).src_msg_ptr);
	reduce->minimax_val = htonl((*state_ptr).minimax_val);
        reduce->timestamp = htonl(reduce_start_time);
	size = ceil_div(ETH_HEADER_SIZE + 20 + LNIC_HEADER_SIZE +  REDUCE_MSG_LEN, 8) * 8;
	nic_send(buffer, size);
      }
    }
  }
  return 0;
}

