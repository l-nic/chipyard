#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

/* Implement a simple RDMA-like client and server.
 * The client sends messages that contain one-sided RDMA opcodes
 * (e.g., read, write, compare-and-swap, indirect-read).
 * The server processes each request and return a response.
 */

#define NUM_REQUESTS 200

// size of RDMA accessible memory array (units of 8B)
#define MEM_SIZE 400000

// one test for each RDMA operation type
#define NUM_TESTS 5

// To avoid using a random number generator we'll encode these random numbers in the binary
// These are uniform random values between 0 and MEM_SIZE-1
uint32_t random_vals[5][200] = {
  {147473, 217235, 390604, 361325, 263433, 220622, 290935, 387699, 98550, 42010, 387864, 159940, 132681, 93386, 277776, 172173, 387640, 33673, 319497, 104535, 325156, 52339, 185297, 167775, 24145, 360511, 87464, 67250, 232445, 29499, 246284, 307535, 254498, 300893, 238760, 86000, 147142, 380776, 20732, 46214, 295091, 383658, 118171, 15606, 352870, 72634, 243497, 336673, 272363, 110360, 180346, 38892, 250998, 87774, 235679, 284489, 134434, 115845, 337733, 353705, 88504, 41026, 376122, 131770, 341676, 251929, 344159, 276991, 238657, 267495, 176118, 33467, 333092, 85561, 86137, 209321, 372061, 243210, 240466, 393169, 317392, 137042, 175842, 275311, 186876, 98197, 325697, 297342, 363076, 252668, 193241, 92292, 169041, 145430, 165140, 163426, 345862, 119229, 104556, 53276, 148908, 300838, 97158, 14176, 89660, 281845, 279787, 115908, 108080, 127763, 123048, 309918, 44637, 229645, 270705, 110349, 367147, 98167, 14845, 281368, 222413, 46136, 70728, 132144, 359601, 366480, 375461, 95193, 19990, 386399, 303991, 305525, 366803, 85985, 306171, 92736, 69699, 373316, 10688, 379782, 369484, 298577, 363290, 109841, 127539, 225579, 129911, 224943, 12262, 54993, 43608, 210088, 148668, 225313, 165351, 335708, 197035, 308275, 361201, 188999, 201465, 41568, 3360, 60210, 104690, 272342, 66136, 69217, 266325, 352493, 243408, 124700, 362480, 63560, 315219, 216417, 75467, 166584, 180195, 45123, 390430, 276716, 320774, 158871, 81100, 14346, 348759, 317179, 150582, 199575, 250819, 92425, 136904, 317320, 57612, 347083, 179150, 314429, 365386, 357426},
  {332348, 391420, 303205, 7805, 252293, 366875, 22082, 251733, 358202, 219528, 185701, 198664, 22908, 378461, 225488, 82723, 316417, 67199, 269307, 116172, 212371, 11311, 156149, 132997, 263403, 3394, 310131, 383423, 6804, 148455, 201402, 279119, 211032, 165920, 275367, 147281, 198356, 149312, 377418, 56025, 296296, 15328, 176874, 317609, 111888, 4572, 128666, 160326, 90576, 267053, 368778, 121551, 73430, 148759, 88509, 264469, 242838, 196521, 243329, 134707, 318163, 159726, 322952, 119007, 89262, 96620, 374609, 21225, 70964, 45140, 187096, 342163, 356472, 78747, 122776, 1498, 248135, 254798, 73268, 211688, 228703, 30347, 230839, 62493, 365714, 109999, 127787, 64963, 334296, 354964, 304501, 312268, 373462, 114375, 83449, 349854, 6644, 1469, 35847, 365806, 267398, 147314, 255934, 210713, 240930, 154919, 255005, 229034, 385125, 260262, 41237, 174428, 169154, 304894, 391247, 34227, 62938, 105797, 115212, 342081, 74779, 153178, 372139, 136790, 272928, 108343, 269856, 238907, 73216, 209663, 240614, 345460, 182930, 148260, 164887, 42940, 201053, 63539, 100748, 128902, 72657, 88372, 20497, 172390, 2188, 122965, 98188, 216061, 357834, 123324, 251331, 127558, 375095, 7628, 35289, 173042, 98302, 198194, 36973, 223204, 54289, 103928, 272782, 111077, 183223, 107070, 389144, 332271, 101237, 153256, 385786, 212793, 272295, 180902, 20015, 61560, 313751, 368400, 300414, 296413, 236108, 49645, 72867, 333001, 110191, 147190, 378972, 111904, 158422, 169091, 305689, 138984, 164775, 249010, 234282, 6127, 101388, 269697, 33492, 240664},
  {366445, 9431, 102360, 194435, 370602, 35392, 137818, 28174, 277467, 114053, 163360, 384910, 206425, 365542, 233202, 380374, 202313, 170077, 302632, 354480, 259622, 82292, 103287, 27840, 291828, 257568, 361298, 407, 220786, 177184, 103852, 247853, 59842, 149510, 320383, 379729, 28425, 217757, 94658, 226361, 383181, 322226, 264807, 294690, 305122, 127191, 70816, 310619, 238629, 116859, 8351, 120600, 70601, 67204, 173179, 36324, 30183, 83881, 44276, 177090, 6699, 172033, 192372, 204416, 74339, 97321, 205690, 18898, 200639, 316304, 155391, 191455, 247419, 128181, 9527, 391834, 155741, 382728, 48468, 53651, 40904, 160224, 197788, 250051, 78411, 159563, 370269, 130739, 237109, 347587, 346723, 279225, 333487, 180977, 8094, 273060, 382596, 292415, 244595, 149838, 245642, 47958, 144790, 20891, 124732, 170904, 11648, 192055, 173896, 182962, 104273, 107619, 190269, 303956, 206989, 10528, 64553, 157637, 235866, 85141, 390912, 87397, 121135, 49026, 225322, 348235, 142088, 354076, 256519, 133410, 184737, 250639, 70313, 227553, 59596, 234715, 43869, 29765, 346108, 270941, 373445, 373547, 87101, 45056, 69375, 2231, 242214, 79658, 76468, 137900, 121702, 291764, 224501, 153294, 316804, 10736, 109904, 373498, 371349, 322037, 352408, 222998, 347057, 103836, 293762, 252165, 338978, 192258, 99967, 34723, 290134, 117094, 121799, 385276, 383402, 148936, 141790, 16996, 314026, 297624, 285979, 59403, 240697, 262159, 165901, 61031, 43786, 342450, 134980, 372216, 364376, 91289, 85363, 375197, 85610, 386891, 142167, 106599, 154915, 241818},
  {265940, 264766, 284175, 194595, 242495, 67912, 339536, 85537, 160980, 172083, 262095, 223433, 331228, 59785, 136876, 91730, 112750, 291792, 238042, 356868, 282509, 329793, 337198, 24207, 376138, 148550, 334369, 159090, 36984, 209751, 4788, 132146, 111312, 53712, 57299, 373297, 180130, 377408, 156033, 8772, 207225, 121789, 140328, 390923, 198258, 373630, 68786, 106335, 389914, 152279, 364804, 309850, 208291, 279910, 110033, 171870, 97196, 381379, 277061, 30455, 206234, 79661, 47489, 19772, 180177, 238695, 102489, 392106, 114522, 136972, 284065, 273888, 169057, 114111, 327007, 71095, 24407, 43964, 119395, 215165, 359824, 224983, 149562, 79465, 157385, 186261, 391254, 191449, 196256, 127823, 198535, 273798, 65944, 161158, 328071, 102268, 46774, 337196, 354016, 373886, 28724, 308352, 187279, 260341, 42059, 372176, 241602, 172645, 194115, 389950, 219951, 345958, 293596, 322472, 76892, 50462, 364990, 157873, 316447, 252668, 151979, 73454, 381331, 31066, 319926, 326338, 374673, 29968, 277409, 43301, 54767, 176165, 130548, 316060, 241623, 195653, 139643, 113553, 327336, 328, 307437, 198211, 58807, 92757, 178025, 75920, 200330, 30958, 184884, 33359, 55706, 293844, 382624, 263307, 331416, 7020, 320919, 360631, 96415, 331934, 42477, 107571, 327972, 61821, 324699, 259930, 244404, 65836, 317461, 78990, 303362, 376801, 153060, 235906, 87438, 201709, 272614, 167061, 341724, 34629, 128505, 331474, 180166, 13411, 78607, 257552, 100743, 52492, 349002, 58783, 208542, 112673, 1308, 223520, 333158, 95957, 286450, 249726, 69247, 321621},
  {3451, 300846, 125926, 178825, 69127, 92653, 47444, 113043, 147055, 31655, 4938, 349100, 298117, 382016, 381143, 190265, 152411, 103086, 189252, 295931, 10205, 324182, 357602, 154284, 181163, 42251, 244037, 307566, 311261, 372015, 330003, 339737, 185807, 388787, 303356, 254709, 32607, 28404, 354911, 334063, 250169, 219652, 345815, 129339, 90032, 382694, 53969, 98180, 78825, 230158, 169329, 124417, 307368, 226238, 59789, 293173, 13350, 8260, 126738, 245116, 44325, 152006, 23241, 309828, 41181, 41764, 148009, 365918, 375153, 66028, 333242, 347998, 372189, 124853, 118742, 312426, 230316, 192355, 185329, 31041, 196389, 227141, 271212, 287458, 332141, 101796, 53479, 265402, 66608, 385460, 283905, 24069, 384615, 178827, 98127, 61318, 137582, 230280, 376510, 381925, 348717, 142322, 88987, 290367, 20526, 84749, 134959, 369116, 273526, 165784, 126003, 265422, 155727, 235522, 235844, 140706, 52521, 249779, 386359, 112952, 109761, 240224, 275216, 182118, 323695, 96530, 197646, 338925, 338476, 59008, 180290, 13759, 306560, 34311, 228434, 230613, 304388, 378147, 270479, 373902, 366747, 158210, 197856, 116969, 354739, 281687, 383303, 240719, 333809, 294660, 246283, 242754, 361223, 71794, 86023, 309468, 296372, 230779, 243077, 316882, 109444, 128387, 138143, 288712, 104652, 243323, 383605, 194893, 139348, 224055, 134337, 391565, 351045, 150889, 390649, 59101, 349980, 10608, 289068, 261207, 310933, 226587, 352100, 244106, 37869, 262841, 195394, 238756, 240715, 190175, 305713, 220374, 250098, 267488, 366477, 59558, 4219, 171418, 138337, 328594}
};

