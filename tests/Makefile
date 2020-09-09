GCC=riscv64-unknown-elf-gcc
OBJDUMP=riscv64-unknown-elf-objdump
CFLAGS=-mcmodel=medany -std=gnu99 -O2 -fno-common -fno-builtin-printf -Wall -ffixed-x30 -ffixed-x31
LDFLAGS=-static -nostdlib -nostartfiles -lgcc
CXX=riscv64-unknown-elf-g++
CPPFLAGS=-mcmodel=medany -I. -Wall -Wextra -pedantic -O2 -ffixed-x30 -ffixed-x31 -fno-common -fno-builtin-printf -Wno-unused-function -fno-exceptions -std=c++11

PROGRAMS = hello pwm blkdev accum charcount nic-loopback big-blkdev pingd
PROGRAMS += lnic-nic-loopback-gpr
PROGRAMS += lnic-nic-loopback-csr
PROGRAMS += icenic-nic-loopback
PROGRAMS += lnic-cpu-loopback-gpr
PROGRAMS += lnic-cpu-loopback-csr
PROGRAMS += icenic-cpu-loopback
PROGRAMS += simple-icenic-cpu-loopback
PROGRAMS += lnic-throughput-gpr
PROGRAMS += icenic-throughput
PROGRAMS += lnic-nn-inference-gpr
PROGRAMS += icenic-nn-inference
PROGRAMS += lnic-othello-gpr
PROGRAMS += lnic-scheduler
PROGRAMS += icenic-othello
PROGRAMS += lnic-stream-gpr
PROGRAMS += icenic-stream
PROGRAMS += lnic-multi-thread-basic
PROGRAMS += lnic-nbody-node-gpr
PROGRAMS += icenic-nbody-node
PROGRAMS += lnic-send-recv
PROGRAMS += multiple_cores
PROGRAMS += lnic-multi-core
PROGRAMS += lnic-multi-core-different-services
PROGRAMS += lnic-incast
PROGRAMS += lnic-single-core-multi-thread
PROGRAMS += lnic-multi-core-multi-thread
PROGRAMS += lnic-multi-core-loopback
PROGRAMS += lnic-incast-multi-service
PROGRAMS += lnic-net-loopback
PROGRAMS += lnic-ping-latency
PROGRAMS += lnic-incast-nolock
PROGRAMS += lnic-evaluation
PROGRAMS += lnic-incast-single-core
PROGRAMS += lnic-multi-core-chain-rep
PROGRAMS += lnic-postcard-agg-sw
PROGRAMS += lnic-postcard-agg-opt
PROGRAMS += lnic-int-collector
PROGRAMS += lnic-int-collector-opt
PROGRAMS += lnic-int-heavy-hitter
PROGRAMS += lnic-int-path-latency

default: $(addsuffix .riscv,$(PROGRAMS))

dumps: $(addsuffix .dump,$(PROGRAMS))

%.o: %.S
	$(GCC) $(CFLAGS) -D__ASSEMBLY__=1 -c $< -o $@

%.o: %.c mmio.h
	$(GCC) $(CFLAGS) -c $< -o $@

%.o: %.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

city_mod.o: mica/util/cityhash/city_mod.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

lnic-multi-core-mica.riscv: lnic-multi-core-mica.o crt.o syscalls.o city_mod.o
	$(CXX) -T link.ld $(LDFLAGS) $^ -o $@

lnic-multi-core-chain-rep.riscv: lnic-multi-core-chain-rep.o crt.o syscalls.o city_mod.o
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
