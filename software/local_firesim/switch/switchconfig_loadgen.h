// THIS FILE IS MACHINE GENERATED. SEE deploy/buildtools/switchmodelconfig.py
        
    #ifdef NUMCLIENTSCONFIG
    #define NUMPORTS 1
    #define NUMDOWNLINKS 1
    #define NUMUPLINKS 0
    #endif
    #ifdef PORTSETUPCONFIG
    ports[0] = new ShmemPort(0, "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002", false);

    #endif
    
    #ifdef MACPORTSCONFIG
    uint16_t mac2port[4]  {0, 0, 0, 0};
    #define NUMIPSKNOWN 3
    #endif

    #ifdef LOADGENSTATS
    #define USE_LOAD_GEN
    // Load balancing tests
//    char* test_type = "ONE_CONTEXT_FOUR_CORES";
//    char* test_type = "FOUR_CONTEXTS_FOUR_CORES";

    // Thread scheduling tests
    char* test_type = "DIF_PRIORITY_LNIC_DRIVEN";
//    char* test_type = "DIF_PRIORITY_TIMER_DRIVEN";

    // Bounded processing time tests
//    char* test_type = "HIGH_PRIORITY_C1_STALL";
//    char* test_type = "LOW_PRIORITY_C1_STALL";

    // Application to generate load for
    char* load_type = "DEFAULT";

    // Distribution parameters
//    char* dist_type = "BIMODAL";
    char* service_dist_type = "FIXED";
    char* request_dist_type = "EXP";
    uint64_t num_requests = 10;
    uint64_t request_rate_lambda_inverse_start = 3200;
    uint64_t request_rate_lambda_inverse_stop = 3200;
    uint64_t request_rate_lambda_inverse_dec = 0;
    uint64_t min_service_time = 300;
    uint64_t max_service_time = 10000;
    uint64_t min_service_key = 0;
    uint64_t max_service_key = 0;
    double exp_dist_scale_factor = 10000;
    double exp_dist_decay_const = 3.5;
    double bimodal_dist_high_mean = 9000;
    double bimodal_dist_high_stdev = 100;
    double bimodal_dist_low_mean = 1600;
    double bimodal_dist_low_stdev = 100;
    double bimodal_dist_fraction_high = 0.2;
    uint64_t fixed_dist_cycles = 1600;
    uint16_t rtt_pkts = 2;
    #endif

