#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int core0_main(uint64_t argc, char** argv) {
    int j = 1;
    for (int i = 0; i < 10000; i++) {
        j += 2;
    }
    printf("Hello from core 0 main at %d\n", j);
    return 0;
}

int core1_main(uint64_t argc, char** argv) {
    for (int i = 0; i < 3; i++)
    printf("Hello from core 1 main at %d\n", i);
    return 0;
}
