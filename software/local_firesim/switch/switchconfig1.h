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

