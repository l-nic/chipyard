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
#define MEM_SIZE 524288

// one test for each RDMA operation type
#define NUM_TESTS 5

// To avoid using a random number generator we'll encode these random numbers in the binary
// These are uniform random values between 0 and MEM_SIZE-1
uint32_t random_vals[5][NUM_REQUESTS] = {
  {357174, 384605, 436363, 86318, 511772, 338466, 223607, 112800, 226774, 70068, 69259, 454428, 459541, 259667, 285313, 235384, 442215, 151387, 176298, 384352, 516085, 374114, 85204, 69663, 154083, 102378, 398215, 194726, 422221, 166245, 21415, 14318, 160267, 199988, 102602, 24755, 2029, 63994, 469914, 115282, 31626, 452099, 447166, 273012, 372278, 354999, 472853, 341864, 256219, 157887, 471028, 399836, 504139, 425648, 8608, 5518, 59592, 404081, 183975, 212570, 302812, 475724, 262342, 393117, 197146, 490638, 361095, 38896, 283638, 302200, 267011, 280232, 311364, 282182, 86502, 207715, 222921, 467415, 292863, 9626, 121670, 258445, 375351, 104610, 7630, 236133, 294182, 96257, 51218, 7083, 332973, 461079, 254339, 136534, 59111, 233610, 518714, 153211, 519200, 383172, 521329, 309244, 109336, 280983, 350608, 394958, 467644, 177138, 60355, 231670, 94131, 100178, 191259, 280040, 307078, 267161, 373546, 503814, 517787, 74132, 293058, 212614, 147708, 154399, 466765, 377087, 85484, 260280, 210879, 235502, 47720, 110137, 355580, 39644, 321253, 360559, 137533, 402813, 467342, 89290, 156954, 239345, 24092, 89512, 231071, 326309, 191809, 9921, 405684, 100838, 177858, 489741, 40653, 493438, 487797, 160043, 83051, 306913, 173975, 174265, 29272, 258647, 113896, 118034, 341871, 496368, 25824, 330416, 211647, 243822, 493393, 423125, 107160, 255075, 375928, 242545, 83321, 310885, 177106, 49589, 18414, 370614, 439231, 28093, 164527, 367215, 185181, 9969, 179975, 180514, 376560, 83423, 27863, 326888, 171638, 456179, 431713, 340868, 4981, 19135}, 
  {522441, 463435, 297808, 258282, 386915, 99016, 388788, 202910, 277714, 162519, 170075, 343957, 201882, 463912, 363078, 72198, 448139, 318849, 327030, 420739, 282594, 167385, 70579, 60818, 274812, 204234, 315706, 232445, 181569, 148006, 461025, 293604, 243383, 376999, 468888, 60473, 492305, 224606, 331638, 321341, 516926, 65642, 453814, 248440, 376366, 281319, 335378, 154094, 486116, 237045, 32107, 190969, 456070, 308683, 516715, 193949, 457198, 341262, 40716, 272638, 122834, 212606, 279662, 459866, 44939, 295964, 233380, 106536, 5410, 5364, 46735, 244706, 158754, 421041, 364857, 351155, 213700, 298788, 499685, 80315, 480700, 94301, 136344, 29627, 500424, 372394, 353613, 295124, 395340, 415582, 117805, 127890, 131474, 318362, 113635, 248719, 378087, 113268, 377530, 150940, 312335, 61739, 52781, 326707, 159532, 485718, 255107, 247774, 33310, 291479, 244075, 182024, 507262, 16693, 315251, 358050, 308065, 93889, 225312, 182362, 81120, 252289, 65768, 502641, 25659, 225697, 311470, 287172, 182417, 404042, 86283, 371479, 385723, 225884, 240577, 236521, 18234, 142126, 8259, 155363, 23266, 455661, 468359, 253919, 452426, 229777, 128483, 377669, 519206, 32290, 422044, 204128, 416353, 448089, 182139, 220067, 521019, 113141, 325177, 208923, 119903, 420330, 98848, 27036, 451930, 330552, 293631, 235538, 14734, 335217, 167620, 134685, 470366, 43012, 411022, 334645, 31465, 227229, 58678, 23506, 304311, 372810, 460554, 258598, 149453, 520960, 131735, 369467, 357800, 225612, 75524, 425694, 41111, 152252, 447178, 422065, 333515, 17115, 191827, 153203},
  {459298, 47522, 420869, 100426, 52921, 21014, 405373, 157523, 108459, 228448, 495652, 46602, 165478, 428368, 385074, 428845, 271322, 333953, 291682, 72199, 465427, 315679, 56838, 150494, 23482, 483352, 220425, 361648, 487185, 376967, 271321, 478925, 12632, 226589, 393638, 237696, 2197, 417106, 449797, 427516, 85787, 56404, 249677, 27202, 293466, 446960, 117741, 42115, 122207, 392209, 306792, 443772, 192450, 259563, 321913, 127615, 336132, 249443, 286640, 115328, 26413, 278511, 154743, 505397, 159183, 36865, 242317, 182985, 246584, 54118, 381887, 373525, 339950, 244786, 467555, 158102, 365906, 240952, 87048, 82877, 2961, 299718, 500512, 376511, 82543, 485505, 183109, 183031, 229993, 312416, 358359, 392994, 40209, 23665, 166122, 384783, 522327, 476746, 466139, 341563, 518896, 373627, 386881, 294103, 342757, 102701, 335361, 514217, 322910, 487261, 197435, 142998, 404037, 27419, 74165, 61479, 235856, 137492, 517365, 299450, 131745, 490631, 345938, 45053, 264754, 394032, 315637, 179079, 103236, 198986, 65978, 425902, 410242, 480002, 490330, 139078, 379558, 132875, 150471, 149726, 325236, 500432, 477123, 2536, 334319, 88789, 269190, 294838, 331123, 208664, 135533, 159481, 100940, 35705, 283676, 23579, 275114, 227714, 403531, 98780, 238858, 340479, 455498, 277868, 330932, 372599, 19739, 322734, 432218, 89505, 474944, 272562, 21943, 266104, 519960, 135230, 65629, 348622, 396641, 25549, 54826, 325173, 144246, 192705, 123892, 137237, 12928, 115733, 491755, 405263, 269393, 336234, 262729, 354327, 29353, 236368, 494001, 266144, 36721, 330367},
  {1581, 48022, 301156, 358940, 149883, 51443, 205897, 252431, 63867, 185963, 168049, 456036, 462564, 130635, 150094, 523654, 492371, 393531, 236183, 398583, 141559, 122711, 447274, 240933, 153904, 5615, 311743, 21334, 297821, 43849, 142464, 137386, 152983, 7906, 189550, 429857, 299125, 406632, 260904, 127717, 304448, 485430, 161562, 394007, 238926, 7964, 113598, 311149, 96154, 469918, 437590, 173571, 118322, 185124, 360855, 472543, 318976, 189158, 282269, 259215, 293412, 29139, 222882, 86795, 249383, 119345, 24432, 98306, 101905, 427837, 380553, 127481, 487101, 411665, 61476, 215425, 391493, 232981, 332467, 359879, 107785, 113575, 14429, 185294, 78932, 430893, 142277, 264528, 129683, 266609, 49392, 150670, 228051, 28785, 381902, 315428, 487942, 238579, 146878, 244337, 291386, 453562, 114904, 212210, 296074, 99345, 97559, 289633, 131002, 128386, 376349, 273832, 148156, 245048, 329353, 401660, 340627, 41787, 37493, 515402, 95541, 420057, 523772, 210774, 373457, 66364, 73495, 522808, 163549, 338242, 414407, 483917, 466577, 171178, 60168, 468188, 334031, 472605, 245452, 313159, 63198, 211251, 423871, 342357, 413486, 367712, 483431, 171308, 489314, 391827, 155829, 341715, 167418, 457937, 61151, 44203, 61647, 447772, 462537, 111136, 78506, 240734, 266945, 373768, 218939, 387283, 474285, 413575, 242672, 12618, 377689, 123205, 394711, 409046, 292907, 383706, 259885, 161574, 252585, 191772, 489182, 167920, 68249, 227435, 292728, 125798, 6498, 104925, 287223, 191279, 293996, 176160, 382264, 229249, 53918, 78507, 205265, 282634, 340286, 496842},
  {49830, 386926, 96671, 260490, 511763, 216705, 168782, 154739, 371500, 228297, 460120, 272398, 201325, 481431, 48176, 405442, 3420, 489700, 431970, 325551, 44925, 328453, 203533, 19418, 361008, 329776, 148286, 40105, 347807, 83480, 282585, 402100, 69696, 481488, 20162, 347780, 189317, 16016, 440191, 94239, 502932, 41355, 488132, 99392, 340539, 330286, 474797, 349791, 108121, 427994, 260974, 459257, 484647, 91166, 116343, 10463, 348973, 429093, 473523, 466610, 221171, 140635, 196729, 439434, 97917, 439456, 375788, 165778, 517925, 439119, 77905, 314744, 103611, 512783, 505635, 221081, 232355, 369114, 33288, 98513, 368172, 315476, 349470, 77085, 505242, 45591, 344668, 318038, 408413, 509214, 355046, 238995, 60532, 368790, 128592, 516228, 520185, 260368, 308888, 449582, 214611, 494704, 88611, 216831, 158282, 206568, 449209, 190331, 246997, 64592, 195023, 62611, 487420, 266293, 122915, 115901, 310343, 396531, 415546, 87374, 183466, 302062, 180486, 401363, 7147, 482316, 346086, 389175, 474799, 126494, 399640, 26642, 391849, 178281, 290089, 516888, 184987, 353153, 31419, 403039, 11940, 332337, 385898, 204086, 45469, 180331, 172893, 140781, 152948, 412397, 290472, 38804, 254650, 250489, 209726, 348282, 176852, 451435, 76404, 370577, 112297, 142306, 383246, 209836, 331068, 149747, 121081, 158856, 85251, 128106, 421222, 310367, 121791, 376258, 465893, 152177, 270677, 48713, 386945, 366705, 97617, 135513, 61149, 196132, 262890, 342041, 154761, 96038, 113568, 231345, 199905, 310775, 514567, 247341, 16207, 269838, 69397, 299170, 494159, 439947}
};

