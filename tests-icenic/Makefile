GCC=riscv64-unknown-elf-gcc
OBJDUMP=riscv64-unknown-elf-objdump
CFLAGS=-mcmodel=medany -std=gnu99 -O2 -fno-common -fno-builtin-printf -Wall -ffixed-x30 -ffixed-x31 -fno-use-cxa-atexit
LDFLAGS=-static -nostdlib -nostartfiles -lgcc
CXX=riscv64-unknown-elf-g++
CPPFLAGS=-mcmodel=medany -I. -Wall -Wextra -pedantic -O2 -ffixed-x30 -ffixed-x31 -fno-common -fno-builtin-printf -Wno-unused-function -fno-exceptions -std=c++11


PROGRAMS = hello pwm blkdev accum charcount nic-loopback big-blkdev pingd
PROGRAMS += icenic-loopback-latency
PROGRAMS += icenic-loopback-throughput
PROGRAMS += icenic-loopback-throughput-batch
PROGRAMS += icenic-stream-batch
PROGRAMS += icenic-dot-product-batch
PROGRAMS += icenic-dot-product-test
PROGRAMS += icenic-nic-loopback
PROGRAMS += simple-icenic-cpu-loopback
PROGRAMS += icenic-tx-rx-throughput
PROGRAMS += icenic-nn-inference
PROGRAMS += icenic-othello
PROGRAMS += icenic-stream-test
PROGRAMS += icenic-nbody-node
PROGRAMS += cache_profile
PROGRAMS += icenic-service-time
PROGRAMS += icenic-service-time-batch

default: $(addsuffix .riscv,$(PROGRAMS))

dumps: $(addsuffix .dump,$(PROGRAMS))

%.o: %.S
	$(GCC) $(CFLAGS) -D__ASSEMBLY__=1 -c $< -o $@

%.o: %.c mmio.h
	$(GCC) $(CFLAGS) -c $< -o $@

%.o: %.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

lnic-multi-core-mica.riscv: lnic-multi-core-mica.o crt.o syscalls.o
	$(CXX) -T link.ld $(LDFLAGS) $^ -o $@

lnic-multi-core-chain-rep.riscv: lnic-multi-core-chain-rep.o crt.o syscalls.o
	$(CXX) -T link.ld $(LDFLAGS) $^ -o $@

lnic-int-collector.riscv: lnic-int-collector.o crt.o syscalls.o link.ld city_mod.o
	$(CXX) -T link.ld $(LDFLAGS) $^ -o $@

lnic-int-collector-opt.riscv: lnic-int-collector-opt.o crt.o syscalls.o link.ld city_mod.o
	$(CXX) -T link.ld $(LDFLAGS) $^ -o $@

%.riscv: %.o crt.o syscalls.o link.ld
	$(GCC) -T link.ld $(LDFLAGS) $< crt.o syscalls.o -o $@

%.dump: %.riscv
	$(OBJDUMP) -D $< > $@

clean:
	rm -f *.riscv *.o *.dump