// This is a table used by INDIRECT_READ operations.
// It contains random offsets into the server_data array.
#define INDIRECTION_TABLE_SIZE 200
uint64_t indirection_table[INDIRECTION_TABLE_SIZE] = {141529, 273922, 275090, 277348, 236684, 368095, 363998, 82072, 203798, 274912, 239592, 6712, 93402, 95133, 166654, 174169, 195902, 137244, 255257, 191083, 207155, 300652, 278078, 126313, 358155, 321822, 157553, 249500, 361206, 208947, 321447, 293173, 157322, 365675, 214119, 339954, 240305, 160975, 29729, 87187, 225964, 194947, 282205, 225071, 191361, 199316, 144529, 288844, 79971, 65565, 217237, 66121, 137024, 37061, 257215, 342519, 167909, 37289, 178812, 309115, 39920, 192375, 391732, 15311, 50998, 84607, 340495, 237718, 287088, 321315, 10701, 239390, 63068, 124462, 61349, 219243, 39111, 331721, 368368, 244907, 338757, 42372, 183092, 167376, 920, 176746, 383777, 356384, 148069, 151805, 270974, 118870, 3468, 312917, 60988, 363200, 188723, 296720, 273384, 109584, 157398, 340118, 393001, 310855, 295438, 188379, 82191, 195482, 226133, 202611, 225576, 118016, 361983, 253700, 321809, 241886, 284590, 151775, 336722, 219815, 296738, 244288, 94131, 58962, 368193, 265788, 146696, 342887, 29259, 392319, 41018, 352477, 172720, 41884, 111267, 281699, 308363, 98763, 131319, 211986, 271988, 115276, 295214, 48529, 326657, 6112, 181933, 54053, 206639, 382419, 138176, 19745, 223963, 117318, 127314, 289795, 187310, 17302, 221192, 39959, 368347, 164262, 253971, 187457, 154234, 240883, 193297, 339802, 161815, 151237, 33806, 380793, 290435, 333396, 235673, 177665, 92985, 325398, 247393, 102463, 58705, 82982, 386804, 157637, 207543, 330849, 378762, 219333, 104066, 330985, 224811, 332525, 102866, 375927, 134198, 322810, 275015, 382070, 316528, 331264};