// This is a table used by INDIRECT_READ operations.
// It contains random offsets into the server_data array.
#define INDIRECTION_TABLE_SIZE 200
uint64_t indirection_table[INDIRECTION_TABLE_SIZE] = {271657, 149071, 502036, 334045, 37708, 152589, 385458, 139038, 70718, 241136, 441198, 71203, 160915, 21539, 3564, 404278, 207502, 40867, 476825, 58053, 47737, 300299, 92247, 207413, 511786, 315783, 162523, 171596, 22784, 473457, 340351, 377987, 432266, 489760, 77499, 198952, 126947, 477246, 426051, 96485, 204762, 78656, 24960, 407249, 284357, 504837, 153013, 36699, 373389, 350554, 412955, 502004, 422739, 490531, 468799, 484261, 239641, 76415, 38929, 256503, 397322, 359552, 2592, 426154, 246287, 73091, 132085, 278654, 227873, 374657, 356313, 143069, 26961, 404939, 135637, 497522, 405522, 476429, 349172, 449962, 391069, 229829, 261595, 388958, 444320, 23617, 200814, 391837, 97953, 122998, 179851, 458410, 14798, 262977, 485114, 266824, 462259, 127075, 432595, 315848, 42790, 489071, 292205, 75675, 392513, 353559, 324329, 235176, 200811, 76978, 82408, 277006, 155777, 169046, 493106, 130986, 264104, 146493, 228510, 175737, 266300, 444827, 176331, 66944, 251128, 271422, 240790, 433267, 481367, 89219, 207303, 401390, 280708, 306601, 466589, 400147, 516705, 20008, 88744, 260669, 369774, 76620, 221121, 80748, 51399, 404090, 49669, 308323, 40177, 291662, 90193, 72814, 426768, 341488, 381814, 260192, 510133, 330624, 345110, 474036, 381574, 7845, 22532, 469373, 136904, 8855, 112178, 207621, 184665, 356959, 455374, 375364, 125691, 204618, 251651, 479640, 14640, 35686, 83552, 363891, 361322, 306813, 177738, 339737, 390580, 70730, 276448, 187114, 456632, 321388, 76541, 183697, 234889, 389632, 386981, 337121, 418349, 38479, 240399, 436447};

