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
    char* test_type = "ONE_CONTEXT_FOUR_CORES";
    char* dist_type = "FIXED";
    uint64_t request_rate_lambda_inverse = 5000;
    uint64_t min_service_time = 300;
    uint64_t max_service_time = 10000;
    double exp_dist_scale_factor = 10000;
    double exp_dist_decay_const = 3.5;
    double bimodal_dist_high_mean = 5000;
    double bimodal_dist_high_stdev = 300;
    double bimodal_dist_low_mean = 500;
    double bimodal_dist_low_stdev = 30;
    double bimodal_dist_fraction_high = 0.5;
    uint64_t fixed_dist_cycles = 800;
    uint16_t rtt_pkts = 2;
    #endif