// SETUP msg:
// - dummy_service_time
// - start_time
// - msg_type
// - addr
#define SETUP_TYPE 0
#define SETUP_MSG_LEN 8*4

// READ msg:
// - dummy_service_time
// - start_time
// - msg_type
// - addr - address to read at server
#define READ_TYPE 1
#define READ_MSG_LEN 8*4

// WRITE msg:
// - dummy_service_time
// - start_time
// - msg_type
// - addr - address to write at server
// - new_val - new value to write at addr
#define WRITE_TYPE 2
#define WRITE_MSG_LEN 8*5

// CAS msg (compare-and-swap):
// - dummy_service_time
// - start_time
// - msg_type
// - addr - address to check / write at server
// - cmp_val - value to check against. If cmp_val == *addr, then write new_val to addr
// - new_val - new value to write if condition is met
#define CAS_TYPE 3
#define CAS_MSG_LEN 8*6

// FA msg (fetch-and-add):
// - dummy_service_time
// - start_time
// - msg_type
// - addr - address to increment
// - inc_val - amount to increment by
#define FA_TYPE 4
#define FA_MSG_LEN 8*5

// INDIRECT_READ msg:
// - dummy_service_time
// - start_time
// - msg_type
// - addr - address that contains pointer to value we want to read at the server
#define INDIRECT_READ_TYPE 5
#define INDIRECT_READ_MSG_LEN 8*4