// SETUP msg:
// - msg_type
// - addr
#define SETUP_TYPE 0
#define SETUP_MSG_LEN 8*2

// READ msg:
// - msg_type
// - addr - address to read at server
#define READ_TYPE 1
#define READ_MSG_LEN 8*2

// WRITE msg:
// - msg_type
// - addr - address to write at server
// - new_val - new value to write at addr
#define WRITE_TYPE 2
#define WRITE_MSG_LEN 8*3

// CAS msg (compare-and-swap):
// - msg_type
// - addr - address to check / write at server
// - cmp_val - value to check against. If cmp_val == *addr, then write new_val to addr
// - new_val - new value to write if condition is met
#define CAS_TYPE 3
#define CAS_MSG_LEN 8*4

// FA msg (fetch-and-add):
// - msg_type
// - addr - address to increment
// - inc_val - amount to increment by
#define FA_TYPE 4
#define FA_MSG_LEN 8*3

// INDIRECT_READ msg:
// - msg_type
// - addr - address that contains pointer to value we want to read at the server
#define INDIRECT_READ_TYPE 5
#define INDIRECT_READ_MSG_LEN 8*2

// RESP msg:
// - msg_type
// - result - result of performing the requested operation
#define RESP_TYPE 6
#define RESP_MSG_LEN 8*2

