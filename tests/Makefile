GCC=riscv64-unknown-elf-gcc
OBJDUMP=riscv64-unknown-elf-objdump
CFLAGS=-mcmodel=medany -std=gnu99 -O2 -fno-common -fno-builtin-printf -Wall -ffixed-x30 -ffixed-x31 -fno-use-cxa-atexit
LDFLAGS=-static -nostdlib -nostartfiles -lgcc
CXX=riscv64-unknown-elf-g++
CPPFLAGS=-mcmodel=medany -I. -Wall -Wextra -pedantic -O2 -fno-common -fno-builtin-printf -Wno-unused-function -fno-exceptions -std=c++11 -ffixed-x30 -ffixed-x31

PROGRAMS = hello pwm blkdev accum charcount nic-loopback big-blkdev pingd
PROGRAMS += lnic-nic-loopback-gpr
PROGRAMS += lnic-nic-loopback-csr
PROGRAMS += icenic-nic-loopback
PROGRAMS += lnic-cpu-loopback-gpr
PROGRAMS += lnic-cpu-loopback-csr
PROGRAMS += icenic-cpu-loopback
PROGRAMS += icenic-mica
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

default: $(addsuffix .riscv,$(PROGRAMS))

dumps: $(addsuffix .dump,$(PROGRAMS))

%.o: %.S
	$(GCC) $(CFLAGS) -D__ASSEMBLY__=1 -c $< -o $@

%.o: %.c mmio.h
	$(GCC) $(CFLAGS) -c $< -o $@

%.o: %.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

icenic-mica.riscv: icenic-mica.o crt.o syscalls.o
	$(CXX) -T link.ld $(LDFLAGS) $^ -o $@

%.riscv: %.o crt.o syscalls.o link.ld
	$(GCC) -T link.ld $(LDFLAGS) $< crt.o syscalls.o -o $@

%.dump: %.riscv
	$(OBJDUMP) -D $< > $@

clean:
	rm -f *.riscv *.o *.dump