// RESP msg:
// - dummy_service_time
// - start_time
// - msg_type
// - result - result of performing the requested operation
#define RESP_TYPE 6
#define RESP_MSG_LEN 8*4

// IP addr's are assigned by firesim starting at 10.0.0.2.
// Server will be the first one and client will be the second.
uint64_t server_ip = 0x0a000002;
uint64_t client_ip = 0x0a000003; // use this for 2 host config
//uint64_t client_ip = 0x0a000001; // use this for load generator config

// This is the server data that is accessed by the client.
uint64_t server_data[MEM_SIZE];

int get_test_name(int msg_type, char *test_name) {
  if (msg_type == READ_TYPE) {
    strcpy(test_name, "READ");
  } else if (msg_type == WRITE_TYPE) {
    strcpy(test_name, "WRITE");
  } else if (msg_type == CAS_TYPE) {
    strcpy(test_name, "CAS");
  } else if (msg_type == FA_TYPE) {
    strcpy(test_name, "FA");
  } else if (msg_type == INDIRECT_READ_TYPE) {
    strcpy(test_name, "INDIRECT_READ");
  }
  return 0;
}

// Check if the provided address is assigned to an active node
// (i.e. either the server or the client)
bool is_active_ip(uint32_t addr) {
  if (addr == server_ip || addr == client_ip) {
    return true;
  }
  return false;
}

int check_app_hdr(uint64_t app_hdr, uint64_t exp_src_ip,
    uint64_t exp_src_context, uint16_t exp_msg_len) {
  // Check src IP
  uint64_t src_ip = (app_hdr & IP_MASK) >> 32;
  if (src_ip != exp_src_ip) {
      printf("ERROR: unexpected src_ip: %lx\n", src_ip);
      return -1;
  }
  // Check src context
  uint64_t src_context = (app_hdr & CONTEXT_MASK) >> 16;
  if (src_context != exp_src_context) {
      printf("ERROR: unexpected src_context: %ld\n", src_context);
      return -1;
  }
  // Check msg length
  uint16_t msg_len = app_hdr & LEN_MASK;
  if (msg_len != exp_msg_len) {
      printf("ERROR: unexpected msg_len: %d\n", msg_len);
      return -1;
  }
  return 0; 
}