// IP addr's are assigned by firesim starting at 10.0.0.2.
// Server will be the first one and client will be the second.
uint64_t server_ip = 0x0a000002;
uint64_t client_ip = 0x0a000003;

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

void send_request(uint64_t addr, uint64_t msg_type) {
  uint64_t app_hdr;
  uint64_t dst_ip = server_ip;

  if (msg_type == READ_TYPE) {
    app_hdr = (dst_ip << 32) | READ_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
  } else if (msg_type == WRITE_TYPE) {
    app_hdr = (dst_ip << 32) | WRITE_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
    lnic_write_r(addr); // new_val
  } else if (msg_type == CAS_TYPE) {
    app_hdr = (dst_ip << 32) | CAS_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
    lnic_write_i(0); // cmp_val
    lnic_write_r(addr); // new_val
  } else if (msg_type == FA_TYPE) {
    app_hdr = (dst_ip << 32) | FA_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
    lnic_write_i(0); // inc_val
  } else if (msg_type == INDIRECT_READ_TYPE) {
    app_hdr = (dst_ip << 32) | INDIRECT_READ_MSG_LEN;
    lnic_write_r(app_hdr);
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

  // Wait to receive SETUP msg from server, which contains address
  // of in-memory object that can be operated upon.
  lnic_wait();
  app_hdr = lnic_read();
  check_app_hdr(app_hdr, server_ip, 0, SETUP_MSG_LEN);
  msg_type = lnic_read();
  if (msg_type != SETUP_TYPE) {
    printf("ERROR: received unexpected msg_type: %ld\n", msg_type);
    return -1;
  }
  addr = lnic_read();

  // Run all the tests
  for (n = 0; n < NUM_TESTS; n++) {
    // Send N requests to the server and record end-to-end response time
    for (i = 0; i < NUM_REQUESTS; i++) {
      // compute the target address we want to access
      target_addr = addr + 8*(random_vals[n][i] % MEM_SIZE);
      msg_type = n + 1;
      start = rdcycle(); // start the clock
      send_request(target_addr, msg_type);
      // wait for response
      lnic_wait();
      end = rdcycle();   // stop the clock
      // process response
      app_hdr = lnic_read();
      check_app_hdr(app_hdr, server_ip, 0, RESP_MSG_LEN);
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

  // Initialize the data that we will let the client manipulate with RDMA ops.
//  for (i = 0 ; i < MEM_SIZE; i++) {
//    server_data[i] = (uint64_t)(&(server_data[i]));
//  }

  // Send SETUP msg with a ptr to data
  printf("Server sending SETUP msg!\n");
  app_hdr = (client_ip << 32) | SETUP_MSG_LEN;
  lnic_write_r(app_hdr);
  lnic_write_i(SETUP_TYPE);
  lnic_write_r(server_data);

  // Loop forever processing client requests.
  while (1) {
    lnic_wait();
    app_hdr = lnic_read();
    msg_type = lnic_read();
    addr = (uint64_t *)lnic_read();
    // check that the provided address is valid
    if ( (addr < server_data) || (addr > &(server_data[MEM_SIZE-1])) ) {
      printf("ERROR: server received request with invalid addr: %lx\n", addr);
      return -1;
    }
    // write the start of the response msg
    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | RESP_MSG_LEN);
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

  if (argc != 3) {
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

