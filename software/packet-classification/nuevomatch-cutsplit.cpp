/*
 * MIT License
 * Copyright (c) 2019 Alon Rashelbach
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <list>
#include <vector>

#include <logging.h>
#include <object_io.h>
#include <cut_split.h>
#include <tuple_merge.h>
#include <serial_nuevomatch.h>
#include <nuevomatch_config.h>
#if 1
// ClassBench acl1_seed_100k workload from NuevoMatch:
//#include "nuevomatch_64_classifier.h"
//#include "nuevomatch_64_classifier_1krules.h"
#include "./nuevomatch/tools/nuevomatch_64_classifier_100rules.h"
#else
char nuevomatch_64_classifier[] = {0};
#endif

#include "lnic.h"

// Expected address of the load generator
uint64_t load_gen_ip = 0x0a000001;

extern "C" {
    extern int _end;
    char* data_end = (char*)&_end + 16384*4;
    extern void sbrk_init(long int* init_addr);
    extern void __libc_init_array();
    extern void __libc_fini_array();
}

void send_startup_msg(int cid, uint64_t context_id) {
  uint64_t app_hdr = (load_gen_ip << 32) | (0 << 16) | (2*8);
  uint64_t cid_to_send = cid;
  lnic_write_r(app_hdr);
  lnic_write_r(cid_to_send);
  lnic_write_r(context_id);
}


int nuevomatch_server(int cid, int context_id) {

	// Set configuration for NuevoMatch
	NuevoMatchConfig config;
	config.num_of_cores = 1;
	config.max_subsets = 1;
	config.start_from_iset = 0;
	config.disable_isets = false;
	config.disable_remainder = false;
	config.disable_bin_search = false;
	config.disable_validation_phase = false;
	config.disable_all_classification = false;
	//config.force_rebuilding_remainder = true;
	config.force_rebuilding_remainder = false;
  config.arbitrary_subset_clore_allocation = nullptr;

#if 1
	const char* remainder_type = "cutsplit";
	uint32_t binth = 8;
	uint32_t threshold = 25;
  config.remainder_classifier = new CutSplit(binth, threshold);
#else
	const char* remainder_type = "tuplemerge";
  config.remainder_classifier = new TupleMerge();
#endif
	config.remainder_type = remainder_type;

  SerialNuevoMatch<1>* classifier = new SerialNuevoMatch<1>(config);

	ObjectReader classifier_handler(nuevomatch_64_classifier, sizeof(nuevomatch_64_classifier));

	// Load nuevomatch
	// This will work for both classifiers without remainder classifier set
	// and classifiers with remainder classifiers set
	classifier->load(classifier_handler);

  send_startup_msg(cid, context_id);

  printf("Packet classification server ready.\n");

  uint64_t app_hdr, sent_time, service_time, class_meta;
#define HEADER_WORDS 3
  uint64_t headers[HEADER_WORDS];
  while (1) {
    lnic_wait();
    app_hdr = lnic_read();
    //printf("[%d] --> Received msg of length: %u bytes\n", cid, (uint16_t)app_hdr);
    service_time = lnic_read();
    sent_time = lnic_read();
    class_meta = lnic_read();

    //for (int i = 0; i < HEADER_WORDS; i++) headers[i] = lnic_read();
    headers[0] = lnic_read();
#if HEADER_WORDS > 1
    headers[1] = lnic_read();
#endif
#if HEADER_WORDS > 2
    headers[2] = lnic_read();
#endif
#if HEADER_WORDS > 3
#error "Add more unrolled iterations for both lread() and lwrite()"
#endif

    classifier_output_t out = classifier->classify((uint32_t*)headers);
    int action = out.action;
    //int action = config.remainder_classifier->classify_sync((uint32_t*)headers, -1);
    uint32_t trace_idx = class_meta >> 32;
    class_meta = ((uint64_t)trace_idx << 32) | ((uint32_t)action);

    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | (8 + 8 + 8 + (HEADER_WORDS * 8)));
    lnic_write_r(service_time);
    lnic_write_r(sent_time);
    lnic_write_r(class_meta);
    //for (int i = 0; i < HEADER_WORDS; i++) lnic_write_r(headers[i]);
    lnic_write_r(headers[0]);
#if HEADER_WORDS > 1
    lnic_write_r(headers[1]);
#endif
#if HEADER_WORDS > 2
    lnic_write_r(headers[2]);
#endif
    lnic_msg_done();
	}

 	delete classifier;
}

extern "C" {
bool is_single_core() { return false; }
int core_main(int argc, char** argv, int cid, int nc) {
  (void)nc;

  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  if (cid > 0) {
    send_startup_msg(cid, context_id);
    return EXIT_SUCCESS;
  }

  // Setup the C++ libraries
  sbrk_init((long int*)data_end);
  atexit(__libc_fini_array);
  __libc_init_array();

  return nuevomatch_server(cid, context_id);
}
}
