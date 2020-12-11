CXX=riscv64-unknown-elf-g++
CXXFLAGS=-std=c++11 -Wall -fPIC
CXXFLAGS+=-mcmodel=medany -I. -O2 -ffixed-x30 -ffixed-x31 -fno-common -fno-builtin-printf -fno-use-cxa-atexit
CXXFLAGS+=-I ../../tests
LDFLAGS=-T ../../tests/link.ld -static -nostartfiles -lgcc -nostdlib

.SUFFIXES:

lnic-euclidean-dist.riscv: lnic-euclidean-dist.cc ../../tests/crt.o ../../tests/syscalls.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f lnic-euclidean-dist.riscv

all: lnic-euclidean-dist.riscv