void send_request(uint64_t send_time, uint64_t addr, uint64_t msg_type) {
  uint64_t app_hdr;
  uint64_t dst_ip = server_ip;

  if (msg_type == READ_TYPE) {
    app_hdr = (dst_ip << 32) | READ_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_i(1); // dummy_service_time
    lnic_write_r(send_time);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
  } else if (msg_type == WRITE_TYPE) {
    app_hdr = (dst_ip << 32) | WRITE_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_i(1); // dummy_service_time
    lnic_write_r(send_time);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
    lnic_write_r(addr); // new_val
  } else if (msg_type == CAS_TYPE) {
    app_hdr = (dst_ip << 32) | CAS_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_i(1); // dummy_service_time
    lnic_write_r(send_time);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
    lnic_write_i(0); // cmp_val
    lnic_write_r(addr); // new_val
  } else if (msg_type == FA_TYPE) {
    app_hdr = (dst_ip << 32) | FA_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_i(1); // dummy_service_time
    lnic_write_r(send_time);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
    lnic_write_i(0); // inc_val
  } else if (msg_type == INDIRECT_READ_TYPE) {
    app_hdr = (dst_ip << 32) | INDIRECT_READ_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_i(1); // dummy_service_time
    lnic_write_r(send_time);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
  } else {
    printf("ERROR: invalid msg_type: %ld\n", msg_type);
  }
}

/* Client:
 * - First, wait for a message from the server which includes
 *   the address of an in-memory object that the client is allowed to
 *   operate on using RDMA ops.
 * - Send N requests to the server (one at a time) and record the
 *   end-to-end completion time.
 * - Print all completion times.
 */
int run_client() {
  uint64_t app_hdr;
  uint64_t start, end;
  int i, n;
  uint64_t msg_type;
  uint64_t addr; // base address of RDMA accessible memory on the server 
  uint64_t target_addr; // addr of data on server to perform RDMA ops on
  uint64_t latencies[NUM_TESTS][NUM_REQUESTS];
  // collect all measurements and printf with this one buffer
  char output_buf[NUM_TESTS][(NUM_REQUESTS + 1)*10];

  printf("Client booted!\n");
  // Wait to receive SETUP msg from server, which contains address
  // of in-memory object that can be operated upon.
  lnic_wait();
  app_hdr = lnic_read();
  check_app_hdr(app_hdr, server_ip, 0, SETUP_MSG_LEN);
  lnic_read(); // dummy_service_time
  lnic_read(); // send_time
  msg_type = lnic_read();
  if (msg_type != SETUP_TYPE) {
    printf("ERROR: received unexpected msg_type: %ld\n", msg_type);
    return -1;
  }
  addr = lnic_read();
  lnic_msg_done();
  printf("Client received SETUP msg w/ addr: 0x%lx\n", addr);

  // Run all the tests
  for (n = 0; n < NUM_TESTS; n++) {
    // Send N requests to the server and record end-to-end response time
    for (i = 0; i < NUM_REQUESTS; i++) {
      // compute the target address we want to access
      target_addr = addr + 8*(random_vals[n][i] % MEM_SIZE);
      msg_type = n + 1;
      start = rdcycle(); // start the clock
      send_request(start, target_addr, msg_type);
      // wait for response
      lnic_wait();
      end = rdcycle();   // stop the clock
      // process response
      app_hdr = lnic_read();
      check_app_hdr(app_hdr, server_ip, 0, RESP_MSG_LEN);
      lnic_read(); // dummy_service_time
      lnic_read(); // send_time
      lnic_read(); // msg_type
      lnic_read(); // result
      lnic_msg_done();
      // record latency
      latencies[n][i] = end - start;
    }
  }
  printf("Measurements complete!\n");

  // Print latency measurements
  for (n = 0; n < NUM_TESTS; n++) {
    uint32_t len_written;
    char test_name[32];
    get_test_name(n+1, test_name);
    len_written = sprintf(output_buf[n], "&&CSV&&,%s", test_name);
    for (i = 0; i < NUM_REQUESTS; i++) {
      len_written += sprintf(output_buf[n] + len_written, ",%ld", latencies[n][i]);
    }
    len_written += sprintf(output_buf[n] + len_written, "\n");
    printf("%s", output_buf[n]);
  }
  printf("Client complete!\n");
  return 0;
}

