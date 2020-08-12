#include "../software/ray-tracing/riscv-tracer/src/frontend/riscv-ray-library.h"

int main(int argc, char** argv) {
    printf("Starting ray tracer\n");
    //return 0;
    return ray_main(argc, argv);
}