#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <raft/raft.h>
#include <sys/time.h>

int __raft_send_requestvote(raft_server_t* raft, void *user_data, raft_node_t *node, msg_requestvote_t* m) {

}

int __raft_send_appendentries(raft_server_t* raft, void *user_data, raft_node_t *node, msg_appendentries_t* m) {

}

int __raft_applylog(raft_server_t* raft, void *udata, raft_entry_t *ety) {

}

int __raft_persist_vote(raft_server_t *raft, void *udata, const int voted_for) {

}

int __raft_persist_term(raft_server_t* raft, void* udata, const int current_term) {

}

int __raft_logentry_offer(raft_server_t* raft, void *udata, raft_entry_t *ety, int ety_idx) {

}

int __raft_logentry_poll(raft_server_t* raft, void *udata, raft_entry_t *entry, int ety_idx) {

}

int __raft_logentry_pop(raft_server_t* raft, void *udata, raft_entry_t *entry, int ety_idx) {

}

void __raft_node_has_sufficient_logs(raft_server_t* raft, void *user_data, raft_node_t* node) {

}

void __raft_log(raft_server_t* raft, raft_node_t* node, void *udata, const char *buf) {

}

typedef struct {
    int node_id;
    uint64_t last_usec;
    raft_server_t* raft;
} server_t;

raft_cbs_t raft_funcs = {
    .send_requestvote            = __raft_send_requestvote,
    .send_appendentries          = __raft_send_appendentries,
    .applylog                    = __raft_applylog,
    .persist_vote                = __raft_persist_vote,
    .persist_term                = __raft_persist_term,
    .log_offer                   = __raft_logentry_offer,
    .log_poll                    = __raft_logentry_poll,
    .log_pop                     = __raft_logentry_pop,
    .node_has_sufficient_logs    = __raft_node_has_sufficient_logs,
    .log                         = __raft_log,
};

server_t *sv = NULL;

typedef enum {
    START,
    JOIN,
    REJOIN,
} server_action_t;

void call_raft_periodic() {
  // raft_periodic() uses msec_elapsed for only request and election timeouts.
  // msec_elapsed is in integer milliseconds which does not work for us because
  // we invoke raft_periodic() much work frequently. Instead, we accumulate
  // cycles over calls to raft_periodic().

  struct timeval system_time;
  int time_retval = gettimeofday(&system_time, NULL);
  if (time_retval < 0) {
      printf("Unable to get time\n");
      exit(-1);
  }
  uint64_t usec_now = system_time.tv_sec*(uint64_t)1000000 + system_time.tv_usec;
  uint64_t usec_elapsed = usec_now - sv->last_usec;
  uint64_t msec_elapsed = usec_elapsed / 1000;
  if (msec_elapsed > 0) {
      sv->last_usec = usec_now;
      raft_periodic(sv->raft, msec_elapsed);
  } else {
      raft_periodic(sv->raft, 0);
  }
}

int main() {
    server_action_t action = START;
    int node_id = 0;
    sv = malloc(sizeof(server_t));
    memset(sv, 0, sizeof(server_t));
    sv->raft = raft_new();
    raft_set_callbacks(sv->raft, &raft_funcs, sv);
    srand(time(NULL));

    sv->node_id = node_id;
    raft_add_node(sv->raft, NULL, sv->node_id, 1);

    if (action == START) {
        raft_become_leader(sv->raft);
        __append_cfg_change(sv, RAFT_LOGTYPE_ADD_NODE, ip_addr, context_id, sv->node_id);
    } else if (action == JOIN) {
        
    } else {
        if (raft_get_num_nodes(sv->raft) == 1) {
            raft_become_leader(sv->raft);
        } else {
            for (int i = 0; i < raft_get_num_nodes(sv->raft); i++) {
                raft_node_t* node = raft_get_node_from_idx(sv->raft, i);
                if (raft_node_get_id(node) == sv->node_id) {
                    continue;
                }

            }
        }
    }

    // Enter periodic handling
    while (true) {
        call_raft_periodic();
        raft_apply_all(sv->raft);
    }

    return 0;
}