/* Server:
 * - First, initialize in-memory object and share ptr with client.
 * - Loop forever, serving RDMA requests.
 */ 
int run_server() {
  uint64_t app_hdr;
  uint64_t msg_type;
  uint64_t *addr;
  int i;
  uint64_t dummy_service_time;
  uint64_t send_time;

  // Send SETUP msg with a ptr to data
  printf("Server sending SETUP msg!\n");
  app_hdr = (client_ip << 32) | SETUP_MSG_LEN;
  lnic_write_r(app_hdr);
  lnic_write_i(1); // dummy_service_time
  lnic_write_i(0); // send_time
  lnic_write_i(SETUP_TYPE);
  lnic_write_r(server_data);

  // Loop forever processing client requests.
  while (1) {
    lnic_wait();
    app_hdr = lnic_read();
    dummy_service_time = lnic_read();
    if (dummy_service_time == 0) exit(0); // the experiment is complete, shutdown the whole system
    send_time = lnic_read();
    msg_type = lnic_read();
    addr = (uint64_t *)lnic_read();
    // check that the provided address is valid
    if ( (addr < server_data) || (addr > &(server_data[MEM_SIZE-1])) ) {
      printf("ERROR: server received request with invalid addr: %lx\n", addr);
      return -1;
    }
    // write the start of the response msg
    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | RESP_MSG_LEN);
    lnic_write_r(dummy_service_time);
    lnic_write_r(send_time);
    lnic_write_r(RESP_TYPE);
    // perform the requested operation and finish writing the response
    if (msg_type == READ_TYPE) {
      lnic_write_r(*addr);
    } else if (msg_type == WRITE_TYPE) {
      *addr = lnic_read();
      lnic_write_r(*addr);
    } else if (msg_type == CAS_TYPE) {
      // conditionally update addr
      if (*addr == lnic_read()) {
        *addr = lnic_read();
      } else {
         lnic_read();
      }
      lnic_write_r(*addr);
    } else if (msg_type == FA_TYPE) {
      *addr += lnic_read();
      lnic_write_r(*addr);
    } else if (msg_type == INDIRECT_READ_TYPE) {
      // addr is actually used to index an indirection table which contain offsets into the server_data array
      uint64_t table_idx = ((uint64_t)addr) % INDIRECTION_TABLE_SIZE;
      uint64_t data_idx = indirection_table[table_idx] % MEM_SIZE;
      lnic_write_r(server_data[data_idx]);
    } else {
      printf("ERROR: server received invalid msg_type: %ld\n", msg_type);
    }
    lnic_msg_done();
  }

  return 0;
}

// Only use core 0, context 0
int main(uint64_t argc, char** argv) {
  uint64_t context_id = 0;
  uint64_t priority = 0;

  if (argc < 3) {
    printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
    return -1;
  }

  char* nic_ip_str = argv[2];
  uint32_t nic_ip_addr_lendian = 0;
  int retval = inet_pton4(nic_ip_str, nic_ip_str + strlen(nic_ip_str), &nic_ip_addr_lendian);

  // Risc-v is little-endian, but we store ip's as big-endian since the NIC works in big-endian
  uint32_t nic_ip_addr = swap32(nic_ip_addr_lendian);
  if (retval != 1 || nic_ip_addr == 0) {
      printf("Supplied NIC IP address is invalid.\n");
      return -1;
  }
  // Non-active nodes should just spin for the duration of the simulation
  if (!is_active_ip(nic_ip_addr)) {
    while(1);
  }

  lnic_add_context(context_id, priority);

  int ret = 0;
  if (nic_ip_addr == server_ip) {
    ret = run_server();
  } else if (nic_ip_addr == client_ip) {
    ret = run_client();
  }
 
  return ret;
}

