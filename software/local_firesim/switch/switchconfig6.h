// THIS FILE IS MACHINE GENERATED. SEE deploy/buildtools/switchmodelconfig.py
        
    #ifdef NUMCLIENTSCONFIG
    #define NUMPORTS 6
    #define NUMDOWNLINKS 6
    #define NUMUPLINKS 0
    #endif
    #ifdef PORTSETUPCONFIG
    ports[0] = new ShmemPort(0, "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002", false);
    ports[1] = new ShmemPort(0, "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003", false);
    ports[2] = new ShmemPort(0, "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004", false);
    ports[3] = new ShmemPort(0, "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005", false);
    ports[4] = new ShmemPort(0, "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006", false);
    ports[5] = new ShmemPort(0, "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007", false);
    
    #endif
    
    #ifdef MACPORTSCONFIG
    uint16_t mac2port[9]  {0, 0, 0, 1, 2, 3, 4, 5, 0};
    #define NUMIPSKNOWN 8
    #endif
    
