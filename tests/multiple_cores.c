#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define NCORES 2

int core_complete[NCORES];

void core0_main() {
    int j = 1;
    for (int i = 0; i < 10000; i++) {
        j += 2;
    }
    printf("Hello from core 0 main at %d\n", j);
}

void core1_main() {
    for (int i = 0; i < 3; i++)
    printf("Hello from core 1 main at %d\n", i);
}

void notify_core_complete(int cid) {
    core_complete[cid] = true;
}

void wait_for_all_cores() {
    bool all_cores_complete = true;
    do {
        all_cores_complete = true;
        for (int i = 0; i < NCORES; i++) {
            if (!core_complete[i]) {
                all_cores_complete = false;
            }
        }
     } while (!all_cores_complete);
}

void thread_entry(int cid, int nc) {
    if (nc != 2 || NCORES != 2) {
        if (cid == 0) {
            printf("This program requires 2 cores but was given %d\n", nc);
        }
        return;
    }

    if (cid == 0) {
        core0_main();
        notify_core_complete(0);
        wait_for_all_cores();
        exit(0);
    } else {
        // cid == 1
        core1_main();
        notify_core_complete(1);
        wait_for_all_cores();
        exit(0);
    }
}
